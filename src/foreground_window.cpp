#include "foreground_window.h"

#include <cwchar>

namespace {

bool ClassEquals(const wchar_t *actual, const wchar_t *expected)
{
    return actual && expected && _wcsicmp(actual, expected) == 0;
}

LONG Distance(LONG lhs, LONG rhs)
{
    return lhs >= rhs ? lhs - rhs : rhs - lhs;
}

} // namespace

ForegroundKind Foreground_ClassifyClass(const wchar_t *className, LONG_PTR exStyle)
{
    if (ClassEquals(className, L"Progman") || ClassEquals(className, L"WorkerW")) {
        return ForegroundKind::Desktop;
    }

    constexpr const wchar_t *kSystemShellClasses[] = {
        L"Shell_TrayWnd",
        L"Shell_SecondaryTrayWnd",
        L"TaskManagerWindow",
        L"TaskSwitcherWnd",
        L"MultitaskingViewFrame",
        L"XamlExplorerHostIslandWindow"
    };
    for (const wchar_t *systemClass : kSystemShellClasses) {
        if (ClassEquals(className, systemClass)) {
            return ForegroundKind::SystemShell;
        }
    }

    constexpr LONG_PTR kOverlayStyle = WS_EX_NOACTIVATE | WS_EX_TOPMOST | WS_EX_TRANSPARENT;
    if (ClassEquals(className, L"BlackHoleWGL") || (exStyle & kOverlayStyle) == kOverlayStyle) {
        return ForegroundKind::BlackholeOverlay;
    }

    return ForegroundKind::Application;
}

ForegroundKind Foreground_Classify(HWND hwnd)
{
    if (!hwnd) {
        return ForegroundKind::SystemShell;
    }

    wchar_t className[128] = {};
    GetClassNameW(hwnd, className, static_cast<int>(sizeof(className) / sizeof(className[0])));
    const LONG_PTR exStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    return Foreground_ClassifyClass(className, exStyle);
}

bool Foreground_CoversMonitor(const RECT &windowRect, const RECT &monitorRect, int tolerance)
{
    if (tolerance < 0) {
        tolerance = 0;
    }
    return Distance(windowRect.left, monitorRect.left) <= tolerance &&
           Distance(windowRect.top, monitorRect.top) <= tolerance &&
           Distance(windowRect.right, monitorRect.right) <= tolerance &&
           Distance(windowRect.bottom, monitorRect.bottom) <= tolerance;
}

bool Foreground_IsBorderlessFullscreen(HWND hwnd)
{
    if (!hwnd || Foreground_Classify(hwnd) != ForegroundKind::Application) {
        return false;
    }

    const LONG_PTR style = GetWindowLongPtrW(hwnd, GWL_STYLE);
    if ((style & WS_MAXIMIZE) != 0) {
        return false;
    }

    RECT windowRect = {};
    if (!GetWindowRect(hwnd, &windowRect)) {
        return false;
    }

    const HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    if (!monitor) {
        return false;
    }

    MONITORINFO monitorInfo = {};
    monitorInfo.cbSize = sizeof(monitorInfo);
    if (!GetMonitorInfoW(monitor, &monitorInfo)) {
        return false;
    }

    return Foreground_CoversMonitor(windowRect, monitorInfo.rcMonitor);
}
