// mpris_monitor.h — MPRIS media playback monitor (Linux)
#pragma once

#include <QObject>
#include <QTimer>
#include <QHash>
#include <QSet>

#include "mpris_backend.h"

// Monitors MPRIS MediaPlayer2 services for playback status.
// Uses injectable MprisBackend for D-Bus communication.
class MprisMonitor : public QObject
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

signals:
    void playingChanged(bool playing);

private:
    void pollPlayers();
    void finishPoll(quint64 generation);
    void updatePlaying();

    QTimer *m_pollTimer = nullptr;
    MprisBackend *m_backend = nullptr;
    bool m_ownsBackend = false;

    // Per-player playback status cache
    QHash<QString, QString> m_playerStatus;

    bool m_anyPlaying = false;
    bool m_pollActive = false;
    bool m_started = false;
    quint64 m_generation = 0;
    QSet<QString> m_pendingPlayers;
    QHash<QString, QString> m_pendingStatus;
    static constexpr int kPollIntervalMs = 2000;
    static constexpr int kMaxPollMs = 500;
};
