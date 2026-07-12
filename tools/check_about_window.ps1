$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot

function Fail-AboutCheck {
    param([Parameter(Mandatory = $true)][string]$Message)
    throw "ABOUT_WINDOW_CHECK_FAILED: $Message"
}

function Require-Text {
    param([string]$Content, [string]$Text, [string]$Message)
    if (-not $Content.Contains($Text)) { Fail-AboutCheck $Message }
}

$aboutPath = Join-Path $root "Blakhole_UI\components\AboutWindow.qml"
$avatarPath = Join-Path $root "Blakhole_UI\components\EAvatar.qml"
$mainPath = Join-Path $root "Blakhole_UI\Main.qml"
$qrcPath = Join-Path $root "Blakhole_UI\src.qrc"

foreach ($path in @($aboutPath, $avatarPath, $mainPath, $qrcPath)) {
    if (-not (Test-Path -LiteralPath $path)) { Fail-AboutCheck "missing $path" }
}

$about = Get-Content -Raw -Encoding UTF8 -LiteralPath $aboutPath
$avatar = Get-Content -Raw -Encoding UTF8 -LiteralPath $avatarPath
$main = Get-Content -Raw -Encoding UTF8 -LiteralPath $mainPath
$qrc = Get-Content -Raw -Encoding UTF8 -LiteralPath $qrcPath
$pendingFeature = [Text.Encoding]::UTF8.GetString([Convert]::FromBase64String("6buR5rSe5ZCe5Zms5pWI5p6c6YCJ6aG5"))

Require-Text $about "Window" "AboutWindow must be an independent QML Window"
Require-Text $about "openAndActivate" "AboutWindow must expose openAndActivate"
Require-Text $about "chooseCustomAvatar" "AboutWindow must expose avatar replacement"
Require-Text $about "github.com/XboxNahida/ghostty-blackhole-main" "GitHub URL is missing"
Require-Text $about "QR_payment.jpg" "Alipay QR resource is missing"
Require-Text $about "WeChat_QR.png" "WeChat QR resource is missing"
Require-Text $about $pendingFeature "pending feature explanation is missing"
Require-Text $avatar "signal clicked" "EAvatar must expose a click signal"
Require-Text $avatar "clickable" "EAvatar must preserve optional click behavior"
Require-Text $main "aboutWindow.openAndActivate()" "Main.qml must open AboutWindow from both entries"
Require-Text $main "customAvatarUrl" "Main.qml must bind the shared avatar URL"
Require-Text $qrc "fonts/pic/QR_payment.jpg" "Alipay QR is not embedded in Qt resources"
Require-Text $qrc "fonts/pic/WeChat_QR.png" "WeChat QR is not embedded in Qt resources"

foreach ($relative in @("Blakhole_UI\fonts\pic\QR_payment.jpg", "Blakhole_UI\fonts\pic\WeChat_QR.png")) {
    if (-not (Test-Path -LiteralPath (Join-Path $root $relative))) {
        Fail-AboutCheck "missing binary resource $relative"
    }
}

"ABOUT_WINDOW_OK"
