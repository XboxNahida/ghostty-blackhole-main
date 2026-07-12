$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$header = Get-Content -Raw -Encoding UTF8 (Join-Path $root "Blakhole_UI\core\blackholecore.h")
$core = Get-Content -Raw -Encoding UTF8 (Join-Path $root "Blakhole_UI\core\blackholecore.cpp")
$page = Get-Content -Raw -Encoding UTF8 (Join-Path $root "Blakhole_UI\pages\IdleListConfig.qml")
$resource = Get-Content -Raw -Encoding UTF8 (Join-Path $root "Blakhole_UI\app.rc")
$manifestPath = Join-Path $root "Blakhole_UI\app.manifest"
$qmlFiles = Get-ChildItem -LiteralPath (Join-Path $root "Blakhole_UI") -Recurse -Filter "*.qml" -File
$qml = ($qmlFiles | ForEach-Object { Get-Content -Raw -Encoding UTF8 $_.FullName }) -join "`n"

foreach ($text in @(
    "Q_INVOKABLE QVariantList runningApplications() const",
    "Q_INVOKABLE QVariantMap chooseExecutable()"
)) {
    if (-not $header.Contains($text)) {
        throw "IDLE_LIST_PICKERS_CHECK_FAILED: missing Core API [$text]"
    }
}

foreach ($text in @(
    "Q_PROPERTY(QString idleDetectionSummary",
    "Q_PROPERTY(bool idleDetectionBlocked",
    "void idleDetectionSummaryChanged()",
    "void idleDetectionBlockedChanged()"
)) {
    if (-not $header.Contains($text)) {
        throw "IDLE_LIST_PICKERS_CHECK_FAILED: missing diagnostic property [$text]"
    }
}

foreach ($text in @(
    "setIdleDetectionState(",
    "bhCore.idleDetectionSummary",
    "bhCore.idleDetectionBlocked"
)) {
    $diagnosticText = if ($text.StartsWith("bhCore.")) { $page } else { $core }
    if (-not $diagnosticText.Contains($text)) {
        throw "IDLE_LIST_PICKERS_CHECK_FAILED: missing diagnostic integration [$text]"
    }
}

foreach ($text in @(
    "ApplicationCatalog_EnumerateRunning(GetCurrentProcessId())",
    "QFileDialog::getOpenFileName",
    '"displayName"',
    '"processName"',
    '"executablePath"',
    '"iconDataUrl"'
)) {
    if (-not $core.Contains($text)) {
        throw "IDLE_LIST_PICKERS_CHECK_FAILED: missing Core implementation [$text]"
    }
}

foreach ($text in @(
    "manualAddRequested",
    "runningPickerRequested",
    "executablePickerRequested",
    "applicationSelected"
)) {
    if (-not $qml.Contains($text)) {
        throw "IDLE_LIST_PICKERS_CHECK_FAILED: missing QML picker contract [$text]"
    }
}

foreach ($text in @(
    "Components.RunningApplicationPicker",
    "bhCore.runningApplications()",
    "bhCore.chooseExecutable()"
)) {
    if (-not $page.Contains($text)) {
        throw "IDLE_LIST_PICKERS_CHECK_FAILED: picker is not connected in page [$text]"
    }
}

$panelCount = ([regex]::Matches($page, "Components\.IdleListPanel\s*\{")).Count
if ($panelCount -ne 3) {
    throw "IDLE_LIST_PICKERS_CHECK_FAILED: expected 3 connected IdleListPanel instances, got $panelCount"
}

if (-not (Test-Path -LiteralPath $manifestPath)) {
    throw "IDLE_LIST_PICKERS_CHECK_FAILED: missing DPI manifest"
}
$manifest = Get-Content -Raw -Encoding UTF8 $manifestPath
if (-not $manifest.Contains("PerMonitorV2") -or -not $resource.Contains('1 24 "app.manifest"')) {
    throw "IDLE_LIST_PICKERS_CHECK_FAILED: Qt UI must embed a PerMonitorV2 manifest"
}

Write-Output "IDLE_LIST_PICKERS_OK"
