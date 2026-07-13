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

    // Remove UTF-8 BOM
    if (content.size() >= 3) {
        unsigned char c0 = static_cast<unsigned char>(content[0]);
        unsigned char c1 = static_cast<unsigned char>(content[1]);
        unsigned char c2 = static_cast<unsigned char>(content[2]);
        if (c0 == 0xEF && c1 == 0xBB && c2 == 0xBF) {
            content = content.substr(3);
        }
    }

    // Normalize CRLF -> LF so runtime patches stay effective
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
                    fprintf(debugLog, "[PATCH] %s: CRITICAL — returning failure\n", p.description);
                    fflush(debugLog);
                }
                allOk = false;
            }
            continue;
        }
        if (p.replacement == nullptr) {
            // Insert-before mode: not used yet, skip
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

bool buildFragmentShader(std::string& out, FILE* debugLog) {
    std::string header = readFile("shaders/frag_desktop_header.glsl");
    std::string body   = readFile("blackhole.glsl");
    if (header.empty() || body.empty()) {
        if (debugLog) { fprintf(debugLog, "[FAIL] Shader file empty: header=%zu, body=%zu\n", header.size(), body.size()); fflush(debugLog); }
        return false;
    }

    // ---- Critical: uMovementTime injection ----
    // Without this, the shader references an undeclared uniform.
    {
        const std::string anchor = "uniform float iTime;";
        size_t pos = header.find(anchor);
        if (pos == std::string::npos) {
            if (debugLog) { fprintf(debugLog, "[FAIL] Critical anchor 'uniform float iTime;' not found in header\n"); fflush(debugLog); }
            return false;
        }
        header.insert(pos + anchor.length(), "\nuniform float uMovementTime;");
        if (debugLog) { fprintf(debugLog, "[OK] uMovementTime uniform injected\n"); fflush(debugLog); }
    }

    // ---- Critical: moveT declaration ----
    // Without this, the shader references an undeclared variable.
    {
        const std::string anchor = "float t = iTime * DRIFT_SPEED;";
        size_t pos = body.find(anchor);
        if (pos == std::string::npos) {
            if (debugLog) { fprintf(debugLog, "[FAIL] Critical anchor 'float t = iTime...' not found in body\n"); fflush(debugLog); }
            return false;
        }
        body.insert(pos + anchor.length(), "\n    float moveT = uMovementTime * DRIFT_SPEED;");
        if (debugLog) { fprintf(debugLog, "[OK] moveT declaration injected\n"); fflush(debugLog); }
    }

    // ---- Optional: replace movement time expressions ----
    {
        const std::pair<const char*, const char*> replacements[] = {
            {"sin(t * 0.21)", "sin(moveT * 0.21)"},
            {"sin(t * 0.083)", "sin(moveT * 0.083)"},
            {"sin(t * 0.157 + 2.0)", "sin(moveT * 0.157 + 2.0)"},
            {"sin(t * 0.117)", "sin(moveT * 0.117)"},
            {"sin(t * 0.83)", "sin(moveT * 0.83)"},
            {"sin(t * 1.31)", "sin(moveT * 1.31)"},
            {"sin(t * 1.03 + 1.0)", "sin(moveT * 1.03 + 1.0)"},
            {"lissa(t * TOKEN_CALM)", "lissa(moveT * TOKEN_CALM)"},
            {"lissa(t * TOKEN_RUSH)", "lissa(moveT * TOKEN_RUSH)"},
            {"cos(t * 0.8)", "cos(moveT * 0.8)"},
            {"sin(t * 1.0)", "sin(moveT * 1.0)"}
        };
        for (const auto& r : replacements) {
            size_t pos = body.find(r.first);
            if (pos != std::string::npos)
                body.replace(pos, strlen(r.first), r.second);
        }
    }

    if (debugLog) {
        fprintf(debugLog, "[DEBUG] header first 3 bytes: %02x %02x %02x\n",
                header.size() >= 3 ? (unsigned char)header[0] : 0,
                header.size() >= 3 ? (unsigned char)header[1] : 0,
                header.size() >= 3 ? (unsigned char)header[2] : 0);
        fprintf(debugLog, "[DEBUG] body first 3 bytes: %02x %02x %02x\n",
                body.size() >= 3 ? (unsigned char)body[0] : 0,
                body.size() >= 3 ? (unsigned char)body[1] : 0,
                body.size() >= 3 ? (unsigned char)body[2] : 0);
        fflush(debugLog);
    }

    // ---- Optional: make key constants overridable by uniforms ----
    {
        struct { const char* name; const char* uniform; } ov[] = {
            {"HOLE_RADIUS", "uHoleRadius > 0.0 ? uHoleRadius :"},
            {"DISK_GAIN",   "uDiskGain > 0.0 ? uDiskGain :"},
            {"DISK_TEMP",   "uDiskTemp > 0.0 ? uDiskTemp :"},
            {"EXPOSURE",    "uExposure > 0.0 ? uExposure :"},
            {"DRIFT_SPEED", "uSpeed > 0.0 ? uSpeed :"},
            {"STAR_GAIN",   "uStarGain > 0.0 ? uStarGain :"},
            {"DISK_INCL",   "uDiskIncl > 0.0 ? uDiskIncl :"},
        };
        for (auto& o : ov) {
            std::string pattern = std::string("const float ") + o.name + " = ";
            size_t pos = body.find(pattern);
            if (pos != std::string::npos) {
                size_t ve = body.find(";", pos);
                if (ve != std::string::npos) {
                    std::string val = body.substr(pos + pattern.length(), ve - pos - pattern.length());
                    body.replace(pos, ve - pos + 1,
                        std::string("float ") + o.name + " = " + o.uniform + " " + val + ";");
                }
            }
        }
    }

    // ---- Optional: inject demoPreset() + demoLook() ----
    {
        size_t dlo = body.find("DiskLook demoLook()");
        if (dlo != std::string::npos) {
            size_t ob = body.find("{", dlo);
            int depth = 0; size_t dle = ob;
            if (ob != std::string::npos) {
                for (dle = ob; dle < body.size(); dle++) {
                    if (body[dle] == 123) depth++;
                    else if (body[dle] == 125) { depth--; if (depth == 0) break; }
                }
            }
            if (dle < body.size()) {
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
                body.replace(dlo, dle - dlo + 1, newFunc);
            }
        }
    }

    // ---- Optional: SIZE_MODE ----
    {
        size_t pos = body.find("#define SIZE_MODE MODE_TOKENS");
        if (pos != std::string::npos)
            body.replace(pos, 29, "#define SIZE_MODE MODE_DEMO");
    }

    // ---- Optional: hole size growth ----
    {
        size_t lp = body.find("mod(iTime, DEMO_SEC) / DEMO_GROW_SEC");
        if (lp != std::string::npos)
            body.replace(lp, 36, "(uFixedSize > 0 ? uFixedLevel : (uGrowEnabled > 0 ? mix(uInitialSize, 1.0, min(iTime / DEMO_GROW_SEC, 1.0)) : min(iTime / DEMO_GROW_SEC, 1.0)))");
    }

    // ---- Optional: uBornProgress ----
    {
        size_t p = body.find("float rh = HOLE_RADIUS * sz;");
        if (p != std::string::npos)
            body.insert(p, "    sz *= uBornProgress;\n");
    }

    // ---- Optional: WORK_AREA fullscreen fixes ----
    {
        size_t p = body.find("const float WORK_AREA");
        if (p != std::string::npos) {
            size_t ve = body.find(";", p);
            if (ve != std::string::npos)
                body.replace(p, ve - p + 1, "const float WORK_AREA = 0.0;");
        }
        size_t sp = body.find("float shield = vis * smoothstep(WORK_AREA");
        if (sp != std::string::npos) {
            size_t ve = body.find(";", sp);
            if (ve != std::string::npos)
                body.replace(sp, ve - sp + 1, "float shield = vis;");
        }
    }

    // ---- Optional: multi-monitor range expansion ----
    {
        {
            const std::string oldLo = "vec2  fullLo = vec2(min(xPad, 0.5), marg);";
            size_t p = body.find(oldLo);
            if (p != std::string::npos) body.replace(p, oldLo.length(), "vec2  fullLo = vec2(xPad, marg);");
        }
        {
            const std::string oldHi = "vec2  fullHi = vec2(max(0.5, 1.0 - xPad),";
            size_t p = body.find(oldHi);
            if (p != std::string::npos) body.replace(p, oldHi.length(), "vec2  fullHi = vec2(1.0 - xPad,");
        }
        {
            const std::string oldR = "float reach  = mix(0.06, max(TOKEN_REACH, 0.06), g);";
            size_t p = body.find(oldR);
            if (p != std::string::npos) body.replace(p, oldR.length(), "float reach  = mix(0.30, max(TOKEN_REACH, 1.0), g);");
        }
    }

    // ---- Optional: randomize initial hole position ----
    {
        size_t p = body.find("const float TOKEN_HOME_X");
        if (p != std::string::npos) {
            size_t ve = body.find(";", p);
            if (ve != std::string::npos)
                body.replace(p, ve - p + 1, "float TOKEN_HOME_X = [redacted];");
        }
        p = body.find("const float TOKEN_HOME_Y");
        if (p != std::string::npos) {
            size_t ve = body.find(";", p);
            if (ve != std::string::npos)
                body.replace(p, ve - p + 1, "float TOKEN_HOME_Y = [redacted];");
        }
    }

    // ---- Critical: mouse-follow mode ----
    {
        const std::string oldCenter =
            "center = (lo + hi) * 0.5 + wander * ampEff\n"
            "               + wobAmp * vec2(cos(moveT * 0.8), sin(moveT * 1.0));";
        const std::string newCenter =
            "center = (uFollowMouse > 0)\n"
            "               ? vec2(TOKEN_HOME_X, TOKEN_HOME_Y)\n"
            "               : ((lo + hi) * 0.5 + wander * ampEff\n"
            "                  + wobAmp * vec2(cos(moveT * 0.8), sin(moveT * 1.0)));";
        size_t p = body.find(oldCenter);
        if (p == std::string::npos) {
            if (debugLog) {
                fprintf(debugLog, "[FAIL] Critical anchor 'mouse-follow center template' not found\n");
                fflush(debugLog);
            }
            return false;
        }
        body.replace(p, oldCenter.length(), newCenter);
        if (debugLog) {
            fprintf(debugLog, "[OK] Mouse-follow shader center injected\n");
            fflush(debugLog);
        }
    }

    // ---- Optional: trajectory randomization ----
    {
        size_t p;
        while ((p = body.find("lissa(moveT * TOKEN_CALM)")) != std::string::npos)
            body.replace(p, 25, "lissa(moveT * TOKEN_CALM + uRandPhase)");
        while ((p = body.find("lissa(moveT * TOKEN_RUSH)")) != std::string::npos)
            body.replace(p, 25, "lissa(moveT * TOKEN_RUSH + uRandPhase)");
        while ((p = body.find("cos(moveT * 0.8)")) != std::string::npos)
            body.replace(p, 16, "cos((moveT + uRandPhase) * 0.8)");
        while ((p = body.find("sin(moveT * 1.0)")) != std::string::npos)
            body.replace(p, 16, "sin((moveT + uRandPhase) * 1.0)");
    }

    // ---- Optional: runtime visual controls ----
    {
        const std::string oldDefl = "* window * shield;";
        const std::string newDefl = "* window * shield * max(uDistortion, 0.0);";
        size_t p = body.find(oldDefl);
        if (p != std::string::npos) body.replace(p, oldDefl.length(), newDefl);

        const std::string oldNear = "vec2  suv = mirrorUV(center + (p + (sp - p) * window * shield) / vec2(aspect, 1.0));";
        const std::string newNear = "vec2  suv = mirrorUV(center + (p + (sp - p) * window * shield * max(uDistortion, 0.0)) / vec2(aspect, 1.0));";
        p = body.find(oldNear);
        if (p != std::string::npos) body.replace(p, oldNear.length(), newNear);

        const std::string finalColor = "    fragColor = vec4(col, 1.0);";
        p = body.find(finalColor);
        if (p != std::string::npos) {
            body.replace(p, finalColor.length(),
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
                "    fragColor = vec4(col, 1.0);");
        }
    }

    // ---- Optional: preset crossfade ----
    {
        size_t p = body.find("const float DEMO_XFADE");
        if (p != std::string::npos) {
            size_t ve = body.find(";", p);
            if (ve != std::string::npos)
                body.replace(p, ve - p + 1, "const float DEMO_XFADE = 0.65;");
        }
    }

    out = header + "\n// ===== blackhole.glsl =====" + body +
          "\nvoid main() { vec4 c; vec2 fc = vec2(gl_FragCoord.x, iResolution.y - gl_FragCoord.y); mainImage(c, fc); fragColor = c; }\n";
    return true;
}
