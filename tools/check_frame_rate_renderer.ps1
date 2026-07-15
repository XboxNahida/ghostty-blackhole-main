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

function RequireOrderInBlock(
    [string]$relativePath,
    [string]$blockStart,
    [string]$blockEnd,
    [string[]]$orderedText
) {
    $fullPath = Join-Path $projectRoot $relativePath
    $text = (Get-Content -Raw -Encoding UTF8 $fullPath) -replace "`r`n", "`n"
    $startIndex = $text.IndexOf($blockStart, [StringComparison]::Ordinal)
    $endIndex = if ($startIndex -ge 0) {
        $text.IndexOf($blockEnd, $startIndex + $blockStart.Length, [StringComparison]::Ordinal)
    } else {
        -1
    }
    if ($startIndex -lt 0 -or $endIndex -lt 0) {
        $script:failures += "$relativePath -> block not found: $blockStart ... $blockEnd"
        return
    }

    $block = $text.Substring($startIndex, $endIndex - $startIndex)
    $previousIndex = -1
    foreach ($expected in $orderedText) {
        $index = $block.IndexOf($expected, [StringComparison]::Ordinal)
        if ($index -lt 0) {
            $script:failures += "$relativePath -> block missing: $expected"
        } elseif ($index -le $previousIndex) {
            $script:failures += "$relativePath -> block order invalid: $expected"
        }
        $previousIndex = $index
    }
}

Require "src\gui_config.h" "int   frameRateLimit = kDefaultFrameRateLimit"
Require "src\frame_limiter.h" "kDefaultFrameRateLimit = 60"
Require "src\gui_config.cpp" 'frameRateLimit=%d'
Require "src\gui_config.cpp" "ParseFrameRateLimitText"
Reject "src\win32_gl.cpp" "wglSwapIntervalEXT(1)"

RequireOrderInBlock "src\gui_config.cpp" `
    "void LoadAdvancedConfig(BlackholeConfig& cfg)" `
    "    fclose(f);" `
    @("ParseFrameRateLimitText", 'sscanf(line, "%63[^=]=%f"')

RequireOrderInBlock "src\main.cpp" `
    "// ---- Create fullscreen black hole window via Win32 + WGL ----" `
    "    setbuf(stderr, NULL);" `
    @("if (!Win32GL_Init(", "return 1;`n    }`n    Win32GL_SetSwapInterval(0)",
      "FrameLimiter frameLimiter(cfg.frameRateLimit)")

RequireOrderInBlock "src\main.cpp" `
    "// ---- Main loop ----" `
    "    Bloom_Destroy(bloom);" `
    @("while (true)", "Win32GL_SwapBuffers(wgl)", "frames++;", "frameLimiter.wait();")

if ($failures.Count -gt 0) {
    throw "FRAME_RATE_RENDERER_CHECK_FAILED: $($failures -join '; ')"
}

"FRAME_RATE_RENDERER_OK"
