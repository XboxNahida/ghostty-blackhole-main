$ErrorActionPreference = "Stop"

$projectRoot = Split-Path -Parent $PSScriptRoot

function Require-Pattern {
    param(
        [string]$RelativePath,
        [string]$Pattern,
        [string]$Description
    )

    $path = Join-Path $projectRoot $RelativePath
    $content = Get-Content -Raw -Encoding UTF8 $path
    if ($content -notmatch $Pattern) {
        throw "Missing $Description in $RelativePath"
    }
}

Require-Pattern 'Blakhole_UI\core\blackholecore.h' 'rendererStartupFailed' 'structured renderer failure signal'
Require-Pattern 'Blakhole_UI\core\blackholecore.h' 'openRendererLogDirectory' 'log directory action'
Require-Pattern 'Blakhole_UI\core\blackholecore.cpp' '8000' 'startup timeout'
Require-Pattern 'Blakhole_UI\core\blackholecore.cpp' '200' 'log polling interval'
Require-Pattern 'Blakhole_UI\core\blackholecore.cpp' 'startRendererInternal\(bool userInitiated\)' 'automatic retry distinction'
Require-Pattern 'Blakhole_UI\core\blackholecore.cpp' 'rendererStartupFailed' 'diagnostic publication'

$header = Get-Content -Raw -Encoding UTF8 (Join-Path $projectRoot 'Blakhole_UI\core\blackholecore.h')
$source = Get-Content -Raw -Encoding UTF8 (Join-Path $projectRoot 'Blakhole_UI\core\blackholecore.cpp')
if ($header -notmatch 'RendererStartupState::Stopping' -and
    $source -notmatch 'RendererStartupState::Stopping' -and
    $source -notmatch 'processFinished\s*\(') {
    throw 'Renderer finished handling must respect the stopping state'
}

'RENDERER_DIAGNOSTICS_BACKEND_OK'
