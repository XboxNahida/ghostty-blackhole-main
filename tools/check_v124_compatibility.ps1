$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$shaderPath = Join-Path $root "shaders\frag_desktop_header.glsl"
$bloomPath = Join-Path $root "src\bloom_renderer.cpp"
$advancedQmlPath = Join-Path $root "Blakhole_UI\pages\AdvancedConfig.qml"

$shader = Get-Content -Raw -Encoding UTF8 -LiteralPath $shaderPath
$bloom = Get-Content -Raw -Encoding UTF8 -LiteralPath $bloomPath
$advancedQml = Get-Content -Raw -Encoding UTF8 -LiteralPath $advancedQmlPath
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

if ($failures.Count -gt 0) {
    foreach ($failure in $failures) {
        Write-Output "V124_COMPATIBILITY_CHECK_FAILED: $failure"
    }
    exit 1
}

Write-Output "V124_COMPATIBILITY_OK"
