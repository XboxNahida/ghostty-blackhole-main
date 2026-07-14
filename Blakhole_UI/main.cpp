#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QIcon>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QSurfaceFormat>
#include "app_version.h"
#include "core/systemtray.h"
#include "core/blackholecore.h"
#include "core/blackholepreviewfbo.h"
#include "core/update_checker.h"

#ifdef Q_OS_WIN
#include <windows.h>
#include <dwmapi.h>
#endif

int main(int argc, char *argv[])
{
    // QQuickFramebufferObject + QOpenGLFunctions requires the OpenGL backend.
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);

    // The preview shaders require desktop GLSL 3.30. Request desktop OpenGL
    // everywhere, but only set the CoreProfile bit on Windows. Passing that
    // bit through EGL fails with EGL_BAD_MATCH on the NVIDIA Wayland driver.
    QSurfaceFormat fmt;
    fmt.setRenderableType(QSurfaceFormat::OpenGL);
    fmt.setVersion(3, 3);
#ifdef Q_OS_WIN
    fmt.setProfile(QSurfaceFormat::CoreProfile);
#endif
    QSurfaceFormat::setDefaultFormat(fmt);
    QApplication app(argc, argv);
    const bool minimizedArgument = app.arguments().contains(QStringLiteral("--minimized"));

    QCoreApplication::setApplicationName(QStringLiteral("Blakhole UI"));
    QCoreApplication::setOrganizationName(QStringLiteral("XboxNahida"));
    QCoreApplication::setApplicationVersion(QStringLiteral(APP_VERSION_STRING));

    QIcon appIcon(":/new/prefix1/fonts/icon.ico");
    if (appIcon.isNull())
        appIcon = QIcon::fromTheme(QStringLiteral("application-x-executable"));
    app.setWindowIcon(appIcon);

    qmlRegisterType<BlackHoleCore>("BlakholeUI", 1, 0, "BlackHoleCore");
    qmlRegisterType<BlackholePreviewFBO>("BlakholeUI", 1, 0, "BlackholePreviewFBO");
    qmlRegisterType<SystemTray>("BlakholeUI", 1, 0, "SystemTray");

    QQmlApplicationEngine engine;
    BlackHoleCore blackHoleCore;
    UpdateChecker updateChecker(QStringLiteral(APP_VERSION_STRING));
    engine.rootContext()->setContextProperty("blackHoleCore", &blackHoleCore);
    engine.rootContext()->setContextProperty("updateChecker", &updateChecker);
    blackHoleCore.loadConfig();

    // MSYS2 的 Qt6 编译时硬编码 QML 导入路径为 <exe_dir>/share/qt6/qml/,
    // 但 windeployqt6 把 QML 模块部署到 <exe_dir>/qml/. 两者不匹配,
    // 导致运行时报 "module QtQuick.Controls is not installed".
    // 显式添加 exe 同级 qml/ 目录到导入路径.
    engine.addImportPath(QCoreApplication::applicationDirPath() + "/qml");

    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);

    engine.loadFromModule("BlakholeUI", "Main");

    // 关联托盘与窗口（跨平台）
    const auto rootObjects = engine.rootObjects();
    if (!rootObjects.isEmpty()) {
        QQuickWindow *window = qobject_cast<QQuickWindow*>(rootObjects.first());
        if (window) {
#ifdef Q_OS_WIN
            HWND hwnd = reinterpret_cast<HWND>(window->winId());
            int cornerPreference = 2;
            DwmSetWindowAttribute(hwnd, 33, &cornerPreference, sizeof(cornerPreference));
#endif

            auto *tray = window->findChild<SystemTray*>();
            if (tray) {
                tray->setWindow(window);

                // 启动后自动隐藏界面并启动黑洞
                if (minimizedArgument || blackHoleCore.launchMinimized()) {
                    tray->show();
                    blackHoleCore.applyAndStart();
                }
            }
        }
    }

    return app.exec();
}
