#include "renderer_instance.h"

#include <cstdio>

std::string RendererInstanceMutexName(int screenIndex) {
    if (screenIndex < 0) {
        return "Local\\BlakholeRendererDefault";
    }

    char name[64];
    std::snprintf(
        name, sizeof(name), "Local\\BlakholeRendererScreen%d", screenIndex);
    return name;
}

bool IsValidRendererChildScreenIndex(
    int screenIndex, std::size_t monitorCount) {
    return screenIndex >= 1
        && static_cast<std::size_t>(screenIndex) < monitorCount;
}

RendererInstanceLock::RendererInstanceLock(int screenIndex) {
    acquire(screenIndex);
}

RendererInstanceLock::~RendererInstanceLock() {
    if (handle_) {
        CloseHandle(handle_);
        handle_ = nullptr;
    }
}

bool RendererInstanceLock::acquire(int screenIndex) {
    if (handle_) return true;

    const std::string name = RendererInstanceMutexName(screenIndex);
    SetLastError(ERROR_SUCCESS);
    HANDLE handle = CreateMutexA(nullptr, TRUE, name.c_str());
    if (!handle) return false;

    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(handle);
        return false;
    }

    handle_ = handle;
    return true;
}

bool RendererInstanceLock::acquired() const {
    return handle_ != nullptr;
}
