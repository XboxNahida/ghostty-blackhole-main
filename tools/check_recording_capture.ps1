$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$main = Get-Content -Encoding UTF8 (Join-Path $root "src/main.cpp") -Raw
$win32 = Get-Content -Encoding UTF8 (Join-Path $root "src/win32_gl.cpp") -Raw
$cfgH = Get-Content -Encoding UTF8 (Join-Path $root "src/gui_config.h") -Raw
$cfgCpp = Get-Content -Encoding UTF8 (Join-Path $root "src/gui_config.cpp") -Raw
$qtCoreH = Get-Content -Encoding UTF8 (Join-Path $root "Blakhole_UI/core/blackholecore.h") -Raw
$qtCoreCpp = Get-Content -Encoding UTF8 (Join-Path $root "Blakhole_UI/core/blackholecore.cpp") -Raw
$qtQml = Get-Content -Encoding UTF8 (Join-Path $root "Blakhole_UI/pages/AdvancedConfig.qml") -Raw

$required = @(
    @{ Name = "allowRecordingCapture config"; Text = "allowRecordingCapture" },
    @{ Name = "recordingCaptureRuntimeAllowed"; Text = "recordingCaptureRuntimeAllowed" },
    @{ Name = "recordingCaptureFrozen"; Text = "recordingCaptureFrozen" },
    @{ Name = "RegisterHotKey"; Text = "RegisterHotKey" },
    @{ Name = "WM_HOTKEY"; Text = "WM_HOTKEY" },
    @{ Name = "Win32GL_TakeRecordingHotkey"; Text = "Win32GL_TakeRecordingHotkey" },
    @{ Name = "UnregisterHotKey"; Text = "UnregisterHotKey" },
    @{ Name = "Win32GL_SetCaptureExcluded"; Text = "Win32GL_SetCaptureExcluded" },
    @{ Name = "WDA_NONE restore"; Text = "WDA_NONE" }
)

foreach ($item in $required) {
    $all = "$main`n$win32`n$cfgH`n$cfgCpp`n$qtCoreH`n$qtCoreCpp`n$qtQml"
    if (-not $all.Contains($item.Text)) {
        throw "RECORDING_CAPTURE_CHECK_FAILED: missing $($item.Name) [$($item.Text)]"
    }
}

if (-not $main.Contains("bool recordingCaptureRuntimeAllowed = cfg.allowRecordingCapture") -or -not $main.Contains("Win32GL_SetCaptureExcluded(wgl, !recordingCaptureRuntimeAllowed)") -or -not $win32.Contains("DWORD affinity = excluded ? WDA_EXCLUDEFROMCAPTURE : WDA_NONE") -or -not $win32.Contains("SetWindowDisplayAffinity(wgl.hwnd, affinity)")) {
    throw "RECORDING_CAPTURE_CHECK_FAILED: capture affinity is not controlled by allowRecordingCapture"
}

if ($main.Contains("captureFreezeIntervalSec") -or $main.Contains("nextCaptureUpdate") -or $main.Contains("now >= nextCaptureUpdate")) {
    throw "RECORDING_CAPTURE_CHECK_FAILED: recording mode must not refresh captured desktop after freeze"
}

if (-not $main.Contains("bool captureUpdateDue = !recordingCaptureFrozen") -or -not $main.Contains("recordingCaptureFrozen = recordingCaptureRuntimeAllowed")) {
    throw "RECORDING_CAPTURE_CHECK_FAILED: one-shot freeze cadence is missing"
}

if (-not $main.Contains("'R'") -or -not $main.Contains("Ctrl+Alt+R")) {
    throw "RECORDING_CAPTURE_CHECK_FAILED: runtime recording capture hotkey is missing"
}

if ($main.Contains("GetAsyncKeyState") -or $main.Contains("ToggleRecordingCaptureHotkeyPressed")) {
    throw "RECORDING_CAPTURE_CHECK_FAILED: runtime hotkey must use WM_HOTKEY instead of keyboard polling"
}

if (-not $qtCoreH.Contains("Q_PROPERTY(bool allowRecordingCapture") -or -not $qtQml.Contains("bhCore.allowRecordingCapture")) {
    throw "RECORDING_CAPTURE_CHECK_FAILED: Qt UI binding is missing"
}

Write-Output "RECORDING_CAPTURE_OK"
