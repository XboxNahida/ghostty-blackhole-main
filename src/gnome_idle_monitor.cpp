// gnome_idle_monitor.cpp — GNOME idle/activity/lock-screen monitor (D-Bus)
#include "gnome_idle_monitor.h"

#include <QDBusConnection>
#include <QDBusPendingCall>
#include <QDBusPendingReply>
#include <QDebug>

static const QString kIdleService    = QStringLiteral("org.gnome.Mutter.IdleMonitor");
static const QString kIdlePath       = QStringLiteral("/org/gnome/Mutter/IdleMonitor/Core");
static const QString kIdleInterface  = QStringLiteral("org.gnome.Mutter.IdleMonitor");
static const QString kSSService      = QStringLiteral("org.gnome.ScreenSaver");
static const QString kSSPath         = QStringLiteral("/org/gnome/ScreenSaver");
static const QString kSSInterface    = QStringLiteral("org.gnome.ScreenSaver");

GnomeIdleMonitor::GnomeIdleMonitor(QObject *parent)
    : QObject(parent)
{
}

QString GnomeIdleMonitor::stateName() const
{
    switch (m_state) {
    case Active:         return QStringLiteral("Active");
    case IdleEligible:   return QStringLiteral("IdleEligible");
    case RendererRunning:return QStringLiteral("RendererRunning");
    case Locked:         return QStringLiteral("Locked");
    case Degraded:       return QStringLiteral("Degraded");
    }
    return QStringLiteral("Unknown");
}

void GnomeIdleMonitor::setIdleSeconds(int seconds)
{
    if (seconds != m_idleSeconds) {
        const bool wasRunning = m_idleWatchId != 0;
        if (wasRunning) removeIdleWatch();
        m_idleSeconds = seconds;
        if (wasRunning) setIdleWatch();
    }
}

void GnomeIdleMonitor::setDBusInterface(QDBusInterface *idleIface, QDBusInterface *ssIface)
{
    if (m_ownedInterfaces) {
        delete m_idleIface;
        delete m_ssIface;
    }
    m_idleIface = idleIface;
    m_ssIface = ssIface;
    m_ownedInterfaces = false;
}

void GnomeIdleMonitor::setRendererRunning(bool running)
{
    if (running && m_state == IdleEligible) {
        transitionTo(RendererRunning);
    } else if (!running && m_state == RendererRunning) {
        transitionTo(Active);
        setIdleWatch();
        setActiveWatch();
    }
}

