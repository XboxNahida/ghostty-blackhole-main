// mpris_monitor_tests.cpp — production-path asynchronous MPRIS tests
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QHash>
#include <QSet>
#include <QThread>
#include <QTimer>

#include <cstdlib>
#include <iostream>

#include "mpris_monitor.h"

namespace {

void Require(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << "\n";
        std::exit(1);
    }
    std::cout << "PASS: " << message << "\n";
}

bool WaitUntil(const std::function<bool()> &condition, int timeoutMs = 1000)
{
    QElapsedTimer elapsed;
    elapsed.start();
    while (!condition() && elapsed.elapsed() < timeoutMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        QThread::msleep(1);
    }
    return condition();
}

class FakeBackend : public MprisBackend
{
public:
    struct Player { QString status; int delayMs = 0; };
    QHash<QString, Player> players;
    int enumerateDelayMs = 0;
    int enumerateCount = 0;
    int statusQueryCount = 0;
    int ownerQueryCount = 0;

    void enumeratePlayers(PlayersCallback callback) override
    {
        ++enumerateCount;
        const QStringList names = players.keys();
        QTimer::singleShot(enumerateDelayMs, [callback = std::move(callback), names]() mutable {
            callback(names);
        });
    }

    void queryPlaybackStatus(const QString &service, StatusCallback callback) override
    {
        ++statusQueryCount;
        const Player player = players.value(service);
        QTimer::singleShot(player.delayMs,
                           [callback = std::move(callback), status = player.status]() mutable {
            callback(status);
        });
    }

    void queryOwner(const QString &service, OwnerCallback callback) override
    {
        ++ownerQueryCount;
        QTimer::singleShot(0, [callback = std::move(callback), service]() mutable {
            callback(QStringLiteral(":owner.") + service);
        });
    }
};

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    {
        MprisMonitor monitor;
        Require(!monitor.isPlaying(), "initial not playing");
        Require(monitor.activePlayers().isEmpty(), "initial no players");
    }

    for (const auto &sample : {qMakePair(QStringLiteral("Playing"), true),
                               qMakePair(QStringLiteral("Paused"), false),
                               qMakePair(QStringLiteral("Stopped"), false)}) {
        MprisMonitor monitor;
        FakeBackend backend;
        backend.players.insert("org.mpris.MediaPlayer2.player", {sample.first, 0});
        monitor.setBackend(&backend);
        monitor.start();
        Require(WaitUntil([&]() { return monitor.activePlayers().size() == 1; }),
                "status reply applied");
        Require(monitor.isPlaying() == sample.second,
                "Playing/Paused/Stopped policy");
        monitor.stop();
    }

    {
        MprisMonitor monitor;
        FakeBackend backend;
        monitor.setBackend(&backend);
        monitor.start();
        Require(WaitUntil([&]() { return backend.enumerateCount == 1; }),
                "startup performs one initial enumeration");
        QThread::msleep(2100);
        QCoreApplication::processEvents();
        Require(backend.enumerateCount == 1, "idle monitoring has no periodic enumeration");

        const QString service = QStringLiteral("org.mpris.MediaPlayer2.events");
        backend.players.insert(service, {"Playing", 0});
        monitor.handleNameOwnerChanged(service, QString(), QStringLiteral(":1.50"));
        Require(WaitUntil([&]() { return monitor.isPlaying(); }),
                "player appearance queries status and enters Playing");

        monitor.handlePropertiesChanged(
            service, QStringLiteral("org.mpris.MediaPlayer2.Player"),
            {{QStringLiteral("PlaybackStatus"), QStringLiteral("Paused")}}, {});
        Require(!monitor.isPlaying(), "PropertiesChanged updates PlaybackStatus directly");

        backend.players[service].status = QStringLiteral("Playing");
        const int queriesBeforeInvalidation = backend.statusQueryCount;
        monitor.handlePropertiesChanged(
            service, QStringLiteral("org.mpris.MediaPlayer2.Player"), {},
            {QStringLiteral("PlaybackStatus")});
        Require(WaitUntil([&]() { return monitor.isPlaying(); }),
                "invalidated PlaybackStatus is queried once");
        Require(backend.statusQueryCount == queriesBeforeInvalidation + 1,
                "invalidation performs one targeted status query");

        monitor.handleNameOwnerChanged(service, QStringLiteral(":1.50"), QString());
        Require(!monitor.isPlaying() && monitor.activePlayers().isEmpty(),
                "player disappearance removes cached status");
        Require(backend.enumerateCount == 1,
                "lifecycle and property events do not repeat ListNames");
        monitor.stop();
    }

    {
        MprisMonitor monitor;
        FakeBackend backend;
        backend.players.insert("org.mpris.MediaPlayer2.paused", {"Paused", 5});
        backend.players.insert("org.mpris.MediaPlayer2.playing", {"Playing", 10});
        monitor.setBackend(&backend);
        monitor.start();
        Require(WaitUntil([&]() { return monitor.isPlaying(); }),
                "multiple players detect Playing");
        Require(monitor.activePlayers().size() == 2, "multiple players retained");
        monitor.stop();
    }

    {
        MprisMonitor monitor;
        FakeBackend backend;
        backend.players.insert("org.mpris.MediaPlayer2.player", {"Playing", 0});
        monitor.setBackend(&backend);
        monitor.start();
        Require(WaitUntil([&]() { return monitor.isPlaying(); }), "playing before disappearance");
        monitor.stop();
        backend.players.clear();
        monitor.start();
        Require(WaitUntil([&]() { return !monitor.isPlaying(); }), "disappearance clears Playing");
        Require(monitor.activePlayers().isEmpty(), "disappearance clears player cache");
        monitor.stop();
    }

    {
        MprisMonitor monitor;
        FakeBackend backend;
        backend.players.insert("org.mpris.MediaPlayer2.player", {"Paused", 0});
        monitor.setBackend(&backend);
        monitor.start();
        monitor.start();
        monitor.start();
        QCoreApplication::processEvents();
        Require(backend.enumerateCount == 1, "repeated start is idempotent");
        monitor.stop();
    }

    {
        MprisMonitor monitor;
        FakeBackend backend;
        backend.players.insert("org.mpris.MediaPlayer2.broken", {"Playing", 900});
        backend.players.insert("org.mpris.MediaPlayer2.healthy", {"Playing", 20});
        monitor.setBackend(&backend);

        int eventLoopTicks = 0;
        QTimer heartbeat;
        QObject::connect(&heartbeat, &QTimer::timeout, [&]() { ++eventLoopTicks; });
        heartbeat.start(10);
        QElapsedTimer elapsed;
        elapsed.start();
        monitor.start();

        Require(WaitUntil([&]() { return monitor.isPlaying(); }, 700),
                "healthy Playing applied within overall deadline");
        Require(elapsed.elapsed() < 700, "poll cycle has a bounded overall deadline");
        Require(eventLoopTicks >= 20, "late player does not stall Qt event loop");
        const int playersAtDeadline = monitor.activePlayers().size();
        Require(playersAtDeadline == 1, "timed-out player omitted from applied snapshot");
        Require(WaitUntil([&]() { return elapsed.elapsed() >= 950; }, 1100),
                "late reply delivered after deadline");
        Require(monitor.activePlayers().size() == playersAtDeadline,
                "late reply ignored safely");
        monitor.stop();
    }

    std::cout << "MPRIS_MONITOR_TESTS_OK\n";
    return 0;
}
