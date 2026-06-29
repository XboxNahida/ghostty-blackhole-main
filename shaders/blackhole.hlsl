// blackhole.hlsl  Schwarzschild black hole pixel shader (D3D11)
// Translated from blackhole.glsl — all physics identical, API different
// GLSL→HLSL: vecN→floatN, texture→.Sample, mix→lerp, uniform→cbuffer
// Cbuffer layout must match BlackHoleUniforms in renderer_interface.h

Texture2D    iChannel0        : register(t0);
SamplerState iChannel0Sampler : register(s0);

cbuffer Params : register(b0) {
    float4 iResolution;                 // offset 0
    float  iTime;                       // offset 16
    float3 _pad0;                       // offset 20 → padding to 32
    float4 iDate;                       // offset 32
    float4 iMouse;                      // offset 48
    float4 iCurrentCursorColor;         // offset 64
    float4 iPreviousCursorColor;        // offset 80
    float  iTimeCursorChange;           // offset 96
    float3 _pad1;                       // offset 100 → padding to 112
    float  holeRadius;                  // offset 112
    float  diskGain;                    // offset 116
    float  diskTemp;                    // offset 120
    float  exposure;                    // offset 124
    float  speed;                       // offset 128
    float  starGain;                    // offset 132
    float  diskIncl;                    // offset 136
    float  _pad2;                       // offset 140 → padding
    int    playMode;                    // offset 144
    float  slotSec;                     // offset 148
    int    presetCount;                 // offset 152
    int    _padEnd;                     // offset 156
    float  presetTemp[64];              // offset 160
    float  presetIncl[64];
    float  presetRoll[64];
    float  presetInner[64];
    float  presetOuter[64];
    float  presetOpac[64];
    float  presetDopp[64];
    float  presetBeam[64];
    float  presetGain[64];
    float  presetContr[64];
    float  presetWind[64];
    float  presetSpeed[64];
    float  presetExpo[64];
    float  presetStar[64];
}

// ── Tunable defaults (used when uniform ≤ 0) ──
static const float HOLE_RADIUS   = 0.0200;
static const float LENS_DEPTH    = 13.0000;
static const float STAR_GAIN     = 0.0000;
static const float DISK_INNER    = 1.8000;
static const float DISK_OUTER    = 8.0000;
static const float DISK_INCL     = 1.5000;
static const float DISK_ROLL     = 0.3500;
static const float DISK_GAIN     = 2.2000;
static const float DISK_OPACITY  = 0.9000;
static const float DISK_TEMP     = 5500.0000;
static const float DOPPLER_MIX   = 0.6000;
static const float DISK_BEAM     = 2.5000;
static const float DISK_SPEED    = 5.0000;
static const float DISK_WIND     = 7.0000;
static const float DISK_CONTRAST = 1.6000;
static const float EXPOSURE      = 1.4000;
static const float DRIFT_SPEED   = 1.0000;
static const float WORK_AREA     = 0.3300;
static const float DILATION_MIN  = 0.2000;
#define B_CRIT 2.5980762
#define N_STEPS 48

// ── Disk look struct ──
struct DiskLook {
    float temp, incl, roll, inner, outer, opac, dopp, beam,
          gain, contr, wind, speed, expo, star;
};

