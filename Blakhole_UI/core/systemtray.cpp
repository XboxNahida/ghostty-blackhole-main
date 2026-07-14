// systemtray.cpp — 系统托盘实现
#include "systemtray.h"
#include <QApplication>
#include <QAction>
#ifndef Q_OS_WIN
#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDebug>
#endif

namespace {

bool trayBackendAvailable()
{
#ifdef Q_OS_WIN
    return QSystemTrayIcon::isSystemTrayAvailable();
#else
    auto *interface = QDBusConnection::sessionBus().interface();
    return interface && interface->isServiceRegistered(
        QStringLiteral("org.kde.StatusNotifierWatcher"));
#endif
}

} // namespace

SystemTray::SystemTray(QObject *parent)
    : SystemTray(trayBackendAvailable(), parent)
{}

SystemTray::SystemTray(bool available, QObject *parent)
    : QObject(parent)
    , m_tray(new QSystemTrayIcon(this))
    , m_menu(new QMenu())
    , m_window(nullptr)
    , m_available(available)
{
    m_tray->setIcon(QIcon(":/new/prefix1/fonts/icon.ico"));
    if (m_tray->icon().isNull())
        m_tray->setIcon(QIcon::fromTheme(QStringLiteral("application-x-executable")));
    m_tray->setToolTip("Blakhole UI");

    QAction *showAction = m_menu->addAction("显示窗口");
    QAction *exitAction = m_menu->addAction("退出");

    connect(showAction, &QAction::triggered, this, [this]() {
        if (m_window) {
            m_window->show();
            m_window->raise();
            m_window->requestActivate();
        }
        m_tray->hide();
        emit visibleChanged();
    });

    connect(exitAction, &QAction::triggered, this, [this]() {
        emit exitRequested();
    });

    m_tray->setContextMenu(m_menu);

    connect(m_tray, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::DoubleClick) {
            if (m_window) {
                m_window->show();
                m_window->raise();
                m_window->requestActivate();
            }
            m_tray->hide();
            emit visibleChanged();
        }
    });

#ifndef Q_OS_WIN
    if (m_available)
        m_available = registerStatusNotifier();
#endif
}

bool SystemTray::isVisible() const
{
#ifndef Q_OS_WIN
    return m_sniVisible;
#else
    return m_tray->isVisible();
#endif
}

void SystemTray::setVisible(bool v)
{
    if (v && !m_available) {
        m_tray->hide();
        if (m_window) m_window->show();
        emit visibleChanged();
        return;
    }
#ifndef Q_OS_WIN
    setSniVisible(v);
#else
    m_tray->setVisible(v);
#endif
    emit visibleChanged();
}

void SystemTray::show()
{
    if (!m_available) {
        if (m_window) { m_window->show(); m_window->raise(); m_window->requestActivate(); }
        m_tray->hide();
        emit visibleChanged();
        return;
    }
#ifndef Q_OS_WIN
    setSniVisible(true);
#else
    m_tray->show();
#endif
    if (m_window) m_window->hide();
    emit visibleChanged();
}

void SystemTray::hide()
{
#ifndef Q_OS_WIN
    setSniVisible(false);
#else
    m_tray->hide();
#endif
    if (m_window) {
        m_window->show();
        m_window->raise();
        m_window->requestActivate();
    }
    emit visibleChanged();
}

void SystemTray::setWindow(QQuickWindow *window)
{
    m_window = window;
}

#ifndef Q_OS_WIN
bool SystemTray::registerStatusNotifier()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    m_sniService = QStringLiteral("org.kde.StatusNotifierItem-%1-1")
        .arg(QCoreApplication::applicationPid());
    if (!bus.registerService(m_sniService)) {
        qWarning() << "SystemTray: cannot register StatusNotifierItem service"
                   << bus.lastError().message();
        return false;
    }
    if (!bus.registerObject(QStringLiteral("/StatusNotifierItem"), this,
            QDBusConnection::ExportAllProperties |
            QDBusConnection::ExportAllSlots |
            QDBusConnection::ExportAllSignals)) {
        qWarning() << "SystemTray: cannot export StatusNotifierItem object"
                   << bus.lastError().message();
        bus.unregisterService(m_sniService);
        return false;
    }

    QDBusInterface watcher(
        QStringLiteral("org.kde.StatusNotifierWatcher"),
        QStringLiteral("/StatusNotifierWatcher"),
        QStringLiteral("org.kde.StatusNotifierWatcher"), bus);
    const QDBusReply<void> reply = watcher.call(
        QStringLiteral("RegisterStatusNotifierItem"), m_sniService);
    if (!reply.isValid()) {
        qWarning() << "SystemTray: StatusNotifierWatcher rejected registration"
                   << reply.error().message();
        bus.unregisterObject(QStringLiteral("/StatusNotifierItem"));
        bus.unregisterService(m_sniService);
        return false;
    }
    qDebug() << "SystemTray: StatusNotifierItem registered as" << m_sniService;
    return true;
}

void SystemTray::setSniVisible(bool visible)
{
    if (m_sniVisible == visible)
        return;
    m_sniVisible = visible;
    emit NewStatus(sniStatus());
}

void SystemTray::Activate(int, int)
{
    hide();
}

void SystemTray::SecondaryActivate(int x, int y)
{
    Activate(x, y);
}

void SystemTray::ContextMenu(int x, int y)
{
    Activate(x, y);
}

void SystemTray::Scroll(int, const QString &)
{}
#endif
