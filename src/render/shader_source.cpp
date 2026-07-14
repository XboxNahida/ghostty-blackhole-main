#include "shader_source.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstring>
#include <utility>

std::string readFile(const char* path) {
    std::ifstream f(path, std::ios::in | std::ios::binary);
    if (!f) { fprintf(stderr, "Cannot open %s\n", path); return ""; }
    std::stringstream ss; ss << f.rdbuf();
    std::string content = ss.str();

    if (content.size() >= 3) {
        unsigned char c0 = static_cast<unsigned char>(content[0]);
        unsigned char c1 = static_cast<unsigned char>(content[1]);
        unsigned char c2 = static_cast<unsigned char>(content[2]);
        if (c0 == 0xEF && c1 == 0xBB && c2 == 0xBF)
            content = content.substr(3);
    }

    content.erase(std::remove(content.begin(), content.end(), '\r'), content.end());
    return content;
}

bool applyPatches(std::string& body, const ShaderPatch* patches, int count,
                  FILE* debugLog)
{
    bool allOk = true;
    for (int i = 0; i < count; i++) {
        const ShaderPatch& p = patches[i];
        size_t pos = body.find(p.anchor);
        if (pos == std::string::npos) {
            if (debugLog) {
                fprintf(debugLog, "[PATCH] %s: anchor not found\n", p.description);
                fflush(debugLog);
            }
            if (p.critical) {
                if (debugLog) {
                    fprintf(debugLog, "[PATCH] %s: CRITICAL \u2014 returning failure\n", p.description);
                    fflush(debugLog);
                }
                allOk = false;
            }
            continue;
        }
        body.replace(pos, strlen(p.anchor), p.replacement);
        if (debugLog) {
            fprintf(debugLog, "[PATCH] %s: applied (%zu -> %zu)\n",
                    p.description, pos, pos + strlen(p.replacement));
        }
    }
    if (debugLog) fflush(debugLog);
    return allOk;
}

static bool findConstFloatDecl(const std::string& body, const char* name,
                               size_t& pos, size_t& valuePos, size_t& end,
                               const char* desc, bool critical, FILE* debugLog)
{
    const std::string pattern = std::string("const float ") + name;
    pos = body.find(pattern);
    if (pos == std::string::npos) {
        if (debugLog) {
            fprintf(debugLog, "[%s] %s: declaration not found\n",
                    critical ? "FAIL" : "PATCH", desc);
            fflush(debugLog);
        }
        return !critical;
    }

    size_t equals = pos + pattern.size();
    while (equals < body.size() && (body[equals] == ' ' || body[equals] == '\t'))
        ++equals;
    if (equals >= body.size() || body[equals] != '=') {
        if (debugLog) {
            fprintf(debugLog, "[FAIL] Assignment operator not found for '%s'\n", desc);
            fflush(debugLog);
        }
        return false;
    }
    valuePos = equals + 1;
    while (valuePos < body.size() && (body[valuePos] == ' ' || body[valuePos] == '\t'))
        ++valuePos;
    end = body.find(';', valuePos);
    if (end == std::string::npos) {
        if (debugLog) {
            fprintf(debugLog, "[FAIL] Semicolon not found for '%s'\n", desc);
            fflush(debugLog);
        }
        return false;
    }
    return true;
}

/// Whitespace-tolerant, semicolon-aware replacement for a const float declaration.
static bool replaceConstFloatDecl(std::string& body, const char* name,
                                  const std::string& newDecl, const char* desc,
                                  bool critical, FILE* debugLog)
{
    size_t pos = 0, valuePos = 0, end = 0;
    if (!findConstFloatDecl(body, name, pos, valuePos, end, desc, critical, debugLog))
        return false;
    if (pos == std::string::npos)
        return true;
    body.replace(pos, end - pos + 1, newDecl);
    if (debugLog) {
        fprintf(debugLog, "[OK] %s: replaced at %zu\n", desc, pos);
        fflush(debugLog);
    }
    return true;
}

