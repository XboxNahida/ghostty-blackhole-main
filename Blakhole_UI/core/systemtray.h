// systemtray.h — 系统托盘管理
#pragma once

#include <QObject>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QQuickWindow>
#ifndef Q_OS_WIN
#include <QDBusObjectPath>
#endif

class SystemTray : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool visible READ isVisible WRITE setVisible NOTIFY visibleChanged)
    Q_PROPERTY(bool available READ isAvailable CONSTANT)
#ifndef Q_OS_WIN
    Q_CLASSINFO("D-Bus Interface", "org.kde.StatusNotifierItem")
    Q_PROPERTY(QString Category READ sniCategory CONSTANT)
    Q_PROPERTY(QString Id READ sniId CONSTANT)
    Q_PROPERTY(QString Title READ sniTitle CONSTANT)
    Q_PROPERTY(QString Status READ sniStatus)
    Q_PROPERTY(QString IconName READ sniIconName CONSTANT)
    Q_PROPERTY(QDBusObjectPath Menu READ sniMenu CONSTANT)
    Q_PROPERTY(bool ItemIsMenu READ sniItemIsMenu CONSTANT)
#endif

public:
    explicit SystemTray(QObject *parent = nullptr);
    SystemTray(bool available, QObject *parent);

    bool isVisible() const;
    bool isAvailable() const { return m_available; }
    void setVisible(bool v);

#ifndef Q_OS_WIN
    QString sniCategory() const { return QStringLiteral("ApplicationStatus"); }
    QString sniId() const { return QStringLiteral("io.github.xboxnahida.Blakhole"); }
    QString sniTitle() const { return QStringLiteral("Blakhole Ubuntu Local"); }
    QString sniStatus() const { return m_sniVisible ? QStringLiteral("Active") : QStringLiteral("Passive"); }
    QString sniIconName() const { return QStringLiteral("io.github.xboxnahida.Blakhole"); }
    QDBusObjectPath sniMenu() const { return QDBusObjectPath(QStringLiteral("/NO_DBUSMENU")); }
    bool sniItemIsMenu() const { return false; }
#endif

    Q_INVOKABLE void show();
    Q_INVOKABLE void hide();
    void setWindow(QQuickWindow *window);

public slots:
#ifndef Q_OS_WIN
    void Activate(int x, int y);
    void SecondaryActivate(int x, int y);
    void ContextMenu(int x, int y);
    void Scroll(int delta, const QString &orientation);
#endif

signals:
    void visibleChanged();
    void activated();
    void exitRequested();
#ifndef Q_OS_WIN
    void NewStatus(const QString &status);
#endif

private:
    QSystemTrayIcon *m_tray;
    QMenu *m_menu;
    QQuickWindow *m_window;
    bool m_available;
#ifndef Q_OS_WIN
    bool m_sniVisible = false;
    QString m_sniService;
    bool registerStatusNotifier();
    void setSniVisible(bool visible);
#endif
};
