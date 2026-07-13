#include "gnome_idle_monitor.h"

#include "gnome_idle_backend.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusPendingReply>
#include <QDebug>

namespace {
const QString kIdleService = QStringLiteral("org.gnome.Mutter.IdleMonitor");
const QString kIdlePath = QStringLiteral("/org/gnome/Mutter/IdleMonitor/Core");
const QString kIdleInterface = QStringLiteral("org.gnome.Mutter.IdleMonitor");
const QString kSSService = QStringLiteral("org.gnome.ScreenSaver");
const QString kSSPath = QStringLiteral("/org/gnome/ScreenSaver");
const QString kSSInterface = QStringLiteral("org.gnome.ScreenSaver");

class DBusBackend final : public GnomeIdleBackend
{
public:
    DBusBackend() { refresh(); }

    quint32 addIdleWatch(quint64 intervalMs) override
    {
        if (!idleServiceAvailable()) return 0;
        QDBusPendingReply<uint> reply = m_idle->asyncCall(QStringLiteral("AddIdleWatch"), intervalMs);
        reply.waitForFinished();
        return reply.isValid() ? reply.value() : 0;
    }

    quint32 addActiveWatch() override
    {
        if (!idleServiceAvailable()) return 0;
        QDBusPendingReply<uint> reply = m_idle->asyncCall(QStringLiteral("AddUserActiveWatch"));
        reply.waitForFinished();
        return reply.isValid() ? reply.value() : 0;
    }

    void removeWatch(quint32 id) override
    {
        if (id && idleServiceAvailable()) m_idle->asyncCall(QStringLiteral("RemoveWatch"), id);
    }

    bool screenSaverActive(bool *ok) override
    {
        *ok = false;
        if (!screenSaverAvailable()) return false;
        QDBusPendingReply<bool> reply = m_screenSaver->asyncCall(QStringLiteral("GetActive"));
        reply.waitForFinished();
        *ok = reply.isValid();
        return *ok && reply.value();
    }

    bool idleServiceAvailable() const override { return m_idle && m_idle->isValid(); }
    bool screenSaverAvailable() const override { return m_screenSaver && m_screenSaver->isValid(); }

    void refresh() override
    {
        m_idle = std::make_unique<QDBusInterface>(kIdleService, kIdlePath, kIdleInterface,
                                                  QDBusConnection::sessionBus());
        m_screenSaver = std::make_unique<QDBusInterface>(kSSService, kSSPath, kSSInterface,
                                                         QDBusConnection::sessionBus());
    }

private:
    std::unique_ptr<QDBusInterface> m_idle;
    std::unique_ptr<QDBusInterface> m_screenSaver;
};
}

GnomeIdleMonitor::GnomeIdleMonitor(QObject *parent) : QObject(parent)
{
    initializeProductionBackend();
}

GnomeIdleMonitor::GnomeIdleMonitor(GnomeIdleBackend *backend, QObject *parent)
    : QObject(parent), m_backend(backend)
{
}

GnomeIdleMonitor::~GnomeIdleMonitor() = default;

void GnomeIdleMonitor::initializeProductionBackend()
{
    m_ownedBackend = std::make_unique<DBusBackend>();
    m_backend = m_ownedBackend.get();
}

QString GnomeIdleMonitor::stateName() const
{
    switch (m_state) {
    case Active: return QStringLiteral("Active");
    case IdleEligible: return QStringLiteral("IdleEligible");
    case RendererRunning: return QStringLiteral("RendererRunning");
    case Locked: return QStringLiteral("Locked");
    case Degraded: return QStringLiteral("Degraded");
    }
    return QStringLiteral("Unknown");
}

void GnomeIdleMonitor::setIdleSeconds(int seconds)
{
    if (seconds == m_idleSeconds) return;
    m_idleSeconds = seconds;
    if (m_started && m_state == Active) {
        removeWatches();
        if (!armWatches()) transitionTo(Degraded);
    }
}

void GnomeIdleMonitor::connectSignals()
{
    if (m_signalsConnected || !m_ownedBackend) return;
    auto bus = QDBusConnection::sessionBus();
    bus.connect(kIdleService, kIdlePath, kIdleInterface, QStringLiteral("WatchFired"),
                this, SLOT(onWatchFired(uint)));
    bus.connect(kSSService, kSSPath, kSSInterface, QStringLiteral("ActiveChanged"),
                this, SLOT(onScreenSaverActiveChanged(bool)));
    m_signalsConnected = true;
}

void GnomeIdleMonitor::disconnectSignals()
{
    if (!m_signalsConnected) return;
    auto bus = QDBusConnection::sessionBus();
    bus.disconnect(kIdleService, kIdlePath, kIdleInterface, QStringLiteral("WatchFired"),
                   this, SLOT(onWatchFired(uint)));
    bus.disconnect(kSSService, kSSPath, kSSInterface, QStringLiteral("ActiveChanged"),
                   this, SLOT(onScreenSaverActiveChanged(bool)));
    m_signalsConnected = false;
}

