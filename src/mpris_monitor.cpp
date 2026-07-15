// mpris_monitor.cpp — MPRIS media playback monitor (Linux)
#include "mpris_monitor.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusPendingCall>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDebug>
#include <QPointer>
#include <QSet>
#include <QTimer>

static const QString kDBusBusName = QStringLiteral("org.freedesktop.DBus");
static const QString kDBusPath    = QStringLiteral("/org/freedesktop/DBus");
static const QString kDBusInterface = QStringLiteral("org.freedesktop.DBus");
static const QString kMprisPrefix = QStringLiteral("org.mpris.MediaPlayer2.");
static const QString kPlayerPath = QStringLiteral("/org/mpris/MediaPlayer2");
static const QString kPropertiesInterface = QStringLiteral("org.freedesktop.DBus.Properties");
static const QString kPlayerInterface = QStringLiteral("org.mpris.MediaPlayer2.Player");
// Default backend. Pending calls keep all D-Bus waits off the UI thread; the
// monitor owns the overall deadline and ignores callbacks from expired polls.
class MprisDBusBackend : public MprisBackend
{
public:
    explicit MprisDBusBackend(QObject *context) : m_context(context) {}

    void enumeratePlayers(PlayersCallback callback) override
    {
        QDBusInterface dbus(kDBusBusName, kDBusPath, kDBusInterface,
                             QDBusConnection::sessionBus());
        auto *watcher = new QDBusPendingCallWatcher(
            dbus.asyncCall(QStringLiteral("ListNames")), m_context);
        QObject::connect(watcher, &QDBusPendingCallWatcher::finished, m_context,
                         [watcher, callback = std::move(callback)]() mutable {
            QStringList players;
            QDBusPendingReply<QStringList> reply = *watcher;
            if (!reply.isError()) {
                for (const QString &name : reply.value())
                    if (name.startsWith(kMprisPrefix)) players.append(name);
            }
            watcher->deleteLater();
            callback(std::move(players));
        });
    }

    void queryOwner(const QString &serviceName,
                    OwnerCallback callback) override
    {
        QDBusInterface dbus(kDBusBusName, kDBusPath, kDBusInterface,
                            QDBusConnection::sessionBus());
        auto *watcher = new QDBusPendingCallWatcher(
            dbus.asyncCall(QStringLiteral("GetNameOwner"), serviceName), m_context);
        QObject::connect(watcher, &QDBusPendingCallWatcher::finished, m_context,
                         [watcher, callback = std::move(callback)]() mutable {
            QDBusPendingReply<QString> reply = *watcher;
            const QString owner = reply.isError() ? QString() : reply.value();
            watcher->deleteLater();
            callback(owner);
        });
    }

    void queryPlaybackStatus(const QString &serviceName,
                             StatusCallback callback) override
    {
        QDBusInterface player(serviceName, QStringLiteral("/org/mpris/MediaPlayer2"),
                               QStringLiteral("org.freedesktop.DBus.Properties"),
                               QDBusConnection::sessionBus());
        auto *watcher = new QDBusPendingCallWatcher(player.asyncCall(
            QStringLiteral("Get"), QStringLiteral("org.mpris.MediaPlayer2.Player"),
            QStringLiteral("PlaybackStatus")), m_context);
        QObject::connect(watcher, &QDBusPendingCallWatcher::finished, m_context,
                         [watcher, callback = std::move(callback)]() mutable {
            QDBusPendingReply<QVariant> reply = *watcher;
            const QString status = reply.isError() ? QString() : reply.value().toString();
            watcher->deleteLater();
            callback(status);
        });
    }

private:
    QObject *m_context;
};

MprisMonitor::MprisMonitor(QObject *parent)
    : QObject(parent)
{
}

MprisMonitor::~MprisMonitor()
{
    if (m_ownsBackend) delete m_backend;
}

QStringList MprisMonitor::activePlayers() const
{
    return m_playerStatus.keys();
}

void MprisMonitor::setBackend(MprisBackend *backend)
{
    if (m_ownsBackend) delete m_backend;
    m_backend = backend;
    m_ownsBackend = false;
}

void MprisMonitor::start()
{
    if (!m_backend) {
        m_backend = new MprisDBusBackend(this);
        m_ownsBackend = true;
    }
    if (m_started) return;
    m_started = true;
    subscribeToDBus();
    initialSync();
}

void MprisMonitor::stop()
{
    unsubscribeFromDBus();
    m_started = false;
    ++m_generation;
    m_playerStatus.clear();
    m_ownerToPlayer.clear();
    m_pendingPlayers.clear();
    m_pendingStatus.clear();
    m_pollActive = false;
    m_anyPlaying = false;
}

