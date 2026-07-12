#include "game_detection.h"

#include <algorithm>
#include <cwctype>
#include <mutex>
#include <utility>

namespace {

constexpr wchar_t kGameCatalogKey[] = L"System\\GameConfigStore\\Children";
constexpr wchar_t kMatchedPathValue[] = L"MatchedExeFullPath";

std::mutex g_catalogMutex;
std::vector<std::wstring> g_cachedCatalog;
bool g_catalogLoaded = false;

std::wstring NormalizePath(std::wstring path)
{
    const auto isSpace = [](wchar_t ch) { return std::iswspace(ch) != 0; };
    while (!path.empty() && isSpace(path.front())) path.erase(path.begin());
    while (!path.empty() && isSpace(path.back())) path.pop_back();
    if (path.size() >= 2 && path.front() == L'"' && path.back() == L'"') {
        path = path.substr(1, path.size() - 2);
    }
    std::transform(path.begin(), path.end(), path.begin(), [](wchar_t ch) {
        if (ch == L'/') return L'\\';
        return static_cast<wchar_t>(std::towlower(ch));
    });
    return path;
}

std::wstring ReadStringValue(HKEY key, const wchar_t *valueName)
{
    DWORD type = 0;
    DWORD byteCount = 0;
    LONG result = RegQueryValueExW(key, valueName, nullptr, &type, nullptr, &byteCount);
    if (result != ERROR_SUCCESS ||
        (type != REG_SZ && type != REG_EXPAND_SZ) ||
        byteCount < sizeof(wchar_t)) {
        return {};
    }

    std::vector<wchar_t> buffer(byteCount / sizeof(wchar_t) + 1, L'\0');
    result = RegQueryValueExW(key, valueName, nullptr, &type,
                              reinterpret_cast<BYTE *>(buffer.data()), &byteCount);
    if (result != ERROR_SUCCESS) return {};

    std::wstring value(buffer.data());
    if (type != REG_EXPAND_SZ || value.empty()) return value;

    DWORD expandedLength = ExpandEnvironmentStringsW(value.c_str(), nullptr, 0);
    if (expandedLength == 0) return value;
    std::vector<wchar_t> expanded(expandedLength, L'\0');
    if (ExpandEnvironmentStringsW(value.c_str(), expanded.data(), expandedLength) == 0) {
        return value;
    }
    return std::wstring(expanded.data());
}

std::vector<std::wstring> CachedCatalog()
{
    std::lock_guard<std::mutex> lock(g_catalogMutex);
    if (!g_catalogLoaded) {
        g_cachedCatalog = GameDetection_LoadWindowsCatalog();
        g_catalogLoaded = true;
    }
    return g_cachedCatalog;
}

} // namespace

std::vector<std::wstring> GameDetection_LoadWindowsCatalog()
{
    HKEY root = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kGameCatalogKey, 0, KEY_READ, &root) !=
        ERROR_SUCCESS) {
        return {};
    }

    DWORD subkeyCount = 0;
    DWORD maxSubkeyLength = 0;
    if (RegQueryInfoKeyW(root, nullptr, nullptr, nullptr, &subkeyCount,
                         &maxSubkeyLength, nullptr, nullptr, nullptr, nullptr,
                         nullptr, nullptr) != ERROR_SUCCESS) {
        RegCloseKey(root);
        return {};
    }

    std::vector<std::wstring> paths;
    std::vector<wchar_t> subkeyName(maxSubkeyLength + 2, L'\0');
    for (DWORD index = 0; index < subkeyCount; ++index) {
        DWORD nameLength = static_cast<DWORD>(subkeyName.size() - 1);
        if (RegEnumKeyExW(root, index, subkeyName.data(), &nameLength, nullptr,
                          nullptr, nullptr, nullptr) != ERROR_SUCCESS) {
            continue;
        }
        subkeyName[nameLength] = L'\0';

        HKEY child = nullptr;
        if (RegOpenKeyExW(root, subkeyName.data(), 0, KEY_READ, &child) != ERROR_SUCCESS) {
            continue;
        }
        std::wstring path = NormalizePath(ReadStringValue(child, kMatchedPathValue));
        RegCloseKey(child);
        if (!path.empty()) paths.push_back(std::move(path));
    }
    RegCloseKey(root);

    std::sort(paths.begin(), paths.end());
    paths.erase(std::unique(paths.begin(), paths.end()), paths.end());
    return paths;
}

bool GameDetection_IsKnownGamePath(
    const std::wstring &executablePath,
    const std::vector<std::wstring> &windowsCatalog)
{
    const std::wstring normalized = NormalizePath(executablePath);
    if (normalized.empty()) return false;
    if (normalized.find(L"\\steamapps\\common\\") != std::wstring::npos) {
        return true;
    }

    return std::any_of(windowsCatalog.begin(), windowsCatalog.end(),
                       [&normalized](const std::wstring &catalogPath) {
                           return normalized == NormalizePath(catalogPath);
                       });
}

bool GameDetection_IsKnownGameProcess(DWORD pid)
{
    if (pid == 0) return false;
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process) return false;

    std::vector<wchar_t> path(32768, L'\0');
    DWORD pathLength = static_cast<DWORD>(path.size());
    const BOOL queried = QueryFullProcessImageNameW(process, 0, path.data(), &pathLength);
    CloseHandle(process);
    if (!queried || pathLength == 0) return false;

    return GameDetection_IsKnownGamePath(
        std::wstring(path.data(), pathLength), CachedCatalog());
}

void GameDetection_RefreshCatalog()
{
    std::vector<std::wstring> catalog = GameDetection_LoadWindowsCatalog();
    std::lock_guard<std::mutex> lock(g_catalogMutex);
    g_cachedCatalog = std::move(catalog);
    g_catalogLoaded = true;
}
