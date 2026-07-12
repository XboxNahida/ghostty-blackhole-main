#pragma once

#include <string>
#include <vector>
#include <windows.h>

std::vector<std::wstring> GameDetection_LoadWindowsCatalog();

bool GameDetection_IsKnownGamePath(
    const std::wstring &executablePath,
    const std::vector<std::wstring> &windowsCatalog);

bool GameDetection_IsKnownGameProcess(DWORD pid);

void GameDetection_RefreshCatalog();
