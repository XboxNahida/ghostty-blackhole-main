# Config Save Architecture Design

## Background

The current project has two configuration files:

- `blackhole_presets.txt`: main preset and runtime mode configuration.
- `blackhole_advanced.txt`: advanced visual and behavior overrides.

The renderer reads both files from its current working directory. The Qt UI already aligns `blackhole_presets.txt` with the renderer through `BlackHoleCore::configFilePath()` and `configDir()`, but `saveAdvancedConfig()` and `loadAdvancedConfig()` still use `QCoreApplication::applicationDirPath()`. This creates multiple real configuration locations and can make UI changes appear saved while the renderer reads stale values.

Recent symptoms match this failure mode:

- `followMouse` remains disabled in the renderer because the file it reads still contains `followMouse=0`.
- `fixedSize` and `growEnabled` appear confusing because their storage and shader semantics are split across the main and advanced config files.

## Goals

1. Make the Qt UI and renderer read and write the same configuration files.
2. Keep the existing text file formats for now to reduce migration risk.
3. Make save timing explicit: settings are saved before renderer start and when the user clicks save or closes the UI.
4. Make size mode semantics clear and mutually exclusive in the UI.
5. Add lightweight checks that catch future broken config links.

## Non-Goals

- Do not replace both text files with JSON in this pass.
- Do not redesign preset storage or shader preset data.
- Do not implement live hot-reload of every setting in this pass.
- Do not move config files outside the project or release directory layout.

## Recommended Approach

Use a staged refactor.

Stage 1 fixes the current bug class by making advanced config use the same directory as main config:

```text
BlackHoleCore::configDir()
  -> blackhole_presets.txt
  -> blackhole_advanced.txt
```

Stage 2 makes the size controls semantically explicit:

```text
Default growth:
  fixedSize=false, growEnabled=false

Custom growth:
  fixedSize=false, growEnabled=true, initialSize=<value>

Fixed size:
  fixedSize=true, growEnabled=false, fixedLevel=<value>
```

Stage 3 extracts small helper functions in `BlackHoleCore` for path and save invariants. A full `ConfigStore` class is explicitly out of scope for this pass; the first implementation should avoid broad churn.

## Data Flow

### Qt UI Save

User changes QML controls. The controls update `BlackHoleCore` properties immediately.

`BlackHoleCore::saveConfig()` writes:

- `configDir()/blackhole_presets.txt`
- `configDir()/blackhole_advanced.txt`
- other existing sidecar files already managed by `saveConfig()`

`BlackHoleCore::startRenderer()` calls `saveConfig()` before spawning the renderer.

### Renderer Load

`blackhole.exe --render` sets its working directory to the project root when running from `build/`, or to the executable directory when running from `release/`.

It then reads:

- `blackhole_presets.txt`
- `blackhole_advanced.txt`
- `shaders/*`

All of these must live in the same config/runtime directory chosen by the UI.

## UI Semantics

The current UI has two checkboxes: gradual growth and fixed size. They are mutually exclusive in QML, but the labels imply that disabling gradual growth means no growth. That is false: the shader's default behavior still grows from small to full.

Replace or relabel the controls so the user sees one conceptual mode:

- `Default birth animation`: renderer uses default shader growth.
- `Custom growth`: renderer grows from `initialSize`.
- `Fixed size`: renderer uses `fixedLevel` and bypasses growth.

Implementation may keep the underlying booleans for compatibility. The UI should ensure only one mode maps to the booleans at a time.

## Error Handling

- If the UI cannot resolve the renderer directory, it should keep the existing fallback to `QCoreApplication::applicationDirPath()`.
- If `blackhole_advanced.txt` is missing, defaults remain valid.
- If old files contain only legacy keys such as `holeRadius`, existing compatibility parsing stays in place.
- Save operations should write complete files, not partial snippets.

## Verification

The implementation must include checks for:

- `saveAdvancedConfig()` and `loadAdvancedConfig()` both use `configDir()`.
- Starting the renderer after setting `followMouse=true` writes `followMouse=1` to the file the renderer will read.
- `fixedSize=true` writes the main config line with fixed-size field `1`.
- Size mode mapping is mutually exclusive.
- Main renderer build passes.
- Qt UI build passes.

## Rollout

1. First commit: advanced config path unification and regression checks.
2. Second commit: size mode UI semantics cleanup.
3. Third commit, only if needed: small config helper extraction and documentation updates.

This keeps each step reversible and testable.
