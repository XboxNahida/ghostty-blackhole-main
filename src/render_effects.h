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

inline float ClampEffectNumber(float value, float fallback, float low, float high) {
    return std::isfinite(value) ? std::clamp(value,low,high) : fallback;
}
inline float ParseEffectNumber(const char* text, float fallback, float low, float high) {
    if (!text) return fallback;
    char* end=nullptr;
    const float value=std::strtof(text,&end);
    if (end==text) return fallback;
    while (*end && std::isspace(static_cast<unsigned char>(*end))) ++end;
    return *end ? fallback : ClampEffectNumber(value,fallback,low,high);
}
struct RippleSettings {
    float amplitude=1;
    int oscillations=3;
    int spread=0;
};
inline int ParseEffectInteger(const char* text, int fallback, int low, int high) {
    const float value=ParseEffectNumber(text,static_cast<float>(fallback),static_cast<float>(low),static_cast<float>(high));
    return std::floor(value)==value ? static_cast<int>(value) : fallback;
}

struct EmergenceSchedule {
    bool active=false;
    double started=0, next=0;
    void start(double now) { active=true; started=now; }
    bool ready(double now) const { return !active && now>=next; }
    void finish(double now, float minimum, float maximum, float roll) {
        minimum=ClampEffectNumber(minimum,10,1,300);
        maximum=ClampEffectNumber(maximum,25,1,300);
        if (minimum>maximum) std::swap(minimum,maximum);
        active=false;
        next=now+minimum+(maximum-minimum)*ClampEffectNumber(roll,0.5f,0,1);
    }
};

struct EmergenceFrame { float scale = 0; bool finished = false; };
inline EmergenceFrame EmergenceFrameAt(double seconds) {
    if (!std::isfinite(seconds)) return {0, true};
    auto ease = [](double t) {
        const float x = static_cast<float>(std::clamp(t, 0.0, 1.0));
        return x*x*x*(x*(x*6.0f-15.0f)+10.0f);
    };
    const float scale = seconds < 1.5 ? ease((seconds-0.3)/1.2)
        : 1.0f-ease((seconds-9.5)/1.2);
    return {scale, seconds >= 10.7};
}

// direction：0 为鼠标波纹，1 为出场外扩，-1 为退场内缩。
struct RippleEvent {
    float x = 0, y = 0;
    double born = 0;
    float strength = 1;
    int direction = 0;
    int oscillations = 0; // 0 保留旧事件公式，点击配置为 1～6。
    int spread = 0;
};
struct RippleState {
    static constexpr int capacity = 16;
    static constexpr double lifetime = 2.4;
    static constexpr double directionalLifetime = 1.2;
    std::array<RippleEvent, capacity> events{};
    int count = 0;
    void clear() { count = 0; }
    void expire(double now) {
        int kept = 0;
        for (int i=0;i<count;++i)
            if (now >= events[i].born && now < events[i].born+(events[i].direction ? directionalLifetime : lifetime)) events[kept++] = events[i];
        count = kept;
    }
    void add(float x, float y, double now, float strength = 1, int direction = 0) {
        if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(now) || !std::isfinite(strength)) return;
        expire(now);
        if (count == capacity) {
            for (int i=1;i<count;++i) events[i-1] = events[i];
            --count;
        }
        events[count++] = {x,y,now,std::clamp(strength,-1.0f,1.0f),std::clamp(direction,-1,1)};
    }
    void addClick(float x, float y, double now, const RippleSettings& settings) {
        const float amplitude=ClampEffectNumber(settings.amplitude,1,0,2);
        if (amplitude==0 || !std::isfinite(x) || !std::isfinite(y) || !std::isfinite(now)) return;
        add(x,y,now);
        auto& event=events[count-1];
        event.strength=amplitude;
        event.oscillations=std::clamp(settings.oscillations,1,6);
        event.spread=NormalizeEffectMode(settings.spread);
    }
};
