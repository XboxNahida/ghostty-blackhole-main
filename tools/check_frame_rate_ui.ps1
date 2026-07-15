$ErrorActionPreference = "Stop"

$projectRoot = Split-Path -Parent $PSScriptRoot
$required = @{
    "Blakhole_UI\core\blackholecore.h" = @(
        "Q_PROPERTY(int frameRateLimit",
        "int frameRateLimit() const;",
        "void setFrameRateLimit(int v);",
        "void frameRateLimitChanged();",
        "m_frameRateLimit = kDefaultFrameRateLimit"
    )
    "Blakhole_UI\core\blackholecore.cpp" = @(
        'out << "frameRateLimit="',
        'key == "frameRateLimit"',
        "NormalizeFrameRateLimit"
    )
    "Blakhole_UI\pages\AdvancedConfig.qml" = @(
        'label: "帧率限制"',
        "to: 241",
        '"无限制"',
        "frameRateLimit === 0 ? 241 : frameRateLimit"
    )
}

$missing = @()
foreach ($relativePath in $required.Keys) {
    $fullPath = Join-Path $projectRoot $relativePath
    $text = Get-Content -Raw -Encoding UTF8 $fullPath
    foreach ($expected in $required[$relativePath]) {
        if (-not $text.Contains($expected)) {
            $missing += "$relativePath -> $expected"
        }
    }
}

if ($missing.Count -gt 0) {
    throw "FRAME_RATE_UI_CHECK_FAILED: $($missing -join '; ')"
}

"FRAME_RATE_UI_OK"
