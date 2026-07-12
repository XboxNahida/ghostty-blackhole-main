$ErrorActionPreference = "Stop"

$ProjectRoot = $PSScriptRoot
$UiBuildDir = Join-Path $ProjectRoot "Blakhole_UI\build\Desktop_Qt_6_11_1_MinGW_64_bit-Release"
$CoreBuildDir = Join-Path $ProjectRoot "build"
$ReleaseDir = Join-Path $ProjectRoot "release"
$RendererSource = Join-Path $CoreBuildDir "blackhole.exe"
$UiSource = Join-Path $UiBuildDir "appBlakholeUI.exe"
$RendererRelease = Join-Path $ReleaseDir "blackhole.exe"
$UiRelease = Join-Path $ReleaseDir "appBlakholeUI.exe"
$Utf8NoBom = New-Object System.Text.UTF8Encoding($false)

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

function Get-CMakeToolPath {
    param(
        [Parameter(Mandatory = $true)][string]$BuildDir,
        [Parameter(Mandatory = $true)][string]$CacheVariable,
        [Parameter(Mandatory = $true)][string]$FileName
    )

    $cachePath = Join-Path $BuildDir "CMakeCache.txt"
    if (Test-Path -LiteralPath $cachePath -PathType Leaf) {
        $line = Get-Content -Encoding UTF8 -LiteralPath $cachePath |
            Where-Object { $_ -match "^${CacheVariable}:.*=(.+)$" } |
            Select-Object -First 1
        if ($line -match "^${CacheVariable}:.*=(.+)$") {
            $candidate = $Matches[1]
            if (Test-Path -LiteralPath $candidate -PathType Leaf) {
                return (Resolve-Path -LiteralPath $candidate).Path
            }
        }
    }

    $command = Get-Command $FileName -ErrorAction SilentlyContinue
    if ($null -ne $command) {
        return $command.Source
    }

    throw "Unable to locate MinGW $FileName for build directory: $BuildDir"
}

function Remove-ReleaseDebugSections {
    param(
        [Parameter(Mandatory = $true)][string]$StripPath,
        [Parameter(Mandatory = $true)][string]$ReleaseExecutable
    )

    $resolvedReleaseRoot = (Resolve-Path -LiteralPath $ReleaseDir).Path.TrimEnd('\') + '\'
    $resolvedExecutable = (Resolve-Path -LiteralPath $ReleaseExecutable).Path
    if (-not $resolvedExecutable.StartsWith($resolvedReleaseRoot, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to strip a file outside release: $resolvedExecutable"
    }
    if ((Split-Path -Leaf $StripPath) -ine "strip.exe") {
        throw "Refusing to use a non-MinGW strip tool: $StripPath"
    }

    & $StripPath --strip-debug $resolvedExecutable
    if ($LASTEXITCODE -ne 0) {
        throw "strip.exe failed for $resolvedExecutable with exit code $LASTEXITCODE"
    }
}

function Write-Utf8NoBom {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$Content
    )

    [IO.File]::WriteAllText($Path, $Content, $Utf8NoBom)
}

if (-not (Test-Path -LiteralPath $RendererSource)) {
    throw "Missing build\blackhole.exe. Run: cmake --build build --config Release"
}

if (-not (Test-Path -LiteralPath $UiSource)) {
    throw "Missing Qt UI build output. Run: cmake --build Blakhole_UI\build\Desktop_Qt_6_11_1_MinGW_64_bit-Release --config Release"
}

$rendererBuildHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $RendererSource).Hash.ToLowerInvariant()
$uiBuildHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $UiSource).Hash.ToLowerInvariant()

Write-Host "[1/7] Preparing release directory..."
New-Item -ItemType Directory -Force -Path $ReleaseDir | Out-Null

Write-Host "[2/7] Copying renderer..."
Copy-Item -LiteralPath $RendererSource -Destination $RendererRelease -Force
Get-ChildItem -LiteralPath $CoreBuildDir -Filter "*.dll" -File -ErrorAction SilentlyContinue |
    Copy-Item -Destination $ReleaseDir -Force

