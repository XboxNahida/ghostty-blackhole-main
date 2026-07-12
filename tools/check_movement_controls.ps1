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

function Reject-Pattern {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$Pattern,
        [Parameter(Mandatory = $true)][string]$Label
    )

    $fullPath = Join-Path $root $Path
    $text = Get-Content -Raw -Encoding UTF8 $fullPath
    if ($text -match $Pattern) {
        throw "Forbidden $Label in $Path"
    }
}

Require-Pattern 'Blakhole_UI\core\blackholecore.h' 'Q_PROPERTY\(int\s+spawnPosition' 'spawn position property'
Require-Pattern 'Blakhole_UI\core\blackholecore.h' 'Q_PROPERTY\(float\s+movementSpeed' 'movement speed property'
Require-Pattern 'Blakhole_UI\pages\AdvancedConfig.qml' 'text:\s*"\u9ed1\u6d1e\u51fa\u73b0\u4f4d\u7f6e"' 'spawn position label'
Require-Pattern 'Blakhole_UI\pages\AdvancedConfig.qml' 'label:\s*"\u9ed1\u6d1e\u79fb\u52a8\u901f\u5ea6"' 'movement speed slider'
Require-Pattern 'Blakhole_UI\pages\AdvancedConfig.qml' '\bFlickable\s*\{' 'scrollable advanced page'
Require-Pattern 'src\gui_config.h' 'int\s+spawnPosition\s*=\s*0' 'renderer spawn position field'
Require-Pattern 'src\gui_config.h' 'float\s+movementSpeed\s*=\s*1\.0f' 'renderer movement speed field'
Require-Pattern 'src\gui_config.cpp' 'spawnPosition=' 'renderer spawn position config key'
Require-Pattern 'src\gui_config.cpp' 'movementSpeed=' 'renderer movement speed config key'
Require-Pattern 'src\gui_config.cpp' 'ClampMovementSpeed' 'renderer movement speed clamp'
Require-Pattern 'src\main.cpp' 'ResolveMovementSpawn' 'spawn resolver use'
Require-Pattern 'src\main.cpp' 'uMovementTime' 'movement time uniform'

Reject-Pattern 'Blakhole_UI\pages\AdvancedConfig.qml' '\u8def\u5f84\u968f\u673a\u5316' 'legacy random path label'
Reject-Pattern 'Blakhole_UI\pages\AdvancedConfig.qml' 'label:\s*"\u52a8\u753b\u901f\u5ea6"' 'legacy animation speed slider'
Reject-Pattern 'Blakhole_UI\pages\AdvancedConfig.qml' '\bScrollBar\b' 'scroll bar'
Reject-Pattern 'Blakhole_UI\pages\AdvancedConfig.qml' '\bScrollView\b' 'scroll view'

$rendererConfigText = Get-Content -Raw -Encoding UTF8 (Join-Path $root 'src\gui_config.cpp')
if ($rendererConfigText -match 'fprintf\([^\r\n]*"randomPath=') {
    throw 'Renderer still saves the legacy randomPath key'
}

$mainText = Get-Content -Raw -Encoding UTF8 (Join-Path $root 'src\main.cpp')
$mouseBlock = [regex]::Match(
    $mainText,
    'if\s*\(cfg\.followMouse\)\s*\{[\s\S]*?\r?\n\s*\}\r?\n\r?\n\s*// \u9ed1\u6d1e\u751f\u957f/\u6e6e\u706d\u8fdb\u5ea6'
)
if (-not $mouseBlock.Success) {
    throw 'Unable to locate renderer mouse-follow block'
}
if ($mouseBlock.Value -match 'movementSpeed') {
    throw 'Mouse-follow physics must keep using real time'
}

'MOVEMENT_CONTROLS_OK'
