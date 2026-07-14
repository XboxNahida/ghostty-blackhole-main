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
methods:   Start, Stop, ReloadConfig, GetState, ConfigureShortcut
signal:    StopShortcutActivated
```

On Ubuntu GNOME, `appBlakholeUI` is the settings and lifecycle controller for
this backend. `Start` reloads the user's presets and advanced settings from
`~/.config/XboxNahida/Blakhole UI/` before enabling the compositor effects, so
pressing Start also applies changes without launching the legacy fullscreen
renderer.

The extension also owns the GNOME Shell global stop shortcut. Its GSettings
schema stores the enabled state and accelerator (default
`<Control><Alt>b`). `ConfigureShortcut` lets the Qt controller update that
state without raw keyboard capture or an X11/XRecord native hook. Activating
the shortcut stops the compositor effect immediately and emits
`StopShortcutActivated`, allowing the controller to stop idle-mode scheduling
as well.
