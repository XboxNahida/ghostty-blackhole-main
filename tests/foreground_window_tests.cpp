#include "foreground_window.h"

#include <cassert>
#include <iostream>
#include <windows.h>

int main()
{
    assert(Foreground_ClassifyClass(L"Progman", 0) == ForegroundKind::Desktop);
    assert(Foreground_ClassifyClass(L"WorkerW", 0) == ForegroundKind::Desktop);
    assert(Foreground_ClassifyClass(L"Shell_TrayWnd", 0) == ForegroundKind::SystemShell);
    assert(Foreground_ClassifyClass(L"Shell_SecondaryTrayWnd", 0) == ForegroundKind::SystemShell);
    assert(Foreground_ClassifyClass(L"BlackHoleWGL", 0) == ForegroundKind::BlackholeOverlay);

    constexpr LONG_PTR overlayStyle = WS_EX_NOACTIVATE | WS_EX_TOPMOST | WS_EX_TRANSPARENT;
    assert(Foreground_ClassifyClass(L"UnknownOverlay", overlayStyle) == ForegroundKind::BlackholeOverlay);
    assert(Foreground_ClassifyClass(L"GameWindow", 0) == ForegroundKind::Application);

    const RECT monitor{1920, 0, 3840, 1080};
    const RECT exact{1920, 0, 3840, 1080};
    const RECT withinTolerance{1919, 1, 3841, 1079};
    const RECT windowed{2000, 100, 3600, 1000};
    assert(Foreground_CoversMonitor(exact, monitor, 2));
    assert(Foreground_CoversMonitor(withinTolerance, monitor, 2));
    assert(!Foreground_CoversMonitor(windowed, monitor, 2));

    std::cout << "FOREGROUND_WINDOW_TESTS_OK\n";
    return 0;
}
