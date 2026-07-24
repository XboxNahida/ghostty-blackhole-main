#pragma once

#include <windows.h>

#include <cstddef>
#include <string>

std::string RendererInstanceMutexName(int screenIndex);
bool IsValidRendererChildScreenIndex(int screenIndex, std::size_t monitorCount);

class RendererInstanceLock {
public:
    RendererInstanceLock() = default;
    explicit RendererInstanceLock(int screenIndex);
    ~RendererInstanceLock();

    RendererInstanceLock(const RendererInstanceLock&) = delete;
    RendererInstanceLock& operator=(const RendererInstanceLock&) = delete;

    bool acquire(int screenIndex);
    bool acquired() const;

private:
    HANDLE handle_ = nullptr;
};