void GnomeIdleMonitor::start()
{
    // Set up D-Bus interfaces if not injected
    if (!m_idleIface) {
        m_idleIface = new QDBusInterface(kIdleService, kIdlePath, kIdleInterface,
                                          QDBusConnection::sessionBus(), this);
        m_ssIface = new QDBusInterface(kSSService, kSSPath, kSSInterface,
                                        QDBusConnection::sessionBus(), this);
        m_ownedInterfaces = true;
    }

    // Watch service availability
    if (!m_serviceWatcher) {
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

    // Connect WatchFired signal to single dispatcher
    QDBusConnection::sessionBus().connect(
        kIdleService, kIdlePath, kIdleInterface,
        QStringLiteral("WatchFired"),
        this, SLOT(onWatchFired(uint)));

    // Connect Screensaver signal
    if (m_ssIface && m_ssIface->isValid()) {
        QDBusConnection::sessionBus().connect(
            kSSService, kSSPath, kSSInterface,
            QStringLiteral("ActiveChanged"),
            this, SLOT(onScreenSaverActiveChanged(bool)));
    }

    // Check initial lock state before arming watches
    setScreenSaverWatch();

    // Only arm watches if not locked and interfaces are valid
    if (m_state != Locked && m_idleIface && m_idleIface->isValid()) {
        setIdleWatch();
        setActiveWatch();
        // Transition to Active only if at least one watch was registered
        if (m_idleWatchId != 0 || m_activeWatchId != 0) {
            transitionTo(Active);
        } else {
            // Watches failed — stay/return to Degraded
            transitionTo(Degraded);
        }
    } else if (m_state != Locked) {
        transitionTo(Degraded);
    }
}

void GnomeIdleMonitor::stop()
{
    removeIdleWatch();
    removeActiveWatch();
    QDBusConnection::sessionBus().disconnect(
        kIdleService, kIdlePath, kIdleInterface,
        QStringLiteral("WatchFired"),
        this, SLOT(onWatchFired(uint)));
    if (m_ownedInterfaces) {
        QDBusConnection::sessionBus().disconnect(
            kSSService, kSSPath, kSSInterface,
            QStringLiteral("ActiveChanged"),
            this, SLOT(onScreenSaverActiveChanged(bool)));
    }
    transitionTo(Degraded);
}

void GnomeIdleMonitor::setIdleWatch()
{
    if (!m_idleIface || !m_idleIface->isValid()) return;
    removeIdleWatch();
    QDBusPendingReply<uint> reply = m_idleIface->asyncCall(
        QStringLiteral("AddIdleWatch"), quint64(m_idleSeconds) * 1000);
    reply.waitForFinished();
    if (reply.isValid()) {
        m_idleWatchId = reply.value();
        qDebug() << "GnomeIdleMonitor: idle watch registered, id=" << m_idleWatchId;
    }
}

void GnomeIdleMonitor::removeIdleWatch()
{
    if (m_idleWatchId == 0) return;
    if (m_idleIface && m_idleIface->isValid()) {
        m_idleIface->asyncCall(QStringLiteral("RemoveWatch"), m_idleWatchId);
    }
    m_idleWatchId = 0;
}

void GnomeIdleMonitor::setActiveWatch()
{
    if (!m_idleIface || !m_idleIface->isValid()) return;
    removeActiveWatch();
    QDBusPendingReply<uint> reply = m_idleIface->asyncCall(
        QStringLiteral("AddUserActiveWatch"));
    reply.waitForFinished();
    if (reply.isValid()) {
        m_activeWatchId = reply.value();
    }
}

void GnomeIdleMonitor::removeActiveWatch()
{
    if (m_activeWatchId == 0) return;
    if (m_idleIface && m_idleIface->isValid()) {
        m_idleIface->asyncCall(QStringLiteral("RemoveWatch"), m_activeWatchId);
    }
    m_activeWatchId = 0;
}

void GnomeIdleMonitor::setScreenSaverWatch()
{
    // Check current lock state
    if (m_ssIface && m_ssIface->isValid()) {
        QDBusPendingReply<bool> reply = m_ssIface->asyncCall(QStringLiteral("GetActive"));
        reply.waitForFinished();
        if (reply.isValid() && reply.value()) {
            transitionTo(Locked);
        }
    }
}

void GnomeIdleMonitor::onWatchFired(uint watchId)
{
    qDebug() << "GnomeIdleMonitor: WatchFired id=" << watchId;
    if (watchId == m_idleWatchId) {
        m_idleWatchId = 0; // one-shot watch consumed
        if (m_state == Active) {
            transitionTo(IdleEligible);
            emit idleEligible();
        }
    } else if (watchId == m_activeWatchId) {
        m_activeWatchId = 0; // one-shot watch consumed
        if (m_state == IdleEligible || m_state == RendererRunning) {
            transitionTo(Active);
            emit activityDetected();
        }
        // Re-arm one-shot watches
        setIdleWatch();
        setActiveWatch();
    }
}

void GnomeIdleMonitor::onScreenSaverActiveChanged(bool active)
{
    qDebug() << "GnomeIdleMonitor: screensaver active =" << active;
    if (active && m_state != Locked) {
        transitionTo(Locked);
        removeIdleWatch();
        removeActiveWatch();
        emit lockDetected();
    } else if (!active && m_state == Locked) {
        transitionTo(Active);
        setIdleWatch();
        setActiveWatch();
        emit unlockDetected();
    }
}

void GnomeIdleMonitor::onDBusServiceRegistered(const QString &service)
{
    qDebug() << "GnomeIdleMonitor: service registered" << service;
    if (service == kSSService) {
        setScreenSaverWatch();
    }
    // Re-arm watches if we were in Degraded and now have valid interfaces
    if (m_state == Degraded && m_idleIface && m_idleIface->isValid()
        && m_ssIface && m_ssIface->isValid()) {
        setScreenSaverWatch();
        if (m_state != Locked) {
            setIdleWatch();
            setActiveWatch();
            if (m_idleWatchId != 0 || m_activeWatchId != 0) {
                transitionTo(Active);
            }
        }
    }
}

void GnomeIdleMonitor::onDBusServiceUnregistered(const QString &service)
{
    qDebug() << "GnomeIdleMonitor: service lost" << service;
    if (service == kIdleService || service == kSSService) {
        if (m_state != Degraded) {
            transitionTo(Degraded);
        }
    }
}

void GnomeIdleMonitor::transitionTo(State s)
{
    if (m_state == s) return;
    m_state = s;
    qDebug() << "GnomeIdleMonitor: state ->" << stateName();
    emit stateChanged(s);
}

// --- Test injection helpers ---
void GnomeIdleMonitor::testInjectWatchFired(uint watchId)
{
    onWatchFired(watchId);
}

void GnomeIdleMonitor::testInjectActiveChanged(bool active)
{
    onScreenSaverActiveChanged(active);
}

void GnomeIdleMonitor::testInjectServiceRegistered(const QString &service)
{
    onDBusServiceRegistered(service);
}

void GnomeIdleMonitor::testInjectServiceUnregistered(const QString &service)
{
    onDBusServiceUnregistered(service);
}
