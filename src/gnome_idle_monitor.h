#pragma once

#include <QObject>
#include <QDBusServiceWatcher>
#include <QTimer>

#include <memory>

class GnomeIdleBackend;

class GnomeIdleMonitor : public QObject
{
    Q_OBJECT
public:
    enum State { Active, IdleEligible, RendererRunning, Locked, Degraded };
    Q_ENUM(State)

    explicit GnomeIdleMonitor(QObject *parent = nullptr);
    explicit GnomeIdleMonitor(GnomeIdleBackend *backend, QObject *parent = nullptr);
    ~GnomeIdleMonitor() override;

    State state() const { return m_state; }
    bool isLocked() const { return m_state == Locked; }
    bool isActive() const { return m_state == Active; }
    bool isIdleEligible() const { return m_state == IdleEligible; }
    bool isRendererRunning() const { return m_state == RendererRunning; }
    QString stateName() const;
    void setIdleSeconds(int seconds);
    int idleSeconds() const { return m_idleSeconds; }
    void start();
    void stop();
    void setRendererRunning(bool running);
    void pollActivity();

    // Event entry points are public so deterministic backends can exercise the
    // same production dispatch path without a session bus.
    void handleWatchFired(quint32 watchId);
    void handleScreenSaverActiveChanged(bool active);
    void handleServiceRegistered(const QString &service);
    void handleServiceUnregistered(const QString &service);

    bool hasIdleWatch() const { return m_idleWatchId != 0; }
    bool hasActiveWatch() const { return m_activeWatchId != 0; }

signals:
    void stateChanged(GnomeIdleMonitor::State newState);
    void idleEligible();
    void activityDetected();
    void lockDetected();
    void unlockDetected();

private slots:
    void onWatchFired(uint watchId) { handleWatchFired(watchId); }
    void onScreenSaverActiveChanged(bool active) { handleScreenSaverActiveChanged(active); }
    void onDBusServiceRegistered(const QString &service) { handleServiceRegistered(service); }
    void onDBusServiceUnregistered(const QString &service) { handleServiceUnregistered(service); }

private:
    void initializeProductionBackend();
    void connectSignals();
    void disconnectSignals();
    void ensureServiceWatcher();
    bool armWatches();
    void removeWatches();
    void recoverIfReady();
    void transitionTo(State state);

    State m_state = Degraded;
    int m_idleSeconds = 300;
    quint32 m_idleWatchId = 0;
    quint32 m_activeWatchId = 0;
    bool m_started = false;
    bool m_signalsConnected = false;
    GnomeIdleBackend *m_backend = nullptr;
    std::unique_ptr<GnomeIdleBackend> m_ownedBackend;
    QDBusServiceWatcher *m_serviceWatcher = nullptr;
    QTimer *m_activityPollTimer = nullptr;
};
