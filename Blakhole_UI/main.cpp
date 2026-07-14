#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QIcon>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QSurfaceFormat>
#ifndef Q_OS_WIN
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#endif
#include "app_version.h"
#include "core/systemtray.h"
#include "core/blackholecore.h"
#include "core/blackholepreviewfbo.h"

#ifdef Q_OS_WIN
#include "core/update_checker.h"
#include <windows.h>
#include <dwmapi.h>
#endif

#ifndef Q_OS_WIN
namespace {

constexpr auto kUiService = "io.github.xboxnahida.Blakhole.UI";
constexpr auto kUiPath = "/io/github/xboxnahida/Blakhole/UI";
constexpr auto kUiInterface = "io.github.xboxnahida.Blakhole.UI";

class ApplicationInstance final : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "io.github.xboxnahida.Blakhole.UI")

public:
    ApplicationInstance(QQuickWindow *window, SystemTray *tray, QObject *parent = nullptr)
        : QObject(parent), m_window(window), m_tray(tray)
    {}

public slots:
    void Activate()
    {
        if (m_tray)
            m_tray->hide();
        else if (m_window) {
            m_window->show();
            m_window->raise();
            m_window->requestActivate();
        }
    }

private:
    QQuickWindow *m_window;
    SystemTray *m_tray;
};

} // namespace
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

#ifndef Q_OS_WIN
    QDBusConnection sessionBus = QDBusConnection::sessionBus();
    if (!sessionBus.registerService(QString::fromLatin1(kUiService))) {
        QDBusInterface existing(
            QString::fromLatin1(kUiService),
            QString::fromLatin1(kUiPath),
            QString::fromLatin1(kUiInterface), sessionBus);
        existing.call(QStringLiteral("Activate"));
        return 0;
    }
#endif

    QIcon appIcon(":/new/prefix1/fonts/icon.ico");
    if (appIcon.isNull())
        appIcon = QIcon::fromTheme(QStringLiteral("application-x-executable"));
    app.setWindowIcon(appIcon);

    qmlRegisterType<BlackHoleCore>("BlakholeUI", 1, 0, "BlackHoleCore");
    qmlRegisterType<BlackholePreviewFBO>("BlakholeUI", 1, 0, "BlackholePreviewFBO");
    qmlRegisterType<SystemTray>("BlakholeUI", 1, 0, "SystemTray");

    QQmlApplicationEngine engine;
    BlackHoleCore blackHoleCore;
#ifdef Q_OS_WIN
    UpdateChecker updateChecker(QStringLiteral(APP_VERSION_STRING));
#endif
    engine.rootContext()->setContextProperty("blackHoleCore", &blackHoleCore);
#ifdef Q_OS_WIN
    engine.rootContext()->setContextProperty("updateChecker", &updateChecker);
#endif
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

#ifndef Q_OS_WIN
                ApplicationInstance instance(window, tray, &app);
                if (!sessionBus.registerObject(
                        QString::fromLatin1(kUiPath), &instance,
                        QDBusConnection::ExportAllSlots)) {
                    qWarning() << "Cannot export single-instance activation object:"
                               << sessionBus.lastError().message();
                    return 1;
                }
#endif

                // XDG autostart/minimized startup resumes the saved lifecycle
                // policy. In always-on mode this starts the effect immediately;
                // in idle mode it arms GNOME IdleMonitor and waits for the
                // configured threshold.
                if (minimizedArgument || blackHoleCore.launchMinimized()) {
                    tray->show();
                    blackHoleCore.applyAndStart();
                }

                return app.exec();
            }
        }
    }

    return app.exec();
}

#ifndef Q_OS_WIN
#include "main.moc"
#endif
