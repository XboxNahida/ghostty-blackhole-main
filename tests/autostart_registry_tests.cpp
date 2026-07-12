#include "autostart_registry.h"

#include <cassert>
#include <iostream>
#include <string>
#include <windows.h>

namespace {

constexpr wchar_t kTestRoot[] = L"Software\\BlackholeScreensaverTests";
constexpr wchar_t kTestRunKey[] = L"Software\\BlackholeScreensaverTests\\Run";

void CleanupTestKey()
{
    RegDeleteTreeW(HKEY_CURRENT_USER, kTestRoot);
}

bool ValueExists(const wchar_t *valueName)
{
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kTestRunKey, 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS) {
        return false;
    }
    const LONG result = RegQueryValueExW(key, valueName, nullptr, nullptr, nullptr, nullptr);
    RegCloseKey(key);
    return result == ERROR_SUCCESS;
}

void SeedLegacyValue()
{
    HKEY key = nullptr;
    DWORD disposition = 0;
    assert(RegCreateKeyExW(HKEY_CURRENT_USER, kTestRunKey, 0, nullptr, 0,
                           KEY_SET_VALUE, nullptr, &key, &disposition) == ERROR_SUCCESS);
    constexpr wchar_t command[] = L"legacy.exe";
    assert(RegSetValueExW(key, L"BlackholeScreensaver", 0, REG_SZ,
                          reinterpret_cast<const BYTE *>(command), sizeof(command)) == ERROR_SUCCESS);
    RegCloseKey(key);
}

} // namespace

int main()
{
    CleanupTestKey();

    const std::wstring exePath = L"C:\\Program Files\\Blackhole\\appBlakholeUI.exe";
    const AutoStartResult enabled = AutoStart_Set(true, exePath, kTestRunKey);
    assert(enabled.success);

    std::wstring command;
    assert(AutoStart_Query(command, kTestRunKey));
    assert(command == L"\"C:\\Program Files\\Blackhole\\appBlakholeUI.exe\"");

    SeedLegacyValue();
    assert(ValueExists(L"BlackholeScreensaver"));

    const AutoStartResult disabled = AutoStart_Set(false, L"", kTestRunKey);
    assert(disabled.success);
    assert(!AutoStart_Query(command, kTestRunKey));
    assert(!ValueExists(L"BlackholeScreensaver"));

    SeedLegacyValue();
    assert(AutoStart_Query(command, kTestRunKey));
    assert(command == L"legacy.exe");
    assert(AutoStart_Set(true, exePath, kTestRunKey).success);
    assert(!ValueExists(L"BlackholeScreensaver"));

    CleanupTestKey();
    std::cout << "AUTOSTART_REGISTRY_TESTS_OK\n";
    return 0;
}
