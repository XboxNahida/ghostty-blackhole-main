# GNOME Shell compositor backend

`blackhole@xboxnahida.github.com` is the Ubuntu GNOME 50 backend for the live
desktop effect.  It applies the same effect to the Shell background and window
groups, so the desktop texture is sampled inside the compositor without an
XDG Desktop Portal capture stream.

The compositor backend now loads the full rotating accretion-disk shader.  The
small lens/ring shader remains in `shaders/prototype.glsl` as a diagnostic
fallback for compositor texture sampling and animation problems.

Control interface:

```text
bus:       io.github.xboxnahida.Blackhole
object:    /io/github/xboxnahida/Blackhole
interface: io.github.xboxnahida.Blackhole
methods:   Start, Stop, ReloadConfig, GetState
```

On Ubuntu GNOME, `appBlakholeUI` is the settings and lifecycle controller for
this backend. `Start` reloads the user's presets and advanced settings from
`~/.config/XboxNahida/Blakhole UI/` before enabling the compositor effects, so
pressing Start also applies changes without launching the legacy fullscreen
renderer.
