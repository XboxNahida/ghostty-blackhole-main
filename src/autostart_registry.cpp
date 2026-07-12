#include "autostart_registry.h"

#include <vector>

namespace {

constexpr wchar_t kCurrentValueName[] = L"BlakholeUI";
constexpr wchar_t kLegacyValueName[] = L"BlackholeScreensaver";

std::wstring QuotedCommand(const std::wstring &exePath)
{
    if (exePath.size() >= 2 && exePath.front() == L'"' && exePath.back() == L'"') {
        return exePath;
    }
    return L"\"" + exePath + L"\"";
}

LONG DeleteValueIfPresent(HKEY key, const wchar_t *valueName)
{
    const LONG result = RegDeleteValueW(key, valueName);
    return result == ERROR_FILE_NOT_FOUND ? ERROR_SUCCESS : result;
}

bool QueryStringValue(HKEY key, const wchar_t *valueName, std::wstring &value)
{
    DWORD type = 0;
    DWORD byteCount = 0;
    LONG result = RegQueryValueExW(key, valueName, nullptr, &type, nullptr, &byteCount);
    if (result != ERROR_SUCCESS || (type != REG_SZ && type != REG_EXPAND_SZ) ||
        byteCount < sizeof(wchar_t)) {
        return false;
    }

    std::vector<wchar_t> buffer(byteCount / sizeof(wchar_t) + 1, L'\0');
    result = RegQueryValueExW(key, valueName, nullptr, &type,
                              reinterpret_cast<BYTE *>(buffer.data()), &byteCount);
    if (result != ERROR_SUCCESS) {
        return false;
    }

    value.assign(buffer.data());
    return true;
}

} // namespace

bool AutoStart_Query(std::wstring &command, const wchar_t *subKey)
{
    command.clear();
    HKEY key = nullptr;
    const LONG openResult = RegOpenKeyExW(HKEY_CURRENT_USER, subKey, 0, KEY_QUERY_VALUE, &key);
    if (openResult != ERROR_SUCCESS) {
        return false;
    }

    const bool found = QueryStringValue(key, kCurrentValueName, command) ||
                       QueryStringValue(key, kLegacyValueName, command);
    RegCloseKey(key);
    return found;
}

AutoStartResult AutoStart_Set(bool enabled,
                              const std::wstring &exePath,
                              const wchar_t *subKey)
{
    HKEY key = nullptr;
    DWORD disposition = 0;
    const LONG openResult = RegCreateKeyExW(HKEY_CURRENT_USER, subKey, 0, nullptr, 0,
                                             KEY_SET_VALUE | KEY_QUERY_VALUE, nullptr,
                                             &key, &disposition);
    if (openResult != ERROR_SUCCESS) {
        return {false, static_cast<DWORD>(openResult)};
    }

    if (!enabled) {
        const LONG currentResult = DeleteValueIfPresent(key, kCurrentValueName);
        const LONG legacyResult = DeleteValueIfPresent(key, kLegacyValueName);
        RegCloseKey(key);
        if (currentResult != ERROR_SUCCESS) {
            return {false, static_cast<DWORD>(currentResult)};
        }
        if (legacyResult != ERROR_SUCCESS) {
            return {false, static_cast<DWORD>(legacyResult)};
        }
        return {true, ERROR_SUCCESS};
    }

    if (exePath.empty()) {
        RegCloseKey(key);
        return {false, ERROR_INVALID_PARAMETER};
    }

    const std::wstring command = QuotedCommand(exePath);
    const DWORD byteCount = static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t));
    const LONG writeResult = RegSetValueExW(key, kCurrentValueName, 0, REG_SZ,
                                             reinterpret_cast<const BYTE *>(command.c_str()),
                                             byteCount);
    const LONG legacyResult = DeleteValueIfPresent(key, kLegacyValueName);
    RegCloseKey(key);
    if (writeResult != ERROR_SUCCESS) {
        return {false, static_cast<DWORD>(writeResult)};
    }
    if (legacyResult != ERROR_SUCCESS) {
        return {false, static_cast<DWORD>(legacyResult)};
    }

    std::wstring savedCommand;
    if (!AutoStart_Query(savedCommand, subKey) || savedCommand != command) {
        return {false, ERROR_WRITE_FAULT};
    }
    return {true, ERROR_SUCCESS};
}
