#!/usr/bin/env bash
set -euo pipefail

readonly bus_name="io.github.xboxnahida.Blackhole"
readonly object_path="/io/github/xboxnahida/Blackhole"
readonly interface="$bus_name"
readonly warmup_seconds="${WARMUP_SECONDS:-5}"
readonly sample_seconds="${SAMPLE_SECONDS:-30}"
readonly rounds="${ROUNDS:-3}"
readonly output_dir="${1:-/tmp/blackhole-resource-profile-$(date +%Y%m%d-%H%M%S)}"

for command in gdbus pidstat pgrep timeout awk; do
    command -v "$command" >/dev/null || {
        echo "Missing required command: $command" >&2
        exit 1
    }
done

get_state() {
    gdbus call --session --dest "$bus_name" --object-path "$object_path" \
        --method "$interface.GetState" | grep -q true
}

set_state() {
    local method="$1"
    gdbus call --session --dest "$bus_name" --object-path "$object_path" \
        --method "$interface.$method" >/dev/null
}

original_state=stopped
if get_state; then
    original_state=running
fi
active_transition_pid=""

restore_state() {
    if [[ -n "$active_transition_pid" ]]; then
        kill "$active_transition_pid" 2>/dev/null || true
        wait "$active_transition_pid" 2>/dev/null || true
        active_transition_pid=""
    fi
    if [[ "$original_state" == running ]]; then
        set_state Start || true
    else
        set_state Stop || true
    fi
}
trap restore_state EXIT INT TERM

shell_pid="$(pgrep -n -x gnome-shell)"
ui_pid="$(pgrep -n -f '(^|/)appBlakholeUI([[:space:]]|$)' || true)"
pids="$shell_pid"
if [[ -n "$ui_pid" ]]; then
    pids+=",$ui_pid"
fi

# An idle-mode UI can call Start in the middle of a nominally stopped sample.
# Reject that setup instead of silently recording a mixture of stopped and
# running frames.  Callers can temporarily raise the idle threshold, restart
# the UI, profile, and then restore the original configuration.
readonly presets_path="${XDG_CONFIG_HOME:-$HOME/.config}/XboxNahida/Blakhole UI/blackhole_presets.txt"
if [[ -n "$ui_pid" && -r "$presets_path" ]]; then
    read -r display_mode idle_seconds _ < <(awk 'NR == 2 {print $1, $2; exit}' "$presets_path")
    if [[ "$display_mode" == 1 && "$idle_seconds" =~ ^[0-9]+$ ]]; then
        current_idle_ms="$(gdbus call --session \
            --dest org.gnome.Mutter.IdleMonitor \
            --object-path /org/gnome/Mutter/IdleMonitor/Core \
            --method org.gnome.Mutter.IdleMonitor.GetIdletime \
            | awk -F'uint64 |,' '{print $2}')"
        estimated_seconds=$((rounds * (4 * sample_seconds + 2 * warmup_seconds) + 5))
        if ((current_idle_ms + estimated_seconds * 1000 >= idle_seconds * 1000)); then
            echo "Idle-mode UI may auto-start during profiling: current_idle_ms=$current_idle_ms" \
                "idle_seconds=$idle_seconds estimated_seconds=$estimated_seconds" >&2
            exit 1
        fi
    fi
fi

mkdir -p "$output_dir"
{
    echo "date=$(date --iso-8601=seconds)"
    echo "gnome_shell_pid=$shell_pid"
    echo "ui_pid=${ui_pid:-not-running}"
    echo "warmup_seconds=$warmup_seconds"
    echo "sample_seconds=$sample_seconds"
    echo "rounds=$rounds"
    echo "original_state=$original_state"
} >"$output_dir/environment.txt"

sample() {
    local state="$1"
    local round="$2"
    local prefix="$output_dir/${state}-round-${round}"
    pidstat -h -u -r -w -p "$pids" 1 "$sample_seconds" >"$prefix.pidstat.txt" &
    local pidstat_pid=$!
    if command -v nvidia-smi >/dev/null; then
        timeout "${sample_seconds}s" nvidia-smi dmon -s pucvmet -d 1 \
            >"$prefix.nvidia-dmon.txt" 2>&1 &
    fi
    wait "$pidstat_pid"
}

for ((round = 1; round <= rounds; round++)); do
    set_state Stop
    sleep "$warmup_seconds"
    sample stopped "$round"

    # Repeated lifecycle transitions make the short 650 ms path measurable
    # over the same sampling window as steady states.
    (
        end=$((SECONDS + sample_seconds))
        while ((SECONDS < end)); do
            set_state Start
            sleep 0.7
            set_state Stop
            sleep 0.7
        done
    ) &
    active_transition_pid=$!
    sample transition "$round"
    wait "$active_transition_pid"
    active_transition_pid=""

    set_state Start
    sleep "$warmup_seconds"
    sample running "$round"

    # This state records the same compositor load plus the UI process. Launch
    # the UI with --minimized before profiling when ui_pid is not-running.
    sample ui-minimized "$round"
done

restore_state
trap - EXIT INT TERM
echo "Raw profile written to $output_dir"
