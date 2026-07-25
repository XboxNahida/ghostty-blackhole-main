#pragma once

enum class DpiAwarenessMode {
    PerMonitorV2,
    PerMonitor,
    System,
    Unchanged
};

DpiAwarenessMode EnableBestDpiAwareness();
const char* DpiAwarenessModeName(DpiAwarenessMode mode);
