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

public:
    explicit SystemTray(QObject *parent = nullptr);

    bool isVisible() const;
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
};
