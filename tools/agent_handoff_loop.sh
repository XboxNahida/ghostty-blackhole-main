#!/usr/bin/env bash
set -u

ROOT="/home/kiarelemb/下载/blackhole"
TURN_FILE="$ROOT/agent_turn.txt"
MAILBOX="$ROOT/debug_state.md"
LOG_FILE="/tmp/blackhole-agent-handoff.log"
LOCK_FILE="/tmp/blackhole-agent-handoff.lock"
STOP_FILE="/tmp/blackhole-agent-handoff.stop"

exec 9>"$LOCK_FILE"
if ! flock -n 9; then
    exit 0
fi

write_turn() {
    local value="$1"
    local tmp="$TURN_FILE.tmp"
    printf '%s\n' "$value" >"$tmp"
    mv "$tmp" "$TURN_FILE"
}

mailbox_status() {
    sed -n 's/^STATUS: \([A-Z_]*\)$/\1/p' "$MAILBOX" | head -1
}

log_line() {
    printf '[%s] %s\n' "$(date --iso-8601=seconds)" "$1" >>"$LOG_FILE"
}

run_deepseek() {
    log_line "DeepSeek turn started"
    reasonix run --max-steps 80 --continue \
        "读取 $MAILBOX 并严格按信箱当前 State 行动。你是 IMPLEMENTER，只做已授权的 DS 任务包；只验收本机 Linux，不做 Windows 兼容。执行快速闭环门禁，更新 State/IMPLEMENTER，完成后提交并清理工作区。不得改写 REVIEWER 区域。" \
        >>"$LOG_FILE" 2>&1
    log_line "DeepSeek turn ended with mailbox status $(mailbox_status)"
}

run_codex() {
    log_line "Codex turn started"
    codex -C "$ROOT" -s workspace-write -a never exec \
        "读取 $MAILBOX 并执行其中 Codex 验收协议。你是 REVIEWER。若 READY_FOR_REVIEW，检查真实 diff/调用点，在 /tmp 全新构建测试，只验收本机 Linux，将结果写回 State/REVIEWER。通过则授权计划中下一 DS；失败则写明最小修复和证据。不要修改 IMPLEMENTER 区域，不要为 Windows 增加工作。" \
        >>"$LOG_FILE" 2>&1
    log_line "Codex turn ended with mailbox status $(mailbox_status)"
}

log_line "Handoff loop started"
while [[ ! -e "$STOP_FILE" ]]; do
    turn="$(tr -d '\r\n' <"$TURN_FILE" 2>/dev/null || true)"
    case "$turn" in
        "deepseek 读取信箱")
            run_deepseek
            status="$(mailbox_status)"
            if [[ "$status" == "READY_FOR_REVIEW" || "$status" == "BLOCKED" ]]; then
                write_turn "codex 读取信箱"
            fi
            ;;
        "codex 读取信箱")
            run_codex
            status="$(mailbox_status)"
            if [[ "$status" == "ACCEPTED" || "$status" == "CHANGES_REQUESTED" ]]; then
                write_turn "deepseek 读取信箱"
            fi
            ;;
        *)
            log_line "Invalid turn file content: $turn"
            ;;
    esac
    sleep 30
done
log_line "Handoff loop stopped"
