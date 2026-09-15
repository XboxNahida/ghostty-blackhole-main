#pragma once
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cctype>

inline int NormalizeEffectMode(int value) { return value >= 0 && value <= 2 ? value : 0; }
inline int ParseEffectMode(const char* text) {
    if (!text) return 0;
    char* end = nullptr;
    const long value = std::strtol(text, &end, 10);
    if (end == text) return 0;
    while (*end && std::isspace(static_cast<unsigned char>(*end))) ++end;
    return !*end && value >= 0 && value <= 2 ? static_cast<int>(value) : 0;
}
inline bool RippleEnabled(int mode, bool holeVisible) {
    return mode == 1 || (mode == 2 && holeVisible);
}

struct RippleEvent { float x = 0, y = 0; double born = 0; };
struct RippleState {
    static constexpr int capacity = 16;
    static constexpr double lifetime = 2.4;
    std::array<RippleEvent, capacity> events{};
    int count = 0;
    void clear() { count = 0; }
    void expire(double now) {
        int kept = 0;
        for (int i=0;i<count;++i)
            if (now >= events[i].born && now-events[i].born < lifetime) events[kept++] = events[i];
        count = kept;
    }
    void add(float x, float y, double now) {
        if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(now)) return;
        expire(now);
        if (count == capacity) {
            for (int i=1;i<count;++i) events[i-1] = events[i];
            --count;
        }
        events[count++] = {x,y,now};
    }
};