bool buildFragmentShaderFromSources(const std::string& header,
                                    const std::string& body,
                                    std::string& out,
                                    FILE* debugLog)
{
    out.clear();
    if (header.empty() || body.empty()) {
        if (debugLog) { fprintf(debugLog, "[FAIL] Shader source empty: header=%zu, body=%zu\n", header.size(), body.size()); fflush(debugLog); }
        return false;
    }

    std::string h = header;
    std::string b = body;

    // ---- CRITICAL: uMovementTime uniform injection in header ----
    {
        const std::string anchor = "uniform float iTime;";
        size_t pos = h.find(anchor);
        if (pos == std::string::npos) {
            if (debugLog) { fprintf(debugLog, "[FAIL] Critical anchor 'uniform float iTime;' not found\n"); fflush(debugLog); }
            return false;
        }
        h.insert(pos + anchor.length(), "\nuniform float uMovementTime;");
        if (debugLog) { fprintf(debugLog, "[OK] uMovementTime uniform injected\n"); fflush(debugLog); }
    }

    // ---- CRITICAL: moveT declaration ----
    {
        const std::string anchor = "float t = iTime * DRIFT_SPEED;";
        size_t pos = b.find(anchor);
        if (pos == std::string::npos) {
            if (debugLog) { fprintf(debugLog, "[FAIL] Critical anchor 'float t = iTime * DRIFT_SPEED;' not found\n"); fflush(debugLog); }
            return false;
        }
        b.insert(pos + anchor.length(), "\n    float moveT = uMovementTime * DRIFT_SPEED;");
        if (debugLog) { fprintf(debugLog, "[OK] moveT declaration injected\n"); fflush(debugLog); }
    }

    // ---- OPTIONAL: movement time expression replacements ----
    {
        const ShaderPatch patches[] = {
            {"sin(t * 0.21)", "sin(moveT * 0.21)", "move-time sin(t*0.21)", false},
            {"sin(t * 0.083)", "sin(moveT * 0.083)", "move-time sin(t*0.083)", false},
            {"sin(t * 0.157 + 2.0)", "sin(moveT * 0.157 + 2.0)", "move-time sin(t*0.157+2)", false},
            {"sin(t * 0.117)", "sin(moveT * 0.117)", "move-time sin(t*0.117)", false},
            {"sin(t * 0.83)", "sin(moveT * 0.83)", "move-time sin(t*0.83)", false},
            {"sin(t * 1.31)", "sin(moveT * 1.31)", "move-time sin(t*1.31)", false},
            {"sin(t * 1.03 + 1.0)", "sin(moveT * 1.03 + 1.0)", "move-time sin(t*1.03+1)", false},
            {"lissa(t * TOKEN_CALM)", "lissa(moveT * TOKEN_CALM)", "move-time lissa(calm)", false},
            {"lissa(t * TOKEN_RUSH)", "lissa(moveT * TOKEN_RUSH)", "move-time lissa(rush)", false},
            {"cos(t * 0.8)", "cos(moveT * 0.8)", "move-time cos(t*0.8)", false},
            {"sin(t * 1.0)", "sin(moveT * 1.0)", "move-time sin(t*1.0)", false},
        };
        if (!applyPatches(b, patches, 11, debugLog)) return false;
    }

    if (debugLog) {
        fprintf(debugLog, "[DEBUG] header first 3 bytes: %02x %02x %02x\n",
                h.size() >= 3 ? (unsigned char)h[0] : 0,
                h.size() >= 3 ? (unsigned char)h[1] : 0,
                h.size() >= 3 ? (unsigned char)h[2] : 0);
        fprintf(debugLog, "[DEBUG] body first 3 bytes: %02x %02x %02x\n",
                b.size() >= 3 ? (unsigned char)b[0] : 0,
                b.size() >= 3 ? (unsigned char)b[1] : 0,
                b.size() >= 3 ? (unsigned char)b[2] : 0);
        fflush(debugLog);
    }

    // ---- CRITICAL: make key constants overridable by uniforms ----
    // Uses semicolon-aware replacement to consume the full declaration.
    {
        struct Override { const char* name; const char* uniform; };
        const Override ov[] = {
            {"HOLE_RADIUS", "uHoleRadius > 0.0 ? uHoleRadius :"},
            {"DISK_GAIN",   "uDiskGain > 0.0 ? uDiskGain :"},
            {"DISK_TEMP",   "uDiskTemp > 0.0 ? uDiskTemp :"},
            {"EXPOSURE",    "uExposure > 0.0 ? uExposure :"},
            {"DRIFT_SPEED", "uSpeed > 0.0 ? uSpeed :"},
            {"STAR_GAIN",   "uStarGain > 0.0 ? uStarGain :"},
            {"DISK_INCL",   "uDiskIncl > 0.0 ? uDiskIncl :"},
        };
        for (const auto& o : ov) {
            size_t pos = 0, valuePos = 0, end = 0;
            if (!findConstFloatDecl(b, o.name, pos, valuePos, end,
                                    o.name, true, debugLog))
                return false;
            std::string val = b.substr(valuePos, end - valuePos);
            std::string replacement = std::string("float ") + o.name + " = " + o.uniform + " " + val + ";";
            b.replace(pos, end - pos + 1, replacement);
        }
    }

    // ---- CRITICAL: demoLook injection ----
    {
        const std::string anchor = "DiskLook demoLook()";
        size_t dlo = b.find(anchor);
        if (dlo == std::string::npos) {
            if (debugLog) { fprintf(debugLog, "[FAIL] Critical anchor 'DiskLook demoLook()' not found\n"); fflush(debugLog); }
            return false;
        }
        size_t ob = b.find("{", dlo);
        int depth = 0; size_t dle = ob;
        if (ob != std::string::npos) {
            for (dle = ob; dle < b.size(); dle++) {
                if (b[dle] == 123) depth++;
                else if (b[dle] == 125) { depth--; if (depth == 0) break; }
            }
        }
        if (dle >= b.size()) {
            if (debugLog) { fprintf(debugLog, "[FAIL] Could not find end of demoLook() body\n"); fflush(debugLog); }
            return false;
        }
        std::string newFunc =
            "DiskLook demoPreset(int i) {\n"
            "    return DiskLook(\n"
            "        uPresetTemp[i], uPresetIncl[i], uPresetRoll[i],\n"
            "        uPresetInner[i], uPresetOuter[i], uPresetOpac[i],\n"
            "        uPresetDopp[i], uPresetBeam[i], uPresetGain[i],\n"
            "        uPresetContr[i], uPresetWind[i], uPresetSpd[i],\n"
            "        uPresetExpo[i], uPresetStar[i]);\n"
            "}\n"
            "\n"
            "DiskLook demoLook() {\n"
            "    if (uPresetCount > 0) {\n"
            "        int n = int(clamp(float(uPresetCount), 1.0, float(MAX_PRESETS)));\n"
            "        float f; int i0, i1;\n"
            "        if (uPlayMode == 0) {\n"
            "            float raw = (iTime + uPresetOffset) / max(uSlotSec, 0.5);\n"
            "            f = smoothstep(1.0 - DEMO_XFADE, 1.0, fract(raw));\n"
            "            i0 = int(min(raw, float(n) - 0.001));\n"
            "            i1 = int(min(raw + 1.0, float(n) - 0.001));\n"
            "        } else if (uPlayMode == 2) {\n"
            "            float raw = (iTime + uPresetOffset) / max(uSlotSec, 0.5);\n"
            "            f = smoothstep(1.0 - DEMO_XFADE, 1.0, fract(raw));\n"
            "            int slot = int(raw);\n"
            "            i0 = int(fract(sin(float(slot) * 127.1 + 311.7) * 43758.5453) * float(n));\n"
            "            i1 = int(fract(sin(float(slot + 1) * 127.1 + 311.7) * 43758.5453) * float(n));\n"
            "        } else {\n"
            "            float raw = (iTime + uPresetOffset) / max(uSlotSec, 0.5);\n"
            "            f = smoothstep(1.0 - DEMO_XFADE, 1.0, fract(raw));\n"
            "            i0 = int(raw) % n;\n"
            "            i1 = (int(raw) + 1) % n;\n"
            "        }\n"
            "        return mixLook(demoPreset(i0), demoPreset(i1), f);\n"
            "    } else {\n"
            "        float u = mod(iTime, DEMO_SEC) / DEMO_SEC * float(DEMO_N);\n"
            "        int   i = int(min(u, float(DEMO_N) - 0.001));\n"
            "        float f = smoothstep(1.0 - DEMO_XFADE, 1.0, fract(u));\n"
            "        return mixLook(DEMO_TOUR[i], DEMO_TOUR[(i + 1) % DEMO_N], f);\n"
            "    }\n"
            "}\n";
        b.replace(dlo, dle - dlo + 1, newFunc);
        if (debugLog) { fprintf(debugLog, "[OK] demoLook() injected\n"); fflush(debugLog); }
    }

    // ---- Semicolon-aware const float replacements (critical) ----
    // These must consume the entire "const float NAME = value;" declaration,
    // not just the prefix, to avoid leaving a stale suffix that breaks GLSL.
    if (!replaceConstFloatDecl(b, "WORK_AREA",   "const float WORK_AREA = 0.0;",
                               "WORK_AREA set to 0.0", true, debugLog))
        return false;
    if (!replaceConstFloatDecl(b, "TOKEN_HOME_X", "float TOKEN_HOME_X = uHomeX;",
                               "TOKEN_HOME_X -> uHomeX", true, debugLog))
        return false;
    if (!replaceConstFloatDecl(b, "TOKEN_HOME_Y", "float TOKEN_HOME_Y = uHomeY;",
                               "TOKEN_HOME_Y -> uHomeY", true, debugLog))
        return false;
    if (!replaceConstFloatDecl(b, "DEMO_XFADE", "const float DEMO_XFADE = 0.65;",
                               "DEMO_XFADE=0.65", false, debugLog))
        return false;

    // ---- CRITICAL: shield simplification (semicolon-aware) ----
    {
        const char* prefix = "float shield = vis * smoothstep(WORK_AREA";
        size_t pp = b.find(prefix);
        if (pp == std::string::npos) {
            if (debugLog) { fprintf(debugLog, "[FAIL] Critical anchor 'shield simplification' not found\n"); fflush(debugLog); }
            return false;
        }
        size_t ve = b.find(";", pp);
        if (ve == std::string::npos) {
            if (debugLog) { fprintf(debugLog, "[FAIL] Semicolon not found for shield simplification\n"); fflush(debugLog); }
            return false;
        }
        b.replace(pp, ve - pp + 1, "float shield = vis;");
        if (debugLog) { fprintf(debugLog, "[OK] shield simplification applied\n"); fflush(debugLog); }
    }

    // ---- applyPatches for remaining exact-match patches ----
    // These use exact anchors (full line or unique expression), so no prefix issue.
    const ShaderPatch remainingPatches[] = {
        // CRITICAL: size, birth, fullscreen, trajectory, visuals
        {"#define SIZE_MODE MODE_TOKENS", "#define SIZE_MODE MODE_DEMO", "SIZE_MODE -> MODE_DEMO", true},
        {"float rh = HOLE_RADIUS * sz;",  "    sz *= uBornProgress;\n    float rh = HOLE_RADIUS * sz;",
         "bornProgress injection", true},
        // CRITICAL: mouse-follow mode (multi-line exact match)
        {"center = (lo + hi) * 0.5 + wander * ampEff\n"
         "               + wobAmp * vec2(cos(moveT * 0.8), sin(moveT * 1.0));",
         "center = (uFollowMouse > 0)\n"
         "               ? vec2(TOKEN_HOME_X, TOKEN_HOME_Y)\n"
         "               : ((lo + hi) * 0.5 + wander * ampEff\n"
         "                  + wobAmp * vec2(cos(moveT * 0.8), sin(moveT * 1.0)));",
         "mouse-follow center", true},
        // CRITICAL: lighting-effect final color injection (full-statement anchor)
        {"    fragColor = vec4(col, 1.0);",
         "    if (uLightingEffect > 0) {\n"
         "        vec3 diskLight = vec3(1.0) - exp(-emitc * L.expo);\n"
         "        float diskEnergy = max(max(diskLight.r, diskLight.g), diskLight.b);\n"
         "        float realDiskMask = smoothstep(0.012, 0.20, diskEnergy);\n"
         "        vec2 diskLocalP = rot(vec2(p.x, -p.y), L.roll);\n"
         "        float diskInclScale = max(abs(cos(L.incl)), 0.16);\n"
         "        vec2 projectedLightP = vec2(diskLocalP.x, diskLocalP.y / diskInclScale);\n"
         "        float projectedLightRadius = max(length(projectedLightP), 0.0005);\n"
         "        float diskInnerScreen = rin * rh / B_CRIT;\n"
         "        float diskOuterScreen = rout * rh / B_CRIT;\n"
         "        float diskWidth = max(diskOuterScreen - diskInnerScreen, rh * 0.35);\n"
         "        float diskUnit = clamp((projectedLightRadius - diskInnerScreen) / diskWidth, 0.0, 1.0);\n"
         "        float outerLightMist = smoothstep(0.006, 0.028, diskEnergy) * (1.0 - smoothstep(0.10, 0.24, diskEnergy));\n"
         "        float nativeGapEntry = smoothstep(0.036, 0.042, diskEnergy);\n"
         "        float nativeGapExit = 1.0 - smoothstep(0.046, 0.052, diskEnergy);\n"
         "        float darkFlowBands = nativeGapEntry * nativeGapExit;\n"
         "        float bandTransmission = 1.0 - darkFlowBands * 0.88;\n"
         "        float diskSide = smoothstep(-0.30, 0.30, projectedLightP.x / projectedLightRadius);\n"
         "        vec3 warmGoldLight = vec3(1.00, 0.40, 0.08) * (1.0 - diskSide);\n"
         "        vec3 coldBlueLight = vec3(0.10, 0.54, 1.00) * diskSide;\n"
         "        vec3 dualToneLight = warmGoldLight + coldBlueLight;\n"
         "        float outerDiskFade = smoothstep(0.15, 1.0, diskUnit);\n"
         "        float nativeDiskDetail = 1.0;\n"
         "        float middleBrightLayer = realDiskMask * nativeDiskDetail * (0.32 + 0.82 * (1.0 - outerDiskFade));\n"
         "        float screenR = length(p);\n"
         "        float ringWidth = max(rh * 0.10, 0.0004);\n"
         "        float lightingPhotonRing = exp(-pow((screenR - rh * 1.10) / ringWidth, 2.0)) * (captured ? 0.0 : 1.0);\n"
         "        float eventHorizonMask = captured ? 1.0 : 0.0;\n"
         "        float subtleDiskLighting = middleBrightLayer * bandTransmission;\n"
         "        vec3 clampedBaseScene = clamp(col * (1.0 - darkFlowBands * 0.32), vec3(0.0), vec3(1.0));\n"
         "        vec3 lightingHdr = dualToneLight * subtleDiskLighting * 0.18;\n"
         "        lightingHdr += dualToneLight * outerLightMist * shield * (1.0 - eventHorizonMask) * 0.035;\n"
         "        lightingHdr += vec3(1.00, 0.86, 0.66) * lightingPhotonRing * 0.12;\n"
         "        col = clampedBaseScene + lightingHdr;\n"
         "        col = mix(col, vec3(0.0), eventHorizonMask);\n"
         "    }\n"
         "    fragColor = vec4(col, 1.0);",
         "lighting-effect final color injection", true},
        // CRITICAL: trajectory randomization
        {"lissa(moveT * TOKEN_CALM)", "lissa(moveT * TOKEN_CALM + uRandPhase)", "trajectory: calm", true},
        {"lissa(moveT * TOKEN_RUSH)", "lissa(moveT * TOKEN_RUSH + uRandPhase)", "trajectory: rush", true},
        // CRITICAL (reclassified from optional): established runtime controls
        {"* window * shield;",
         "* window * shield * max(uDistortion, 0.0);", "distortion multiplier", true},
        {"mod(iTime, DEMO_SEC) / DEMO_GROW_SEC",
         "(uFixedSize > 0 ? uFixedLevel : (uGrowEnabled > 0 ? mix(uInitialSize, 1.0, min(iTime / DEMO_GROW_SEC, 1.0)) : min(iTime / DEMO_GROW_SEC, 1.0)))",
         "hole growth formula", true},
        // OPTIONAL: cosmetic / edge-case patches
        {"vec2  fullLo = vec2(min(xPad, 0.5), marg);", "vec2  fullLo = vec2(xPad, marg);", "multimon fullLo", false},
        {"vec2  fullHi = vec2(max(0.5, 1.0 - xPad),", "vec2  fullHi = vec2(1.0 - xPad,", "multimon fullHi", false},
        {"float reach  = mix(0.06, max(TOKEN_REACH, 0.06), g);",
         "float reach  = mix(0.30, max(TOKEN_REACH, 1.0), g);", "multimon reach", false},
        {"cos(moveT * 0.8)", "cos((moveT + uRandPhase) * 0.8)", "trajectory: cos", false},
        {"sin(moveT * 1.0)", "sin((moveT + uRandPhase) * 1.0)", "trajectory: sin", false},
    };
    if (!applyPatches(b, remainingPatches,
                      sizeof(remainingPatches)/sizeof(remainingPatches[0]),
                      debugLog))
        return false;

    out = h + "\n// ===== blackhole.glsl =====" + b +
          "\nvoid main() { vec4 c; vec2 fc = vec2(gl_FragCoord.x, iResolution.y - gl_FragCoord.y); mainImage(c, fc); fragColor = c; }\n";
    if (debugLog) { fprintf(debugLog, "[OK] Shader assembly complete (%zu bytes)\n", out.size()); fflush(debugLog); }
    return true;
}

bool buildFragmentShader(std::string& out, FILE* debugLog) {
    out.clear();
    std::string header = readFile("shaders/frag_desktop_header.glsl");
    std::string body   = readFile("blackhole.glsl");
    if (header.empty() || body.empty()) {
        if (debugLog) { fprintf(debugLog, "[FAIL] Shader file empty: header=%zu, body=%zu\n", header.size(), body.size()); fflush(debugLog); }
        return false;
    }
    return buildFragmentShaderFromSources(header, body, out, debugLog);
}
