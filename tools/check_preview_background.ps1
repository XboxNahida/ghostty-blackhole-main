$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$sourcePath = Join-Path $root "Blakhole_UI\fonts\pic\Starry_sky_background.png"
$qrcPath = Join-Path $root "Blakhole_UI\src.qrc"
$rendererPath = Join-Path $root "Blakhole_UI\core\blackholepreviewfbo.cpp"
$packagePath = Join-Path $root "package_release.ps1"

if (-not (Test-Path -LiteralPath $sourcePath -PathType Leaf)) {
    throw "PREVIEW_BACKGROUND_CHECK_FAILED: source background image is missing"
}

$qrc = Get-Content -Raw -Encoding UTF8 -LiteralPath $qrcPath
$renderer = Get-Content -Raw -Encoding UTF8 -LiteralPath $rendererPath
$package = Get-Content -Raw -Encoding UTF8 -LiteralPath $packagePath

if ($qrc -notmatch 'fonts/pic/Starry_sky_background\.png') {
    throw "PREVIEW_BACKGROUND_CHECK_FAILED: background image is not embedded in Qt resources"
}
if ($renderer -notmatch ':/new/prefix1/fonts/pic/Starry_sky_background\.png') {
    throw "PREVIEW_BACKGROUND_CHECK_FAILED: preview renderer does not try the Qt resource path"
}
if ($package -notmatch 'Starry_sky_background\.png') {
    throw "PREVIEW_BACKGROUND_CHECK_FAILED: packaging does not preserve the external background file"
}

"PREVIEW_BACKGROUND_OK"
