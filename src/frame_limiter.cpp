#include "frame_limiter.h"

#include <algorithm>
#include <cerrno>
#include <cctype>
#include <climits>
#include <cmath>
#include <cstdlib>

namespace {

constexpr DWORD kCreateWaitableTimerHighResolution = 0x00000002;
constexpr double kHundredNanosecondsPerSecond = 10000000.0;

} // namespace

int NormalizeFrameRateLimit(int value)
{
    if (value == 0) return 0;
    return std::clamp(value, kMinimumFrameRateLimit, kMaximumFrameRateLimit);
}

int ParseFrameRateLimitText(const char *text)
{
    if (!text) return kDefaultFrameRateLimit;

    errno = 0;
    char *end = nullptr;
    const long long value = std::strtoll(text, &end, 10);
    if (end == text) return kDefaultFrameRateLimit;

    while (*end != '\0' && std::isspace(static_cast<unsigned char>(*end))) ++end;
    if (*end != '\0') return kDefaultFrameRateLimit;

    if (errno == ERANGE) return value == LLONG_MIN
        ? kMinimumFrameRateLimit
        : kMaximumFrameRateLimit;
    if (errno != 0) return kDefaultFrameRateLimit;
    if (value == 0) return 0;
    if (value < kMinimumFrameRateLimit) return kMinimumFrameRateLimit;
    if (value > kMaximumFrameRateLimit) return kMaximumFrameRateLimit;
    return static_cast<int>(value);
}

FrameDeadlineDecision AdvanceFrameDeadline(int frameRateLimit,
                                           double nowSeconds,
                                           double previousDeadlineSeconds)
{
    const int normalizedLimit = NormalizeFrameRateLimit(frameRateLimit);
    if (normalizedLimit == 0) return {false, nowSeconds};

    const double framePeriodSeconds = 1.0 / static_cast<double>(normalizedLimit);
    const bool invalidDeadline = !std::isfinite(previousDeadlineSeconds)
        || previousDeadlineSeconds <= 0.0;
    const bool missedByMoreThanOnePeriod =
        nowSeconds - previousDeadlineSeconds > framePeriodSeconds;

    if (invalidDeadline || missedByMoreThanOnePeriod) {
        return {true, nowSeconds + framePeriodSeconds};
    }
    return {true, previousDeadlineSeconds + framePeriodSeconds};
}

FrameLimiter::FrameLimiter(int frameRateLimit)
    : m_frameRateLimit(NormalizeFrameRateLimit(frameRateLimit))
{
    m_waitableTimer = CreateWaitableTimerExW(
        nullptr, nullptr, kCreateWaitableTimerHighResolution, TIMER_ALL_ACCESS);
    if (!m_waitableTimer) {
        m_waitableTimer = CreateWaitableTimerW(nullptr, TRUE, nullptr);
    }
}

FrameLimiter::~FrameLimiter()
{
    if (m_waitableTimer) CloseHandle(m_waitableTimer);
}

void FrameLimiter::setFrameRateLimit(int frameRateLimit)
{
    const int normalizedLimit = NormalizeFrameRateLimit(frameRateLimit);
    if (normalizedLimit == m_frameRateLimit) return;
    m_frameRateLimit = normalizedLimit;
    reset();
}

int FrameLimiter::frameRateLimit() const
{
    return m_frameRateLimit;
}

void FrameLimiter::reset()
{
    m_nextDeadlineSeconds = 0.0;
}

void FrameLimiter::wait()
{
    const double currentSeconds = nowSeconds();
    const FrameDeadlineDecision decision = AdvanceFrameDeadline(
        m_frameRateLimit, currentSeconds, m_nextDeadlineSeconds);
    m_nextDeadlineSeconds = decision.nextDeadlineSeconds;
    if (decision.shouldWait) waitUntil(decision.nextDeadlineSeconds);
}

double FrameLimiter::nowSeconds()
{
    LARGE_INTEGER frequency = {};
    LARGE_INTEGER counter = {};
    if (!QueryPerformanceFrequency(&frequency) || frequency.QuadPart <= 0
        || !QueryPerformanceCounter(&counter)) {
        return static_cast<double>(GetTickCount64()) / 1000.0;
    }
    return static_cast<double>(counter.QuadPart)
        / static_cast<double>(frequency.QuadPart);
}

void FrameLimiter::waitUntil(double deadlineSeconds)
{
    double remainingSeconds = deadlineSeconds - nowSeconds();
    if (remainingSeconds <= 0.0) return;

    if (m_waitableTimer) {
        LARGE_INTEGER dueTime = {};
        dueTime.QuadPart = -std::max<LONGLONG>(
            1, static_cast<LONGLONG>(remainingSeconds * kHundredNanosecondsPerSecond));
        if (SetWaitableTimer(m_waitableTimer, &dueTime, 0, nullptr, nullptr, FALSE)) {
            const DWORD waitResult = WaitForSingleObject(m_waitableTimer, INFINITE);
            if (frame_limiter_detail::IsSuccessfulTimerWait(waitResult)) return;
        }
    }

    while (nowSeconds() < deadlineSeconds) Sleep(1);
}
