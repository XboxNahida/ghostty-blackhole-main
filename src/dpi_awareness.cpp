#include "dpi_awareness.h"

#include <windows.h>

namespace {

using SetProcessDpiAwarenessContextFn = BOOL(WINAPI*)(HANDLE);

const HANDLE DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_COMPAT =
    reinterpret_cast<HANDLE>(static_cast<LONG_PTR>(-3));
const HANDLE DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2_COMPAT =
    reinterpret_cast<HANDLE>(static_cast<LONG_PTR>(-4));

}

DpiAwarenessMode EnableBestDpiAwareness()
{
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    auto setContext = user32
        ? reinterpret_cast<SetProcessDpiAwarenessContextFn>(
              GetProcAddress(user32, "SetProcessDpiAwarenessContext"))
        : nullptr;

    if (setContext) {
        if (setContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2_COMPAT)) {
            return DpiAwarenessMode::PerMonitorV2;
        }
        if (setContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_COMPAT)) {
            return DpiAwarenessMode::PerMonitor;
        }
    }
    if (SetProcessDPIAware()) {
        return DpiAwarenessMode::System;
    }
    return DpiAwarenessMode::Unchanged;
}

const char* DpiAwarenessModeName(DpiAwarenessMode mode)
{
    switch (mode) {
    case DpiAwarenessMode::PerMonitorV2: return "PerMonitorV2";
    case DpiAwarenessMode::PerMonitor: return "PerMonitor";
    case DpiAwarenessMode::System: return "System";
    case DpiAwarenessMode::Unchanged: return "Unchanged";
    }
    return "Unknown";
}
