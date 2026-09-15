#pragma once

#include <algorithm>
#include <cmath>

// 消费时钟与黑洞位置解耦：移开黑洞不能重新生成已吞掉的桌面。
class ConsumptionState {
public:
    void advance(double seconds) {
        if (std::isfinite(seconds) && seconds > 0.0)
            seconds_ = std::min(seconds_ + seconds, 86400.0);
    }
    float elapsed() const { return static_cast<float>(seconds_); }
    float desktopProgress() const {
        return static_cast<float>(std::min(seconds_ / 60.0, 1.0));
    }
    float formulaMix() const {
        const float x = static_cast<float>(std::clamp((seconds_ - 72.0) / 8.0, 0.0, 1.0));
        return x * x * (3.0f - 2.0f * x);
    }
private:
    double seconds_ = 0.0;
};

inline float ConsumptionMovementSpeed(float speed, bool enabled) {
    const float safeSpeed = std::isfinite(speed) ? std::max(speed, 0.0f) : 0.0f;
    return enabled ? std::min(safeSpeed, 0.3f) : safeSpeed;
}
