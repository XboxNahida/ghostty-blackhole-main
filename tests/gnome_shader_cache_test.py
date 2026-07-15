#!/usr/bin/env python3
"""Static regression checks for the shared configured shader cache."""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
EFFECT = (ROOT / "gnome-extension/blackhole@xboxnahida.github.com/effect.js").read_text()
EXTENSION = (ROOT / "gnome-extension/blackhole@xboxnahida.github.com/extension.js").read_text()


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"FAIL: {message}")


require("export class ConfiguredShaderCache" in EFFECT, "shared shader cache exists")
require("configFingerprint()" in EFFECT, "cache fingerprints configuration metadata")
require("this._baseShader = loadShader" in EFFECT, "base shader template is cached")
require("fingerprint === this._fingerprint" in EFFECT, "unchanged config reuses cached source")
require("revision: ++this._revision" in EFFECT, "rebuilt shader receives a new revision")
require("configured.revision === this._shaderRevision" in EFFECT,
        "effects skip unchanged shader source")
require("this._shaderCache = new ConfiguredShaderCache(this.path)" in EXTENSION,
        "extension owns one cache for both effects")
require("const configured = this._shaderCache.get();" in EXTENSION,
        "Start obtains one shared configured source")
require("this._shaderCache.get(true)" in EXTENSION,
        "ReloadConfig forces a rebuild")
require("effect.reloadConfig()" not in EXTENSION,
        "effects never rebuild configuration independently")

print("GNOME shader cache regression checks passed")
