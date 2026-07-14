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

static const QString kDBusBusName = QStringLiteral("org.freedesktop.DBus");
static const QString kDBusPath    = QStringLiteral("/org/freedesktop/DBus");
static const QString kDBusInterface = QStringLiteral("org.freedesktop.DBus");
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
                static const QString prefix = QStringLiteral("org.mpris.MediaPlayer2.");
                for (const QString &name : reply.value())
                    if (name.startsWith(prefix)) players.append(name);
            }
            watcher->deleteLater();
            callback(std::move(players));
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
    if (!m_pollTimer) {
        m_pollTimer = new QTimer(this);
        m_pollTimer->setInterval(kPollIntervalMs);
        connect(m_pollTimer, &QTimer::timeout, this, [this]() {
            pollPlayers();
        });
    }
    if (m_started) return;
    m_started = true;
    pollPlayers();
    m_pollTimer->start();
}

void MprisMonitor::stop()
{
    if (m_pollTimer) m_pollTimer->stop();
    m_started = false;
    ++m_generation;
    m_playerStatus.clear();
    m_pendingPlayers.clear();
    m_pendingStatus.clear();
    m_pollActive = false;
    m_anyPlaying = false;
}

void MprisMonitor::pollPlayers()
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
            self->m_backend->queryPlaybackStatus(service, [self, generation, service](QString status) {
                if (!self || !self->m_started || generation != self->m_generation || !self->m_pollActive) return;
                self->m_pendingStatus.insert(service, std::move(status));
                self->m_pendingPlayers.remove(service);
                if (self->m_pendingPlayers.isEmpty()) self->finishPoll(generation);
            });
        }
    });
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
