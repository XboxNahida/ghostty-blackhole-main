param(
    [string]$ReleaseDir
)

$ErrorActionPreference = "Stop"

if ([string]::IsNullOrWhiteSpace($ReleaseDir)) {
    $ReleaseDir = $PSScriptRoot
}
if (-not (Test-Path -LiteralPath $ReleaseDir -PathType Container)) {
    throw "RELEASE_CHECKSUMS_FAILED: release directory not found: $ReleaseDir"
}

$releaseRoot = (Resolve-Path -LiteralPath $ReleaseDir).Path.TrimEnd('\') + '\'
$manifestPath = Join-Path $ReleaseDir "release_checksums.sha256"
if (-not (Test-Path -LiteralPath $manifestPath -PathType Leaf)) {
    throw "RELEASE_CHECKSUMS_FAILED: missing release_checksums.sha256"
}

$failures = [System.Collections.Generic.List[string]]::new()
$manifestEntries = @{}
$lineNumber = 0
foreach ($line in Get-Content -Encoding UTF8 -LiteralPath $manifestPath) {
    $lineNumber++
    if ($line -cnotmatch '^([0-9a-f]{64})  ([a-z0-9][a-z0-9._/-]*)$') {
        $failures.Add("invalid manifest line $lineNumber")
        continue
    }

    $expectedHash = $Matches[1]
    $relativeName = $Matches[2]
    $segments = $relativeName.Split('/')
    if ($relativeName.StartsWith('/') -or $segments -contains '..' -or $segments -contains '.') {
        $failures.Add("unsafe manifest path: $relativeName")
        continue
    }
    if ($relativeName -ceq "release_checksums.sha256") {
        $failures.Add("manifest must not contain itself")
        continue
    }
    if ($manifestEntries.ContainsKey($relativeName)) {
        $failures.Add("duplicate manifest entry: $relativeName")
        continue
    }

    $manifestEntries[$relativeName] = $expectedHash
    $path = Join-Path $ReleaseDir ($relativeName.Replace('/', '\'))
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        $failures.Add("missing file: $relativeName")
        continue
    }
    $resolvedPath = (Resolve-Path -LiteralPath $path).Path
    if (-not $resolvedPath.StartsWith($releaseRoot, [StringComparison]::OrdinalIgnoreCase)) {
        $failures.Add("path escapes release directory: $relativeName")
        continue
    }

    $actualHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $resolvedPath).Hash.ToLowerInvariant()
    if ($actualHash -cne $expectedHash) {
        $failures.Add("checksum mismatch: $relativeName")
    }
}

$actualEntries = @{}
foreach ($file in Get-ChildItem -LiteralPath $ReleaseDir -Recurse -File) {
    $relativeName = $file.FullName.Substring($releaseRoot.Length).Replace('\', '/').ToLowerInvariant()
    if ($relativeName -ceq "release_checksums.sha256") {
        continue
    }
    if ($actualEntries.ContainsKey($relativeName)) {
        $failures.Add("case-colliding release paths: $relativeName")
        continue
    }
    $actualEntries[$relativeName] = $true
}

foreach ($actualName in $actualEntries.Keys) {
    if (-not $manifestEntries.ContainsKey($actualName)) {
        $failures.Add("manifest is missing release file: $actualName")
    }
}
foreach ($manifestName in $manifestEntries.Keys) {
    if (-not $actualEntries.ContainsKey($manifestName)) {
        $failures.Add("manifest has an extra entry: $manifestName")
    }
}

if ($failures.Count -gt 0) {
    foreach ($failure in $failures) {
        Write-Error "RELEASE_CHECKSUMS_FAILED: $failure" -ErrorAction Continue
    }
    exit 1
}

Write-Output "RELEASE_CHECKSUMS_OK files=$($manifestEntries.Count)"
