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
Require-Pattern "src\main.cpp" "ringBirth" "accretion ring birth phase"
Require-Pattern "src\main.cpp" "outerLensBlend" "smooth outer lens blend"
Require-Pattern "src\main.cpp" "longTailLens" "screen-wide long-tail lens field"
Require-Pattern "src\main.cpp" "swallowLensField" "edge-free swallow lens field"
Require-Pattern "src\main.cpp" "screenWideLensBlend" "screen-wide lens blend without a hard shell"
Require-Pattern "src\main.cpp" "edgeFreeWarp" "edge-free warp displacement"
Require-Pattern "src\main.cpp" "singleLensP" "single-pass swallow lens sample"
Require-Pattern "src\main.cpp" "noShellLensP" "shell-free lens sample"
Require-Pattern "src\main.cpp" "adaptiveCollapse" "distance-adaptive disk collapse"
Require-Pattern "src\main.cpp" "curvatureFalloff" "distance-dependent line curvature"
Require-Pattern "src\main.cpp" "accretionOrbitPhase" "time-varying accretion orbit phase"
Require-Pattern "src\main.cpp" "orbitalInfall" "orbital infall sample field"
Require-Pattern "src\main.cpp" "infallTangentFlow" "visible tangent flow into disk"
Require-Pattern "src\main.cpp" "projectedDiskP" "preset-projected accretion disk field"
Require-Pattern "src\main.cpp" "presetDiskAspect" "preset inclination disk aspect"
Require-Pattern "src\main.cpp" "projectedDiskRadius" "projected disk radius for collapse mask"
Require-Pattern "src\main.cpp" "screenTangentDir" "projected disk tangent direction"
Require-Pattern "src\main.cpp" "geodesicDiskEnergy" "geodesic disk energy driven collapse"
Require-Pattern "src\main.cpp" "geodesicDiskMask" "geodesic disk mask for real accretion shape"
Require-Pattern "src\main.cpp" "projectedCollapse" "projected fallback collapse field"
Require-Pattern "src\main.cpp" "gravityWellField" "continuous gravity-well swallow field"
Require-Pattern "src\main.cpp" "softDiskMatterField" "soft disk matter field without a hard shell"
Require-Pattern "src\main.cpp" "smoothInfallField" "smooth UI infall field"
Require-Pattern "src\main.cpp" "colorlessOuterLens" "colorless outer lens field"
Require-Pattern "src\main.cpp" "realAccretionMask" "preset-derived accretion mask"
Require-Pattern "src\main.cpp" "programmaticAccretion" "programmatic accretion disk color"
Require-Pattern "src\main.cpp" "uiFreeAccretion" "UI-free accretion disk color"
Require-Pattern "src\main.cpp" "uiSuppression" "UI color suppression near accretion disk"
Require-Pattern "src\main.cpp" "nearFieldUiCutoff" "near-field UI sample cutoff"
Require-Pattern "src\main.cpp" "wideTidalErase" "wide tidal UI debris erasure"
Require-Pattern "src\main.cpp" "uiDebrisSuppression" "broad warped UI debris suppression"
Require-Pattern "src\main.cpp" "tidalUiErase" "tidal UI color erasure near disk"
Require-Pattern "src\main.cpp" "opacityWake" "disk opacity wake for swallowed UI"
Require-Pattern "src\main.cpp" "photonRingFeather" "soft photon ring feather"
Require-Pattern "src\main.cpp" "softShadowMask" "soft event-horizon shadow transition"
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

if ($mainText -match "smoothstep\(0\.03,\s*0\.72,\s*originalWindow\)") {
    throw "Screen swallow still uses originalWindow as a visible lens boundary"
}

if ($mainText -match "ringRadius\s*=\s*mix|ringWidth\s*=\s*mix|ringGlow|accretionMatter") {
    throw "Screen swallow still uses a fixed synthetic colored ring instead of the preset disk"
}

if ($mainText -match "collapseToDisk|collapseP|spiralRot|diskLineP") {
    throw "Screen swallow still applies a second collapse/spiral warp after lensing"
}

if ($mainText -match "diskBandDistance\s*=\s*abs\(r\s*-\s*diskMid\)") {
    throw "Screen swallow still uses a circular radius band instead of preset-projected disk geometry"
}

if ($mainText -match "adaptiveCollapse\s*=\s*softCollapse\s*\*") {
    throw "Screen swallow still drives adaptiveCollapse only from projected geometry instead of geodesic disk energy"
}

if ($mainText -match "adaptiveCollapse\s*=\s*clamp\s*\(\s*max\s*\(\s*projectedCollapse") {
    throw "Screen swallow still hard-switches collapse through projected disk bands"
}

if ($mainText -match "col\s*=\s*mix\(col,\s*col\s*\*\s*mix") {
    throw "Screen swallow still tints the outer lens like a glass shell"
}

if ($mainText -match "mix\s*\(\s*mix\s*\(\s*baseLensP\s*,\s*singleLensP\s*,\s*outerLensBlend\s*\)") {
    throw "Screen swallow still blends through baseLensP, which creates a glass-shell sampling boundary"
}

if ($mainText -match "swallowedColor\s*=\s*deUiBg\s*\*\s*trans") {
    throw "Screen swallow still lets sampled UI color feed the accretion disk"
}

if ($mainText -match "uiSuppression\s*=\s*clamp\s*\(\s*max\s*\(\s*tidalUiErase\s*,\s*photonRingFeather") {
    throw "Screen swallow still suppresses only the near disk and leaves warped UI debris visible"
}

Write-Output "SCREEN_SWALLOW_OK"
