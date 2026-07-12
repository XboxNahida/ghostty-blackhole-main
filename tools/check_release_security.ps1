param(
    [string]$ReleaseDir,
    [string]$ObjdumpPath
)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($ReleaseDir)) {
    $ReleaseDir = Join-Path $root "release"
}

$failures = [System.Collections.Generic.List[string]]::new()

function Add-Failure {
    param([Parameter(Mandatory = $true)][string]$Message)
    $failures.Add($Message)
}

function Resolve-Objdump {
    if (-not [string]::IsNullOrWhiteSpace($ObjdumpPath)) {
        if (-not (Test-Path -LiteralPath $ObjdumpPath -PathType Leaf)) {
            throw "RELEASE_SECURITY_CHECK_FAILED: objdump not found: $ObjdumpPath"
        }
        return (Resolve-Path -LiteralPath $ObjdumpPath).Path
    }

    $command = Get-Command objdump.exe -ErrorAction SilentlyContinue
    if ($null -ne $command) {
        return $command.Source
    }

    foreach ($cachePath in @(
        (Join-Path $root "build\CMakeCache.txt"),
        (Join-Path $root "Blakhole_UI\build\Desktop_Qt_6_11_1_MinGW_64_bit-Release\CMakeCache.txt")
    )) {
        if (-not (Test-Path -LiteralPath $cachePath)) {
            continue
        }
        $compilerLine = Get-Content -Encoding UTF8 -LiteralPath $cachePath |
            Where-Object { $_ -match '^CMAKE_CXX_COMPILER:.*=(.+)$' } |
            Select-Object -First 1
        if ($compilerLine -match '^CMAKE_CXX_COMPILER:.*=(.+)$') {
            $candidate = Join-Path (Split-Path -Parent $Matches[1]) "objdump.exe"
            if (Test-Path -LiteralPath $candidate -PathType Leaf) {
                return $candidate
            }
        }
    }

    throw "RELEASE_SECURITY_CHECK_FAILED: MinGW objdump.exe not found"
}

function Get-ObjdumpOutput {
    param(
        [Parameter(Mandatory = $true)][string]$Tool,
        [Parameter(Mandatory = $true)][string]$Argument,
        [Parameter(Mandatory = $true)][string]$Path
    )

    $output = & $Tool $Argument $Path 2>&1
    if ($LASTEXITCODE -ne 0) {
        Add-Failure "objdump failed for $(Split-Path -Leaf $Path): $output"
        return ""
    }
    return ($output -join "`n")
}

if (-not (Test-Path -LiteralPath $ReleaseDir -PathType Container)) {
    throw "RELEASE_SECURITY_CHECK_FAILED: release directory not found: $ReleaseDir"
}

$objdump = Resolve-Objdump
$executables = @(
    @{ Name = "blackhole.exe"; ProductName = "Blakhole Renderer"; OriginalFilename = "blackhole.exe" },
    @{ Name = "appBlakholeUI.exe"; ProductName = "Blakhole UI"; OriginalFilename = "appBlakholeUI.exe" }
)

