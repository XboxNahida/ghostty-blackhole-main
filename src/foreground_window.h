#pragma once

#include <windows.h>

enum class ForegroundKind {
    Application,
    Desktop,
    SystemShell,
    BlackholeOverlay
};

ForegroundKind Foreground_ClassifyClass(const wchar_t *className, LONG_PTR exStyle);
ForegroundKind Foreground_Classify(HWND hwnd);

bool Foreground_CoversMonitor(const RECT &windowRect,
                              const RECT &monitorRect,
                              int tolerance = 2);

bool Foreground_IsBorderlessFullscreen(HWND hwnd);
