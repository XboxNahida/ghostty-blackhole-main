#include "../src/render_effects.h"
#include <cstdio>
#include <limits>
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); return 1; } } while (0)
int main() {
    CHECK(ParseEffectNumber("nan",1,0,2)==1);
    CHECK(ParseEffectNumber("9",1,0,2)==2);
    CHECK(ParseEffectNumber("1.5junk",1,0,2)==1);
    RippleSettings click; click.amplitude=1.5f; click.oscillations=1; click.spread=2;
    RippleState configured; configured.addClick(0.4f,0.3f,0,click);
    CHECK(configured.count==1 && configured.events[0].strength==1.5f);
    CHECK(configured.events[0].oscillations==1 && configured.events[0].spread==2);
    click.amplitude=0;
    CHECK(configured.events[0].strength==1.5f);
    configured.addClick(0.4f,0.3f,0,click); CHECK(configured.count==1);
    EmergenceSchedule schedule; schedule.start(0);
    CHECK(schedule.active && schedule.started==0);
    schedule.finish(10.7,10,25,0.5f);
    CHECK(!schedule.active && !schedule.ready(28.19) && schedule.ready(28.2));
    schedule.start(28.2); CHECK(schedule.active && schedule.started==28.2);
    schedule.finish(38.9,25,10,1); CHECK(schedule.next==63.9);
    CHECK(EmergenceFrameAt(0).scale == 0);
    CHECK(EmergenceFrameAt(0.3).scale == 0);
    CHECK(EmergenceFrameAt(0.9).scale > 0.49f && EmergenceFrameAt(0.9).scale < 0.51f);
    CHECK(EmergenceFrameAt(1.5).scale == 1);
    CHECK(EmergenceFrameAt(9.5).scale == 1);
    CHECK(EmergenceFrameAt(10.1).scale > 0.49f && EmergenceFrameAt(10.1).scale < 0.51f);
    CHECK(EmergenceFrameAt(10.7).scale == 0);
    CHECK(!EmergenceFrameAt(10.69).finished);
    CHECK(EmergenceFrameAt(10.7).finished);
    for (int i=0;i<120;++i) {
        const auto frame = EmergenceFrameAt(i*0.12);
        CHECK(frame.scale >= 0 && frame.scale <= 1);
    }
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
    state.add(0.5f,0.5f,20,-0.25f);
    CHECK(state.events[0].strength == -0.25f);
    state.clear();
    state.add(0.5f,0.5f,20,0.55f,1);
    CHECK(state.events[0].direction==1);
    state.expire(21.19);
    CHECK(state.count==1);
    state.expire(21.2);
    CHECK(state.count==0);
    state.add(0.5f,0.5f,30,0.4f,-1);
    CHECK(state.events[0].direction==-1);
    state.expire(31.2);
    CHECK(state.count==0);
    state.add(1, 2, std::numeric_limits<double>::quiet_NaN());
    CHECK(state.count == 0);
    std::puts("render effects state PASS");
}
