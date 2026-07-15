param(
    [string]$RendererPath,
    [string]$UiPath
)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot

if ([string]::IsNullOrWhiteSpace($RendererPath)) {
    $RendererPath = Join-Path $root "build\blackhole.exe"
}
if ([string]::IsNullOrWhiteSpace($UiPath)) {
    $UiPath = Join-Path $root "Blakhole_UI\build\Desktop_Qt_6_11_1_MinGW_64_bit-Release\appBlakholeUI.exe"
}

function Fail-IdentityCheck {
    param([Parameter(Mandatory = $true)][string]$Message)
    throw "RELEASE_IDENTITY_CHECK_FAILED: $Message"
}

function Require-FileText {
    param(
        [Parameter(Mandatory = $true)][string]$RelativePath,
        [Parameter(Mandatory = $true)][string[]]$RequiredText
    )

    $path = Join-Path $root $RelativePath
    if (-not (Test-Path -LiteralPath $path)) {
        Fail-IdentityCheck "missing $RelativePath"
    }

    $content = Get-Content -Raw -Encoding UTF8 -LiteralPath $path
    foreach ($text in $RequiredText) {
        if (-not $content.Contains($text)) {
            Fail-IdentityCheck "$RelativePath missing [$text]"
        }
    }
}

Require-FileText "cmake\AppVersion.cmake" @(
    'set(BLACKHOLE_VERSION "1.2.1")',
    "BLACKHOLE_VERSION_MAJOR",
    "BLACKHOLE_VERSION_MINOR",
    "BLACKHOLE_VERSION_PATCH"
)
Require-FileText "cmake\app_version.h.in" @("APP_VERSION_STRING")
Require-FileText "cmake\renderer_version.rc.in" @(
    'VALUE "CompanyName", "XboxNahida"',
    'VALUE "Comments", "Official source: https://github.com/XboxNahida/ghostty-blackhole-main"',
    'VALUE "ProductName", "Blakhole Renderer"',
    'VALUE "OriginalFilename", "blackhole.exe"'
)
Require-FileText "Blakhole_UI\app_version.rc.in" @(
    'VALUE "CompanyName", "XboxNahida"',
    'VALUE "Comments", "Official source: https://github.com/XboxNahida/ghostty-blackhole-main"',
    'VALUE "ProductName", "Blakhole UI"',
    'VALUE "OriginalFilename", "appBlakholeUI.exe"'
)
Require-FileText "Blakhole_UI\main.cpp" @(
    "APP_VERSION_STRING",
    "setApplicationVersion"
)

$rendererDescription = [Text.Encoding]::UTF8.GetString([Convert]::FromBase64String("Qmxha2hvbGUg5pys5Zyw5riy5p+T5Zmo"))
$uiDescription = [Text.Encoding]::UTF8.GetString([Convert]::FromBase64String("6buR5rSe5Y+v6KeG5YyW6YWN572u5bel5YW3"))
$officialSource = "Official source: https://github.com/XboxNahida/ghostty-blackhole-main"
$builtFiles = @(
    @{ Path = $RendererPath; Description = $rendererDescription },
    @{ Path = $UiPath; Description = $uiDescription }
)

foreach ($builtFile in $builtFiles) {
    $path = $builtFile.Path
    if (-not (Test-Path -LiteralPath $path)) {
        Fail-IdentityCheck "missing build output $path"
    }

    $version = (Get-Item -LiteralPath $path).VersionInfo
    foreach ($property in @("Comments", "FileDescription", "ProductName", "CompanyName", "FileVersion", "ProductVersion", "OriginalFilename")) {
        if ([string]::IsNullOrWhiteSpace([string]$version.$property)) {
            Fail-IdentityCheck "$(Split-Path -Leaf $path) has empty $property"
        }
    }
    if ($version.ProductVersion -cne "1.2.1") {
        Fail-IdentityCheck "$(Split-Path -Leaf $path) ProductVersion is $($version.ProductVersion), expected 1.2.1"
    }
    if ($version.FileVersion -cne "1.2.1.0") {
        Fail-IdentityCheck "$(Split-Path -Leaf $path) FileVersion is $($version.FileVersion), expected 1.2.1.0"
    }
    if ($version.FileDescription -cne $builtFile.Description) {
        Fail-IdentityCheck "$(Split-Path -Leaf $path) FileDescription has an encoding mismatch"
    }
    if ($version.Comments -cne $officialSource) {
        Fail-IdentityCheck "$(Split-Path -Leaf $path) Comments is [$($version.Comments)], expected official source URL"
    }
}

"RELEASE_IDENTITY_OK version=1.2.1"
