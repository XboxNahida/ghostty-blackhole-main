#include "../src/render_effects.h"
#include <cstdio>
#include <limits>
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); return 1; } } while (0)
int main() {
    CHECK(ParseEffectMode("2") == 2);
    CHECK(ParseEffectMode("3") == 0);
    CHECK(ParseEffectMode("-1") == 0);
    CHECK(ParseEffectMode("1.5") == 0);
    CHECK(ParseEffectMode("nan") == 0);
    CHECK(!RippleEnabled(0, true));
    CHECK(RippleEnabled(1, false));
    CHECK(!RippleEnabled(2, false));
    CHECK(RippleEnabled(2, true));
    RippleState state;
    state.add(120, 300, 10.0);
    state.add(600, 20, 10.1);
    state.expire(10.2);
    CHECK(state.count == 2 && state.events[0].x == 120);
    CHECK(state.events[1].y == 20);
    for (int i=0;i<100;++i) state.add(static_cast<float>(i), 0, 10.2);
    CHECK(state.count == RippleState::capacity);
    CHECK(state.events[state.count-1].x == 99);
    state.expire(20.0);
    CHECK(state.count == 0);
    state.add(1, 2, std::numeric_limits<double>::quiet_NaN());
    CHECK(state.count == 0);
    std::puts("render effects state PASS");
}
