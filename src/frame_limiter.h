#pragma once

#include <windows.h>

inline constexpr int kDefaultFrameRateLimit = 60;
inline constexpr int kMinimumFrameRateLimit = 30;
inline constexpr int kMaximumFrameRateLimit = 240;

namespace frame_limiter_detail {

constexpr bool IsSuccessfulTimerWait(DWORD waitResult)
{
    return waitResult == WAIT_OBJECT_0;
}

} // namespace frame_limiter_detail

int NormalizeFrameRateLimit(int value);
int ParseFrameRateLimitText(const char *text);

struct FrameDeadlineDecision {
    bool shouldWait = false;
    double nextDeadlineSeconds = 0.0;
};

FrameDeadlineDecision AdvanceFrameDeadline(int frameRateLimit,
                                           double nowSeconds,
                                           double previousDeadlineSeconds);

class FrameLimiter {
public:
    explicit FrameLimiter(int frameRateLimit);
    ~FrameLimiter();
    FrameLimiter(const FrameLimiter &) = delete;
    FrameLimiter &operator=(const FrameLimiter &) = delete;

    void setFrameRateLimit(int frameRateLimit);
    int frameRateLimit() const;
    void reset();
    void wait();

private:
    static double nowSeconds();
    void waitUntil(double deadlineSeconds);

    int m_frameRateLimit = kDefaultFrameRateLimit;
    double m_nextDeadlineSeconds = 0.0;
    HANDLE m_waitableTimer = nullptr;
};
