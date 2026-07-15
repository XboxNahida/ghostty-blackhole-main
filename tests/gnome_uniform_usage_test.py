#!/usr/bin/env python3
"""Keep GNOME compositor per-frame uniform submissions minimal and valid."""

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
EXTENSION = ROOT / "gnome-extension/blackhole@xboxnahida.github.com"
EFFECT = (EXTENSION / "effect.js").read_text()
HEADER = (EXTENSION / "shaders/compositor_header.glsl").read_text()
SHADER = (EXTENSION / "shaders/blackhole.glsl").read_text()

EXPECTED_PER_FRAME = {
    "iTime",
    "uResolutionX",
    "uResolutionY",
    "iChannel0",
    "uTransition",
    "uHomeX",
    "uHomeY",
    "uRandPhase",
}


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"FAIL: {message}")


paint_start = EFFECT.index("    vfunc_paint_target(")
paint_end = EFFECT.index("\n    }\n});", paint_start)
paint_method = EFFECT[paint_start:paint_end]
effect_without_paint = EFFECT[:paint_start] + EFFECT[paint_end:]

submitted = set(re.findall(r"set_uniform_value\('([^']+)'", paint_method))
require(
    submitted == EXPECTED_PER_FRAME,
    "vfunc_paint_target must submit exactly the eight compositor uniforms "
    f"(expected {sorted(EXPECTED_PER_FRAME)}, found {sorted(submitted)})",
)

# Declarations alone do not make a uniform live. Search the compositor body and
# the GLSL snippets/replacements embedded in effect.js, excluding the setters.
header_without_declarations = re.sub(
    r"^\s*uniform\s+[^;]+;\s*$", "", HEADER, flags=re.MULTILINE
)
compositor_usage = header_without_declarations + SHADER + effect_without_paint
for name in submitted:
    require(
        re.search(rf"\b{re.escape(name)}\b", compositor_usage) is not None,
        f"per-frame uniform {name} is referenced by the compositor shader",
    )

removed = {
    "uHoleRadius",
    "uDiskGain",
    "uDiskTemp",
    "uExposure",
    "uSpeed",
    "uStarGain",
    "uDiskIncl",
    "uBornProgress",
    "uDistortion",
    "uSlotSec",
    "uPresetOffset",
}
declared = set(re.findall(r"^\s*uniform\s+\w+\s+(\w+)", HEADER, re.MULTILINE))
require(not (removed & submitted), "unused legacy uniforms are not submitted")
require(not (removed & declared), "unused legacy uniforms are not declared")

print("GNOME compositor uniform regression checks passed")
