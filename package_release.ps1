param(
    [switch]$NoBuild
)

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

function Assert-CleanGitState {
    & git -C $ProjectRoot diff --quiet --ignore-submodules --
    if ($LASTEXITCODE -ne 0) {
        throw "Tracked working tree is dirty. Commit or restore tracked changes before packaging."
    }
    & git -C $ProjectRoot diff --cached --quiet --ignore-submodules --
    if ($LASTEXITCODE -ne 0) {
        throw "Git index has staged changes. Commit or unstage them before packaging."
    }
}

function Test-GnuStrip {
    param([Parameter(Mandatory = $true)][string]$Path)

    if ((Split-Path -Leaf $Path) -ine "strip.exe") {
        return $false
    }
    $versionOutput = & $Path --version 2>&1
    if ($LASTEXITCODE -ne 0) {
        return $false
    }
    return (($versionOutput -join "`n") -match '(?i)GNU strip|GNU Binutils|MinGW')
}

function Get-MingwStripPath {
    param(
        [Parameter(Mandatory = $true)][string]$BuildDir,
        [Parameter(Mandatory = $true)][string]$FileName
    )

    $cachePath = Join-Path $BuildDir "CMakeCache.txt"
    if (Test-Path -LiteralPath $cachePath -PathType Leaf) {
        $line = Get-Content -Encoding UTF8 -LiteralPath $cachePath |
            Where-Object { $_ -match '^CMAKE_STRIP:.*=(.+)$' } |
            Select-Object -First 1
        if ($line -match '^CMAKE_STRIP:.*=(.+)$') {
            $candidate = $Matches[1]
            if ((Test-Path -LiteralPath $candidate -PathType Leaf) -and (Test-GnuStrip -Path $candidate)) {
                return (Resolve-Path -LiteralPath $candidate).Path
            }
            throw "CMAKE_STRIP is not a GNU/MinGW strip.exe: $candidate"
        }
    }

    $command = Get-Command $FileName -ErrorAction SilentlyContinue
    if ($null -ne $command -and (Test-GnuStrip -Path $command.Source)) {
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

Assert-CleanGitState
$commit = (& git -C $ProjectRoot rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0 -or $commit -notmatch '^[0-9a-f]{40}$') {
    throw "Unable to determine the clean Git commit for RELEASE_INFO.txt"
}

if (-not $NoBuild) {
    Write-Host "[1/9] Clean-building renderer and Qt UI from commit $commit..."
    & cmake --build $CoreBuildDir --config Release --clean-first
    if ($LASTEXITCODE -ne 0) {
        throw "Renderer clean build failed with exit code $LASTEXITCODE"
    }
    & cmake --build $UiBuildDir --config Release --clean-first
    if ($LASTEXITCODE -ne 0) {
        throw "Qt UI clean build failed with exit code $LASTEXITCODE"
    }
    Assert-CleanGitState
}

if (-not (Test-Path -LiteralPath $RendererSource)) {
    throw "Missing build\blackhole.exe. Run: cmake --build build --config Release"
}

if (-not (Test-Path -LiteralPath $UiSource)) {
    throw "Missing Qt UI build output. Run: cmake --build Blakhole_UI\build\Desktop_Qt_6_11_1_MinGW_64_bit-Release --config Release"
}

$rendererBuildHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $RendererSource).Hash.ToLowerInvariant()
$uiBuildHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $UiSource).Hash.ToLowerInvariant()

Write-Host "[2/9] Preparing release directory..."
New-Item -ItemType Directory -Force -Path $ReleaseDir | Out-Null

Write-Host "[3/9] Copying renderer..."
Copy-Item -LiteralPath $RendererSource -Destination $RendererRelease -Force
Get-ChildItem -LiteralPath $CoreBuildDir -Filter "*.dll" -File -ErrorAction SilentlyContinue |
    Copy-Item -Destination $ReleaseDir -Force

Write-Host "[4/9] Copying Qt UI and deployed runtime..."
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

Write-Host "[5/9] Copying shaders, icons, default configs, and release documents..."
Copy-DirectoryIfExists -Source (Join-Path $ProjectRoot "shaders") -Destination (Join-Path $ReleaseDir "shaders")

$previewShaderSource = Join-Path $UiBuildDir "blackhole_preview.glsl"
if (-not (Test-Path -LiteralPath $previewShaderSource -PathType Leaf)) {
    throw "Missing Qt preview shader: $previewShaderSource"
}
Copy-Item -LiteralPath $previewShaderSource -Destination (Join-Path $ReleaseDir "blackhole_preview.glsl") -Force