// ── Helper: get disk look from either GUI overrides or preset ──
DiskLook getDiskLook() {
    DiskLook L;
    if (presetCount > 0) {
        float totalSlots = float(presetCount);
        float slotDuration = max(slotSec, 0.1);
        int idx;
        if (playMode == 1) {
            idx = int(fmod(iTime / slotDuration, totalSlots));
        } else if (playMode == 2) {
            float rng = frac(sin(iTime * 0.1 + 17.0) * 43758.5453);
            idx = int(rng * totalSlots);
        } else {
            idx = min(int(iTime / slotDuration), presetCount - 1);
        }
        idx = clamp(idx, 0, presetCount - 1);
        L.temp  = presetTemp[idx];
        L.incl  = presetIncl[idx];
        L.roll  = presetRoll[idx];
        L.inner = presetInner[idx];
        L.outer = presetOuter[idx];
        L.opac  = presetOpac[idx];
        L.dopp  = presetDopp[idx];
        L.beam  = presetBeam[idx];
        L.gain  = presetGain[idx];
        L.contr = presetContr[idx];
        L.wind  = presetWind[idx];
        L.speed = presetSpeed[idx];
        L.expo  = presetExpo[idx];
        L.star  = presetStar[idx];
    } else {
        L.temp  = DISK_TEMP;  L.incl  = DISK_INCL;  L.roll  = DISK_ROLL;
        L.inner = DISK_INNER; L.outer = DISK_OUTER; L.opac  = DISK_OPACITY;
        L.dopp  = DOPPLER_MIX; L.beam = DISK_BEAM;  L.gain  = DISK_GAIN;
        L.contr = DISK_CONTRAST; L.wind = DISK_WIND; L.speed = DISK_SPEED;
        L.expo  = EXPOSURE;  L.star  = STAR_GAIN;
    }
    if (diskTemp  > 0.0) L.temp  = diskTemp;
    if (diskIncl  > 0.0) L.incl  = diskIncl;
    if (diskGain  > 0.0) L.gain  = diskGain;
    if (exposure  > 0.0) L.expo  = exposure;
    if (starGain  > 0.0) L.star  = starGain;
    return L;
}

// ── Hash ──
float hash21(float2 p) {
    p = frac(p * float2(234.34, 435.345));
    p += dot(p, p + 34.23);
    return frac(p.x * p.y);
}

// ── Value noise with Y wrapping ──
float vnoiseWrapY(float2 p, float perY) {
    float2 i = floor(p);
    float2 f = frac(p);
    f = f * f * (3.0 - 2.0 * f);
    float y0 = mod_glsl(i.y, perY);
    float y1 = mod_glsl(i.y + 1.0, perY);
    return lerp(
        lerp(hash21(float2(i.x, y0)), hash21(float2(i.x + 1.0, y0)), f.x),
        lerp(hash21(float2(i.x, y1)), hash21(float2(i.x + 1.0, y1)), f.x),
        f.y);
}

// ── GLSL-compatible mod (HLSL fmod truncates; GLSL mod floors) ──
float mod_glsl(float x, float y) { return x - y * floor(x / y); }

// ── Mirror UV ──
float2 mirrorUV(float2 u) {
    return 1.0 - abs(1.0 - mod_glsl(u, 2.0));
}

// ── 2D Rotation ──
float2 rot2(float2 v, float a) {
    float c = cos(a), s = sin(a);
    return float2(c * v.x - s * v.y, s * v.x + c * v.y);
}

// ── Lissajous wander ──
float2 lissa(float t) {
    return float2(0.75 * sin(t * 0.37) + 0.25 * sin(t * 0.83 + 1.0),
                  0.70 * sin(t * 0.54 + 2.1) + 0.30 * sin(t * 1.07));
}

// ── Blackbody color (Tanner Helland fit) ──
float3 blackbody(float T) {
    float t = clamp(T, 1500.0, 40000.0) / 100.0;
    float r = t <= 66.0 ? 1.0
                        : clamp(1.292936 * pow(t - 60.0, -0.1332047), 0.0, 1.0);
    float g = t <= 66.0 ? clamp(0.3900816 * log(t) - 0.6318414, 0.0, 1.0)
                        : clamp(1.1298909 * pow(t - 60.0, -0.0755148), 0.0, 1.0);
    float b = t >= 66.0 ? 1.0
                        : (t <= 19.0 ? 0.0
                                     : clamp(0.5432068 * log(t - 10.0) - 1.1962540, 0.0, 1.0));
    return float3(r, g, b);
}

