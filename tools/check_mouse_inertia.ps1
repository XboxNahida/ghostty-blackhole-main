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
Require-Pattern 'Blakhole_UI\core\blackholecore.h' 'mouseInertiaChanged' 'Qt signal'
Require-Pattern 'Blakhole_UI\core\blackholecore.cpp' 'mouseInertia=' 'Qt config save/read key'
Require-Pattern 'Blakhole_UI\pages\AdvancedConfig.qml' 'mouseInertia' 'QML local property'
Require-Pattern 'Blakhole_UI\pages\AdvancedConfig.qml' 'bhCore\.mouseInertia' 'QML core binding'
Require-Pattern 'src\gui_config.h' 'mouseInertia' 'renderer config field'
Require-Pattern 'src\gui_config.cpp' 'mouseInertia=' 'renderer config save/read key'
Require-Pattern 'src\main.cpp' 'cfg\.mouseInertia' 'renderer mouse inertia logic'
Require-Pattern 'src\main.cpp' 'wanderRadius' 'mouse wander radius'
Require-Pattern 'src\main.cpp' 'mouseVelX' 'mouse velocity state'
Require-Pattern 'src\main.cpp' 'spring' 'spring follow model'
Require-Pattern 'src\main.cpp' 'damping' 'velocity damping'
Require-Pattern 'src\main.cpp' 'maxSpeed' 'velocity clamp'

'MOUSE_INERTIA_LINK_OK'
