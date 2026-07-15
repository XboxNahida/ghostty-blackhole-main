// mpris_monitor.h — MPRIS media playback monitor (Linux)
#pragma once

#include <QObject>
#include <QDBusContext>
#include <QHash>
#include <QSet>
#include <QVariantMap>

#include "mpris_backend.h"

// Monitors MPRIS MediaPlayer2 services for playback status.
// Uses injectable MprisBackend for D-Bus communication.
class MprisMonitor : public QObject, protected QDBusContext
{
    Q_OBJECT
public:
    explicit MprisMonitor(QObject *parent = nullptr);
    ~MprisMonitor() override;

    bool isPlaying() const { return m_anyPlaying; }
    QStringList activePlayers() const;

    // Replace backend (for testing: inject MprisFakeBackend)
    void setBackend(MprisBackend *backend);

    // Start/stop monitoring
    void start();
    void stop();

    // Event entry points are public so the D-Bus-independent tests exercise
    // the same cache transitions as the production signal handlers.
    void handleNameOwnerChanged(const QString &name,
                                const QString &oldOwner,
                                const QString &newOwner);
    void handlePropertiesChanged(const QString &service,
                                 const QString &interfaceName,
                                 const QVariantMap &changedProperties,
                                 const QStringList &invalidatedProperties);

signals:
    void playingChanged(bool playing);

private:
    void initialSync();
    void finishPoll(quint64 generation);
    void queryPlayer(const QString &service, quint64 generation);
    void subscribeToDBus();
    void unsubscribeFromDBus();
    void updatePlaying();

private slots:
    void onNameOwnerChanged(const QString &name,
                            const QString &oldOwner,
                            const QString &newOwner);
    void onPropertiesChanged(const QString &interfaceName,
                             const QVariantMap &changedProperties,
                             const QStringList &invalidatedProperties);

private:
    MprisBackend *m_backend = nullptr;
    bool m_ownsBackend = false;

    // Per-player playback status cache
    QHash<QString, QString> m_playerStatus;
    QHash<QString, QString> m_ownerToPlayer;

    bool m_anyPlaying = false;
    bool m_pollActive = false;
    bool m_started = false;
    quint64 m_generation = 0;
    QSet<QString> m_pendingPlayers;
    QHash<QString, QString> m_pendingStatus;
    static constexpr int kMaxPollMs = 500;
};
