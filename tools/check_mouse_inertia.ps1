$ErrorActionPreference = 'Stop'

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

Require-Pattern 'Blakhole_UI\core\blackholecore.h' 'Q_PROPERTY\(float\s+mouseInertia' 'Qt property'
Require-Pattern 'Blakhole_UI\core\blackholecore.h' 'Q_PROPERTY\(bool\s+limitMouseOvershoot' 'Qt overshoot limit property'
Require-Pattern 'Blakhole_UI\core\blackholecore.h' 'mouseInertiaChanged' 'Qt signal'
Require-Pattern 'Blakhole_UI\core\blackholecore.h' 'limitMouseOvershootChanged' 'Qt overshoot limit signal'
Require-Pattern 'Blakhole_UI\core\blackholecore.cpp' 'mouseInertia=' 'Qt config save/read key'
Require-Pattern 'Blakhole_UI\core\blackholecore.cpp' 'limitMouseOvershoot=' 'Qt overshoot limit save/read key'
Require-Pattern 'Blakhole_UI\pages\AdvancedConfig.qml' 'mouseInertia' 'QML local property'
Require-Pattern 'Blakhole_UI\pages\AdvancedConfig.qml' 'bhCore\.mouseInertia' 'QML core binding'
Require-Pattern 'Blakhole_UI\pages\AdvancedConfig.qml' 'limitMouseOvershoot' 'QML overshoot limit property'
Require-Pattern 'Blakhole_UI\pages\AdvancedConfig.qml' 'bhCore\.limitMouseOvershoot' 'QML overshoot limit binding'
Require-Pattern 'src\gui_config.h' 'mouseInertia' 'renderer config field'
Require-Pattern 'src\gui_config.h' 'limitMouseOvershoot' 'renderer overshoot limit field'
Require-Pattern 'src\gui_config.cpp' 'mouseInertia=' 'renderer config save/read key'
Require-Pattern 'src\gui_config.cpp' 'limitMouseOvershoot=' 'renderer overshoot limit save/read key'
Require-Pattern 'src\main.cpp' 'cfg\.mouseInertia' 'renderer mouse inertia logic'
Require-Pattern 'src\main.cpp' 'cfg\.limitMouseOvershoot' 'renderer overshoot limit branch'
Require-Pattern 'src\main.cpp' 'wanderRadius' 'mouse wander radius'
Require-Pattern 'src\main.cpp' 'mouseVelX' 'mouse velocity state'
Require-Pattern 'src\main.cpp' 'gravityStrength' 'gravity well attraction strength'
Require-Pattern 'src\main.cpp' 'gravitySoftening' 'gravity well softening distance'
Require-Pattern 'src\main.cpp' 'settleRadius' 'near-cursor settle radius'
Require-Pattern 'src\main.cpp' 'settleSpeed' 'near-cursor settle speed'
Require-Pattern 'src\main.cpp' 'gravityDeadZone' 'near-cursor gravity dead zone'
Require-Pattern 'src\main.cpp' 'maxGravityAccel' 'gravity acceleration cap'
Require-Pattern 'src\main.cpp' 'maxGravitySpeed' 'gravity speed cap'
Require-Pattern 'src\main.cpp' 'farReturnStrength' 'far-distance return force'
Require-Pattern 'src\main.cpp' 'worldMargin' 'physics boundary margin'
Require-Pattern 'src\main.cpp' 'renderMargin' 'off-screen render margin'
Require-Pattern 'src\main.cpp' 'maxSeparation' 'large-distance safety boundary'
Require-Pattern 'src\main.cpp' 'allowedRadius' 'bounded overshoot radius'
Require-Pattern 'src\main.cpp' 'outwardVel' 'outward velocity boundary damping'
Require-Pattern 'src\main.cpp' 'mouseDist' 'distance-to-cursor boundary check'

$mainText = Get-Content -Raw -Encoding UTF8 (Join-Path $root 'src\main.cpp')
if ($mainText -match 'float\s+spring\s*=') {
    throw 'Spring model is still present in src\main.cpp'
}
if ($mainText -match 'float\s+damping\s*=') {
    throw 'Elastic damping model is still present in src\main.cpp'
}
if ($mainText -match 'orbitForce') {
    throw 'Continuous orbit force is still present in src\main.cpp'
}
if ($mainText -match 'tangent[XY]') {
    throw 'Continuous tangent force is still present in src\main.cpp'
}
if ($mainText -match 'nearBrakeRadius') {
    throw 'Near-cursor braking model is still present in src\main.cpp'
}
if ($mainText -match 'float\s+maxSpeed\s*=') {
    throw 'Legacy hard velocity clamp is still present in src\main.cpp'
}
if ($mainText -match 'frameHomeX\s*<\s*0\.0f') {
    throw 'Unconditional screen-edge render clamp is still present in src\main.cpp'
}
if ($mainText -notmatch 'if\s*\(cfg\.limitMouseOvershoot\s*&&\s*gravityAccel\s*>\s*maxGravityAccel\)') {
    throw 'Gravity acceleration cap must only apply when overshoot limiting is enabled'
}
if ($mainText -notmatch 'if\s*\(cfg\.limitMouseOvershoot\s*&&\s*gravitySpeed\s*>\s*maxGravitySpeed') {
    throw 'Gravity speed cap must only apply when overshoot limiting is enabled'
}
if ($mainText -notmatch 'if\s*\(cfg\.limitMouseOvershoot\)\s*\{[\s\S]*worldMargin') {
    throw 'Physics world boundary must only apply when overshoot limiting is enabled'
}
if ($mainText -notmatch 'if\s*\(cfg\.limitMouseOvershoot\)\s*\{[\s\S]*renderMargin') {
    throw 'Render boundary must only apply when overshoot limiting is enabled'
}

'MOUSE_INERTIA_LINK_OK'
