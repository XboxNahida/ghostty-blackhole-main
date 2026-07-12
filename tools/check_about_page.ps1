$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$mainPath = Join-Path $root "Blakhole_UI\Main.qml"
$pagePath = Join-Path $root "Blakhole_UI\pages\AboutPage.qml"
$qrcPath = Join-Path $root "Blakhole_UI\src.qrc"

function Fail-AboutPageCheck {
    param([Parameter(Mandatory = $true)][string]$Message)
    throw "ABOUT_PAGE_CHECK_FAILED: $Message"
}

function Require-Match {
    param([string]$Content, [string]$Pattern, [string]$Message)
    if ($Content -notmatch $Pattern) { Fail-AboutPageCheck $Message }
}

foreach ($path in @($mainPath, $pagePath, $qrcPath)) {
    if (-not (Test-Path -LiteralPath $path)) { Fail-AboutPageCheck "missing $path" }
}

$main = Get-Content -Raw -Encoding UTF8 -LiteralPath $mainPath
$page = Get-Content -Raw -Encoding UTF8 -LiteralPath $pagePath
$qrc = Get-Content -Raw -Encoding UTF8 -LiteralPath $qrcPath
$pendingFeature = [Text.Encoding]::UTF8.GetString([Convert]::FromBase64String("6buR5rSe5ZCe5Zms5pWI5p6c6YCJ6aG5"))

if ($main -match 'Components\.AboutWindow\s*\{') {
    Fail-AboutPageCheck "Main.qml must not instantiate AboutWindow"
}
Require-Match $main 'readonly\s+property\s+int\s+aboutPageIndex\s*:\s*4' "Main.qml must define the about page index"
Require-Match $main 'function\s+navigateToAbout\s*\(\s*\)\s*\{[\s\S]*?(?:root\.)?currentPageIndex\s*===\s*(?:root\.)?aboutPageIndex[\s\S]*?return[\s\S]*?(?:root\.)?previousPageIndex\s*=\s*(?:root\.)?currentPageIndex[\s\S]*?(?:root\.)?currentPageIndex\s*=\s*(?:root\.)?aboutPageIndex' "about navigation must preserve the previous page on repeated entry"
Require-Match $main 'id\s*:\s*avatar[\s\S]*?onClicked\s*:\s*root\.navigateToAbout\(\)' "avatar entry must navigate to AboutPage"
Require-Match $main 'text\s*:\s*"(?:\\u5173\\u4e8e|关于)\s+Blakhole UI"[\s\S]*?MouseArea\s*\{[\s\S]*?onClicked\s*:\s*\{[\s\S]*?settingsDrawer\.close\(\)[\s\S]*?root\.navigateToAbout\(\)' "settings entry must close the drawer and navigate to AboutPage"
Require-Match $main 'Pages\.AboutPage\s*\{[\s\S]*?visible\s*:\s*root\.currentPageIndex\s*===\s*root\.aboutPageIndex' "content area must display AboutPage"
Require-Match $main 'Pages\.AboutPage\s*\{[\s\S]*?onBackRequested\s*:\s*root\.currentPageIndex\s*=\s*previousPageIndex' "AboutPage back action must restore the previous page"

Require-Match $page '^\s*import[\s\S]*?\bItem\s*\{' "AboutPage must be an in-window Item page"
if ($page -match '(?m)^\s*(?:Application)?Window\s*\{' -or $page -match '(?m)^\s*Popup\s*\{') {
    Fail-AboutPageCheck "AboutPage must not create Window or Popup types"
}
if ($page -match '\bScrollBar\b' -or $page -match '\bScrollView\b') {
    Fail-AboutPageCheck "AboutPage must use a Flickable without visible scroll bars"
}
Require-Match $page '\bFlickable\s*\{' "AboutPage must preserve wheel and touch scrolling"
Require-Match $page 'signal\s+backRequested\s*\(\s*\)' "AboutPage must expose a back request"
Require-Match $page 'onClicked\s*:\s*root\.backRequested\(\)' "back button must emit backRequested"
Require-Match $page 'customAvatarUrl' "AboutPage must display the persisted avatar"
Require-Match $page 'chooseCustomAvatar\s*\(\s*\)' "AboutPage avatar must open the custom avatar picker"
Require-Match $page 'github\.com/XboxNahida/ghostty-blackhole-main' "GitHub URL is missing"
Require-Match $page ([regex]::Escape($pendingFeature)) "pending black-hole swallowing effect explanation is missing"
Require-Match $page 'QR_payment\.jpg' "Alipay QR resource is missing"
Require-Match $page 'WeChat_QR\.png' "WeChat QR resource is missing"
Require-Match $qrc 'fonts/pic/QR_payment\.jpg' "Alipay QR is not embedded in Qt resources"
Require-Match $qrc 'fonts/pic/WeChat_QR\.png' "WeChat QR is not embedded in Qt resources"

"ABOUT_PAGE_OK"
