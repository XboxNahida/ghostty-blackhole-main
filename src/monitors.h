// monitors.h  显示器枚举与选择
#pragma once
#include <windows.h>
#include <vector>

struct MonitorInfo {
    RECT  rc;          // 虚拟桌面坐标
    bool  isPrimary;   // MONITORINFOF_PRIMARY
    HMONITOR hMon;
};

// 枚举所有显示器，主屏排在最前
std::vector<MonitorInfo> EnumerateMonitors();

// 主屏（isPrimary==true）。若没有则返回第一个。
MonitorInfo GetPrimaryMonitor();

// 副屏（第一个 isPrimary==false）。若没有则 fallback 到主屏。
MonitorInfo GetSecondaryMonitor();
