#include "game_detection.h"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cwctype>
#include <iostream>
#include <string>
#include <vector>

namespace {

std::wstring Lower(std::wstring value)
{
    std::transform(value.begin(), value.end(), value.begin(),
                   [](wchar_t ch) { return static_cast<wchar_t>(std::towlower(ch)); });
    return value;
}

} // namespace

int main()
{
    assert(GameDetection_IsKnownGamePath(
        L"D:\\SteamLibrary\\steamapps\\common\\Example Game\\game.exe", {}));
    assert(GameDetection_IsKnownGamePath(
        L"d:/steamlibrary/STEAMAPPS/common/Example Game/game.exe", {}));

    const std::vector<std::wstring> catalog = {
        L"D:\\Games\\Known Game\\game.exe"
    };
    assert(GameDetection_IsKnownGamePath(
        L"d:/games/known game/GAME.EXE", catalog));
    assert(!GameDetection_IsKnownGamePath(
        L"C:\\Program Files\\Browser\\browser.exe", catalog));
    assert(!GameDetection_IsKnownGamePath(
        L"C:\\Program Files (x86)\\Steam\\steam.exe", catalog));
    assert(!GameDetection_IsKnownGamePath(L"", catalog));

    const auto started = std::chrono::steady_clock::now();
    const std::vector<std::wstring> windowsCatalog = GameDetection_LoadWindowsCatalog();
    const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - started).count();
    assert(elapsedMs < 1000);
    for (const std::wstring &path : windowsCatalog) {
        assert(!path.empty());
        assert(path == Lower(path));
        assert(path.find(L'/') == std::wstring::npos);
    }

    std::cout << "GAME_DETECTION_TESTS_OK catalog=" << windowsCatalog.size()
              << " elapsedMs=" << elapsedMs << "\n";
    return 0;
}