foreach ($executable in $executables) {
    $path = Join-Path $ReleaseDir $executable.Name
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        Add-Failure "missing $($executable.Name)"
        continue
    }

    $version = (Get-Item -LiteralPath $path).VersionInfo
    foreach ($property in @("FileDescription", "ProductName", "CompanyName", "FileVersion", "ProductVersion", "OriginalFilename")) {
        if ([string]::IsNullOrWhiteSpace([string]$version.$property)) {
            Add-Failure "$($executable.Name) has empty VERSIONINFO $property"
        }
    }
    if ($version.ProductName -cne $executable.ProductName) {
        Add-Failure "$($executable.Name) ProductName is [$($version.ProductName)]"
    }
    if ($version.ProductVersion -cne "1.2.0") {
        Add-Failure "$($executable.Name) ProductVersion is [$($version.ProductVersion)]"
    }
    if ($version.FileVersion -cne "1.2.0.0") {
        Add-Failure "$($executable.Name) FileVersion is [$($version.FileVersion)]"
    }
    if ($version.CompanyName -cne "XboxNahida") {
        Add-Failure "$($executable.Name) CompanyName is [$($version.CompanyName)]"
    }
    if ($version.OriginalFilename -cne $executable.OriginalFilename) {
        Add-Failure "$($executable.Name) OriginalFilename is [$($version.OriginalFilename)]"
    }
    $signature = Get-AuthenticodeSignature -LiteralPath $path
    if ([string]$signature.Status -cne "NotSigned") {
        Add-Failure "$($executable.Name) signature status is [$($signature.Status)], expected NotSigned"
    }

    $headers = Get-ObjdumpOutput -Tool $objdump -Argument "-p" -Path $path
    $sections = Get-ObjdumpOutput -Tool $objdump -Argument "-h" -Path $path
    if ($headers -notmatch '(?m)^\s+DYNAMIC_BASE\s*$') {
        Add-Failure "$($executable.Name) does not enable ASLR/DYNAMIC_BASE"
    }
    if ($headers -notmatch '(?m)^\s+NX_COMPAT\s*$') {
        Add-Failure "$($executable.Name) does not enable DEP/NX_COMPAT"
    }
    if ($sections -match '(?im)^\s*\d+\s+\.debug(?:_|\b)') {
        Add-Failure "$($executable.Name) still contains .debug_* sections"
    }
    if ($sections -match '(?im)^\s*\d+\s+\.?UPX\d*\b') {
        Add-Failure "$($executable.Name) contains a UPX section"
    }

    if ($executable.Name -ceq "blackhole.exe") {
        $forbiddenImports = @(
            @{ Pattern = '(?im)^\s*DLL Name:\s*(?:WS2_32|WINHTTP|WININET|URLMON)\.dll\s*$'; Name = "network library" },
            @{ Pattern = '(?im)\b(?:WSAStartup|socket|connect|send|recv|WinHttp\w*|InternetOpen\w*|URLDownloadToFile\w*)\b'; Name = "network API" },
            @{ Pattern = '(?im)\b(?:WriteProcessMemory|NtWriteVirtualMemory|ZwWriteVirtualMemory|CreateRemoteThread|CreateRemoteThreadEx|NtCreateThreadEx|RtlCreateUserThread|VirtualAllocEx|VirtualProtectEx|SetThreadContext|QueueUserAPC|NtQueueApcThread|MapViewOfFile2)\b'; Name = "common process injection API" },
            @{ Pattern = '(?im)\b(?:RegSetValue\w*|RegCreateKey\w*|RegDeleteValue\w*|RegDeleteKey\w*)\b'; Name = "Registry write API" },
            @{ Pattern = '(?im)\bGetAsyncKeyState\b'; Name = "GetAsyncKeyState" }
        )
        foreach ($forbidden in $forbiddenImports) {
            if ($headers -match $forbidden.Pattern) {
                Add-Failure "blackhole.exe imports $($forbidden.Name)"
            }
        }
    }
}

$packageScriptPath = Join-Path $root "package_release.ps1"
$packageScript = Get-Content -Raw -Encoding UTF8 -LiteralPath $packageScriptPath
foreach ($contract in @(
    @{ Pattern = 'git\s+-C\s+\$ProjectRoot\s+diff\s+--quiet'; Message = "package script does not reject tracked working-tree changes" },
    @{ Pattern = 'git\s+-C\s+\$ProjectRoot\s+diff\s+--cached\s+--quiet'; Message = "package script does not reject staged changes" },
    @{ Pattern = '(?s)cmake.+--build.+\$CoreBuildDir.+--clean-first'; Message = "package script does not clean-build the renderer by default" },
    @{ Pattern = '(?s)cmake.+--build.+\$UiBuildDir.+--clean-first'; Message = "package script does not clean-build the Qt UI by default" },
    @{ Pattern = 'verify_release_checksums\.ps1'; Message = "package script does not include the standalone checksum verifier" },
    @{ Pattern = '--version'; Message = "package script does not validate strip.exe identity" }
)) {
    if ($packageScript -notmatch $contract.Pattern) {
        Add-Failure $contract.Message
    }
}

$releaseInfo = ""
$releaseInfoPath = Join-Path $ReleaseDir "RELEASE_INFO.txt"
if (-not (Test-Path -LiteralPath $releaseInfoPath -PathType Leaf)) {
    Add-Failure "missing RELEASE_INFO.txt"
}
else {
    $releaseInfo = Get-Content -Raw -Encoding UTF8 -LiteralPath $releaseInfoPath
    foreach ($requiredPattern in @(
        '(?m)^Version: 1\.2\.0$',
        '(?m)^Tag: v1\.2\.0$',
        '(?m)^Commit: [0-9a-f]{40}$',
        '(?m)^BuildTimeUTC: \d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z$',
        '(?m)^BuildTimeLocal: \d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}[+-]\d{2}:\d{2}$',
        '(?m)^SignatureStatus: Unsigned/NotSigned$'
    )) {
        if ($releaseInfo -notmatch $requiredPattern) {
            Add-Failure "RELEASE_INFO.txt is missing or has an invalid field: $requiredPattern"
        }
    }
}

