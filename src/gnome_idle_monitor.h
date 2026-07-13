// gnome_idle_monitor.h — GNOME idle/activity/lock-screen monitor (D-Bus)
#pragma once

#include <QObject>
#include <QDBusInterface>
#include <QDBusServiceWatcher>
#include <QTimer>

// GNOME idle/lock state machine
// Active -> IdleEligible (idle watch fired)
// IdleEligible -> RendererRunning (policy allows -> startRenderer)
// RendererRunning -> Active (user activity detected)
// Any -> Locked (screensaver active -> stopRenderer)
// Locked -> Active (screensaver inactive -> re-arm watches)
class GnomeIdleMonitor : public QObject
{
    Q_OBJECT
public:
    enum State { Active, IdleEligible, RendererRunning, Locked, Degraded };
    Q_ENUM(State)

    explicit GnomeIdleMonitor(QObject *parent = nullptr);

    State state() const { return m_state; }
    bool isLocked() const { return m_state == Locked; }
    bool isActive() const { return m_state == Active; }
    bool isIdleEligible() const { return m_state == IdleEligible; }
    bool isRendererRunning() const { return m_state == RendererRunning; }
    QString stateName() const;

    // Config: idle threshold in seconds
    void setIdleSeconds(int seconds);
    int idleSeconds() const { return m_idleSeconds; }

    // Start/stop monitoring
    void start();
    void stop();

    // For testing: inject a fake D-Bus backend
    void setDBusInterface(QDBusInterface *idleIface, QDBusInterface *ssIface);

    // Called by BlackHoleCore when renderer starts/stops
    void setRendererRunning(bool running);

signals:
    void stateChanged(GnomeIdleMonitor::State newState);
    void idleEligible();       // policy may start renderer
    void activityDetected();   // user returned -> stop renderer
    void lockDetected();       // screen locked -> stop renderer
    void unlockDetected();     // screen unlocked -> re-arm

private slots:
    void onWatchFired(uint watchId);
    void onScreenSaverActiveChanged(bool active);
    void onDBusServiceRegistered(const QString &service);
    void onDBusServiceUnregistered(const QString &service);

private:
    void setIdleWatch();
    void removeIdleWatch();
    void setActiveWatch();
    void removeActiveWatch();
    void setScreenSaverWatch();
    void transitionTo(State s);

    State m_state = Degraded;
    int m_idleSeconds = 300; // default 5 min
    uint m_idleWatchId = 0;
    uint m_activeWatchId = 0;

    QDBusInterface *m_idleIface = nullptr;
    QDBusInterface *m_ssIface = nullptr;
    bool m_ownedInterfaces = false;

    QDBusServiceWatcher *m_serviceWatcher = nullptr;
};
