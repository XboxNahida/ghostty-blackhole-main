// mpris_backend.h — Injectable backend for MprisMonitor
#pragma once

#include <QStringList>
#include <QString>
#include <functional>

// Abstract backend for MPRIS player detection.
// Production: MprisDBusBackend (real D-Bus calls with timeout).
// Testing: MprisFakeBackend (deterministic results).
struct MprisBackend {
    virtual ~MprisBackend() = default;

    // Enumerate MPRIS services on session bus
    using PlayersCallback = std::function<void(QStringList)>;
    using StatusCallback = std::function<void(QString)>;
    using OwnerCallback = std::function<void(QString)>;

    virtual void enumeratePlayers(PlayersCallback callback) = 0;

    // Query a single player's PlaybackStatus; returns empty on error/timeout
    virtual void queryPlaybackStatus(const QString &serviceName,
                                     StatusCallback callback) = 0;

    // Resolve the unique bus owner used as the sender of PropertiesChanged.
    virtual void queryOwner(const QString &serviceName,
                            OwnerCallback callback) = 0;
};