$buildArtifacts = @(
    @{ Path = (Join-Path $root "build\blackhole.exe"); HashField = "BuildRendererSHA256" },
    @{ Path = (Join-Path $root "Blakhole_UI\build\Desktop_Qt_6_11_1_MinGW_64_bit-Release\appBlakholeUI.exe"); HashField = "BuildUiSHA256" }
)
foreach ($artifact in $buildArtifacts) {
    if (-not (Test-Path -LiteralPath $artifact.Path -PathType Leaf)) {
        Add-Failure "missing unstripped build artifact: $($artifact.Path)"
        continue
    }

    $sections = Get-ObjdumpOutput -Tool $objdump -Argument "-h" -Path $artifact.Path
    if ($sections -notmatch '(?im)^\s*\d+\s+\.debug(?:_|\b)') {
        Add-Failure "build artifact no longer contains expected .debug_* sections: $($artifact.Path)"
    }

    $actualHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $artifact.Path).Hash.ToLowerInvariant()
    $fieldPattern = "(?m)^$([regex]::Escape($artifact.HashField)): ([0-9a-f]{64})$"
    if ($releaseInfo -notmatch $fieldPattern) {
        Add-Failure "RELEASE_INFO.txt is missing $($artifact.HashField)"
    }
    elseif ($Matches[1] -cne $actualHash) {
        Add-Failure "RELEASE_INFO.txt $($artifact.HashField) does not match the build artifact"
    }
}

$checksumPath = Join-Path $ReleaseDir "release_checksums.sha256"
if (-not (Test-Path -LiteralPath $checksumPath -PathType Leaf)) {
    Add-Failure "missing release_checksums.sha256"
}
else {
    $manifestEntries = @{}
    $lineNumber = 0
    foreach ($line in Get-Content -Encoding UTF8 -LiteralPath $checksumPath) {
        $lineNumber++
        if ([string]::IsNullOrWhiteSpace($line)) {
            Add-Failure "release_checksums.sha256 contains an empty line at $lineNumber"
            continue
        }
        if ($line -cnotmatch '^([0-9a-f]{64})  ([a-z0-9][a-z0-9._/-]*)$') {
            Add-Failure "release_checksums.sha256 has an invalid line at $lineNumber"
            continue
        }

        $expectedHash = $Matches[1]
        $relativeName = $Matches[2]
        if ($relativeName.Contains("..") -or $relativeName.StartsWith("/")) {
            Add-Failure "release_checksums.sha256 has an unsafe path: $relativeName"
            continue
        }
        if ($manifestEntries.ContainsKey($relativeName)) {
            Add-Failure "release_checksums.sha256 has a duplicate entry: $relativeName"
            continue
        }

        $manifestEntries[$relativeName] = $expectedHash
        $actualPath = Join-Path $ReleaseDir ($relativeName.Replace('/', '\'))
        if (-not (Test-Path -LiteralPath $actualPath -PathType Leaf)) {
            Add-Failure "checksum target is missing: $relativeName"
            continue
        }
        $actualHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $actualPath).Hash.ToLowerInvariant()
        if ($actualHash -cne $expectedHash) {
            Add-Failure "checksum mismatch: $relativeName"
        }
    }

    $releaseRoot = (Resolve-Path -LiteralPath $ReleaseDir).Path.TrimEnd('\') + '\'
    $actualEntries = @{}
    foreach ($file in Get-ChildItem -LiteralPath $ReleaseDir -Recurse -File) {
        $relativeName = $file.FullName.Substring($releaseRoot.Length).Replace('\', '/').ToLowerInvariant()
        if ($relativeName -ceq "release_checksums.sha256") {
            continue
        }
        if ($actualEntries.ContainsKey($relativeName)) {
            Add-Failure "release contains case-colliding paths: $relativeName"
            continue
        }
        $actualEntries[$relativeName] = $true
    }

    foreach ($actualName in $actualEntries.Keys) {
        if (-not $manifestEntries.ContainsKey($actualName)) {
            Add-Failure "release_checksums.sha256 is missing release file: $actualName"
        }
    }
    foreach ($manifestName in $manifestEntries.Keys) {
        if (-not $actualEntries.ContainsKey($manifestName)) {
            Add-Failure "release_checksums.sha256 has an extra entry: $manifestName"
        }
    }
}

$verifierPath = Join-Path $ReleaseDir "verify_release_checksums.ps1"
if (-not (Test-Path -LiteralPath $verifierPath -PathType Leaf)) {
    Add-Failure "missing standalone verify_release_checksums.ps1"
}

if ($failures.Count -gt 0) {
    foreach ($failure in $failures) {
        Write-Error "RELEASE_SECURITY_CHECK_FAILED: $failure" -ErrorAction Continue
    }
    exit 1
}

Write-Output "RELEASE_SECURITY_OK version=1.2.0"
