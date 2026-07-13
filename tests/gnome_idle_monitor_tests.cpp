// gnome_idle_monitor_tests.cpp — DS-06: GNOME idle monitor state machine tests
#include <QCoreApplication>
#include <QDBusInterface>
#include <QDebug>

#include <cstdlib>
#include <iostream>

#include "gnome_idle_monitor.h"

namespace {

void Require(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << "\n";
        std::exit(1);
    }
    std::cout << "PASS: " << message << "\n";
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    // We test GnomeIdleMonitor's state machine by injecting minimal
    // QDBusInterface objects. Since QDBusInterface is concrete and
    // has no session bus in test, the asyncCall() will fail — but
    // the monitor should handle this gracefully (Degraded state)
    // and we can verify state transitions manually.

    // Test 1: Initial state is Degraded
    {
        GnomeIdleMonitor monitor;
        Require(monitor.state() == GnomeIdleMonitor::Degraded,
                "initial state is Degraded");
    }

    // Test 2: Start/stop lifecycle
    {
        GnomeIdleMonitor monitor;
        monitor.start();
        // QDBusInterface creates proxy objects; start() transitions to Active
        // (D-Bus calls may or may not succeed but state machine advances)
        monitor.stop();
        Require(monitor.state() == GnomeIdleMonitor::Degraded,
                "stop returns to Degraded");

        // Double stop should be safe
        monitor.stop();
        Require(monitor.state() == GnomeIdleMonitor::Degraded,
                "double stop stays Degraded");
    }

    // Test 3: setIdleSeconds
    {
        GnomeIdleMonitor monitor;
        Require(monitor.idleSeconds() == 300, "default idle seconds is 300");
        monitor.setIdleSeconds(600);
        Require(monitor.idleSeconds() == 600, "setIdleSeconds updates value");
    }

    // Test 4: State name
    {
        GnomeIdleMonitor monitor;
        Require(monitor.stateName() == "Degraded", "stateName returns Degraded");
    }

    // Test 5: isLocked/isActive/isIdleEligible helpers
    {
        GnomeIdleMonitor monitor;
        Require(!monitor.isLocked(), "initial not locked");
        Require(!monitor.isActive(), "initial not active");
        Require(!monitor.isIdleEligible(), "initial not idle eligible");
        Require(!monitor.isRendererRunning(), "initial not rendering");
    }

    // Test 6: setRendererRunning on Degraded state (no-op)
    {
        GnomeIdleMonitor monitor;
        monitor.setRendererRunning(true);
        Require(monitor.state() == GnomeIdleMonitor::Degraded,
                "setRendererRunning(true) on Degraded is no-op");
        monitor.setRendererRunning(false);
        Require(monitor.state() == GnomeIdleMonitor::Degraded,
                "setRendererRunning(false) on Degraded is no-op");
    }

    std::cout << "GNOME_IDLE_MONITOR_TESTS_OK\n";
    return 0;
}
