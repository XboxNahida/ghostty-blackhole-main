$ErrorActionPreference = "Stop"

$projectRoot = Split-Path -Parent $PSScriptRoot

function Get-ProjectText {
    param([Parameter(Mandatory = $true)][string]$RelativePath)

    $path = Join-Path $projectRoot $RelativePath
    if (-not (Test-Path -LiteralPath $path)) {
        throw "Missing file: $RelativePath"
    }
    return [System.IO.File]::ReadAllText($path, [System.Text.Encoding]::UTF8)
}

function Require-Pattern {
    param(
        [Parameter(Mandatory = $true)][string]$RelativePath,
        [Parameter(Mandatory = $true)][string]$Pattern,
        [Parameter(Mandatory = $true)][string]$Description
    )

    $text = Get-ProjectText $RelativePath
    if ($text -notmatch $Pattern) {
        throw "Missing ${Description}: $RelativePath"
    }
}

Require-Pattern "src\gui_config.h" "bool\s+lightingEffect\s*=\s*false" "renderer lighting flag"
Require-Pattern "src\gui_config.cpp" 'strcmp\(key,\s*"lightingEffect"\)' "renderer lighting read key"
Require-Pattern "src\gui_config.cpp" 'strcmp\(key,\s*"screenSwallow"\)[^\r\n]*lightingEffect' "legacy renderer migration"
Require-Pattern "Blakhole_UI\core\blackholecore.h" "Q_PROPERTY\(bool\s+lightingEffect" "Qt lighting property"
Require-Pattern "Blakhole_UI\core\blackholecore.cpp" 'out\s*<<\s*"lightingEffect="' "Qt lighting save key"
Require-Pattern "Blakhole_UI\core\blackholecore.cpp" 'key\s*==\s*"screenSwallow"[^\r\n]*m_lightingEffect' "legacy Qt migration"
$qml = Get-ProjectText "Blakhole_UI\pages\AdvancedConfig.qml"
if ($qml -notmatch 'id:\s*lightingCheck' -or $qml -notmatch 'bhCore\.lightingEffect') {
    throw "Missing lighting checkbox binding: Blakhole_UI\pages\AdvancedConfig.qml"
}
if ($qml -match 'screenSwallow|swallowStrength') {
    throw "Advanced UI still exposes swallow/strength controls"
}

$defaultConfig = Get-ProjectText "blackhole_advanced.txt"
if ($defaultConfig -notmatch '(?m)^lightingEffect=0$') {
    throw "Missing default lighting key: blackhole_advanced.txt"
}

$rendererConfig = Get-ProjectText "src\gui_config.cpp"
if ($rendererConfig -match 'fprintf\([^\r\n]*"(?:screenSwallow|swallowStrength)=') {
    throw "Renderer still saves legacy swallow keys"
}

$qtConfig = Get-ProjectText "Blakhole_UI\core\blackholecore.cpp"
if ($qtConfig -match 'out\s*<<\s*"(?:screenSwallow|swallowStrength)=') {
    throw "Qt UI still saves legacy swallow keys"
}

"LIGHTING_EFFECT_OK"