Write-Host "[3/7] Copying Qt UI and deployed runtime..."
Copy-Item -LiteralPath $UiSource -Destination $UiRelease -Force
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

Write-Host "[4/7] Copying shaders, icons, default configs, and release documents..."
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

foreach ($file in @("LICENSE", "SECURITY.md")) {
    Copy-FileIfExists -Source (Join-Path $ProjectRoot $file) -Destination $ReleaseDir
}

Write-Host "[5/7] Removing debug sections from release copies only..."
$rendererStrip = Get-CMakeToolPath -BuildDir $CoreBuildDir -CacheVariable "CMAKE_STRIP" -FileName "strip.exe"
$uiStrip = Get-CMakeToolPath -BuildDir $UiBuildDir -CacheVariable "CMAKE_STRIP" -FileName "strip.exe"
Remove-ReleaseDebugSections -StripPath $rendererStrip -ReleaseExecutable $RendererRelease
Remove-ReleaseDebugSections -StripPath $uiStrip -ReleaseExecutable $UiRelease

$commit = (& git -C $ProjectRoot rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0 -or $commit -notmatch '^[0-9a-f]{40}$') {
    throw "Unable to determine the Git commit for RELEASE_INFO.txt"
}

$buildTime = [DateTimeOffset]::Now
$releaseInfo = @(
    "Version: 1.2.0",
    "Tag: v1.2.0",
    "Commit: $commit",
    "BuildTimeUTC: $($buildTime.UtcDateTime.ToString('yyyy-MM-ddTHH:mm:ssZ'))",
    "BuildTimeLocal: $($buildTime.ToString('yyyy-MM-ddTHH:mm:sszzz'))",
    "SignatureStatus: Unsigned/NotSigned",
    "BuildRendererSHA256: $rendererBuildHash",
    "BuildUiSHA256: $uiBuildHash"
) -join "`n"
Write-Utf8NoBom -Path (Join-Path $ReleaseDir "RELEASE_INFO.txt") -Content ($releaseInfo + "`n")

Write-Host "[6/7] Verifying release package..."
$requiredFiles = @(
    "appBlakholeUI.exe",
    "blackhole.exe",
    "blackhole.glsl",
    "blackhole_presets.txt",
    "blackhole_advanced.txt",
    "LICENSE",
    "SECURITY.md",
    "RELEASE_INFO.txt",
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

$checksumFiles = @(
    "appBlakholeUI.exe",
    "blackhole.exe",
    "blackhole.glsl",
    "blackhole_advanced.txt",
    "blackhole_presets.txt",
    "LICENSE",
    "RELEASE_INFO.txt",
    "SECURITY.md",
    "platforms\qwindows.dll",
    "shaders\vert.glsl",
    "shaders\frag_desktop_header.glsl",
    "shaders\frag_preview_header.glsl"
)
$checksumLines = foreach ($relativePath in $checksumFiles) {
    $path = Join-Path $ReleaseDir $relativePath
    $hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $path).Hash.ToLowerInvariant()
    $manifestName = $relativePath.Replace('\', '/').ToLowerInvariant()
    "$hash  $manifestName"
}
$checksumContent = (($checksumLines | Sort-Object) -join "`n") + "`n"
Write-Utf8NoBom -Path (Join-Path $ReleaseDir "release_checksums.sha256") -Content $checksumContent

Write-Host "[7/7] Confirming build artifacts were not modified..."
if ((Get-FileHash -Algorithm SHA256 -LiteralPath $RendererSource).Hash.ToLowerInvariant() -cne $rendererBuildHash) {
    throw "Packaging modified build\blackhole.exe"
}
if ((Get-FileHash -Algorithm SHA256 -LiteralPath $UiSource).Hash.ToLowerInvariant() -cne $uiBuildHash) {
    throw "Packaging modified the Qt UI build executable"
}

Write-Host "Release package ready: $ReleaseDir"
Get-ChildItem -LiteralPath $ReleaseDir -Filter "*.exe" -File |
    Select-Object Name, @{Name = "MB"; Expression = { [math]::Round($_.Length / 1MB, 1) } }
