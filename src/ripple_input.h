#pragma once
#include "render_effects.h"
#include <windows.h>

// The low-level hook is serviced on the renderer's message thread. Its callback
// only enqueues coordinates; rendering never runs inside the hook.
class RippleInput {
public:
    RippleInput() = default;
    ~RippleInput() { enable(false); }
    RippleInput(const RippleInput&) = delete;
    RippleInput& operator=(const RippleInput&) = delete;
    bool enable(bool active) {
        if (!active) {
            if (hook_) UnhookWindowsHookEx(hook_);
            hook_ = nullptr; pending_ = 0;
            if (owner_ == this) owner_ = nullptr;
            return true;
        }
        if (hook_) return true;
        owner_ = this;
        // Low-level hooks are process-global; a null module handle is required
        // for the callback that lives in this executable on Windows.
        hook_ = SetWindowsHookExW(WH_MOUSE_LL, callback, nullptr, 0);
        if (!hook_) owner_ = nullptr;
        return hook_ != nullptr;
    }
    void drain(RippleState& state, int x, int y, int width, int height, double now) {
        for (int i=0;i<pending_;++i) {
            const float nx = static_cast<float>(points_[i].x-x)/std::max(width,1);
            const float ny = static_cast<float>(points_[i].y-y)/std::max(height,1);
            if (nx>=0 && nx<=1 && ny>=0 && ny<=1) state.add(nx,1.0f-ny,now);
        }
        pending_ = 0;
    }
private:
    static LRESULT CALLBACK callback(int code, WPARAM w, LPARAM l) {
        if (code == HC_ACTION && owner_ && (w == WM_LBUTTONDOWN || w == WM_RBUTTONDOWN)) {
            const auto* mouse = reinterpret_cast<const MSLLHOOKSTRUCT*>(l);
            if (owner_->pending_ < RippleState::capacity) owner_->points_[owner_->pending_++] = mouse->pt;
        }
        return CallNextHookEx(nullptr, code, w, l);
    }
    inline static RippleInput* owner_ = nullptr;
    HHOOK hook_ = nullptr;
    POINT points_[RippleState::capacity]{};
    int pending_ = 0;
};