void MprisMonitor::initialSync()
{
    if (!m_backend || m_pollActive) return;

    m_pollActive = true;
    const quint64 generation = ++m_generation;
    m_pendingPlayers.clear();
    m_pendingStatus.clear();
    QTimer::singleShot(kMaxPollMs, this, [this, generation]() { finishPoll(generation); });

    QPointer<MprisMonitor> self(this);
    m_backend->enumeratePlayers([self, generation](QStringList players) {
        if (!self || !self->m_started || generation != self->m_generation || !self->m_pollActive) return;
        self->m_pendingPlayers = QSet<QString>(players.begin(), players.end());
        if (self->m_pendingPlayers.isEmpty()) {
            self->finishPoll(generation);
            return;
        }
        for (const QString &service : std::as_const(players)) {
            self->m_backend->queryOwner(service, [self, generation, service](QString owner) {
                if (!self || !self->m_started || generation != self->m_generation || owner.isEmpty()) return;
                self->m_ownerToPlayer.insert(owner, service);
            });
            self->m_backend->queryPlaybackStatus(service, [self, generation, service](QString status) {
                if (!self || !self->m_started || generation != self->m_generation || !self->m_pollActive) return;
                self->m_pendingStatus.insert(service, std::move(status));
                self->m_pendingPlayers.remove(service);
                if (self->m_pendingPlayers.isEmpty()) self->finishPoll(generation);
            });
        }
    });
}

void MprisMonitor::queryPlayer(const QString &service, quint64 generation)
{
    if (!m_backend || service.isEmpty()) return;
    QPointer<MprisMonitor> self(this);
    m_backend->queryPlaybackStatus(service, [self, generation, service](QString status) {
        if (!self || !self->m_started || generation != self->m_generation) return;
        self->m_playerStatus.insert(service, std::move(status));
        self->updatePlaying();
    });
}

void MprisMonitor::subscribeToDBus()
{
    auto bus = QDBusConnection::sessionBus();
    bus.connect(kDBusBusName, kDBusPath, kDBusInterface,
                QStringLiteral("NameOwnerChanged"), this,
                SLOT(onNameOwnerChanged(QString,QString,QString)));
    bus.connect(QString(), kPlayerPath, kPropertiesInterface,
                QStringLiteral("PropertiesChanged"), this,
                SLOT(onPropertiesChanged(QString,QVariantMap,QStringList)));
}

void MprisMonitor::unsubscribeFromDBus()
{
    auto bus = QDBusConnection::sessionBus();
    bus.disconnect(kDBusBusName, kDBusPath, kDBusInterface,
                   QStringLiteral("NameOwnerChanged"), this,
                   SLOT(onNameOwnerChanged(QString,QString,QString)));
    bus.disconnect(QString(), kPlayerPath, kPropertiesInterface,
                   QStringLiteral("PropertiesChanged"), this,
                   SLOT(onPropertiesChanged(QString,QVariantMap,QStringList)));
}

void MprisMonitor::onNameOwnerChanged(const QString &name,
                                      const QString &oldOwner,
                                      const QString &newOwner)
{
    handleNameOwnerChanged(name, oldOwner, newOwner);
}

void MprisMonitor::onPropertiesChanged(const QString &interfaceName,
                                       const QVariantMap &changedProperties,
                                       const QStringList &invalidatedProperties)
{
    handlePropertiesChanged(m_ownerToPlayer.value(message().service()), interfaceName,
                            changedProperties, invalidatedProperties);
}

void MprisMonitor::handleNameOwnerChanged(const QString &name,
                                          const QString &oldOwner,
                                          const QString &newOwner)
{
    if (!m_started || !name.startsWith(kMprisPrefix)) return;

    if (!oldOwner.isEmpty()) m_ownerToPlayer.remove(oldOwner);
    if (newOwner.isEmpty()) {
        m_playerStatus.remove(name);
        updatePlaying();
        return;
    }

    m_ownerToPlayer.insert(newOwner, name);
    queryPlayer(name, m_generation);
}

void MprisMonitor::handlePropertiesChanged(const QString &service,
                                           const QString &interfaceName,
                                           const QVariantMap &changedProperties,
                                           const QStringList &invalidatedProperties)
{
    if (!m_started || service.isEmpty() || interfaceName != kPlayerInterface) return;

    const auto status = changedProperties.constFind(QStringLiteral("PlaybackStatus"));
    if (status != changedProperties.constEnd()) {
        m_playerStatus.insert(service, status->toString());
        updatePlaying();
    } else if (invalidatedProperties.contains(QStringLiteral("PlaybackStatus"))) {
        queryPlayer(service, m_generation);
    }
}

void MprisMonitor::finishPoll(quint64 generation)
{
    if (generation != m_generation || !m_pollActive) return;
    m_playerStatus = m_pendingStatus;
    m_pendingPlayers.clear();
    m_pendingStatus.clear();
    m_pollActive = false;
    updatePlaying();
}

void MprisMonitor::updatePlaying()
{
    bool anyPlaying = false;
    for (auto it = m_playerStatus.constBegin(); it != m_playerStatus.constEnd(); ++it) {
        if (it.value() == QStringLiteral("Playing")) {
            anyPlaying = true;
            break;
        }
    }
    if (anyPlaying != m_anyPlaying) {
        m_anyPlaying = anyPlaying;
        qDebug() << "MprisMonitor: playing =" << m_anyPlaying;
        emit playingChanged(m_anyPlaying);
    }
}
