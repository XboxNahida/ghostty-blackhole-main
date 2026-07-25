$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$shaderPath = Join-Path $root "shaders\frag_desktop_header.glsl"
$bloomPath = Join-Path $root "src\bloom_renderer.cpp"
$advancedQmlPath = Join-Path $root "Blakhole_UI\pages\AdvancedConfig.qml"
$mainPath = Join-Path $root "src\main.cpp"
$win32GlPath = Join-Path $root "src\win32_gl.cpp"
$wgcPath = Join-Path $root "src\capture_wgc.cpp"
$dpiPath = Join-Path $root "src\dpi_awareness.cpp"

$shader = Get-Content -Raw -Encoding UTF8 -LiteralPath $shaderPath
$bloom = Get-Content -Raw -Encoding UTF8 -LiteralPath $bloomPath
$advancedQml = Get-Content -Raw -Encoding UTF8 -LiteralPath $advancedQmlPath
$main = Get-Content -Raw -Encoding UTF8 -LiteralPath $mainPath
$win32Gl = Get-Content -Raw -Encoding UTF8 -LiteralPath $win32GlPath
$wgc = Get-Content -Raw -Encoding UTF8 -LiteralPath $wgcPath
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

$lightingWarning = [Text.Encoding]::UTF8.GetString(
    [Convert]::FromBase64String(
        "5a6e6aqM5oCn5Yqf6IO977yM5bCa5pyq5a6M5oiQ77yb5ZCv55So5ZCO5Y+v6IO95Ye6546w5rOb55m944CB5a+55q+U5bqm5Y+Y5YyW5oiW55S76Z2i5byC5bi4"))
if (-not $advancedQml.Contains($lightingWarning)) {
    Add-Failure "accretion lighting warning is missing"
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

if ($failures.Count -gt 0) {
    foreach ($failure in $failures) {
        Write-Output "V124_COMPATIBILITY_CHECK_FAILED: $failure"
    }
    exit 1
}

Write-Output "V124_COMPATIBILITY_OK"
