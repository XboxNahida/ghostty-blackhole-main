#!/usr/bin/env python3
"""Static regression checks for the Linux event-driven idle path."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
CORE = (ROOT / "Blakhole_UI/core/blackholecore.cpp").read_text()


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


apply_start = CORE[CORE.index("void BlackHoleCore::applyAndStart()") : CORE.index("void BlackHoleCore::stopAll()")]
linux_branch = apply_start[apply_start.index("#else") : apply_start.index("#endif", apply_start.index("#else"))]
require("m_gnomeIdle->start()" in linux_branch, "Linux idle mode must start IdleMonitor directly")
require("m_mpris->start()" in linux_branch, "Linux idle mode must start MPRIS directly")
require("m_idleTimer->start()" not in linux_branch, "Linux idle mode must not start the one-second timer")

require(
    "connect(m_gnomeIdle, &GnomeIdleMonitor::stateChanged" in CORE,
    "IdleMonitor state changes must refresh Linux idle UI state",
)
playing_handler = CORE[CORE.index("connect(m_mpris, &MprisMonitor::playingChanged") : CORE.index(
    "if (!QDBusConnection::sessionBus().connect"
)]
require(
    "updateLinuxIdleDetectionState();" in playing_handler,
    "MPRIS state changes must refresh Linux idle UI state",
)
require(
    "m_gnomeIdle && m_gnomeIdle->isStarted()" in CORE,
    "systemActive must use the event monitor's enabled state on Linux",
)

print("Linux event-driven idle regression checks passed")
