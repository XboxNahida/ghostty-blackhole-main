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

$errorHandler = [regex]::Match(
    $source,
    'connect\(m_rendererProcess,\s*&QProcess::errorOccurred,[\s\S]*?\n\s*\}\);')
if (-not $errorHandler.Success) {
    throw 'Missing renderer errorOccurred handler'
}
if ($errorHandler.Value -match 'emit\s+rendererError') {
    throw 'errorOccurred must not directly publish legacy rendererError'
}
if ($errorHandler.Value -notmatch 'RendererStartupState::Starting' -or
    $errorHandler.Value -notmatch 'QProcess::FailedToStart') {
    throw 'FailedToStart diagnostics must be limited to the Starting state'
}

$startFunctionStart = $source.IndexOf('void BlackHoleCore::startRendererInternal(bool userInitiated)')
$startFunctionEnd = $source.IndexOf('void BlackHoleCore::pollRendererStartup()', $startFunctionStart)
if ($startFunctionStart -lt 0 -or $startFunctionEnd -lt 0) {
    throw 'Missing renderer start lifecycle functions'
}
$startFunction = $source.Substring($startFunctionStart, $startFunctionEnd - $startFunctionStart)
$failedCleanup = $startFunction.IndexOf('terminateRendererProcess();')
$failedStateGuard = $startFunction.IndexOf('RendererStartupState::Failed')
$runningAttemptReturn = $startFunction.IndexOf('renderer already running')
$clearFailureLatch = $startFunction.IndexOf('m_rendererFailureLatched = false')
if ($failedCleanup -lt 0 -or $failedStateGuard -lt 0 -or
    $failedCleanup -lt $failedStateGuard) {
    throw 'User retry must clean up a failed renderer process'
}
if ($clearFailureLatch -lt 0 -or $clearFailureLatch -lt $failedCleanup -or
    $clearFailureLatch -lt $runningAttemptReturn) {
    throw 'User retry must clear the failure latch only after failed-process cleanup'
}

$publishFunctionStart = $source.IndexOf('void BlackHoleCore::publishRendererDiagnostic')
$publishFunctionEnd = $source.IndexOf('void BlackHoleCore::openRendererLogDirectory', $publishFunctionStart)
if ($publishFunctionStart -lt 0 -or $publishFunctionEnd -lt 0) {
    throw 'Missing renderer diagnostic publication lifecycle functions'
}
$publishFunction = $source.Substring($publishFunctionStart, $publishFunctionEnd - $publishFunctionStart)
$publishCleanup = $publishFunction.IndexOf('terminateRendererProcess();')
$structuredSignal = $publishFunction.IndexOf('emit rendererStartupFailed')
if ($publishCleanup -lt 0) {
    throw 'Publishing a renderer failure must clean up the failed process'
}
if ($structuredSignal -lt 0 -or $publishCleanup -gt $structuredSignal) {
    throw 'Failed renderer cleanup must complete before structured failure publication'
}

if ($header -notmatch 'RendererStartupState::Stopping' -and
    $source -notmatch 'RendererStartupState::Stopping' -and
    $source -notmatch 'processFinished\s*\(') {
    throw 'Renderer finished handling must respect the stopping state'
}

'RENDERER_DIAGNOSTICS_BACKEND_OK'
