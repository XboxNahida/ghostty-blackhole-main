#include "frame_limiter.h"

#include <cmath>
#include <cstdlib>

namespace {

void require(bool condition)
{
    if (!condition) std::abort();
}

void requireNear(double actual, double expected)
{
    require(std::fabs(actual - expected) < 0.000001);
}

} // namespace

int main()
{
    require(NormalizeFrameRateLimit(0) == 0);
    require(NormalizeFrameRateLimit(1) == 30);
    require(NormalizeFrameRateLimit(30) == 30);
    require(NormalizeFrameRateLimit(60) == 60);
    require(NormalizeFrameRateLimit(240) == 240);
    require(NormalizeFrameRateLimit(241) == 240);

    const FrameDeadlineDecision unlimited = AdvanceFrameDeadline(0, 10.0, 8.0);
    require(!unlimited.shouldWait);
    requireNear(unlimited.nextDeadlineSeconds, 10.0);

    const FrameDeadlineDecision first = AdvanceFrameDeadline(60, 10.0, 0.0);
    require(first.shouldWait);
    requireNear(first.nextDeadlineSeconds, 10.0 + 1.0 / 60.0);

    const FrameDeadlineDecision steady = AdvanceFrameDeadline(
        60, 10.016, first.nextDeadlineSeconds);
    require(steady.shouldWait);
    requireNear(steady.nextDeadlineSeconds, first.nextDeadlineSeconds + 1.0 / 60.0);

    const FrameDeadlineDecision late = AdvanceFrameDeadline(60, 11.0, 10.02);
    require(late.shouldWait);
    requireNear(late.nextDeadlineSeconds, 11.0 + 1.0 / 60.0);
    return 0;
}
