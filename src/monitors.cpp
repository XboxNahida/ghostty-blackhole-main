// monitors.cpp  EnumDisplayMonitors wrapper
#include "monitors.h"
#include <algorithm>
#include <cstdio>

static BOOL CALLBACK MonitorEnumProc(HMONITOR hMon, HDC hdc, LPRECT rc, LPARAM lParam) {
    auto* list = (std::vector<MonitorInfo>*)lParam;
    MONITORINFO mi = { sizeof(mi) };
    if (GetMonitorInfoW(hMon, &mi)) {
        MonitorInfo info;
        info.rc = mi.rcMonitor;
        info.isPrimary = (mi.dwFlags & MONITORINFOF_PRIMARY) != 0;
        info.hMon = hMon;
        list->push_back(info);
    }
    return TRUE;
}

std::vector<MonitorInfo> EnumerateMonitors() {
    std::vector<MonitorInfo> list;
    EnumDisplayMonitors(nullptr, nullptr, MonitorEnumProc, (LPARAM)&list);
    std::sort(list.begin(), list.end(),
        [](const MonitorInfo& a, const MonitorInfo& b) { return a.isPrimary > b.isPrimary; });
    return list;
}

MonitorInfo GetPrimaryMonitor() {
    auto list = EnumerateMonitors();
    for (auto& m : list) if (m.isPrimary) return m;
    if (!list.empty()) return list[0];
    MonitorInfo fb = {};
    fb.hMon = MonitorFromWindow(nullptr, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO mi = { sizeof(mi) };
    GetMonitorInfoW(fb.hMon, &mi);
    fb.rc = mi.rcMonitor;
    fb.isPrimary = true;
    return fb;
}

MonitorInfo GetSecondaryMonitor() {
    auto list = EnumerateMonitors();
    for (auto& m : list) if (!m.isPrimary) return m;
    fprintf(stderr, "[Monitors] No secondary monitor, fallback to primary\n");
    return GetPrimaryMonitor();
}
