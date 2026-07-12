$ErrorActionPreference = "Stop"

$projectRoot = Split-Path -Parent $PSScriptRoot
$mainPath = Join-Path $projectRoot "Blakhole_UI\Main.qml"
$pagesDir = Join-Path $projectRoot "Blakhole_UI\pages"

$mainText = Get-Content -Raw -Encoding UTF8 $mainPath
$missing = @()

Get-ChildItem -LiteralPath $pagesDir -Filter "*.qml" -File | ForEach-Object {
    $pageText = Get-Content -Raw -Encoding UTF8 $_.FullName
    if ($pageText -notmatch "property\s+var\s+bhCore") {
        return
    }

    $pageName = [System.IO.Path]::GetFileNameWithoutExtension($_.Name)
    $pattern = "Pages\.$([regex]::Escape($pageName))\s*\{(?<body>[\s\S]*?)\n\s*\}"
    $match = [regex]::Match($mainText, $pattern)
    if (-not $match.Success) {
        return
    }

    if ($match.Groups["body"].Value -notmatch "bhCore\s*:\s*blackHoleCore") {
        $missing += $pageName
    }
}

if ($missing.Count -gt 0) {
    throw "Missing bhCore binding in Main.qml: $($missing -join ', ')"
}

$blackholeConfig = Get-Content -Raw -Encoding UTF8 (Join-Path $pagesDir "BlackholeConfig.qml")
if ($blackholeConfig -match 'bhCore\.startRenderer\s*\(' -or
    $blackholeConfig -notmatch 'else\s+bhCore\.applyAndStart\s*\(\s*\)') {
    throw "Blackhole start button must use applyAndStart() so idle mode starts the idle timer"
}

"QML_CORE_BINDINGS_OK"
