// autostart_xdg.h — XDG autostart (.desktop file) for Linux
#pragma once

#include <QString>

struct AutoStartXdgResult {
    bool success = false;
    QString errorMessage;
};

// Write or remove ~/.config/autostart/io.github.xboxnahida.Blakhole.desktop.
// When enabled, creates a .desktop file that launches the UI with --minimized.
// When disabled, deletes it. Uses QSaveFile for atomic writes.
AutoStartXdgResult AutoStart_XDG_Set(bool enabled, const QString &exePath);

// Check whether our autostart .desktop file currently exists.
bool AutoStart_XDG_Query();
