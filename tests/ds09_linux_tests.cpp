#include "autostart_xdg.h"
#include "renderer_process_state.h"
#include "screen_selection_linux.h"
#include "systemtray.h"
#include <QApplication>
#include <QQuickWindow>
#include <QFile>
#include <QTemporaryDir>
#include <cassert>

int main(int argc, char **argv) {
    QTemporaryDir config;
    qputenv("XDG_CONFIG_HOME", config.path().toUtf8());
    QApplication app(argc, argv);

    QTemporaryDir bin;
    const QString exe = bin.filePath(QStringLiteral("black hole \\\"$`"));
    QFile f(exe); assert(f.open(QIODevice::WriteOnly)); f.write("#!/bin/sh\n"); f.close();
    assert(f.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner));
    assert(AutoStart_XDG_Set(true, exe).success); assert(AutoStart_XDG_Query());
    QFile desktop(config.filePath("autostart/io.github.xboxnahida.Blakhole.desktop"));
    assert(desktop.open(QIODevice::ReadOnly)); const QByteArray body = desktop.readAll();
    assert(body.contains(" --minimized")); assert(body.contains("Exec=\"")); assert(body.contains("\\$"));
    assert(AutoStart_XDG_Set(false, exe).success); assert(!AutoStart_XDG_Query());
    assert(!AutoStart_XDG_Set(true, QStringLiteral("relative/program")).success);

    const QStringList a{"DP-1", "HDMI-1"};
    assert(ResolveLinuxDisplays(1, "HDMI-1", a) == QVector<int>{1});
    const QStringList reordered{"HDMI-1", "DP-1"};
    assert(ResolveLinuxDisplays(1, "HDMI-1", reordered) == QVector<int>{0});
    assert(ResolveLinuxDisplays(1, "missing", a) == QVector<int>{1});
    assert(ResolveLinuxDisplays(2, {}, a) == QVector<int>{0});
    assert(ResolveLinuxDisplays(3, {}, a) == (QVector<int>{0, 1}));

    RendererProcessState state;
    assert(state.processStarted()); assert(!state.processStarted());
    assert(!state.processStopped()); assert(state.runningCount() == 1);
    assert(state.processStopped()); assert(state.runningCount() == 0);
    state.reset(); assert(state.runningCount() == 0);

    QQuickWindow window;
    SystemTray tray(false, nullptr); tray.setWindow(&window); tray.show();
    assert(!tray.isVisible()); assert(window.isVisible());
}
