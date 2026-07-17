$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$qmlPath = Join-Path $root "Blakhole_UI\pages\BlackholeConfig.qml"
$corePath = Join-Path $root "Blakhole_UI\core\blackholecore.cpp"
$qml = Get-Content -Raw -Encoding UTF8 -LiteralPath $qmlPath
$core = Get-Content -Raw -Encoding UTF8 -LiteralPath $corePath
$failures = [System.Collections.Generic.List[string]]::new()

function Add-Failure([string]$message) {
    $failures.Add($message)
}

$slotLabel = [Text.Encoding]::UTF8.GetString(
    [Convert]::FromBase64String("6buR5rSe5Y+Y5o2i56eS5pWw"))

if (-not $qml.Contains($slotLabel) -and
    -not $qml.Contains('\u9ed1\u6d1e\u53d8\u6362\u79d2\u6570')) {
    Add-Failure "missing black-hole transition seconds label"
}
if ($qml -notmatch '(?s)id:\s*slotSpin.+?valueFromText:\s*function\s*\(text,\s*locale\).+?Number\.fromLocaleString\(locale,\s*text\).+?Math\.round\(parsed\s*\*\s*10\)') {
    Add-Failure "slotSpin does not convert decimal text to tenths"
}
if ($core -notmatch '(?s)int\s+dm\s*=\s*0;\s*ls\s*>>\s*dm;\s*if\s*\(ls\.status\(\)\s*==\s*QTextStream::Ok\)\s*m_screenTarget\s*=\s*dm;') {
    Add-Failure "screenTarget is not restored from a valid final field"
}
if ($core -match 'if\s*\(!ls\.atEnd\(\)\)\s*m_screenTarget\s*=\s*dm') {
    Add-Failure "screenTarget still rejects the final field at end of line"
}

if ($failures.Count -gt 0) {
    foreach ($failure in $failures) {
        Write-Output "V123_SETTINGS_FIXES_CHECK_FAILED: $failure"
    }
    exit 1
}

Write-Output "V123_SETTINGS_FIXES_OK"
