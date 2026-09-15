#include "fragment_shader_builder.h"
#include <algorithm>
#include <cstring>
#include <fstream>
#include <sstream>
#include <utility>
#include <regex>

static std::string readFile(const char* path) {
    std::ifstream f(path, std::ios::in | std::ios::binary);
    if (!f) { fprintf(stderr, "Cannot open %s\n", path); return ""; }
    std::stringstream ss; ss << f.rdbuf();
    std::string content = ss.str();

    // 移除 UTF-8 BOM (0xEF 0xBB 0xBF) - 使用 unsigned char 比较
    if (content.size() >= 3) {
        unsigned char c0 = static_cast<unsigned char>(content[0]);
        unsigned char c1 = static_cast<unsigned char>(content[1]);
        unsigned char c2 = static_cast<unsigned char>(content[2]);
        if (c0 == 0xEF && c1 == 0xBB && c2 == 0xBF) {
            content = content.substr(3);
        }
    }

    // Runtime shader patches use LF templates. Normalize Windows CRLF files so
    // multi-line replacements remain effective in deployed Release folders.
    content.erase(std::remove(content.begin(), content.end(), '\r'), content.end());
    return content;
}

bool BuildFragmentShader(std::string& out, FILE* debugLog, bool& consumptionShaderAvailable, bool enableConsumption) {
    std::string header = readFile("shaders/frag_desktop_header.glsl");
    std::string body   = readFile("blackhole.glsl");
    if (header.empty() || body.empty()) {
        if (debugLog) { fprintf(debugLog, "[FAIL] Shader file empty: header=%zu, body=%zu\n", header.size(), body.size()); fflush(debugLog); }
        return false;
    }

    const std::string timeUniform = "uniform float iTime;";
    size_t timeUniformPos = header.find(timeUniform);
    if (timeUniformPos != std::string::npos) {
        header.insert(timeUniformPos + timeUniform.length(), "\nuniform float uMovementTime;");
    }

    const std::string realMovementTime = "float t = iTime * DRIFT_SPEED;";
    size_t movementTimePos = body.find(realMovementTime);
    if (movementTimePos != std::string::npos) {
        body.insert(movementTimePos + realMovementTime.length(),
                    "\n    float moveT = uMovementTime * DRIFT_SPEED;");
    }

    const std::pair<const char*, const char*> movementTimeExpressions[] = {
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
    for (const auto& expression : movementTimeExpressions) {
        size_t pos = body.find(expression.first);
        if (pos != std::string::npos) {
            body.replace(pos, strlen(expression.first), expression.second);
        }
    }

    // 检查是否还有 BOM
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

    // Make key constants overridable by uniforms
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
        // 对齐空格不是语义；原来的精确单空格匹配会漏掉 HOLE_RADIUS 等常量。
        const std::regex declaration(std::string("const float ")+o.name+
            (std::strcmp(o.name,"HOLE_RADIUS")==0 ? "\\s*=\\s*([^;]+);" : " = ([^;]+);"));
        std::smatch match;
        if (std::regex_search(body,match,declaration)) {
            body.replace(static_cast<size_t>(match.position()),static_cast<size_t>(match.length()),
                std::string("float ")+o.name+" = "+o.uniform+" "+match[1].str()+";");
        }
    }

    // Add custom demoLook that checks uUseCustom
    {
        size_t dlo = body.find("DiskLook demoLook()");
        if (dlo != std::string::npos) {
            size_t ob = body.find("{", dlo);
            int d = 0; size_t dle = ob;
            if (ob != std::string::npos) {
                for (dle = ob; dle < body.size(); dle++) {
                    if (body[dle] == 123) d++;
                    else if (body[dle] == 125) { d--; if (d == 0) break; }
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

    size_t pos = body.find("#define SIZE_MODE MODE_TOKENS");
    if (pos != std::string::npos)
        body.replace(pos, 29, "#define SIZE_MODE MODE_DEMO");

    // Remove time wrapping from hole size: grow to full and stay there.
    // Priority: fixed size > optional gradual growth > full size.
    {
        size_t lp = body.find("mod(iTime, DEMO_SEC) / DEMO_GROW_SEC");
        if (lp != std::string::npos)
            body.replace(lp, 36, "(uFixedSize > 0 ? uFixedLevel : (uGrowEnabled > 0 ? mix(uInitialSize, 1.0, min(iTime / DEMO_GROW_SEC, 1.0)) : min(iTime / DEMO_GROW_SEC, 1.0)))");
    }

    // Apply uBornProgress to sz for smooth hole birth/die
    {
        size_t pos = body.find("float rh = HOLE_RADIUS * sz;");
        if (pos != std::string::npos) {
            body.insert(pos, "    sz *= uBornProgress;\n");
        }
    }

    // ---- Full-screen fixes: remove WORK_AREA shield so hole can roam entire screen ----
    {
        // Set WORK_AREA to 0 so position constraints allow full-screen range
        size_t p = body.find("const float WORK_AREA");
        if (p != std::string::npos) {
            size_t ve = body.find(";", p);
            if (ve != std::string::npos)
                body.replace(p, ve - p + 1, "const float WORK_AREA = 0.0;");
        }
        // Remove the shield fade: let distortion cover the entire screen
        size_t sp = body.find("float shield = vis * smoothstep(WORK_AREA");
        if (sp != std::string::npos) {
            size_t ve = body.find(";", sp);
            if (ve != std::string::npos)
                body.replace(sp, ve - sp + 1, "float shield = vis;");
        }
    }

    // ---- Multi-monitor range expansion: let hole roam entire screen, not just home quadrant ----
    // fullLo.x: min(xPad, 0.5) → xPad           (allow left edge, not clamped to right of center)
    // fullHi.x: max(0.5, 1.0 - xPad) → 1.0 - xPad (allow right edge)
    // reach:    mix(0.06, max(TOKEN_REACH, 0.06), g) → mix(0.30, max(TOKEN_REACH, 1.0), g)
    //           (start at 30% roam box, expand to full screen at g=1; preserves small-to-large anim)
    {
        {
            const std::string oldLo = "vec2  fullLo = vec2(min(xPad, 0.5), marg);";
            const std::string newLo = "vec2  fullLo = vec2(xPad, marg);";
            size_t p = body.find(oldLo);
            if (p != std::string::npos) body.replace(p, oldLo.length(), newLo);
        }
        {
            const std::string oldHi = "vec2  fullHi = vec2(max(0.5, 1.0 - xPad),";
            const std::string newHi = "vec2  fullHi = vec2(1.0 - xPad,";
            size_t p = body.find(oldHi);
            if (p != std::string::npos) body.replace(p, oldHi.length(), newHi);
        }
        {
            const std::string oldR = "float reach  = mix(0.06, max(TOKEN_REACH, 0.06), g);";
            const std::string newR = "float reach  = mix(0.30, max(TOKEN_REACH, 1.0), g);";
            size_t p = body.find(oldR);
            if (p != std::string::npos) body.replace(p, oldR.length(), newR);
        }
    }

    // ---- Randomize initial hole position: replace TOKEN_HOME_X/Y consts with uniform refs ----
    // GLSL const requires compile-time initializer, so change const -> float
    // so it can pick up the uniform value at runtime.
    {
        size_t p = body.find("const float TOKEN_HOME_X");
        if (p != std::string::npos) {
            size_t ve = body.find(";", p);
            if (ve != std::string::npos)
                body.replace(p, ve - p + 1, "float TOKEN_HOME_X = uHomeX;");
        }
        p = body.find("const float TOKEN_HOME_Y");
        if (p != std::string::npos) {
            size_t ve = body.find(";", p);
            if (ve != std::string::npos)
                body.replace(p, ve - p + 1, "float TOKEN_HOME_Y = uHomeY;");
        }
    }

    // ---- Mouse-follow mode: lock center to runtime home coordinates ----
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
                fprintf(debugLog, "[FAIL] Mouse-follow shader center template not found\n");
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

    // ---- Randomize trajectory: add uRandPhase to lissa calls ----
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

    // ---- Runtime visual controls ----
    {
        const std::string oldDefl = "* window * shield;";
        const std::string newDefl = "* window * shield * max(uDistortion, 0.0);";
        size_t p = body.find(oldDefl);
        if (p != std::string::npos) body.replace(p, oldDefl.length(), newDefl);

        const std::string oldNear = "vec2  suv = mirrorUV(center + (p + (sp - p) * window * shield) / vec2(aspect, 1.0));";
        const std::string newNear = "vec2  suv = mirrorUV(center + (p + (sp - p) * window * shield * max(uDistortion, 0.0)) / vec2(aspect, 1.0));";
        p = body.find(oldNear);
        if (p != std::string::npos) body.replace(p, oldNear.length(), newNear);

    }

    // 旧实验着色只由此组装入口使用；新模式独立渲染，关闭时保留基础代码。
    consumptionShaderAvailable = false;
    if (enableConsumption) {
        const std::string palette = readFile("shaders/consumption_palette.glsl");
        const std::string effect = readFile("shaders/consumption.glsl");
        const std::string radius = "float rh = HOLE_RADIUS * sz;";
        size_t entry = body.find("void mainImage(");
        size_t atRadius = body.find(radius);
        if (!palette.empty() && !effect.empty() && entry != std::string::npos && atRadius != std::string::npos) {
            body.insert(atRadius + radius.size(),
                "\n    if (uLightingEffect > 0) { center = consumptionCenter(vec2(uHomeX, uHomeY), uMovementTime);\n"
                "        if (uDiskTemp >= 0.0) L.temp=uDiskTemp; if (uDiskGain >= 0.0) L.gain=uDiskGain;\n"
                "        if (uDiskIncl >= 0.0) L.incl=uDiskIncl; if (uExposure >= 0.0) L.expo=uExposure; if (uStarGain >= 0.0) L.star=uStarGain;\n"
                "        if (uTransportPass > 0) { float fullShadow = sqrt(TOKEN_AREA_MAX * aspect / 3.1415927) * (HOLE_RADIUS / 0.08);\n"
                "            if (gl_FragCoord.x < 1.0) fragColor=vec4(rh,L.incl,L.roll,fullShadow);\n"
                "            else if (gl_FragCoord.x < 2.0) fragColor=vec4(center,L.temp,L.inner);\n"
                "            else if (gl_FragCoord.x < 3.0) fragColor=vec4(L.outer,L.opac,L.dopp,L.beam);\n"
                "            else fragColor=vec4(L.gain,L.contr,L.wind,L.speed); return; }\n"
                "        fragColor = vec4(consumptionRender(uv, center, rh, L), 1.0); return; }\n");
            body.insert(entry, palette + "\n" + effect + "\n");
            consumptionShaderAvailable = true;
        } else if (debugLog) {
            fprintf(debugLog, "[Consumption][WARN] Effect source unavailable; using base renderer\n");
        }
    }

    // ---- Preset crossfade: 0.65 = 65% of slot for slow, cinematic transitions ----
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
