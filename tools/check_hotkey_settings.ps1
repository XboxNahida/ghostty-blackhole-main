$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot

function Require-Pattern {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$Pattern,
        [Parameter(Mandatory = $true)][string]$Label
    )

    $fullPath = Join-Path $root $Path
    $text = Get-Content -Raw -Encoding UTF8 $fullPath
    if ($text -notmatch $Pattern) {
        throw "Missing $Label in $Path"
    }
}

Require-Pattern "Blakhole_UI\core\blackholecore.h" "Q_PROPERTY\(bool\s+closeHotkeyEnabled" "close hotkey enabled Qt property"
Require-Pattern "Blakhole_UI\core\blackholecore.h" "Q_PROPERTY\(QString\s+closeHotkeySequence" "close hotkey sequence Qt property"
Require-Pattern "Blakhole_UI\core\blackholecore.h" "closeHotkeyStatusChanged" "close hotkey status signal"
Require-Pattern "Blakhole_UI\core\blackholecore.cpp" "RegisterHotKey" "Windows global hotkey registration"
Require-Pattern "Blakhole_UI\core\blackholecore.cpp" "UnregisterHotKey" "Windows global hotkey cleanup"
Require-Pattern "Blakhole_UI\core\blackholecore.cpp" "WM_HOTKEY" "Windows hotkey event handling"
Require-Pattern "Blakhole_UI\core\blackholecore.cpp" "stopAll\(\)" "hotkey stops renderer"
Require-Pattern "Blakhole_UI\core\blackholecore.cpp" "closeHotkeyEnabled=" "system config saves enabled flag"
Require-Pattern "Blakhole_UI\core\blackholecore.cpp" "closeHotkeySequence=" "system config saves sequence"
Require-Pattern "Blakhole_UI\Main.qml" "closeHotkeyEnabled" "settings drawer hotkey enabled binding"
Require-Pattern "Blakhole_UI\Main.qml" "recordingHotkey" "settings drawer hotkey recording state"
Require-Pattern "Blakhole_UI\Main.qml" "closeHotkeySequence" "settings drawer hotkey sequence binding"
Require-Pattern "Blakhole_UI\Main.qml" "hotkeyRecorder" "dedicated hotkey recording focus item"
Require-Pattern "Blakhole_UI\Main.qml" "hotkeyRecorder\.forceActiveFocus" "record button moves focus to recorder"
Require-Pattern "Blakhole_UI\Main.qml" "onCloseHotkeySequenceChanged" "QML sequence sync from core"
Require-Pattern "Blakhole_UI\Main.qml" "onCloseHotkeyStatusChanged" "QML status sync from core"

Write-Output "HOTKEY_SETTINGS_OK"
