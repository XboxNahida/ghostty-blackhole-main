#!/usr/bin/env python3
"""Static regression checks for minimized startup and preview lazy loading."""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MAIN_CPP = (ROOT / "Blakhole_UI/main.cpp").read_text(encoding="utf-8")
MAIN_QML = (ROOT / "Blakhole_UI/Main.qml").read_text(encoding="utf-8")
CONFIG_QML = (ROOT / "Blakhole_UI/pages/BlackholeConfig.qml").read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"FAIL: {message}")


require(
    'setContextProperty("startMinimized", startMinimized)' in MAIN_CPP,
    "C++ exposes minimized state before loading Main.qml",
)
require("setPersistentGraphics(false)" in MAIN_CPP,
        "hidden tray window releases persistent graphics resources")
require("setPersistentSceneGraph(false)" in MAIN_CPP,
        "hidden tray window releases the persistent scene graph")
require("visible: !startMinimized" in MAIN_QML, "minimized startup never shows the root window")
require("id: blackholeConfigLoader" in MAIN_QML, "configuration page has a loader")
require("active: root.uiActivated" in MAIN_QML, "configuration page waits for first activation")
require("id: previewLoader" in CONFIG_QML, "small preview has a loader")
require(
    "active: configPage.previewsEnabled" in CONFIG_QML,
    "small preview exists only while the configuration page is visible",
)
require("id: largePreviewLoader" in CONFIG_QML, "large preview has a loader")
require("active: false" in CONFIG_QML[CONFIG_QML.index("id: largePreviewLoader") :],
        "large preview starts unloaded")
require("onClosed: largePreviewLoader.active = false" in CONFIG_QML,
        "closing the large preview releases it")

print("QML lazy preview regression checks passed")
