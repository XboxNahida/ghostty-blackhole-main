// autostart_xdg.cpp — XDG autostart (.desktop file) for Linux
#include "autostart_xdg.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTextStream>

// The autostart directory and our application's .desktop file name.
static QString autostartDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
           + QStringLiteral("/autostart");
}

static QString desktopFilePath()
{
    return autostartDir() + QStringLiteral("/io.github.xboxnahida.Blakhole.desktop");
}

static QString desktopExecQuote(const QString &value)
{
    QString escaped = value;
    escaped.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
    escaped.replace(QLatin1Char('"'), QStringLiteral("\\\""));
    escaped.replace(QLatin1Char('`'), QStringLiteral("\\`"));
    escaped.replace(QLatin1Char('$'), QStringLiteral("\\$"));
    return QLatin1Char('"') + escaped + QLatin1Char('"');
}

AutoStartXdgResult AutoStart_XDG_Set(bool enabled, const QString &exePath)
{
    const QString path = desktopFilePath();

    if (!enabled) {
        // Remove our .desktop file only.
        QFile f(path);
        if (f.exists()) {
            if (!f.remove()) {
                return {false, QStringLiteral("Cannot remove autostart file: ") + f.errorString()};
            }
        }
        return {true, {}};
    }

    const QFileInfo executable(exePath);
    if (!executable.isAbsolute() || !executable.isFile() || !executable.isExecutable()) {
        return {false, QStringLiteral("Executable must be an absolute executable file: ") + exePath};
    }

    // Ensure the autostart directory exists
    const QString dir = autostartDir();
    if (!QDir().mkpath(dir)) {
        return {false, QStringLiteral("Cannot create autostart directory: ") + dir};
    }

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return {false, QStringLiteral("Cannot open autostart file for writing: ") + file.errorString()};
    }

    QTextStream out(&file);
    out << "[Desktop Entry]\n"
        << "Type=Application\n"
        << "Name=Blakhole\n"
        << "Comment=Black hole screensaver\n"
        << "Exec=" << desktopExecQuote(executable.absoluteFilePath()) << " --minimized\n"
        << "Terminal=false\n"
        << "Categories=Utility;\n"
        << "X-GNOME-Autostart-enabled=true\n";

    if (!file.commit()) {
        return {false, QStringLiteral("Cannot write autostart file atomically: ") + file.errorString()};
    }

    // Verify by checking existence
    if (!QFileInfo::exists(path)) {
        return {false, QStringLiteral("Autostart file was not created: ") + path};
    }

    return {true, {}};
}

bool AutoStart_XDG_Query()
{
    return QFileInfo::exists(desktopFilePath());
}
