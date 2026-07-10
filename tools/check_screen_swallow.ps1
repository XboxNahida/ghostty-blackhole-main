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
Require-Pattern "src\main.cpp" "lensAmplification" "original lens amplification"
Require-Pattern "src\main.cpp" "dimensionCollapse" "near-field dimension collapse"
Require-Pattern "src\main.cpp" "collapseToDisk" "collapse into accretion disk"
Require-Pattern "src\main.cpp" "ringBirth" "accretion ring birth phase"
Require-Pattern "src\main.cpp" "ringGlow" "accretion ring glow"
Require-Pattern "src\main.cpp" "ringWidth" "accretion ring width"
Require-Pattern "src\main.cpp" "outerLensBlend" "smooth outer lens blend"
Require-Pattern "src\main.cpp" "softCollapse" "softened collapse boundary"
Require-Pattern "src\main.cpp" "boundaryDither" "non-circular collapse boundary dither"
Require-Pattern "src\main.cpp" "luminosityBoost" "swallow brightness compensation"
Require-Pattern "src\main.cpp" "swallowPhase" "shader injection swallow phase"
Require-Pattern "src\main.cpp" "continuousSwallow" "continuous runtime swallow phase"
Require-Pattern "src\main.cpp" "eventHorizonMask" "event horizon blackening"
Require-Pattern "src\main.cpp" "effectiveShaderSpeed" "slow shader trajectory speed in swallow mode"
Require-Pattern "src\main.cpp" "swallowMotionScale" "slow mouse-follow motion in swallow mode"

$mainText = Get-Content -Raw -Encoding UTF8 (Join-Path $PSScriptRoot "..\src\main.cpp")
if ($mainText -match "vec2\s+suckedP\s*=\s*lensP\s*\*\s*\(1\.0\s*\+\s*2\.8\s*\*\s*swallowWarp\)") {
    throw "Screen swallow still uses continuous radial warp instead of fragmented tidal infall"
}

if ($mainText -match "swallowPhase\s*=\s*[^;]*1\.0\s*-\s*uBornProgress") {
    throw "Screen swallow still depends on birth/die progress instead of running continuously"
}

if ($mainText -match "fragmentCell|fragmentHash|shardGate|cellScramble|tidalFilament|filamentPhase") {
    throw "Screen swallow still uses standalone shard/filament effects instead of amplified original lens collapse"
}

if ($mainText -match "col\s*=\s*mix\([^;]+collapseToDisk\);") {
    throw "Screen swallow still darkens the whole collapse disk with a hard radial weight"
}

Write-Output "SCREEN_SWALLOW_OK"
