$ErrorActionPreference = "Stop"

$projectRoot = Split-Path -Parent $PSScriptRoot
$programDirectoryText = [Text.Encoding]::UTF8.GetString(
    [Convert]::FromBase64String('5omT5byA56iL5bqP55uu5b2V'))
$logDirectoryText = [Text.Encoding]::UTF8.GetString(
    [Convert]::FromBase64String('5omT5byA5pel5b+X55uu5b2V'))

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
Require-Pattern 'Blakhole_UI\components\RendererErrorDialog.qml' 'function showFailure' 'dialog failure API'
Require-Pattern 'Blakhole_UI\components\RendererErrorDialog.qml' 'copy\(\)' 'copy details action'
Require-Pattern 'Blakhole_UI\Main.qml' 'onRendererStartupFailed' 'renderer failure connection'
Require-Pattern 'Blakhole_UI\Main.qml' 'root\.showNormal\(\)' 'restore minimized window'
Require-Pattern 'Blakhole_UI\Main.qml' 'root\.requestActivate\(\)' 'activate restored window'
Require-Pattern 'Blakhole_UI\Main.qml' 'systemTray\.visible\s*=\s*false' 'tray state reset'

$dialog = Get-Content -Raw -Encoding UTF8 (Join-Path $projectRoot 'Blakhole_UI\components\RendererErrorDialog.qml')
if ($dialog.Contains($logDirectoryText)) {
    throw 'Renderer failure dialog must identify the action as opening the program directory'
}
$buttonBlocks = [regex]::Matches(
    $dialog,
    '(?ms)^\s{16}Components\.EButton\s*\{.*?^\s{16}\}')
$programDirectoryButtons = @($buttonBlocks | Where-Object {
    $_.Value.Contains($programDirectoryText)
})
if ($programDirectoryButtons.Count -ne 1) {
    throw 'Renderer failure dialog must have exactly one program directory button'
}
if ($programDirectoryButtons[0].Value -notmatch
    'onClicked\s*:\s*if\s*\(core\)\s*core\.openRendererLogDirectory\s*\(\s*\)') {
    throw 'Program directory button must call openRendererLogDirectory'
}

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

$consumeFunctionStart = $source.IndexOf('RendererDiagnostic BlackHoleCore::consumeRendererStartupLog')
if ($consumeFunctionStart -lt 0) {
    throw 'Missing shared bounded renderer log consumer'
}
$consumeFunctionEnd = $source.IndexOf('void BlackHoleCore::pollRendererStartup()', $consumeFunctionStart)
if ($consumeFunctionEnd -lt 0) {
    throw 'Missing renderer startup polling function after shared log consumer'
}
$consumeFunction = $source.Substring(
    $consumeFunctionStart, $consumeFunctionEnd - $consumeFunctionStart)
if ($consumeFunction -match '\.readAll\s*\(') {
    throw 'Renderer log consumer must not use unbounded readAll()'
}
if ($consumeFunction -match 'lastModified\s*\(') {
    throw 'Renderer log consumer must not treat modification time as replacement'
}
if ($consumeFunction -notmatch '\.seek\s*\(' -or
    $consumeFunction -notmatch '\.read\s*\(\s*kMaxRendererLogReadBytes\s*\)') {
    throw 'Renderer log consumer must seek to an offset and use bounded chunk reads'
}
if ($consumeFunction -match 'readOffset\s*=\s*logInfo\.size\(\)\s*-\s*kMaxRendererLogReadBytes') {
    throw 'Renderer log backlog must be consumed sequentially instead of seeking to its tail'
}

$pollFunctionStart = $consumeFunctionEnd
$pollFunctionEnd = $source.IndexOf('void BlackHoleCore::publishRendererDiagnostic', $pollFunctionStart)
$pollFunction = $source.Substring($pollFunctionStart, $pollFunctionEnd - $pollFunctionStart)
if ($pollFunction -notmatch 'consumeRendererStartupLog\s*\(\s*kMaxRendererLogReadBytes\s*\)') {
    throw 'Timer polling must use the shared bounded renderer log consumer'
}

$finishedHandlerStart = $source.IndexOf(
    'connect(m_rendererProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished)')
$finishedHandlerEnd = $source.IndexOf(
    'connect(m_rendererProcess, &QProcess::errorOccurred', $finishedHandlerStart)
if ($finishedHandlerStart -lt 0 -or $finishedHandlerEnd -lt 0) {
    throw 'Missing renderer finished handler'
}
$finishedHandler = $source.Substring(
    $finishedHandlerStart, $finishedHandlerEnd - $finishedHandlerStart)
$finishedConsume = $finishedHandler.IndexOf('consumeRendererStartupLog')
$finishedState = $finishedHandler.IndexOf('processFinished')
if ($finishedConsume -lt 0 -or $finishedState -lt 0 -or
    $finishedConsume -gt $finishedState) {
    throw 'Renderer finished handler must consume remaining log before processFinished'
}
if ($finishedHandler -notmatch 'kMaxRendererLogDrainBytes') {
    throw 'Renderer finished log drain must have an explicit total byte limit'
}

'RENDERER_DIAGNOSTICS_UI_OK'
