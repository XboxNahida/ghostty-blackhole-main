// systemtray.h — 系统托盘管理
#pragma once

#include <QObject>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QQuickWindow>

class SystemTray : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool visible READ isVisible WRITE setVisible NOTIFY visibleChanged)
    Q_PROPERTY(bool available READ isAvailable CONSTANT)

public:
    explicit SystemTray(QObject *parent = nullptr);
    SystemTray(bool available, QObject *parent);

    bool isVisible() const;
    bool isAvailable() const { return m_available; }
    void setVisible(bool v);

    Q_INVOKABLE void show();
    Q_INVOKABLE void hide();

public slots:
    void setWindow(QQuickWindow *window);

signals:
    void visibleChanged();
    void activated();
    void exitRequested();

private:
    QSystemTrayIcon *m_tray;
    QMenu *m_menu;
    QQuickWindow *m_window;
    bool m_available;
};
