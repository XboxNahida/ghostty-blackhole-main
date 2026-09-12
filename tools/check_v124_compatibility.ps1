$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$shaderPath = Join-Path $root "shaders\frag_desktop_header.glsl"
$bloomPath = Join-Path $root "src\bloom_renderer.cpp"
$advancedQmlPath = Join-Path $root "Blakhole_UI\pages\AdvancedConfig.qml"
$mainPath = Join-Path $root "src\main.cpp"
$win32GlPath = Join-Path $root "src\win32_gl.cpp"
$wgcPath = Join-Path $root "src\capture_wgc.cpp"
$dpiPath = Join-Path $root "src\dpi_awareness.cpp"
$wgcHeaderPath = Join-Path $root "src\capture_wgc.h"
$d3dRendererHeaderPath = Join-Path $root "src\d3d11_renderer.h"

$shader = Get-Content -Raw -Encoding UTF8 -LiteralPath $shaderPath
$bloom = Get-Content -Raw -Encoding UTF8 -LiteralPath $bloomPath
$advancedQml = Get-Content -Raw -Encoding UTF8 -LiteralPath $advancedQmlPath
$main = Get-Content -Raw -Encoding UTF8 -LiteralPath $mainPath
$win32Gl = Get-Content -Raw -Encoding UTF8 -LiteralPath $win32GlPath
$wgc = Get-Content -Raw -Encoding UTF8 -LiteralPath $wgcPath
$wgcHeader = Get-Content -Raw -Encoding UTF8 -LiteralPath $wgcHeaderPath
$d3dRendererHeader = Get-Content -Raw -Encoding UTF8 -LiteralPath $d3dRendererHeaderPath
$failures = [System.Collections.Generic.List[string]]::new()

function Add-Failure([string]$message) {
    $failures.Add($message)
}

foreach ($entry in @(
    @{ Name = "source desktop shader"; Content = $shader }
)) {
    if ($entry.Content -notmatch '(?m)^\s*out\s+vec4\s+fragColor\s*;\s*$') {
        Add-Failure "$($entry.Name) does not declare an explicit fragment output"
    }
    if ($entry.Content -match '\bgl_FragColor\b') {
        Add-Failure "$($entry.Name) still uses gl_FragColor"
    }
}

if ($bloom -notmatch '(?m)^\s*uv\s*=\s*p\s*;\s*$') {
    Add-Failure "Bloom fullscreen triangle does not use uv = p"
}
if ($bloom -match 'uv\s*=\s*p\s*\*\s*0\.5') {
    Add-Failure "Bloom fullscreen triangle still halves texture coordinates"
}

$movementLabel = [Text.Encoding]::UTF8.GetString(
    [Convert]::FromBase64String("6buR5rSe56e75Yqo6YCf5bqm"))
$movementPattern = '(?s)label:\s*"' + [regex]::Escape($movementLabel) +
    '".+?from:\s*0(?:\.0)?\s*;\s*to:\s*3\.0'
if ($advancedQml -notmatch $movementPattern) {
    Add-Failure "movement speed slider does not allow zero"
}

if (-not $advancedQml.Contains('实验性') -or -not $advancedQml.Contains('吞噬后不恢复')) {
    Add-Failure "experimental snapshot behavior warning is missing"
}

if (-not (Test-Path -LiteralPath $dpiPath -PathType Leaf)) {
    Add-Failure "DPI awareness module is missing"
} else {
    $dpi = Get-Content -Raw -Encoding UTF8 -LiteralPath $dpiPath
    if ($dpi -notmatch 'DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2') {
        Add-Failure "DPI awareness module does not prefer Per-Monitor V2"
    }
}

$dpiCall = $main.IndexOf("EnableBestDpiAwareness()")
$workingDirectorySetup = $main.IndexOf("Set working directory to project root")
if ($dpiCall -lt 0 -or $workingDirectorySetup -lt 0 -or $dpiCall -gt $workingDirectorySetup) {
    Add-Failure "DPI awareness is not enabled before renderer initialization"
}
if ($win32Gl -match '\bSetProcessDPIAware\s*\(') {
    Add-Failure "win32_gl.cpp still changes process DPI awareness"
}
if ($wgc -match '\bSetProcessDPIAware\s*\(') {
    Add-Failure "capture_wgc.cpp still changes process DPI awareness"
}

if ($wgcHeader -match '\bWGC_GetFrame\s*\(' -or
    $wgcHeader -match '\bWGC_CopyToStaging\s*\(') {
    Add-Failure "WGC header still exposes capture-pool textures"
}
if ($wgcHeader -notmatch '\bWGC_TryGetMappedFrame\s*\(' -or
    $wgcHeader -notmatch '\bWGC_GetStableFrame\s*\(') {
    Add-Failure "WGC stable frame APIs are missing"
}
if ($main -match '\bWGC_GetFrame\s*\(' -or
    $main -match '\bWGC_CopyToStaging\s*\(') {
    Add-Failure "main.cpp still handles WGC capture-pool textures"
}
if ($main -notmatch '\bWGC_TryGetMappedFrame\s*\(' -or
    $main -notmatch '\bWGC_GetStableFrame\s*\(') {
    Add-Failure "main.cpp does not use stable WGC APIs"
}
if ($main -notmatch 'const\s+bool\s+captureAuto' -or
    $main -notmatch '(?s)WGC primary failed in auto mode.+?WGC_Release\(wgcPri\).+?useWGC\s*=\s*false' -or
    $main -notmatch '(?s)WGC secondary failed in auto mode.+?WGC_Release\(wgcPri\).+?useWGC\s*=\s*false') {
    Add-Failure "auto capture mode does not fall back from WGC initialization to DXGI"
}
if ($wgc -notmatch 'struct\s+WGCFrameLease' -or
    $wgc -notmatch '(?s)WGC_TryGetMappedFrame.+?CopyResource.+?Map\s*\(') {
    Add-Failure "WGC mapped-frame path does not hold a frame lease through Map"
}
if ($d3dRendererHeader -match '\bframeQueue_\b') {
    Add-Failure "D3D11 renderer still relies on delayed WGC frame references"
}

if ($failures.Count -gt 0) {
    foreach ($failure in $failures) {
        Write-Output "V124_COMPATIBILITY_CHECK_FAILED: $failure"
    }
    exit 1
}

Write-Output "V124_COMPATIBILITY_OK"