// ── Procedural starfield ──
float3 stars(float3 d) {
    float2 sph = float2(atan2(d.x, -d.z), asin(clamp(d.y, -1.0, 1.0)));
    float2 g   = sph * 40.0;
    float2 id  = floor(g);
    float  h   = hash21(id);
    if (h < 0.92) return float3(0.0, 0.0, 0.0);
    float2 f   = frac(g) - 0.5;
    float2 off = (float2(hash21(id + 17.3), hash21(id + 31.7)) - 0.5) * 0.7;
    float  spark = smoothstep(0.10, 0.0, length(f - off));
    float  tw    = 0.7 + 0.3 * sin(iTime * (0.5 + 2.0 * hash21(id + 5.1)) + 40.0 * h);
    float3 tint  = lerp(float3(1.0, 0.82, 0.60), float3(0.75, 0.85, 1.0), hash21(id + 2.9));
    return tint * spark * tw * ((h - 0.92) / 0.08);
}

// ── Pixel Shader entry ──
struct PSInput {
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
};

float4 main(PSInput input) : SV_TARGET {
    float2 res    = iResolution.xy;
    float2 uv     = input.uv;
    float  aspect = res.x / res.y;
    float  yUp    = 1.0 - uv.y;  // uv.y: 0=top 1=bottom, yUp: 1=top 0=bottom
    float  t      = iTime * DRIFT_SPEED;

    DiskLook L = getDiskLook();
    float rin  = max(L.inner, 1.6);
    float rout = max(L.outer, rin + 0.5);

    float useHoleR = holeRadius > 0.0 ? holeRadius : HOLE_RADIUS;
    float rh = useHoleR;
    float I  = 1.0;
    float dil = lerp(1.0, DILATION_MIN, I);
    float vis = 1.0;

    float2 center = float2(0.5, 0.5)
                  + float2(0.08 * sin(t * 0.21) + 0.03 * sin(t * 0.083),
                           0.06 * sin(t * 0.157 + 2.0) + 0.03 * sin(t * 0.117));

    float shield = vis * smoothstep(WORK_AREA, WORK_AREA + 0.18, yUp);
    float2 p     = (uv - center) * float2(aspect, 1.0);
    float  plen  = length(p);
    float  W     = B_CRIT / max(rh, 1e-4);
    float2 pr    = rot2(float2(p.x, p.y), L.roll) * W;
    float  b     = length(pr);
    float  window = exp(-pow(plen / (7.0 * rh), 2.0));
    float  bmax  = rout + 3.0;
    float  Z0    = max(14.0, rout + 5.0);

    // ═══ Far field: weak deflection ═══
    if (b >= bmax) {
        float u    = Z0 * rsqrt(Z0 * Z0 + b * b);
        float defl = (2.0 / (W * W)) / max(plen, 1e-4)
                   * (1.29 * u + 0.07) * max(LENS_DEPTH - 2.14 * u + 0.75, 0.0)
                   * window * shield;
        float2 dir = p / max(plen, 1e-5);
        float3 term;
        float ab = 0.035 * smoothstep(1.0, 2.0, b / bmax);
        for (int i = 0; i < 3; i++) {
            float k   = 1.0 + (float(i) - 1.0) * ab;
            float2 sp = p - dir * defl * k;
            float2 suv = mirrorUV(center + sp / float2(aspect, 1.0));
            term[i] = iChannel0.Sample(iChannel0Sampler, suv)[i];
        }
        float3 d = normalize(float3(-(pr / b) * (2.0 / b), -1.0));
        return float4(term + stars(d) * L.star * window * shield, 1.0);
    }

    // ═══ Near field: trace geodesic ═══
    float3 x  = float3(pr, Z0);
    float3 v  = float3(0.0, 0.0, -1.0);
    float  h2 = dot(pr, pr);
    float  ci = cos(L.incl), si = sin(L.incl);
    float3 n  = float3(0.0, si, ci);
    float3 e2 = float3(0.0, ci, -si);
    float  sdir = L.speed < 0.0 ? -1.0 : 1.0;
    float  spd  = abs(L.speed);

    float3 emitc    = float3(0.0, 0.0, 0.0);
    float  trans    = 1.0;
    bool   captured = false;
    float  sPrev    = dot(x, n);
    float3 xPrev    = x;

    [unroll(48)]
    for (int i = 0; i < N_STEPS; i++) {
        float r2 = dot(x, x);
        if (r2 < 1.0) { captured = true; break; }
        if (x.z < -Z0 && v.z < 0.0) break;
        if (r2 > 4.0 * Z0 * Z0) break;
        float r  = sqrt(r2);
        float dt = clamp(0.16 * r, 0.03, 1.5);

        float3 a = -1.5 * h2 * x / (r2 * r2 * r);
        v += a * (0.5 * dt);
        x += v * dt;
        r2 = dot(x, x);
        r  = sqrt(r2);
        a  = -1.5 * h2 * x / (r2 * r2 * r);
        v += a * (0.5 * dt);

        float s = dot(x, n);
        if (s * sPrev < 0.0 && trans > 0.02) {
            float tc = sPrev / (sPrev - s);
            float3 xc = lerp(xPrev, x, tc);
            float  rc = length(xc);
            if (rc > rin && rc < rout) {
                float band = smoothstep(rin, rin * 1.25, rc)
                           * (1.0 - smoothstep(rout * 0.70, rout, rc));
                float phi   = atan2(dot(xc, e2), xc.x);
                float turns = phi / 6.2831853;
                float kep   = pow(rin / rc, 1.5);
                float gloc  = sqrt(max(1.0 - 1.5 / rc, 0.02));
                float swirl = rc * L.wind * 0.12 - t * kep * spd * gloc * dil * sdir;
                float streaks = vnoiseWrapY(float2(rc * 2.8, turns * 19.0 + swirl * 3.0), 19.0) * 0.65
                              + vnoiseWrapY(float2(rc * 1.0, turns * 9.0  + swirl * 1.5 + 7.0), 9.0) * 0.35;
                streaks = 0.35 + L.contr * streaks * streaks;

                float3 gasdir = normalize(cross(n, xc)) * sdir;
                float  beta   = clamp(rsqrt(max(2.0 * (rc - 1.0), 0.2)), 0.0, 0.99);
                float  g      = gloc / max(1.0 + beta * dot(gasdir, normalize(v)), 0.05);
                g = lerp(1.0, g, L.dopp);

                float xpr   = max(1.0 - sqrt(rin / rc), 0.0);
                float tprof = pow(rin / rc, 0.75) * pow(xpr, 0.25) / 0.488;
                float3 cbb  = blackbody(L.temp * tprof * g);
                float boost = pow(g, L.beam);

                float density = band * streaks;
                emitc += trans * cbb * (L.gain * 2.2 * density * tprof * tprof * boost);
                trans *= 1.0 - clamp(L.opac * density, 0.0, 1.0);
            }
        }
        sPrev = s;
        xPrev = x;
    }
    if (!captured && dot(x, x) < 4.0) captured = true;

    // ── Background ──
    float3 bg = float3(0.0, 0.0, 0.0);
    if (!captured) {
        float3 d = normalize(v);
        bg += stars(d) * L.star * window * shield;
        if (d.z < -0.05) {
            float tpl = (-LENS_DEPTH - x.z) / d.z;
            float3 hp = x + d * tpl;
            float2 q  = rot2(hp.xy, -L.roll) / W;
            float2 sp = float2(q.x, -q.y);  // world y-up → screen y-down
            float2 suv = mirrorUV(center + (p + (sp - p) * window * shield) / float2(aspect, 1.0));
            float toward = smoothstep(0.05, 0.35, -d.z);
            bg += iChannel0.Sample(iChannel0Sampler, suv).rgb * toward;
        }
    }

    float3 col = bg * trans + (float3(1.0, 1.0, 1.0) - exp(-emitc * L.expo));
    return float4(col, 1.0);
}
