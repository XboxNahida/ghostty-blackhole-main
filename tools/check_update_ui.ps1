$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot

function Fail-UpdateUiCheck {
    param([Parameter(Mandatory = $true)][string]$Message)
    throw "UPDATE_UI_CHECK_FAILED: $Message"
}

function Require-Text {
    param([string]$Content, [string]$Text, [string]$Message)
    if (-not $Content.Contains($Text)) { Fail-UpdateUiCheck $Message }
}

function Require-Match {
    param([string]$Content, [string]$Pattern, [string]$Message)
    if ($Content -notmatch $Pattern) { Fail-UpdateUiCheck $Message }
}

$mainPath = Join-Path $root "Blakhole_UI\Main.qml"
$dialogPath = Join-Path $root "Blakhole_UI\components\UpdateDialog.qml"

foreach ($path in @($mainPath, $dialogPath)) {
    if (-not (Test-Path -LiteralPath $path)) { Fail-UpdateUiCheck "missing $path" }
}

$main = Get-Content -Raw -Encoding UTF8 -LiteralPath $mainPath
$dialog = Get-Content -Raw -Encoding UTF8 -LiteralPath $dialogPath
$checkUpdateText = [Text.Encoding]::UTF8.GetString([Convert]::FromBase64String("5qOA5p+l5pu05paw"))
$currentVersionPrefix = [Text.Encoding]::UTF8.GetString([Convert]::FromBase64String("5b2T5YmN54mI5pysIHY="))

Require-Text -Content $main -Text "Components.UpdateDialog" -Message "Main.qml must create the shared update dialog"
Require-Match -Content $main -Pattern 'id\s*:\s*settingsUpdateBadge[\s\S]*?width\s*:\s*\d+[\s\S]*?height\s*:\s*\d+[\s\S]*?visible\s*:\s*updateChecker\.updateAvailable' -Message "settings badge must have fixed dimensions and bind visibility only to updateAvailable"
Require-Text -Content $main -Text ('text: "' + $checkUpdateText + '"') -Message "settings drawer must expose the manual update row"
Require-Text -Content $main -Text ('"' + $currentVersionPrefix + '" + Qt.application.version') -Message "settings row must show the current application version"
Require-Text -Content $main -Text "updateChecker.statusText" -Message "settings row must show the update status"
Require-Text -Content $main -Text "updateChecker.checkAutomatically()" -Message "startup must trigger the automatic check"
$completedBlocks = [regex]::Matches($main, 'Component\.onCompleted\s*:\s*\{(?<body>[\s\S]*?)\n\s*\}')
$automaticBlocks = @($completedBlocks | Where-Object {
    $_.Groups['body'].Value -match 'updateChecker\.checkAutomatically\s*\(\s*\)'
})
if ($automaticBlocks.Count -ne 1) {
    Fail-UpdateUiCheck "automatic check must be called from Component.onCompleted"
}
$completedBody = $automaticBlocks[0].Groups['body'].Value
if ($completedBody -match 'updateDialog\.open\s*\(') {
    Fail-UpdateUiCheck "automatic startup path must never open the update dialog"
}
Require-Match -Content $main -Pattern 'onClicked\s*:\s*updateChecker\.checkManually\s*\(\s*\)' -Message "manual button must call checkManually()"
Require-Match -Content $main -Pattern 'Connections\s*\{[\s\S]*?target\s*:\s*updateChecker[\s\S]*?function\s+onManualResultReady\s*\(\s*\)[\s\S]*?updateDialog\.open\s*\(\s*\)' -Message "manualResultReady must open the update dialog"
if ([regex]::Matches($main, 'updateDialog\.open\s*\(\s*\)').Count -ne 1) {
    Fail-UpdateUiCheck "UpdateDialog must open exclusively from manualResultReady"
}

Require-Text -Content $dialog -Text "Qt.application.version" -Message "dialog must show the current application version"
Require-Text -Content $dialog -Text "checker.latestVersion" -Message "dialog must show the latest release version"
Require-Text -Content $dialog -Text "checker.latestNotes" -Message "dialog must show release notes"
Require-Text -Content $dialog -Text "checker.openDownloadPage()" -Message "download button must open the release page"
Require-Text -Content $dialog -Text "checker.ignoreCurrentRelease()" -Message "ignore button must suppress the current release"
Require-Match -Content $dialog -Pattern 'checker\.ignoreCurrentRelease\s*\(\s*\)[\s\S]*?dialogRoot\.close\s*\(\s*\)' -Message "ignore action must close the dialog"
Require-Text -Content $dialog -Text "wrapMode: Text.WordWrap" -Message "dialog text must wrap instead of overflowing"

"UPDATE_UI_OK"
