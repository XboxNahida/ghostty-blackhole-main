$ErrorActionPreference = "Stop"

function Require-Pattern {
    param(
        [string]$Path,
        [string]$Pattern,
        [string]$Description
    )

    $fullPath = Join-Path $PSScriptRoot "..\$Path"
    if (-not (Test-Path $fullPath)) {
        throw "Missing file for $Description`: $Path"
    }

    $text = Get-Content -Raw -Encoding UTF8 $fullPath
    if ($text -notmatch $Pattern) {
        throw "Missing $Description in $Path"
    }
}

Require-Pattern "Blakhole_UI\core\blackholecore.h" "Q_PROPERTY\(float\s+swallowStrength" "Qt swallow strength property"
Require-Pattern "Blakhole_UI\core\blackholecore.h" "swallowStrengthChanged" "Qt swallow strength signal"
Require-Pattern "Blakhole_UI\core\blackholecore.cpp" "swallowStrength=" "Qt advanced config save/read key"
Require-Pattern "Blakhole_UI\pages\AdvancedConfig.qml" "swallowStrength" "QML swallow strength state"
Require-Pattern "Blakhole_UI\pages\AdvancedConfig.qml" "bhCore\.swallowStrength" "QML core swallow strength binding"
Require-Pattern "src\gui_config.h" "swallowStrength" "renderer config field"
Require-Pattern "src\gui_config.cpp" "swallowStrength=" "renderer config save/read key"
Require-Pattern "src\main.cpp" "uSwallowStrength" "renderer swallow strength uniform"
Require-Pattern "src\main.cpp" "cfg\.swallowStrength" "renderer swallow strength upload"
Require-Pattern "shaders\frag_desktop_header.glsl" "uSwallowStrength" "desktop shader swallow strength uniform"
Require-Pattern "src\main.cpp" "swallowWarp" "shader injection radial swallow warp"
Require-Pattern "src\main.cpp" "swallowPhase" "shader injection swallow phase"

Write-Output "SCREEN_SWALLOW_OK"
