// hotkey.h  Independent global hotkey module (minimal deps: only <windows.h>)
#pragma once
#include <windows.h>

struct HotkeyConfig {
    UINT modifiers = MOD_CONTROL | MOD_SHIFT;  // MOD_CONTROL, MOD_SHIFT, MOD_ALT, MOD_WIN
    UINT vk        = 'Q';                      // virtual-key code

    // Register / unregister as a global hotkey (ID = hotkeyId)
    bool Register(HWND hwnd, int hotkeyId = 1) const;
    void Unregister(HWND hwnd, int hotkeyId = 1) const;

    // Human-readable string, e.g. "Ctrl+Shift+Q"
    void Format(char* buf, int bufSize) const;

    // Serialize for config file, e.g. "CS:Q"
    void ToString(char* buf, int bufSize) const;

    // Deserialize from config string; returns true on success
    bool FromString(const char* str);

    // Check if this hotkey is "none" (no key assigned)
    bool IsNone() const { return vk == 0; }

    // Reset to default: Ctrl+Shift+Q
    void SetDefault() { modifiers = MOD_CONTROL | MOD_SHIFT; vk = 'Q'; }
};
