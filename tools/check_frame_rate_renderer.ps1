$ErrorActionPreference = "Stop"

$projectRoot = Split-Path -Parent $PSScriptRoot
$failures = @()

function Require([string]$relativePath, [string]$expected) {
    $fullPath = Join-Path $projectRoot $relativePath
    $text = Get-Content -Raw -Encoding UTF8 $fullPath
    if (-not $text.Contains($expected)) {
        $script:failures += "$relativePath -> missing: $expected"
    }
}

function Reject([string]$relativePath, [string]$unexpected) {
    $fullPath = Join-Path $projectRoot $relativePath
    $text = Get-Content -Raw -Encoding UTF8 $fullPath
    if ($text.Contains($unexpected)) {
        $script:failures += "$relativePath -> forbidden: $unexpected"
    }
}

Require "src\gui_config.h" "int   frameRateLimit = kDefaultFrameRateLimit"
Require "src\gui_config.cpp" 'frameRateLimit=%d'
Require "src\gui_config.cpp" 'strcmp(key, "frameRateLimit") == 0'
Require "src\main.cpp" "FrameLimiter frameLimiter(cfg.frameRateLimit)"
Require "src\main.cpp" "frameLimiter.wait();"
Require "src\main.cpp" "Win32GL_SetSwapInterval(0)"
Reject "src\win32_gl.cpp" "wglSwapIntervalEXT(1)"

if ($failures.Count -gt 0) {
    throw "FRAME_RATE_RENDERER_CHECK_FAILED: $($failures -join '; ')"
}

"FRAME_RATE_RENDERER_OK"
