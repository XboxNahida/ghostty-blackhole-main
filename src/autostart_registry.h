#pragma once

#include <string>
#include <windows.h>

inline constexpr wchar_t kAutoStartRunKey[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";

struct AutoStartResult {
    bool success = false;
    DWORD errorCode = ERROR_SUCCESS;
};

AutoStartResult AutoStart_Set(bool enabled,
                              const std::wstring &exePath,
                              const wchar_t *subKey = kAutoStartRunKey);

bool AutoStart_Query(std::wstring &command,
                     const wchar_t *subKey = kAutoStartRunKey);
