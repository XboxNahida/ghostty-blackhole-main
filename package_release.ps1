$ErrorActionPreference = "Stop"

$ProjectRoot = $PSScriptRoot
$UiBuildDir = Join-Path $ProjectRoot "Blakhole_UI\build\Desktop_Qt_6_11_1_MinGW_64_bit-Release"
$CoreBuildDir = Join-Path $ProjectRoot "build"
$ReleaseDir = Join-Path $ProjectRoot "release"

function Copy-DirectoryIfExists {
    param(
        [Parameter(Mandatory = $true)][string]$Source,
        [Parameter(Mandatory = $true)][string]$Destination
    )

    if (-not (Test-Path -LiteralPath $Source)) {
        return
    }

    New-Item -ItemType Directory -Force -Path $Destination | Out-Null
    & robocopy $Source $Destination /E /NFL /NDL /NJH /NJS /NP | Out-Null
    if ($LASTEXITCODE -gt 7) {
        throw "robocopy failed: $Source -> $Destination, exit code $LASTEXITCODE"
    }
}

function Copy-FileIfExists {
    param(
        [Parameter(Mandatory = $true)][string]$Source,
        [Parameter(Mandatory = $true)][string]$Destination
    )

    if (Test-Path -LiteralPath $Source) {
        Copy-Item -LiteralPath $Source -Destination $Destination -Force
    }
}

if (-not (Test-Path -LiteralPath (Join-Path $CoreBuildDir "blackhole.exe"))) {
    throw "Missing build\blackhole.exe. Run: cmake --build build --config Release"
}

if (-not (Test-Path -LiteralPath (Join-Path $UiBuildDir "appBlakholeUI.exe"))) {
    throw "Missing Qt UI build output. Run: cmake --build Blakhole_UI\build\Desktop_Qt_6_11_1_MinGW_64_bit-Release --config Release"
}

Write-Host "[1/5] Preparing release directory..."
New-Item -ItemType Directory -Force -Path $ReleaseDir | Out-Null

Write-Host "[2/5] Copying renderer..."
Copy-Item -LiteralPath (Join-Path $CoreBuildDir "blackhole.exe") -Destination $ReleaseDir -Force
Get-ChildItem -LiteralPath $CoreBuildDir -Filter "*.dll" -File -ErrorAction SilentlyContinue |
    Copy-Item -Destination $ReleaseDir -Force

Write-Host "[3/5] Copying Qt UI and deployed runtime..."
Copy-Item -LiteralPath (Join-Path $UiBuildDir "appBlakholeUI.exe") -Destination $ReleaseDir -Force
Get-ChildItem -LiteralPath $UiBuildDir -Filter "*.dll" -File -ErrorAction SilentlyContinue |
    Copy-Item -Destination $ReleaseDir -Force

foreach ($dir in @(
    "BlakholeUI",
    "fonts",
    "generic",
    "iconengines",
    "imageformats",
    "networkinformation",
    "platforms",
    "qml",
    "qmltooling",
    "styles",
    "tls",
    "translations"
)) {
    Copy-DirectoryIfExists `
        -Source (Join-Path $UiBuildDir $dir) `
        -Destination (Join-Path $ReleaseDir $dir)
}

Write-Host "[4/5] Copying shaders, icons, and default configs..."
Copy-DirectoryIfExists -Source (Join-Path $ProjectRoot "shaders") -Destination (Join-Path $ReleaseDir "shaders")

foreach ($file in @(
    "blackhole.glsl",
    "blackhole_preview.glsl",
    "blackhole.ico",
    "blackhole_ui.ico",
    "blackhole_presets.txt",
    "blackhole_advanced.txt",
    "blackhole_idlelist.txt",
    "blackhole_schedule.txt",
    "blackhole_system.txt",
    "blackhole_lists.txt"
)) {
    Copy-FileIfExists -Source (Join-Path $ProjectRoot $file) -Destination $ReleaseDir
}

Write-Host "[5/5] Verifying release package..."
$requiredFiles = @(
    "appBlakholeUI.exe",
    "blackhole.exe",
    "blackhole.glsl",
    "blackhole_presets.txt",
    "blackhole_advanced.txt",
    "platforms\qwindows.dll",
    "shaders\vert.glsl",
    "shaders\frag_desktop_header.glsl",
    "shaders\frag_preview_header.glsl"
)

$missing = @()
foreach ($file in $requiredFiles) {
    if (-not (Test-Path -LiteralPath (Join-Path $ReleaseDir $file))) {
        $missing += $file
    }
}

if ($missing.Count -gt 0) {
    throw "Release package is missing: $($missing -join ', ')"
}

Write-Host "Release package ready: $ReleaseDir"
Get-ChildItem -LiteralPath $ReleaseDir -Filter "*.exe" -File |
    Select-Object Name, @{Name = "MB"; Expression = { [math]::Round($_.Length / 1MB, 1) } }