$previewBackgroundSource = Join-Path $ProjectRoot "Blakhole_UI\fonts\pic\Starry_sky_background.png"
$previewBackgroundDestination = Join-Path $ReleaseDir "fonts\pic\Starry_sky_background.png"
if (-not (Test-Path -LiteralPath $previewBackgroundSource -PathType Leaf)) {
    throw "Missing Qt preview background: $previewBackgroundSource"
}
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $previewBackgroundDestination) | Out-Null
Copy-Item -LiteralPath $previewBackgroundSource -Destination $previewBackgroundDestination -Force

foreach ($paymentQrName in @("QR_payment.jpg", "WeChat_QR.png")) {
    $paymentQrSource = Join-Path $ProjectRoot "Blakhole_UI\fonts\pic\$paymentQrName"
    if (-not (Test-Path -LiteralPath $paymentQrSource -PathType Leaf)) {
        throw "Missing local payment QR file: $paymentQrSource"
    }
    Copy-Item -LiteralPath $paymentQrSource `
        -Destination (Join-Path $ReleaseDir "fonts\pic\$paymentQrName") -Force
}

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
Copy-Item -LiteralPath (Join-Path $ProjectRoot "tools\verify_release_checksums.ps1") `
    -Destination (Join-Path $ReleaseDir "verify_release_checksums.ps1") -Force

Write-Host "[6/9] Removing debug sections from release copies only..."
$rendererStrip = Get-MingwStripPath -BuildDir $CoreBuildDir -FileName "strip.exe"
$uiStrip = Get-MingwStripPath -BuildDir $UiBuildDir -FileName "strip.exe"
Remove-ReleaseDebugSections -StripPath $rendererStrip -ReleaseExecutable $RendererRelease
Remove-ReleaseDebugSections -StripPath $uiStrip -ReleaseExecutable $UiRelease

$buildTime = [DateTimeOffset]::Now
$releaseInfo = @(
    "Version: 1.2.1",
    "Tag: v1.2.1",
    "Commit: $commit",
    "BuildTimeUTC: $($buildTime.UtcDateTime.ToString('yyyy-MM-ddTHH:mm:ssZ'))",
    "BuildTimeLocal: $($buildTime.ToString('yyyy-MM-ddTHH:mm:sszzz'))",
    "SignatureStatus: Unsigned/NotSigned",
    "BuildRendererSHA256: $rendererBuildHash",
    "BuildUiSHA256: $uiBuildHash"
) -join "`n"
Write-Utf8NoBom -Path (Join-Path $ReleaseDir "RELEASE_INFO.txt") -Content ($releaseInfo + "`n")

Write-Host "[7/9] Verifying release package..."
$requiredFiles = @(
    "appBlakholeUI.exe",
    "blackhole.exe",
    "blackhole.glsl",
    "blackhole_preview.glsl",
    "fonts\pic\Starry_sky_background.png",
    "fonts\pic\QR_payment.jpg",
    "fonts\pic\WeChat_QR.png",
    "blackhole_presets.txt",
    "blackhole_advanced.txt",
    "LICENSE",
    "SECURITY.md",
    "RELEASE_INFO.txt",
    "verify_release_checksums.ps1",
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

$releaseRoot = (Resolve-Path -LiteralPath $ReleaseDir).Path.TrimEnd('\') + '\'
$checksumLines = foreach ($file in Get-ChildItem -LiteralPath $ReleaseDir -Recurse -File) {
    $manifestName = $file.FullName.Substring($releaseRoot.Length).Replace('\', '/').ToLowerInvariant()
    if ($manifestName -ceq "release_checksums.sha256") {
        continue
    }
    $hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $file.FullName).Hash.ToLowerInvariant()
    "$hash  $manifestName"
}
$checksumContent = (($checksumLines | Sort-Object) -join "`n") + "`n"
Write-Utf8NoBom -Path (Join-Path $ReleaseDir "release_checksums.sha256") -Content $checksumContent

Write-Host "[8/9] Verifying complete checksum manifest..."
& powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $ReleaseDir "verify_release_checksums.ps1")
if ($LASTEXITCODE -ne 0) {
    throw "Release checksum verification failed with exit code $LASTEXITCODE"
}

Write-Host "[9/9] Confirming build artifacts were not modified..."
if ((Get-FileHash -Algorithm SHA256 -LiteralPath $RendererSource).Hash.ToLowerInvariant() -cne $rendererBuildHash) {
    throw "Packaging modified build\blackhole.exe"
}
if ((Get-FileHash -Algorithm SHA256 -LiteralPath $UiSource).Hash.ToLowerInvariant() -cne $uiBuildHash) {
    throw "Packaging modified the Qt UI build executable"
}

Write-Host "Release package ready: $ReleaseDir"
Get-ChildItem -LiteralPath $ReleaseDir -Filter "*.exe" -File |
    Select-Object Name, @{Name = "MB"; Expression = { [math]::Round($_.Length / 1MB, 1) } }
