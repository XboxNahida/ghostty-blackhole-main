#include "gnome_idle_backend.h"
#include "gnome_idle_monitor.h"

#include <QCoreApplication>

#include <cstdlib>
#include <iostream>
#include <vector>

namespace {
const QString kIdleService = QStringLiteral("org.gnome.Mutter.IdleMonitor");
const QString kScreenSaverService = QStringLiteral("org.gnome.ScreenSaver");

void require(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

struct FakeBackend final : GnomeIdleBackend {
    bool idleAvailable = true;
    bool screenSaverAvailableValue = true;
    bool locked = false;
    bool screenSaverReplyOk = true;
    quint32 nextIdleId = 11;
    quint32 nextActiveId = 22;
    quint64 lastIdleInterval = 0;
    quint64 idleTime = 300000;
    bool idleTimeReplyOk = true;
    int refreshCount = 0;
    std::vector<quint32> removed;

    quint32 addIdleWatch(quint64 intervalMs) override
    {
        lastIdleInterval = intervalMs;
        return idleAvailable ? nextIdleId : 0;
    }
    quint32 addActiveWatch() override { return idleAvailable ? nextActiveId : 0; }
    void removeWatch(quint32 id) override { removed.push_back(id); }
    quint64 idleTimeMs(bool *ok) override { *ok = idleTimeReplyOk; return idleTime; }
    bool screenSaverActive(bool *ok) override
    {
        *ok = screenSaverReplyOk;
        return locked;
    }
    bool idleServiceAvailable() const override { return idleAvailable; }
    bool screenSaverAvailable() const override { return screenSaverAvailableValue; }
    void refresh() override { ++refreshCount; }
};
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    {
        FakeBackend backend;
        GnomeIdleMonitor monitor(&backend);
        require(monitor.state() == GnomeIdleMonitor::Degraded, "initial state is Degraded");
        monitor.start();
        require(monitor.isActive(), "start with both services and watches becomes Active");
        require(monitor.hasIdleWatch() && monitor.hasActiveWatch(), "both watches registered");
        require(backend.lastIdleInterval == 300000, "default idle interval passed to backend");
        monitor.setIdleSeconds(45);
        require(backend.lastIdleInterval == 45000, "updated idle interval re-arms production watches");
    }

    {
        FakeBackend backend;
        backend.locked = true;
        GnomeIdleMonitor monitor(&backend);
        monitor.start();
        require(monitor.isLocked(), "initial locked state is read from backend");
        require(!monitor.hasIdleWatch() && !monitor.hasActiveWatch(), "locked startup registers no watches");
    }

    {
        FakeBackend backend;
        GnomeIdleMonitor monitor(&backend);
        bool idleSignal = false;
        bool activitySignal = false;
        QObject::connect(&monitor, &GnomeIdleMonitor::idleEligible, [&] { idleSignal = true; });
        QObject::connect(&monitor, &GnomeIdleMonitor::activityDetected, [&] { activitySignal = true; });
        monitor.start();
        monitor.handleWatchFired(11);
        require(monitor.isIdleEligible() && idleSignal, "exact idle ID enters IdleEligible");
        monitor.setRendererRunning(true);
        require(monitor.isRendererRunning(), "renderer callback enters RendererRunning");
        monitor.handleWatchFired(22);
        require(activitySignal && monitor.isActive(), "exact active ID stops renderer and re-arms");
        require(monitor.hasIdleWatch() && monitor.hasActiveWatch(), "activity re-arms both watches");
    }

    {
        FakeBackend backend;
        GnomeIdleMonitor monitor(&backend);
        bool activitySignal = false;
        QObject::connect(&monitor, &GnomeIdleMonitor::activityDetected, [&] { activitySignal = true; });
        monitor.start();
        monitor.handleWatchFired(11);
        monitor.setRendererRunning(true);
        backend.idleTime = 0;
        monitor.pollActivity();
        require(activitySignal && monitor.isActive(), "idle-time fallback detects resumed activity");
    }

    {
        FakeBackend backend;
        GnomeIdleMonitor monitor(&backend);
        bool lockedSignal = false;
        bool unlockedSignal = false;
        QObject::connect(&monitor, &GnomeIdleMonitor::lockDetected, [&] { lockedSignal = true; });
        QObject::connect(&monitor, &GnomeIdleMonitor::unlockDetected, [&] { unlockedSignal = true; });
        monitor.start();
        monitor.handleScreenSaverActiveChanged(true);
        require(monitor.isLocked() && lockedSignal, "lock event enters Locked and emits");
        require(!monitor.hasIdleWatch() && !monitor.hasActiveWatch(), "lock clears both IDs");
        require(backend.removed.size() == 2 && backend.removed[0] == 11 && backend.removed[1] == 22,
                "lock removes exact registered IDs through backend");
        backend.nextIdleId = 31;
        backend.nextActiveId = 32;
        monitor.handleScreenSaverActiveChanged(false);
        require(monitor.isActive() && unlockedSignal, "unlock re-arms and enters Active");
        monitor.handleWatchFired(31);
        require(monitor.isIdleEligible(), "unlock uses newly registered idle ID");
    }

    {
        FakeBackend backend;
        backend.nextActiveId = 0;
        GnomeIdleMonitor monitor(&backend);
        monitor.start();
        require(monitor.state() == GnomeIdleMonitor::Degraded, "partial watch registration degrades");
        require(!monitor.hasIdleWatch() && !monitor.hasActiveWatch(), "partial registration is rolled back");
        require(backend.removed.size() == 1 && backend.removed[0] == 11,
                "successful half of partial registration is removed");
    }

    {
        FakeBackend backend;
        GnomeIdleMonitor monitor(&backend);
        bool emitted = false;
        QObject::connect(&monitor, &GnomeIdleMonitor::idleEligible, [&] { emitted = true; });
        monitor.start();
        monitor.stop();
        monitor.handleWatchFired(11);
        monitor.handleScreenSaverActiveChanged(true);
        monitor.handleServiceRegistered(kIdleService);
        require(monitor.state() == GnomeIdleMonitor::Degraded && !emitted,
                "all stale callbacks are suppressed after stop");
        require(backend.refreshCount == 0, "stopped service callback cannot recover");
    }

    {
        FakeBackend backend;
        GnomeIdleMonitor monitor(&backend);
        monitor.start();
        backend.idleAvailable = false;
        monitor.handleServiceUnregistered(kIdleService);
        require(monitor.state() == GnomeIdleMonitor::Degraded, "service loss degrades");
        require(!monitor.hasIdleWatch() && !monitor.hasActiveWatch(), "service loss clears watch IDs");
        require(backend.removed.size() == 2, "service loss removes both registered watches");
        monitor.handleServiceRegistered(kScreenSaverService);
        require(monitor.state() == GnomeIdleMonitor::Degraded,
                "recovery waits until both services are available");
        backend.idleAvailable = true;
        backend.nextIdleId = 41;
        backend.nextActiveId = 42;
        monitor.handleServiceRegistered(kIdleService);
        require(monitor.isActive() && monitor.hasIdleWatch() && monitor.hasActiveWatch(),
                "both-service recovery registers both watches and becomes Active");
        require(backend.refreshCount == 3, "loss and registration events refresh backend proxies");
    }

    {
        FakeBackend backend;
        backend.screenSaverReplyOk = false;
        GnomeIdleMonitor monitor(&backend);
        monitor.start();
        require(monitor.state() == GnomeIdleMonitor::Degraded, "GetActive failure degrades");
    }

    std::cout << "GNOME_IDLE_MONITOR_TESTS_OK\n";
    return 0;
}