void GnomeIdleMonitor::ensureServiceWatcher()
{
    if (m_serviceWatcher || !m_ownedBackend) return;
    m_serviceWatcher = new QDBusServiceWatcher(this);
    m_serviceWatcher->setConnection(QDBusConnection::sessionBus());
    m_serviceWatcher->setWatchMode(QDBusServiceWatcher::WatchForRegistration |
                                   QDBusServiceWatcher::WatchForUnregistration);
    m_serviceWatcher->addWatchedService(kIdleService);
    m_serviceWatcher->addWatchedService(kSSService);
    connect(m_serviceWatcher, &QDBusServiceWatcher::serviceRegistered,
            this, &GnomeIdleMonitor::onDBusServiceRegistered);
    connect(m_serviceWatcher, &QDBusServiceWatcher::serviceUnregistered,
            this, &GnomeIdleMonitor::onDBusServiceUnregistered);
}

bool GnomeIdleMonitor::armWatches()
{
    if (!m_started || !m_backend->idleServiceAvailable()) return false;
    removeWatches();
    m_idleWatchId = m_backend->addIdleWatch(quint64(m_idleSeconds) * 1000);
    m_activeWatchId = m_backend->addActiveWatch();
    if (m_idleWatchId && m_activeWatchId) return true;
    removeWatches();
    return false;
}

void GnomeIdleMonitor::removeWatches()
{
    if (m_idleWatchId) m_backend->removeWatch(m_idleWatchId);
    if (m_activeWatchId) m_backend->removeWatch(m_activeWatchId);
    m_idleWatchId = 0;
    m_activeWatchId = 0;
}

void GnomeIdleMonitor::start()
{
    if (m_started) return;
    m_started = true;
    ensureServiceWatcher();
    connectSignals();
    recoverIfReady();
}

void GnomeIdleMonitor::stop()
{
    m_started = false;
    removeWatches();
    disconnectSignals();
    transitionTo(Degraded);
}

void GnomeIdleMonitor::recoverIfReady()
{
    if (!m_started) return;
    if (!m_backend->idleServiceAvailable() || !m_backend->screenSaverAvailable()) {
        transitionTo(Degraded);
        return;
    }
    bool ok = false;
    const bool locked = m_backend->screenSaverActive(&ok);
    if (!ok) {
        transitionTo(Degraded);
    } else if (locked) {
        removeWatches();
        transitionTo(Locked);
    } else if (armWatches()) {
        transitionTo(Active);
    } else {
        transitionTo(Degraded);
    }
}

void GnomeIdleMonitor::setRendererRunning(bool running)
{
    if (!m_started) return;
    if (running && m_state == IdleEligible) transitionTo(RendererRunning);
    else if (!running && m_state == RendererRunning) {
        if (armWatches()) transitionTo(Active);
        else transitionTo(Degraded);
    }
}

void GnomeIdleMonitor::handleWatchFired(quint32 watchId)
{
    if (!m_started) return;
    if (watchId == m_idleWatchId && m_state == Active) {
        m_idleWatchId = 0;
        transitionTo(IdleEligible);
        emit idleEligible();
    } else if (watchId == m_activeWatchId &&
               (m_state == IdleEligible || m_state == RendererRunning)) {
        m_activeWatchId = 0;
        if (armWatches()) {
            // Complete the state transition before notifying BlackHoleCore.
            // stopRenderer() emits rendererStopped synchronously; Active makes
            // that callback a no-op instead of registering a second watch pair.
            transitionTo(Active);
            emit activityDetected();
        } else {
            transitionTo(Degraded);
        }
    }
}

void GnomeIdleMonitor::handleScreenSaverActiveChanged(bool active)
{
    if (!m_started) return;
    if (active && m_state != Locked) {
        removeWatches();
        transitionTo(Locked);
        emit lockDetected();
    } else if (!active && m_state == Locked) {
        if (armWatches()) {
            transitionTo(Active);
            emit unlockDetected();
        } else {
            transitionTo(Degraded);
        }
    }
}

void GnomeIdleMonitor::handleServiceRegistered(const QString &service)
{
    if (!m_started || (service != kIdleService && service != kSSService)) return;
    m_backend->refresh();
    recoverIfReady();
}

void GnomeIdleMonitor::handleServiceUnregistered(const QString &service)
{
    if (!m_started || (service != kIdleService && service != kSSService)) return;
    removeWatches();
    m_backend->refresh();
    transitionTo(Degraded);
}

void GnomeIdleMonitor::transitionTo(State state)
{
    if (m_state == state) return;
    m_state = state;
    qDebug() << "GnomeIdleMonitor: state ->" << stateName();
    emit stateChanged(state);
}
