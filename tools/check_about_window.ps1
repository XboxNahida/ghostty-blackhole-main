$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$avatarPath = Join-Path $root "Blakhole_UI\components\EAvatar.qml"
$pageCheckPath = Join-Path $PSScriptRoot "check_about_page.ps1"

function Fail-AboutCheck {
    param([Parameter(Mandatory = $true)][string]$Message)
    throw "ABOUT_WINDOW_CHECK_FAILED: $Message"
}

foreach ($path in @($avatarPath, $pageCheckPath)) {
    if (-not (Test-Path -LiteralPath $path)) { Fail-AboutCheck "missing $path" }
}

& $pageCheckPath | Out-Null

$avatar = Get-Content -Raw -Encoding UTF8 -LiteralPath $avatarPath
if ($avatar -notmatch 'signal\s+clicked\s*\(') {
    Fail-AboutCheck "EAvatar must expose a click signal"
}
if ($avatar -notmatch 'property\s+bool\s+clickable') {
    Fail-AboutCheck "EAvatar must preserve optional click behavior"
}

"ABOUT_WINDOW_OK"
