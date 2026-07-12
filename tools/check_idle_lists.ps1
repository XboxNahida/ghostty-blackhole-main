$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$header = Get-Content -Raw -Encoding UTF8 (Join-Path $root "Blakhole_UI\core\blackholecore.h")
$core = Get-Content -Raw -Encoding UTF8 (Join-Path $root "Blakhole_UI\core\blackholecore.cpp")
$qml = Get-Content -Raw -Encoding UTF8 (Join-Path $root "Blakhole_UI\pages\IdleListConfig.qml")

$requiredHeader = @(
    "Q_PROPERTY(QStringList idleForceBlocklist",
    "QStringList idleForceBlocklist() const",
    "void setIdleForceBlocklist(const QStringList &list)",
    "void idleForceBlocklistChanged()",
    "QStringList m_idleForceBlocklist"
)
foreach ($text in $requiredHeader) {
    if (-not $header.Contains($text)) {
        throw "IDLE_LISTS_CHECK_FAILED: missing header contract [$text]"
    }
}

$requiredCore = @(
    "# Blackhole Idle Lists v2",
    "[mediaHints]",
    "[/mediaHints]",
    "[forceBlocklist]",
    "[/forceBlocklist]",
    'line == "[blacklist]"',
    "m_idleForceBlocklist"
)
foreach ($text in $requiredCore) {
    if (-not $core.Contains($text)) {
        throw "IDLE_LISTS_CHECK_FAILED: missing config or runtime contract [$text]"
    }
}

$legacyMatch = [regex]::Match(
    $core,
    'line\s*==\s*"\[blacklist\]"[^\r\n]*m_idleBlacklist')
if (-not $legacyMatch.Success) {
    throw "IDLE_LISTS_CHECK_FAILED: legacy [blacklist] must migrate to media hints"
}

$forceIndex = $core.IndexOf("m_idleForceBlocklist", $core.IndexOf("void BlackHoleCore::checkIdle()"))
$mediaIndex = $core.IndexOf("MediaSession_QueryCurrent", $core.IndexOf("void BlackHoleCore::checkIdle()"))
$audioIndex = $core.IndexOf("IAudioSessionManager2", $core.IndexOf("void BlackHoleCore::checkIdle()"))
if ($forceIndex -lt 0 -or $mediaIndex -lt 0 -or $audioIndex -lt 0 -or
    $forceIndex -gt $mediaIndex -or $forceIndex -gt $audioIndex) {
    throw "IDLE_LISTS_CHECK_FAILED: foreground force blocklist must run before media and audio checks"
}

foreach ($text in @(
    "bool forceBlocked = false",
    "forceBlocked = true",
    "forceBlocked || (watchingVideo && !m_videoAsIdle)",
    "watchingVideo = false"
)) {
    if (-not $core.Contains($text)) {
        throw "IDLE_LISTS_CHECK_FAILED: missing list priority contract [$text]"
    }
}

foreach ($pattern in @(
    "idleForceBlocklist",
    "\u59cb\u7ec8\u5141\u8bb8\u89e6\u53d1",
    "\u5a92\u4f53\u8bc6\u522b\u63d0\u793a",
    "\u524d\u53f0\u5f3a\u5236\u4e0d\u89e6\u53d1"
)) {
    if (-not [regex]::IsMatch($qml, $pattern)) {
        throw "IDLE_LISTS_CHECK_FAILED: missing QML semantics [$pattern]"
    }
}
if ([regex]::IsMatch($qml, "\u9ed1\u540d\u5355 \(\u59cb\u7ec8\u963b\u6b62\)")) {
    throw "IDLE_LISTS_CHECK_FAILED: media hints must not be labelled as always blocking"
}

Write-Output "IDLE_LISTS_OK"
