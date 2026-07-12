param(
    [ValidateSet("All", "AutoStart", "Foreground", "MediaSession")]
    [string]$Only = "All"
)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$compiler = "C:\msys64\ucrt64\bin\g++.exe"
$outputDir = Join-Path $root "build\tests"

if (-not (Test-Path -LiteralPath $compiler)) {
    throw "IDLE_PHASE1_TESTS_FAILED: compiler not found: $compiler"
}

New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

function Build-And-Run {
    param(
        [Parameter(Mandatory = $true)][string]$Name,
        [Parameter(Mandatory = $true)][string[]]$Sources,
        [Parameter(Mandatory = $true)][string[]]$Libraries
    )

    $exe = Join-Path $outputDir "$Name.exe"
    $args = @("-std=c++17", "-Wall", "-Wextra", "-Werror", "-Isrc") + $Sources + @("-o", $exe) + $Libraries
    & $compiler @args
    if ($LASTEXITCODE -ne 0) {
        throw "IDLE_PHASE1_TESTS_FAILED: compile $Name"
    }

    & $exe
    if ($LASTEXITCODE -ne 0) {
        throw "IDLE_PHASE1_TESTS_FAILED: run $Name"
    }
}

Push-Location $root
try {
    if ($Only -eq "All" -or $Only -eq "AutoStart") {
        Build-And-Run -Name "autostart_registry_tests" `
            -Sources @("tests/autostart_registry_tests.cpp", "src/autostart_registry.cpp") `
            -Libraries @("-ladvapi32")
    }

    if ($Only -eq "All" -or $Only -eq "Foreground") {
        Build-And-Run -Name "foreground_window_tests" `
            -Sources @("tests/foreground_window_tests.cpp", "src/foreground_window.cpp") `
            -Libraries @("-luser32")
    }

    if ($Only -eq "All" -or $Only -eq "MediaSession") {
        Build-And-Run -Name "media_session_tests" `
            -Sources @("tests/media_session_tests.cpp", "src/media_session.cpp") `
            -Libraries @("-lruntimeobject", "-lole32")
    }
} finally {
    Pop-Location
}

Write-Output "IDLE_PHASE1_TESTS_OK"
