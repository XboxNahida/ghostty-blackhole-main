#include "consumption_state.h"
#include <cmath>
#include <cstdio>
#include <limits>

#define CHECK(condition) do { if (!(condition)) { std::fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition); return 1; } } while (false)
int main() {
    CHECK(ConsumptionMovementSpeed(1.0f, false) == 1.0f);
    CHECK(ConsumptionMovementSpeed(1.0f, true) == 0.3f);
    CHECK(ConsumptionMovementSpeed(0.0f, true) == 0.0f);
    CHECK(ConsumptionMovementSpeed(0.1f, true) == 0.1f);
    ConsumptionState state;
    state.advance(-1.0);
    state.advance(std::numeric_limits<double>::quiet_NaN());
    CHECK(state.elapsed() == 0.0f);
    CHECK(state.desktopProgress() == 0.0f && state.formulaMix() == 0.0f);
    state.advance(30.0);
    CHECK(std::abs(state.desktopProgress() - 0.5f) < 0.0001f);
    CHECK(state.formulaMix() == 0.0f);
    state.advance(30.0);
    CHECK(state.desktopProgress() == 1.0f && state.formulaMix() == 0.0f);
    state.advance(12.0);
    CHECK(state.formulaMix() == 0.0f);
    state.advance(4.0);
    CHECK(state.formulaMix() > 0.0f && state.formulaMix() < 1.0f);
    state.advance(10000.0);
    CHECK(state.desktopProgress() == 1.0f && state.formulaMix() == 1.0f);
    state.advance(-20000.0);
    CHECK(state.desktopProgress() == 1.0f && state.formulaMix() == 1.0f);
    std::puts("CONSUMPTION_STATE_OK");
}
