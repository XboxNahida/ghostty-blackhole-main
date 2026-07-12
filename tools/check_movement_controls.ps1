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

Reject-Pattern 'Blakhole_UI\pages\AdvancedConfig.qml' '\u8def\u5f84\u968f\u673a\u5316' 'legacy random path label'
Reject-Pattern 'Blakhole_UI\pages\AdvancedConfig.qml' 'label:\s*"\u52a8\u753b\u901f\u5ea6"' 'legacy animation speed slider'
Reject-Pattern 'Blakhole_UI\pages\AdvancedConfig.qml' '\bScrollBar\b' 'scroll bar'
Reject-Pattern 'Blakhole_UI\pages\AdvancedConfig.qml' '\bScrollView\b' 'scroll view'

'MOVEMENT_CONTROLS_OK'
