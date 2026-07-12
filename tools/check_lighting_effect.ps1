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

Require-Pattern "shaders\frag_desktop_header.glsl" "uniform\s+int\s+uLightingEffect" "lighting uniform"
Require-Pattern "src\main.cpp" "diskLocalP" "preset-local disk coordinates"
Require-Pattern "src\main.cpp" "outerLightMist" "outer light mist"
Require-Pattern "src\main.cpp" "darkFlowBands" "moving dark bands"
Require-Pattern "src\main.cpp" "nativeGapEntry" "native disk-gap entry"
Require-Pattern "src\main.cpp" "nativeGapExit" "native disk-gap exit"
Require-Pattern "src\main.cpp" "outerDiskFade" "outer-to-inner disk gradient"
Require-Pattern "src\main.cpp" "warmGoldLight" "gold lighting"
Require-Pattern "src\main.cpp" "coldBlueLight" "blue lighting"
Require-Pattern "src\main.cpp" "lightingPhotonRing" "photon ring lighting"
Require-Pattern "src\main.cpp" 'lightingLensScale\s*=\s*cfg\.lightingEffect\s*\?\s*0\.42f' "lighting lens suppression"
Require-Pattern "src\main.cpp" "subtleDiskLighting" "subtle disk lighting"
Require-Pattern "src\main.cpp" "clampedBaseScene" "isolated lighting HDR source"
Require-Pattern "src\main.cpp" 'nativeGapEntry\s*=\s*smoothstep\(0\.036,\s*0\.042' "hairline native gap entry"
Require-Pattern "src\main.cpp" 'nativeGapExit\s*=\s*1\.0\s*-\s*smoothstep\(0\.046,\s*0\.052' "hairline native gap exit"

$mainText = Get-ProjectText "src\main.cpp"
$desktopHeader = Get-ProjectText "shaders\frag_desktop_header.glsl"
$legacyShaderPattern = 'uScreenSwallow|uSwallowStrength|continuousSwallow|gravityWellField|uiDebrisSuppression|effectiveShaderSpeed|swallowMotionScale'
if ($mainText -match $legacyShaderPattern -or $desktopHeader -match $legacyShaderPattern) {
    throw "Renderer still contains legacy swallow shader or motion logic"
}
if ($mainText -match 'outerLightMist\s*=\s*analyticDiskBand' -or $mainText -match 'sin\(bandPhase') {
    throw "Lighting still uses an analytic glass shell or synthetic circular dark bands"
}

Require-Pattern "src\bloom_renderer.h" "struct\s+BloomRenderer" "Bloom state"
Require-Pattern "src\bloom_renderer.h" "Bloom_BeginScene" "Bloom begin interface"
Require-Pattern "src\bloom_renderer.h" "Bloom_EndScene" "Bloom composite interface"
Require-Pattern "src\bloom_renderer.cpp" "GL_RGBA16F" "HDR scene texture"
Require-Pattern "src\bloom_renderer.cpp" 'uv\s*=\s*p\s*\*\s*0\.5' "normalized fullscreen texture coordinates"
Require-Pattern "src\bloom_renderer.cpp" "blurDirection" "separable Gaussian blur"
Require-Pattern "src\bloom_renderer.cpp" "bloomTexture" "Bloom composite sampler"
Require-Pattern "src\bloom_renderer.cpp" 'source\s*-\s*vec3\(1\.02\)' "isolated HDR Bloom threshold"
Require-Pattern "src\bloom_renderer.cpp" "preservedScene" "scene-preserving Bloom composite"
Require-Pattern "src\bloom_renderer.cpp" "kBlurPasses\s*=\s*3" "controlled Bloom radius"
Require-Pattern "src\bloom_renderer.cpp" 'bloom\s*\*\s*0\.38' "controlled Bloom intensity"
Require-Pattern "src\main.cpp" "Bloom_BeginScene" "main loop Bloom begin"
Require-Pattern "src\main.cpp" "Bloom_EndScene" "main loop Bloom end"

"LIGHTING_EFFECT_OK"
