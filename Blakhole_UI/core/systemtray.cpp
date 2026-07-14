// systemtray.cpp — 系统托盘实现
#include "systemtray.h"
#include <QApplication>
#include <QAction>

SystemTray::SystemTray(QObject *parent)
    : SystemTray(QSystemTrayIcon::isSystemTrayAvailable(), parent)
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
}

bool SystemTray::isVisible() const
{
    return m_tray->isVisible();
}

void SystemTray::setVisible(bool v)
{
    if (v && !m_available) {
        m_tray->hide();
        if (m_window) m_window->show();
        emit visibleChanged();
        return;
    }
    m_tray->setVisible(v);
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
    m_tray->show();
    if (m_window) m_window->hide();
    emit visibleChanged();
}

void SystemTray::hide()
{
    m_tray->hide();
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
