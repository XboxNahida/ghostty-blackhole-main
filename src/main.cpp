// blackhole standalone  Windows OpenGL host for blackhole.glsl
// v5: ImGui config panel + uniform-overridable shader params
// Build: Ctrl+Shift+B in VS Code

// D3D11: 取消注释下行切换渲染路径
// #define BLACKHOLE_USE_D3D11

#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <ctime>
#include <string>
#include <fstream>
#include <sstream>
#include <vector>
#include <cstring>
#include <windows.h>
#include <d3d11.h>
#include <dwmapi.h>
#include <wtsapi32.h>

#include "capture_wgc.h"
#include "capture_dxgi.h"
#include "gl_texture.h"
#include "gui_config.h"
#include "win32_gl.h"
#include "monitors.h"
#ifdef BLACKHOLE_USE_D3D11
#include "d3d11_renderer.h"
#include "win32_window.h"
#endif

#ifndef DWMWA_BORDER_COLOR
#define DWMWA_BORDER_COLOR 34  // Windows 11 accent border (not in SDK 8.1)
#endif

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#ifndef BLACKHOLE_USE_D3D11
#include <GL/gl.h>
#endif
#include <mmdeviceapi.h>
#include <audiopolicy.h>
#include <endpointvolume.h>
#include <tlhelp32.h>
// === DEBUG LOGGING ===

#ifndef BLACKHOLE_USE_D3D11
#ifndef GL_COMPILE_STATUS
#include <GL/glcorearb.h>
#endif
#endif

#ifndef BLACKHOLE_USE_D3D11
#define DECL_GL_FUNC(ret, name, args) \
    typedef ret (WINAPI *PFN_##name##_PROC) args; \
    static PFN_##name##_PROC gl_##name = nullptr

DECL_GL_FUNC(GLuint, CreateShader, (GLenum));
DECL_GL_FUNC(void,   ShaderSource, (GLuint, GLsizei, const GLchar**, const GLint*));
DECL_GL_FUNC(void,   CompileShader, (GLuint));
DECL_GL_FUNC(void,   GetShaderiv, (GLuint, GLenum, GLint*));
DECL_GL_FUNC(void,   GetShaderInfoLog, (GLuint, GLsizei, GLsizei*, GLchar*));
DECL_GL_FUNC(GLuint, CreateProgram, (void));
DECL_GL_FUNC(void,   AttachShader, (GLuint, GLuint));
DECL_GL_FUNC(void,   LinkProgram, (GLuint));
DECL_GL_FUNC(void,   GetProgramiv, (GLuint, GLenum, GLint*));
DECL_GL_FUNC(void,   GetProgramInfoLog, (GLuint, GLsizei, GLsizei*, GLchar*));
DECL_GL_FUNC(void,   DeleteShader, (GLuint));
DECL_GL_FUNC(void,   UseProgram, (GLuint));
DECL_GL_FUNC(GLint,  GetUniformLocation, (GLuint, const GLchar*));
DECL_GL_FUNC(void,   Uniform3f, (GLint, GLfloat, GLfloat, GLfloat));
DECL_GL_FUNC(void,   Uniform1f, (GLint, GLfloat));
DECL_GL_FUNC(void,   Uniform1i, (GLint, GLint));
DECL_GL_FUNC(void,   ActiveTexture, (GLenum));
DECL_GL_FUNC(void,   Uniform4f, (GLint, GLfloat, GLfloat, GLfloat, GLfloat));
DECL_GL_FUNC(void,   Uniform1fv, (GLint, GLsizei, const GLfloat*));
DECL_GL_FUNC(void,   GenVertexArrays, (GLsizei, GLuint*));
DECL_GL_FUNC(void,   GenBuffers, (GLsizei, GLuint*));
DECL_GL_FUNC(void,   BindVertexArray, (GLuint));
DECL_GL_FUNC(void,   BindBuffer, (GLenum, GLuint));
DECL_GL_FUNC(void,   BufferData, (GLenum, GLsizeiptr, const void*, GLenum));
DECL_GL_FUNC(void,   VertexAttribPointer, (GLuint, GLint, GLenum, GLboolean, GLsizei, const void*));
DECL_GL_FUNC(void,   EnableVertexAttribArray, (GLuint));
DECL_GL_FUNC(void,   DrawArrays, (GLenum, GLint, GLsizei));
DECL_GL_FUNC(void,   DeleteVertexArrays, (GLsizei, const GLuint*));
DECL_GL_FUNC(void,   DeleteBuffers, (GLsizei, const GLuint*));
DECL_GL_FUNC(void,   DeleteProgram, (GLuint));

#define LOAD_GL_FUNC(name) do { \
    gl_##name = (PFN_##name##_PROC)Win32GL_GetProcAddress("gl" #name); \
    if (!gl_##name) { fprintf(stderr, "Failed to load gl" #name "\n"); return false; } \
} while(0)

static bool loadGLFunctions() {
    LOAD_GL_FUNC(CreateShader);
    LOAD_GL_FUNC(ShaderSource);
    LOAD_GL_FUNC(CompileShader);
    LOAD_GL_FUNC(GetShaderiv);
    LOAD_GL_FUNC(GetShaderInfoLog);
    LOAD_GL_FUNC(CreateProgram);
    LOAD_GL_FUNC(AttachShader);
    LOAD_GL_FUNC(LinkProgram);
    LOAD_GL_FUNC(GetProgramiv);
    LOAD_GL_FUNC(GetProgramInfoLog);
    LOAD_GL_FUNC(DeleteShader);
    LOAD_GL_FUNC(UseProgram);
    LOAD_GL_FUNC(GetUniformLocation);
    LOAD_GL_FUNC(Uniform3f);
    LOAD_GL_FUNC(Uniform1f);
    LOAD_GL_FUNC(Uniform1i);
    LOAD_GL_FUNC(ActiveTexture);
    LOAD_GL_FUNC(Uniform4f);
    LOAD_GL_FUNC(Uniform1fv);
    LOAD_GL_FUNC(GenVertexArrays);
    LOAD_GL_FUNC(GenBuffers);
    LOAD_GL_FUNC(BindVertexArray);
    LOAD_GL_FUNC(BindBuffer);
    LOAD_GL_FUNC(BufferData);
    LOAD_GL_FUNC(VertexAttribPointer);
    LOAD_GL_FUNC(EnableVertexAttribArray);
    LOAD_GL_FUNC(DrawArrays);
    LOAD_GL_FUNC(DeleteVertexArrays);
    LOAD_GL_FUNC(DeleteBuffers);
    LOAD_GL_FUNC(DeleteProgram);
    return true;
}
#endif

#ifndef BLACKHOLE_USE_D3D11
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
    return content;
}

static GLuint compileShader(GLenum type, const std::string& source, FILE* debugLog) {
    GLuint shader = gl_CreateShader(type);
    const char* src = source.c_str();
    gl_ShaderSource(shader, 1, &src, nullptr);
    gl_CompileShader(shader);
    GLint ok = 0; gl_GetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    char log[4096]; gl_GetShaderInfoLog(shader, sizeof(log), nullptr, log);
    if (log[0] && debugLog) {
        fprintf(debugLog, "[SHADER_ERROR][%s] %s\n", type==GL_VERTEX_SHADER?"vert":"frag", log);
        fflush(debugLog);
    }
    if (!ok) {
        if (debugLog) {
            fprintf(debugLog, "[FAIL] Shader compilation failed (type=%s)\n", type==GL_VERTEX_SHADER?"vert":"frag");
            fprintf(debugLog, "[SOURCE] First 500 chars:\n%s\n", source.substr(0, 500).c_str());
            fflush(debugLog);
        }
        gl_DeleteShader(shader);
        return 0;
    }
    if (debugLog) { fprintf(debugLog, "[OK] Shader compiled (type=%s, ID=%u)\n", type==GL_VERTEX_SHADER?"vert":"frag", shader); fflush(debugLog); }
    return shader;
}

static GLuint createProgram(const std::string& vert, const std::string& frag, FILE* debugLog) {
    if (debugLog) { fprintf(debugLog, "[Init] Compiling vertex shader...\n"); fflush(debugLog); }
    GLuint vs = compileShader(GL_VERTEX_SHADER, vert, debugLog);
    if (!vs) return 0;
    if (debugLog) { fprintf(debugLog, "[Init] Compiling fragment shader...\n"); fflush(debugLog); }
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, frag, debugLog);
    if (!fs) { gl_DeleteShader(vs); return 0; }
    GLuint prog = gl_CreateProgram();
    gl_AttachShader(prog, vs); gl_AttachShader(prog, fs);
    gl_LinkProgram(prog);
    GLint ok = 0; gl_GetProgramiv(prog, GL_LINK_STATUS, &ok);
    char log[4096]; gl_GetProgramInfoLog(prog, sizeof(log), nullptr, log);
    if (log[0] && debugLog) {
        fprintf(debugLog, "[LINK_ERROR] %s\n", log);
        fflush(debugLog);
    }
    if (!ok) {
        if (debugLog) { fprintf(debugLog, "[FAIL] Program link failed\n"); fflush(debugLog); }
        gl_DeleteProgram(prog); gl_DeleteShader(vs); gl_DeleteShader(fs); return 0;
    }
    if (debugLog) { fprintf(debugLog, "[OK] Program linked (ID=%u)\n", prog); fflush(debugLog); }
    gl_DeleteShader(vs); gl_DeleteShader(fs);
    return prog;
}

static bool buildFragmentShader(std::string& out, FILE* debugLog) {
    std::string header = readFile("shaders/frag_desktop_header.glsl");
    std::string body   = readFile("blackhole.glsl");
    if (header.empty() || body.empty()) {
        if (debugLog) { fprintf(debugLog, "[FAIL] Shader file empty: header=%zu, body=%zu\n", header.size(), body.size()); fflush(debugLog); }
        return false;
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
        std::string p = std::string("const float ") + o.name + " = ";
        size_t pos = body.find(p);
        if (pos != std::string::npos) {
            size_t ve = body.find(";", pos);
            if (ve != std::string::npos) {
                std::string v = body.substr(pos + p.length(), ve - pos - p.length());
                body.replace(pos, ve - pos + 1,
                    std::string("float ") + o.name + " = " + o.uniform + " " + v + ";");
            }
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
            "               + wobAmp * vec2(cos(t * 0.8), sin(t * 1.0));";
        const std::string newCenter =
            "center = (uFollowMouse > 0)\n"
            "               ? clamp(vec2(TOKEN_HOME_X, TOKEN_HOME_Y), fullLo, fullHi)\n"
            "               : ((lo + hi) * 0.5 + wander * ampEff\n"
            "                  + wobAmp * vec2(cos(t * 0.8), sin(t * 1.0)));";
        size_t p = body.find(oldCenter);
        if (p != std::string::npos) body.replace(p, oldCenter.length(), newCenter);
    }

    // ---- Randomize trajectory: add uRandPhase to lissa calls ----
    {
        size_t p;
        while ((p = body.find("lissa(t * TOKEN_CALM)")) != std::string::npos)
            body.replace(p, 21, "lissa(t * TOKEN_CALM + uRandPhase)");
        while ((p = body.find("lissa(t * TOKEN_RUSH)")) != std::string::npos)
            body.replace(p, 21, "lissa(t * TOKEN_RUSH + uRandPhase)");
        while ((p = body.find("cos(t * 0.8)")) != std::string::npos)
            body.replace(p, 12, "cos((t + uRandPhase) * 0.8)");
        while ((p = body.find("sin(t * 1.0)")) != std::string::npos)
            body.replace(p, 12, "sin((t + uRandPhase) * 1.0)");
    }

    // ---- Runtime visual controls ----
    {
        const std::string oldDefl = "* window * shield;";
        const std::string newDefl = "* window * shield * max(uDistortion, 0.0);";
        size_t p = body.find(oldDefl);
        if (p != std::string::npos) body.replace(p, oldDefl.length(), newDefl);

        const std::string oldNear = "(p + (sp - p) * window * shield) / vec2(aspect, 1.0)";
        const std::string newNear = "(p + (sp - p) * window * shield * max(uDistortion, 0.0)) / vec2(aspect, 1.0)";
        p = body.find(oldNear);
        if (p != std::string::npos) body.replace(p, oldNear.length(), newNear);

        const std::string finalColor = "    fragColor = vec4(col, 1.0);";
        p = body.find(finalColor);
        if (p != std::string::npos) {
            body.replace(p, finalColor.length(),
                "    if (uScreenSwallow > 0) {\n"
                "        float swallow = (1.0 - uBornProgress) * smoothstep(1.15, 0.0, length((uv - center) * vec2(aspect, 1.0)));\n"
                "        col *= mix(1.0, 0.45, swallow);\n"
                "    }\n"
                "    fragColor = vec4(col, 1.0);");
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
#endif

// IAudioMeterInformation GUID (missing from MinGW headers)
static const GUID IID_IAudioMeterInformation2 = {0xC02216F6,0x8C67,0x4B5B,{0x9D,0x00,0xD0,0x08,0xE7,0x3E,0x00,0x64}};
struct IAudioMeterInformation2 : IUnknown {
    virtual HRESULT STDMETHODCALLTYPE GetPeakValue(float*) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetMeteringChannelCount(UINT*) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetChannelsPeakValues(UINT, float*) = 0;
    virtual HRESULT STDMETHODCALLTYPE QueryHardwareSupport(DWORD*) = 0;
};

// Get process name from PID
static void GetProcessName(DWORD pid, char* out, int maxLen) {
    out[0] = 0;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return;
    PROCESSENTRY32W peW = { sizeof(peW) };
    if (Process32FirstW(snap, &peW)) {
        do {
            if (peW.th32ProcessID == pid) {
                int len = WideCharToMultiByte(CP_UTF8, 0, peW.szExeFile, -1, out, maxLen, NULL, NULL);
                if (len == 0) { out[0] = 0; break; }
                for (int i = 0; out[i] && i < maxLen - 1; i++) {
                    unsigned char c = (unsigned char)out[i];
                    if (c < 0x80) out[i] = (char)tolower(c);
                }
                out[maxLen - 1] = 0;
                break;
            }
        } while (Process32NextW(snap, &peW));
    }
    CloseHandle(snap);
}

// Check if current foreground window is a video/game app that should suppress black hole
static bool isWatchingVideo() {
    HWND fg = GetForegroundWindow();
    if (!fg) return false;

    // 排除黑洞自己的渲染窗口（WS_EX_NOACTIVATE + WS_EX_TOPMOST + WS_EX_TRANSPARENT）
    {
        LONG_PTR ex = GetWindowLongPtrW(fg, GWL_EXSTYLE);
        if ((ex & (WS_EX_NOACTIVATE | WS_EX_TOPMOST | WS_EX_TRANSPARENT))
            == (WS_EX_NOACTIVATE | WS_EX_TOPMOST | WS_EX_TRANSPARENT))
            return false;
        // 也通过窗口类名排除
        wchar_t cls[64] = {};
        if (GetClassNameW(fg, cls, 64) && wcscmp(cls, L"BlackHoleWGL") == 0)
            return false;
    }

    // Method 1: D3D exclusive fullscreen (catches most fullscreen games)
    typedef enum { QUNS_NOT_PRESENT=1, QUNS_BUSY=2, QUNS_RUNNING_D3D_FULL_SCREEN=3,
                   QUNS_PRESENTATION_MODE=4 } QUNS;
    typedef HRESULT (WINAPI *PFN_QUNS)(QUNS*);
    static PFN_QUNS pfnQuns = nullptr;
    if (!pfnQuns) pfnQuns = (PFN_QUNS)GetProcAddress(GetModuleHandleA("shell32.dll"), "SHQueryUserNotificationState");
    if (pfnQuns) {
        QUNS state;
        if (SUCCEEDED(pfnQuns(&state)) && (state == QUNS_RUNNING_D3D_FULL_SCREEN || state == QUNS_PRESENTATION_MODE))
            return true;
    }

    // Method 1b: any foreground window covering entire screen (borderless fullscreen games)
    // 但排除最大化的普通窗口（只针对真正的全屏游戏）
    {
        RECT r;
        if (GetWindowRect(fg, &r)) {
            int sw = GetSystemMetrics(SM_CXSCREEN), sh = GetSystemMetrics(SM_CYSCREEN);
            int ww = r.right - r.left, wh = r.bottom - r.top;
            // 只有窗口完全覆盖屏幕且不是最大化窗口时才认为是全屏游戏
            // 最大化窗口 (WS_MAXIMIZE) 不算全屏，避免误判编辑器/浏览器
            LONG_PTR style = GetWindowLongPtrW(fg, GWL_STYLE);
            if (ww >= sw && wh >= sh && !(style & WS_MAXIMIZE)) return true;
        }
    }

    // Get foreground process name
    DWORD pid = 0;
    GetWindowThreadProcessId(fg, &pid);
    if (!pid) return false;
    char pname[260]; GetProcessName(pid, pname, sizeof(pname));
    if (!pname[0]) return false;

    // Check if process is a known video player, game launcher, or browser
    bool isDedicatedVideoPlayer = (strstr(pname, "vlc") || strstr(pname, "mpv") || strstr(pname, "potplayer") ||
                    strstr(pname, "mpc") || strstr(pname, "wmplayer") || strstr(pname, "bilibili") ||
                    strstr(pname, "哔哩哔哩") || strstr(pname, "bili") ||
                    strstr(pname, "iqiyi") || strstr(pname, "爱奇艺") ||
                    strstr(pname, "youku") || strstr(pname, "优酷") ||
                    strstr(pname, "mgtv") || strstr(pname, "芒果") ||
                    strstr(pname, "douyin") || strstr(pname, "抖音") ||
                    strstr(pname, "kuaishou") || strstr(pname, "快手") ||
                    strstr(pname, "腾讯视频") || strstr(pname, "qqlive") ||
                    strstr(pname, "nvidia"));
    bool isBrowser = (strstr(pname, "chrome") || strstr(pname, "msedge") || strstr(pname, "firefox") ||
                      strstr(pname, "opera") || strstr(pname, "brave"));
    // Common game launchers (Steam overlay, EOS, Ubisoft Connect, etc.)
    // These indicate user is likely in-game even if game exe name isn't matched
    bool isGameLauncher = (strstr(pname, "steam") || strstr(pname, "epic") || strstr(pname, "ubisoft") ||
                          strstr(pname, "ubiconnect") || strstr(pname, "eaapp") || strstr(pname, "origin") ||
                          strstr(pname, "battlenet") || strstr(pname, "riot") || strstr(pname, "gog") ||
                          strstr(pname, "xbox") || strstr(pname, "gamebar"));
    bool uwpDetected = false;
    // UWP apps run under ApplicationFrameHost.exe  check window title for media players
    bool isUWPVideo = false;
    if (strstr(pname, "applicationframehost")) {
        WCHAR wtitle[256] = {};
        GetWindowTextW(fg, wtitle, 256);
        if (wtitle[0]) {
            if (wcsstr(wtitle, L"电影") || wcsstr(wtitle, L"电视") || wcsstr(wtitle, L"媒体") ||
                wcsstr(wtitle, L"播放") || wcsstr(wtitle, L"视频") || wcsstr(wtitle, L"Movies") ||
                wcsstr(wtitle, L"Media") || wcsstr(wtitle, L"Player") || wcsstr(wtitle, L"Video") ||
                wcsstr(wtitle, L"TV") || wcsstr(wtitle, L"影视") || wcsstr(wtitle, L"Films")) {
                isUWPVideo = true; uwpDetected = true;
            }
        }
    }
    // Game launcher in foreground = user is gaming, skip audio check
    if (isGameLauncher) return true;
    // Not a known video app or browser  no need for audio check
    if (!isDedicatedVideoPlayer && !isBrowser && !isUWPVideo) return false;

    // For browsers: need window title with video keywords AND significant audio
    if (isBrowser && !uwpDetected) {
        WCHAR wtitle[256] = {};
        GetWindowTextW(fg, wtitle, 256);
        bool hasVideoKeyword = (wcsstr(wtitle, L"YouTube") || wcsstr(wtitle, L"Youtube") ||
                                wcsstr(wtitle, L"youtube") || wcsstr(wtitle, L"Bilibili") ||
                                wcsstr(wtitle, L"bilibili") || wcsstr(wtitle, L"哔哩") ||
                                wcsstr(wtitle, L"视频") || wcsstr(wtitle, L"Video") ||
                                wcsstr(wtitle, L"播放") || wcsstr(wtitle, L"Netflix") ||
                                wcsstr(wtitle, L"爱奇艺") || wcsstr(wtitle, L"优酷") ||
                                wcsstr(wtitle, L"腾讯") || wcsstr(wtitle, L"抖音") ||
                                wcsstr(wtitle, L"电影") || wcsstr(wtitle, L"Movie") ||
                                wcsstr(wtitle, L"直播") || wcsstr(wtitle, L"Live"));
        // 浏览器没有视频关键词标题，不阻止触发
        if (!hasVideoKeyword) return false;
    }

    // Method 3: check if this app has audio
    CoInitializeEx(NULL, COINIT_MULTITHREADED); // safe to call multiple times
    IMMDeviceEnumerator* en = nullptr;
    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL,
                                   __uuidof(IMMDeviceEnumerator), (void**)&en);
    if (FAILED(hr)) return false;

    IMMDevice* dev = nullptr;
    hr = en->GetDefaultAudioEndpoint(eRender, eConsole, &dev);
    en->Release();
    if (FAILED(hr)) return false;

    IAudioSessionManager2* mgr = nullptr;
    hr = dev->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, NULL, (void**)&mgr);
    dev->Release();
    if (FAILED(hr)) return false;

    IAudioSessionEnumerator* se = nullptr;
    hr = mgr->GetSessionEnumerator(&se);
    if (FAILED(hr)) { mgr->Release(); return false; }

    int count = 0; se->GetCount(&count);
    bool hasAudio = false;
    for (int i = 0; i < count && !hasAudio; i++) {
        IAudioSessionControl* sc = nullptr;
        if (FAILED(se->GetSession(i, &sc))) continue;
        IAudioSessionControl2* sc2 = nullptr;
        if (SUCCEEDED(sc->QueryInterface(__uuidof(IAudioSessionControl2), (void**)&sc2))) {
            DWORD spid; sc2->GetProcessId(&spid);
            if (spid != GetCurrentProcessId()) {
                // For UWP apps, check ANY audio (foreground is just the shell process)
                // For other apps, match by process name (handles multi-process: browsers, Electron, etc.)
                bool match;
                if (uwpDetected) {
                    match = true;  // match all sessions for UWP media apps
                    char spname[260]; GetProcessName(spid, spname, sizeof(spname));
                } else {
                    char spname[260]; GetProcessName(spid, spname, sizeof(spname));
                    match = spname[0] && (strcmp(spname, pname) == 0);
                }
                if (match) {
                    IAudioMeterInformation2* meter = nullptr;
                    if (SUCCEEDED(sc2->QueryInterface(IID_IAudioMeterInformation2, (void**)&meter))) {
                        float peak = 0; meter->GetPeakValue(&peak);
                        // 提高阈值，避免浏览器微小音频（广告、通知）误判
                        if (peak > 0.02f) hasAudio = true;
                        meter->Release();
                    }
                }
            }
            sc2->Release();
        }
        sc->Release();
    }
    se->Release(); mgr->Release();
    return hasAudio;
}

static bool isIdle(DWORD ms) {
    LASTINPUTINFO lii = { sizeof(LASTINPUTINFO) };
    return GetLastInputInfo(&lii) && (GetTickCount() - lii.dwTime) >= ms;
}

// ---- Renderer process management (monitor mode) ----
static PROCESS_INFORMATION g_pi = {};
static bool g_sessionLocked = false;  // 跟踪当前会话是否被锁屏

// ---- 一屏一黑洞 (displayMode==3): 子渲染器管理 ----
// 父渲染器为每个副屏 spawn 一个 "blackhole.exe --render --screen N" 子进程。
// 用 Job Object 保证父进程退出（含被强杀）时子进程自动终止。
static HWND FindWindowByPID(DWORD pid);  // 前向声明（定义在下方）
static HANDLE g_hChildJob = nullptr;
static std::vector<PROCESS_INFORMATION> g_childRenderers;

static void EnsureChildJob() {
    if (g_hChildJob) return;
    g_hChildJob = CreateJobObjectA(nullptr, nullptr);
    if (!g_hChildJob) return;
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION info = {};
    info.BasicLimitInformation.LimitFlags =
        JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    SetInformationJobObject(g_hChildJob, JobObjectExtendedLimitInformation,
                            &info, sizeof(info));
}

static void SpawnChildRenderer(const char* selfPath, int screenIdx) {
    EnsureChildJob();
    STARTUPINFOA si = {}; si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    char cmd[MAX_PATH + 32];
    snprintf(cmd, sizeof(cmd), "\"%s\" --render --screen %d", selfPath, screenIdx);
    PROCESS_INFORMATION pi = {};
    if (CreateProcessA(NULL, cmd, NULL, NULL, FALSE,
                       CREATE_SUSPENDED, NULL, NULL, &si, &pi)) {
        // 必须在挂起状态下加入 Job Object，再恢复主线程，最后关闭线程句柄。
        // 顺序不能错：AssignProcessToJobObject 要在 ResumeThread 之前完成，
        // 而 ResumeThread 需要的是线程句柄 (pi.hThread) 不是进程句柄。
        if (g_hChildJob) AssignProcessToJobObject(g_hChildJob, pi.hProcess);
        ResumeThread(pi.hThread);
        CloseHandle(pi.hThread);
        pi.hThread = NULL;
        g_childRenderers.push_back(pi);
    }
}

static void KillChildRenderers() {
    for (auto& pi : g_childRenderers) {
        if (!pi.hProcess) continue;
        HWND hwnd = FindWindowByPID(pi.dwProcessId);
        if (hwnd) PostMessage(hwnd, WM_CLOSE, 0, 0);
    }
    for (auto& pi : g_childRenderers) {
        if (!pi.hProcess) continue;
        if (WaitForSingleObject(pi.hProcess, 2000) == WAIT_TIMEOUT)
            TerminateProcess(pi.hProcess, 0);
        CloseHandle(pi.hProcess);
    }
    g_childRenderers.clear();
}

static void MonitorSpawn(const char* selfPath) {
    if (g_pi.hProcess) return;
    STARTUPINFOA si = {}; si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    char cmd[MAX_PATH + 16];
    snprintf(cmd, sizeof(cmd), "\"%s\" --render", selfPath);
    if (CreateProcessA(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &g_pi))
        CloseHandle(g_pi.hThread);
}

static HWND FindWindowByPID(DWORD pid) {
    struct Ctx { DWORD pid; HWND hwnd; } ctx = { pid, NULL };
    EnumWindows([](HWND h, LPARAM lp) -> BOOL {
        auto* c = (Ctx*)lp;
        DWORD p; GetWindowThreadProcessId(h, &p);
        if (p == c->pid) { c->hwnd = h; return FALSE; }
        return TRUE;
    }, (LPARAM)&ctx);
    return ctx.hwnd;
}

static void MonitorKill() {
    if (!g_pi.hProcess) return;
    HWND hwnd = FindWindowByPID(g_pi.dwProcessId);
    if (hwnd) {
        PostMessage(hwnd, WM_CLOSE, 0, 0);
        if (WaitForSingleObject(g_pi.hProcess, 2000) == WAIT_TIMEOUT)
            TerminateProcess(g_pi.hProcess, 0);
    } else {
        TerminateProcess(g_pi.hProcess, 0);
    }
    CloseHandle(g_pi.hProcess);
    g_pi.hProcess = NULL;
}

static bool MonitorRunning() {
    if (!g_pi.hProcess) return false;
    DWORD code;
    if (GetExitCodeProcess(g_pi.hProcess, &code) && code == STILL_ACTIVE)
        return true;
    CloseHandle(g_pi.hProcess);
    g_pi.hProcess = NULL;
    return false;
}

// ---- Main ----
int main(int argc, char* argv[]) {
    ShowWindow(GetConsoleWindow(), SW_HIDE);
    SetProcessDPIAware();  // 声明 DPI 感知，防止 Windows 虚拟化缩放

    // Set working directory to project root
    {
        char p[MAX_PATH]; GetModuleFileNameA(nullptr, p, MAX_PATH);
        char* s = strrchr(p, '\\'); if (s) *s = 0;
        s = strrchr(p, '\\');
        if (s && (strcmp(s+1,"build")==0 || strcmp(s+1,"Build")==0)) *s = 0;
        SetCurrentDirectoryA(p);
    }

    bool isRenderer = (argc >= 2 && strcmp(argv[1], "--render") == 0);
    bool isConfig = (argc >= 2 && strcmp(argv[1], "--config") == 0);
    bool isMonitor = (argc >= 2 && strcmp(argv[1], "--monitor") == 0);

    // --screen <idx>: 一屏一黑洞模式的子进程参数，指定渲染到第几个显示器
    // 父进程不传 --screen，子进程传 --screen N (N>=1)
    int screenIdx = -1;
    if (isRenderer && argc >= 4 && strcmp(argv[2], "--screen") == 0) {
        screenIdx = atoi(argv[3]);
    }

    // 直接写入调试文件（子进程用独立文件名避免冲突）
    char debugName[64] = "blackhole_debug.txt";
    if (screenIdx >= 0)
        snprintf(debugName, sizeof(debugName), "blackhole_debug_screen%d.txt", screenIdx);
    FILE* debugLog = fopen(debugName, "w");
    if (debugLog) {
        fprintf(debugLog, "========== BLACKHOLE START ==========\n");
        fprintf(debugLog, "[Init] isRenderer=%d, argc=%d\n", isRenderer, argc);
        if (argc >= 2) fprintf(debugLog, "[Init] argv[1]='%s'\n", argv[1]);
        fflush(debugLog);
    }
    // 让 WGC 捕获模块也把诊断写到同一个 debug 日志（用于排查黄边框）
    WGC_SetDebugLog(debugLog);

    // 主程序启动时杀掉旧的 blackhole 进程（避免新旧实例冲突）
    // --render 子进程不杀（它由 monitor 管理）
    if (!isRenderer) {
        DWORD myPid = GetCurrentProcessId();
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        PROCESSENTRY32 pe = { sizeof(pe) };
        if (Process32First(snap, &pe)) {
            do {
                if (stricmp(pe.szExeFile, "blackhole.exe") == 0 && pe.th32ProcessID != myPid) {
                    HANDLE h = OpenProcess(PROCESS_TERMINATE, FALSE, pe.th32ProcessID);
                    if (h) { TerminateProcess(h, 0); CloseHandle(h); }
                }
            } while (Process32Next(snap, &pe));
        }
        CloseHandle(snap);
        Sleep(200);
    }

    BlackholeConfig cfg;

    if (isRenderer) {
        // === RENDERER: load config from file ===
        char names[64][64];
        if (!LoadPresetsFromFile(cfg, names))
            InitDefaultPresets(cfg);
        LoadAdvancedConfig(cfg);
        cfg.mode = 0;

        // === 一屏一黑洞 (displayMode==3) 多进程分发 ===
        // 父进程 (screenIdx==-1): 枚举显示器，为副屏 spawn 子进程，本进程负责主屏
        // 子进程 (screenIdx>=1): 强制 displayMode=0，绑定到指定显示器
        if (screenIdx < 0 && cfg.displayMode == 3) {
            auto monitors = EnumerateMonitors();
            if (monitors.size() <= 1) {
                if (debugLog) { fprintf(debugLog, "[Init] 一屏一黑洞: 只有 %zu 屏, fallback 到 displayMode=0\n", monitors.size()); fflush(debugLog); }
                cfg.displayMode = 0;
            } else {
                char selfPath[MAX_PATH]; GetModuleFileNameA(nullptr, selfPath, MAX_PATH);
                // 为 monitors[1..N-1] 各 spawn 一个子进程
                for (size_t i = 1; i < monitors.size(); i++) {
                    SpawnChildRenderer(selfPath, (int)i);
                }
                // 本进程负责 monitors[0] (主屏)，降级为 displayMode=0
                cfg.displayMode = 0;
                if (debugLog) { fprintf(debugLog, "[Init] 一屏一黑洞: 父进程负责屏0, spawn %zu 个子进程\n", monitors.size() - 1); fflush(debugLog); }
            }
        } else if (screenIdx >= 0) {
            // 子进程: 强制 displayMode=0，显示器选择在下方用 screenIdx 覆盖
            cfg.displayMode = 0;
            if (debugLog) { fprintf(debugLog, "[Init] 子渲染器 screenIdx=%d, displayMode forced to 0\n", screenIdx); fflush(debugLog); }
        }
    } else if (isConfig) {
        // === CONFIG ONLY: show config panel, save and exit ===
        if (!glfwInit()) { fprintf(stderr, "glfwInit failed\n"); return 1; }
        if (!GUI_ShowConfigPanel(cfg)) { glfwTerminate(); return 0; }
        // Save config and exit
        char names[64][64] = {};
        SavePresetsToFile(cfg, names);
        SaveAdvancedConfig(cfg);
        glfwTerminate();
        return 0;
    } else {
        // === CONFIG + MONITOR (normal launch) or MONITOR ONLY (--monitor) ===
        char selfPath[MAX_PATH];
        GetModuleFileNameA(NULL, selfPath, MAX_PATH);

        if (isMonitor) {
            // --monitor: skip config panel, load from file
            char names[64][64];
            if (!LoadPresetsFromFile(cfg, names))
                InitDefaultPresets(cfg);
            LoadAdvancedConfig(cfg);
            if (debugLog) { fprintf(debugLog, "[Monitor] loaded idleSec=%d mode=%d\n", cfg.idleSec, cfg.mode); fflush(debugLog); }
        } else {
            // Normal launch: show config panel first
            if (!glfwInit()) { fprintf(stderr, "glfwInit failed\n"); return 1; }
            if (!GUI_ShowConfigPanel(cfg)) { glfwTerminate(); return 0; }
            char names[64][64] = {};
            SavePresetsToFile(cfg, names);
            SaveAdvancedConfig(cfg);
            glfwTerminate();
        }

        // === Tray icon monitor ===
        #define WM_TRAYICON (WM_USER + 1)
        #define ID_TRAY_EXIT 1001
        #define ID_TRAY_CONFIG 1002

        NOTIFYICONDATAA nid = {};
        nid.cbSize = sizeof(nid);
        nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
        nid.uCallbackMessage = WM_TRAYICON;
        nid.hIcon = LoadIconW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(100));
        strcpy(nid.szTip, "Black Hole Monitor");

        // Create hidden message-only window for tray
        WNDCLASSA wc = {};
        wc.lpfnWndProc = [](HWND h, UINT m, WPARAM w, LPARAM l) -> LRESULT {
            if (m == WM_TRAYICON && l == WM_RBUTTONUP) {
                HMENU menu = CreatePopupMenu();
                AppendMenuW(menu, MF_STRING, ID_TRAY_CONFIG, L"配置");
                AppendMenuW(menu, MF_STRING, ID_TRAY_EXIT, L"退出");
                POINT pt; GetCursorPos(&pt);
                SetForegroundWindow(h);
                TrackPopupMenu(menu, TPM_RIGHTALIGN|TPM_BOTTOMALIGN, pt.x, pt.y, 0, h, NULL);
                DestroyMenu(menu);
            }
            if (m == WM_COMMAND && LOWORD(w) == ID_TRAY_EXIT)
                PostQuitMessage(0);
            if (m == WM_COMMAND && LOWORD(w) == ID_TRAY_CONFIG) {
                // 启动配置界面
                auto* pSelf = (char*)GetWindowLongPtrA(h, GWLP_USERDATA);
                char cmd[MAX_PATH + 16];
                snprintf(cmd, sizeof(cmd), "\"%s\" --config", pSelf);
                STARTUPINFOA si = {}; si.cb = sizeof(si);
                PROCESS_INFORMATION pi;
                CreateProcessA(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
                CloseHandle(pi.hProcess);
                CloseHandle(pi.hThread);
            }
            if (m == WM_WTSSESSION_CHANGE) {
                // 跟踪会话锁/解锁：锁屏时不 spawn，解锁时立即重新评估
                if (w == WTS_SESSION_LOCK) {
                    g_sessionLocked = true;
                    // 锁屏时主动杀掉 renderer（双保险，避免 renderer 在锁屏期间继续消耗 GPU）
                    if (MonitorRunning()) MonitorKill();
                } else if (w == WTS_SESSION_UNLOCK) {
                    g_sessionLocked = false;
                }
                return 0;
            }
            if (m == WM_TIMER && w == 1) {
                auto* pSelf = (char*)GetWindowLongPtrA(h, GWLP_USERDATA);
                auto* pCfg  = (BlackholeConfig*)(pSelf + MAX_PATH);
                // 锁屏期间不 spawn renderer（避免 spin-kill 循环）
                if (g_sessionLocked) {
                    bool running = MonitorRunning();
                    if (running) MonitorKill();
                    return DefWindowProcA(h, m, w, l);
                }
                bool idle = isIdle((DWORD)pCfg->idleSec * 1000) && (pCfg->videoAsIdle || !isWatchingVideo());
                bool running = MonitorRunning();
                if (pCfg->mode == 0) {
                    if (!running) MonitorSpawn(pSelf);
                } else {
                    if (idle && !running) MonitorSpawn(pSelf);
                    if (!idle && running)  MonitorKill();
                }
            }
            return DefWindowProcA(h, m, w, l);
        };
        wc.hInstance = GetModuleHandleA(NULL);
        wc.lpszClassName = "BHMon";
        RegisterClassA(&wc);
        HWND monHwnd = CreateWindowA("BHMon", "", 0,0,0,0,0, NULL, NULL, wc.hInstance, NULL);

        // Store selfPath + cfg in window userdata for timer callback
        char userBuf[MAX_PATH + sizeof(BlackholeConfig)];
        memcpy(userBuf, selfPath, MAX_PATH);
        memcpy(userBuf + MAX_PATH, &cfg, sizeof(cfg));
        SetWindowLongPtrA(monHwnd, GWLP_USERDATA, (LONG_PTR)userBuf);

        nid.hWnd = monHwnd;
        Shell_NotifyIconA(NIM_ADD, &nid);

        // 注册会话通知（跟踪锁屏/解锁，避免锁屏后 renderer 失效、解锁后黑屏）
        WTSRegisterSessionNotification(monHwnd, NOTIFY_FOR_THIS_SESSION);

        // Start renderer immediately in mode 0
        if (cfg.mode == 0) MonitorSpawn(selfPath);

        SetTimer(monHwnd, 1, 1000, NULL);  // 1s detection for gaming responsiveness
        fprintf(stderr, "[Monitor] mode=%d idleSec=%d (tray icon ready)\n", cfg.mode, cfg.idleSec);

        MSG msg;
        while (GetMessageA(&msg, NULL, 0, 0)) { TranslateMessage(&msg); DispatchMessageA(&msg); }

        KillTimer(monHwnd, 1);
        WTSUnRegisterSessionNotification(monHwnd);
        Shell_NotifyIconA(NIM_DELETE, &nid);
        MonitorKill();
        return 0;
    }


    // === OpenGL/WGL ===
#ifndef BLACKHOLE_USE_D3D11
    // ---- 显示器选择 ----
    MonitorInfo monPrimary = GetPrimaryMonitor();
    MonitorInfo monSecondary = GetSecondaryMonitor();
    bool hasSecondary = (monSecondary.hMon != monPrimary.hMon);

    // 一屏一黑洞子进程: 覆盖 monPrimary 为指定显示器
    if (screenIdx >= 0) {
        auto monitors = EnumerateMonitors();
        if ((size_t)screenIdx < monitors.size()) {
            monPrimary = monitors[screenIdx];
            if (debugLog) { fprintf(debugLog, "[Init] screenIdx=%d → 绑定到显示器 (%d,%d %dx%d) primary=%d\n",
                screenIdx, monPrimary.rc.left, monPrimary.rc.top,
                monPrimary.rc.right - monPrimary.rc.left,
                monPrimary.rc.bottom - monPrimary.rc.top,
                (int)monPrimary.isPrimary); fflush(debugLog); }
        } else {
            if (debugLog) { fprintf(debugLog, "[WARN] screenIdx=%d 超出显示器数量, fallback 主屏\n", screenIdx); fflush(debugLog); }
        }
    }

    int winX, winY, winW, winH;
    if (cfg.displayMode == 0) {
        // 主屏
        winX = monPrimary.rc.left; winY = monPrimary.rc.top;
        winW = monPrimary.rc.right - monPrimary.rc.left;
        winH = monPrimary.rc.bottom - monPrimary.rc.top;
    } else if (cfg.displayMode == 1) {
        // 副屏（无副屏则 fallback 到主屏）
        winX = monSecondary.rc.left; winY = monSecondary.rc.top;
        winW = monSecondary.rc.right - monSecondary.rc.left;
        winH = monSecondary.rc.bottom - monSecondary.rc.top;
    } else {
        // 主+副穿梭（虚拟桌面）
        winX = GetSystemMetrics(SM_XVIRTUALSCREEN);
        winY = GetSystemMetrics(SM_YVIRTUALSCREEN);
        winW = GetSystemMetrics(SM_CXVIRTUALSCREEN);
        winH = GetSystemMetrics(SM_CYVIRTUALSCREEN);
        if (!hasSecondary) {
            if (debugLog) { fprintf(debugLog, "[WARN] No secondary monitor, cross-screen mode fallback to primary\n"); fflush(debugLog); }
            winX = monPrimary.rc.left; winY = monPrimary.rc.top;
            winW = monPrimary.rc.right - monPrimary.rc.left;
            winH = monPrimary.rc.bottom - monPrimary.rc.top;
        }
    }
    if (debugLog) { fprintf(debugLog, "[Init] displayMode=%d window=%dx%d @(%d,%d)\n", cfg.displayMode, winW, winH, winX, winY); fflush(debugLog); }

    // ---- Create fullscreen black hole window via Win32 + WGL ----
    char winTitle[64];
    snprintf(winTitle, sizeof(winTitle), "BH_%u", GetCurrentProcessId());
    Win32GL wgl;
    if (debugLog) { fprintf(debugLog, "[Init] Creating window...\n"); fflush(debugLog); }
    if (!Win32GL_Init(wgl, winTitle, winX, winY, winW, winH)) {
        if (debugLog) { fprintf(debugLog, "[FAIL] Win32GL_Init failed!\n"); fclose(debugLog); }
        return 1;
    }
    if (debugLog) { fprintf(debugLog, "[OK] Window created: %dx%d @(%d,%d)\n", wgl.width, wgl.height, wgl.targetX, wgl.targetY); fflush(debugLog); }

    setbuf(stderr, NULL);

    if (debugLog) { fprintf(debugLog, "[Init] Loading GL functions...\n"); fflush(debugLog); }
    if (!loadGLFunctions()) {
        if (debugLog) { fprintf(debugLog, "[FAIL] loadGLFunctions failed!\n"); fclose(debugLog); }
        Win32GL_Shutdown(wgl); return 1;
    }
    if (debugLog) { fprintf(debugLog, "[OK] GL functions loaded\n"); fflush(debugLog); }

    // ---- Capture: auto-detect border support, allow manual override ----
    // captureMode: -1=auto, 0=WGC, 1=DXGI
    // displayMode 2 (跨屏) 时两个捕获器都建，其余只建主屏一个
    WGCCapture wgcPri, wgcSec; DXGICapture dxgiPri, dxgiSec;
    bool useWGC = false;
    if (cfg.captureMode == 0) {
        useWGC = true;
    } else if (cfg.captureMode == 1) {
        useWGC = false;
    } else {
        if (debugLog) { fprintf(debugLog, "[Init] Auto-detecting capture backend...\n"); fflush(debugLog); }
        useWGC = WGC_ProbeBorderSupport();
        if (debugLog) { fprintf(debugLog, "[Init] Auto-detect result: useWGC=%d\n", (int)useWGC); fflush(debugLog); }
    }

    bool crossScreen = (cfg.displayMode == 2) && hasSecondary;
    HMONITOR hMonPri = monPrimary.hMon;
    HMONITOR hMonSec = monSecondary.hMon;
    // 副屏模式：主捕获器要绑到副屏 hMon
    if (cfg.displayMode == 1) hMonPri = hMonSec;

    int capW=0, capH=0; bool capOk;
    if (useWGC) {
        if (debugLog) { fprintf(debugLog, "[Init] Initializing WGC primary...\n"); fflush(debugLog); }
        capOk = WGC_Init(wgcPri, hMonPri); capW=wgcPri.width; capH=wgcPri.height;
        if (!capOk) {
            if (debugLog) { fprintf(debugLog, "[FAIL] WGC_Init primary failed!\n"); fclose(debugLog); }
            Win32GL_Shutdown(wgl); return 1;
        }
        if (crossScreen) {
            if (debugLog) { fprintf(debugLog, "[Init] Initializing WGC secondary...\n"); fflush(debugLog); }
            if (!WGC_Init(wgcSec, hMonSec)) {
                if (debugLog) { fprintf(debugLog, "[WARN] WGC secondary failed, falling back to primary-only\n"); fflush(debugLog); }
                crossScreen = false;
            }
        }
    } else {
        if (debugLog) { fprintf(debugLog, "[Init] Initializing DXGI primary...\n"); fflush(debugLog); }
        capOk = DXGI_Init(dxgiPri, hMonPri); capW=dxgiPri.width; capH=dxgiPri.height;
        if (!capOk) {
            if (debugLog) { fprintf(debugLog, "[FAIL] DXGI_Init primary failed!\n"); fclose(debugLog); }
            Win32GL_Shutdown(wgl); return 1;
        }
        if (crossScreen) {
            if (debugLog) { fprintf(debugLog, "[Init] Initializing DXGI secondary...\n"); fflush(debugLog); }
            if (!DXGI_Init(dxgiSec, hMonSec)) {
                if (debugLog) { fprintf(debugLog, "[WARN] DXGI secondary failed, falling back to primary-only\n"); fflush(debugLog); }
                crossScreen = false;
            }
        }
    }
    if (debugLog) { fprintf(debugLog, "[OK] Primary capture: %dx%d, crossScreen=%d\n", capW, capH, (int)crossScreen); fflush(debugLog); }

    GLTextureUpload glTex;
    if (debugLog) { fprintf(debugLog, "[Init] Creating GL texture...\n"); fflush(debugLog); }
    if (!GLTex_Init(glTex, wgl.width, wgl.height)) {
        if (debugLog) { fprintf(debugLog, "[FAIL] GLTex_Init failed!\n"); fclose(debugLog); }
        if (useWGC) { WGC_Release(wgcPri); if (crossScreen) WGC_Release(wgcSec); }
        else { DXGI_Release(dxgiPri); if (crossScreen) DXGI_Release(dxgiSec); }
        Win32GL_Shutdown(wgl); return 1;
    }
    if (debugLog) { fprintf(debugLog, "[OK] GL texture created: ID=%u\n", glTex.tex); fflush(debugLog); }

    // ---- Shader ----
    if (debugLog) { fprintf(debugLog, "[Init] Loading shaders...\n"); fflush(debugLog); }
    std::string vertSrc = readFile("shaders/vert.glsl");
    std::string fragSrc;
    if (vertSrc.empty()) {
        if (debugLog) { fprintf(debugLog, "[FAIL] vert.glsl not found or empty!\n"); fclose(debugLog); }
        GLTex_Shutdown(glTex);
        if (useWGC) { WGC_Release(wgcPri); if (crossScreen) WGC_Release(wgcSec); }
        else { DXGI_Release(dxgiPri); if (crossScreen) DXGI_Release(dxgiSec); }
        Win32GL_Shutdown(wgl); return 1;
    }
    if (debugLog) { fprintf(debugLog, "[OK] vert.glsl loaded (%zu bytes)\n", vertSrc.size()); fflush(debugLog); }

    if (!buildFragmentShader(fragSrc, debugLog)) {
        if (debugLog) { fprintf(debugLog, "[FAIL] buildFragmentShader failed!\n"); fclose(debugLog); }
        GLTex_Shutdown(glTex);
        if (useWGC) { WGC_Release(wgcPri); if (crossScreen) WGC_Release(wgcSec); }
        else { DXGI_Release(dxgiPri); if (crossScreen) DXGI_Release(dxgiSec); }
        Win32GL_Shutdown(wgl); return 1;
    }
    if (debugLog) { fprintf(debugLog, "[OK] Fragment shader built (%zu bytes)\n", fragSrc.size()); fflush(debugLog); }

    GLuint program = createProgram(vertSrc, fragSrc, debugLog);
    if (!program) {
        if (debugLog) { fprintf(debugLog, "[CRITICAL] Shader program creation FAILED!\n"); fclose(debugLog); }
        GLTex_Shutdown(glTex);
        if (useWGC) { WGC_Release(wgcPri); if (crossScreen) WGC_Release(wgcSec); }
        else { DXGI_Release(dxgiPri); if (crossScreen) DXGI_Release(dxgiSec); }
        Win32GL_Shutdown(wgl); return 1;
    }
    if (debugLog) { fprintf(debugLog, "[OK] Shader program created (ID=%u)\n", program); fflush(debugLog); }

    // Full-screen quad
    float verts[] = { -1,-1, 1,-1, -1,1, 1,1 };
    GLuint vao, vbo;
    gl_GenVertexArrays(1, &vao); gl_GenBuffers(1, &vbo);
    gl_BindVertexArray(vao);
    gl_BindBuffer(GL_ARRAY_BUFFER, vbo);
    gl_BufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
    gl_VertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2*sizeof(float), (void*)0);
    gl_EnableVertexAttribArray(0);
    gl_BindVertexArray(0);

    GLint locRes   = gl_GetUniformLocation(program, "iResolution");
    GLint locTime  = gl_GetUniformLocation(program, "iTime");
    GLint locDate  = gl_GetUniformLocation(program, "iDate");
    GLint locCh0   = gl_GetUniformLocation(program, "iChannel0");
    GLint loc_uHR  = gl_GetUniformLocation(program, "uHoleRadius");
    GLint loc_uDG  = gl_GetUniformLocation(program, "uDiskGain");
    GLint loc_uDT  = gl_GetUniformLocation(program, "uDiskTemp");
    GLint loc_uEX  = gl_GetUniformLocation(program, "uExposure");
    GLint loc_uSP  = gl_GetUniformLocation(program, "uSpeed");
    GLint loc_uSG  = gl_GetUniformLocation(program, "uStarGain");
    GLint loc_uDI  = gl_GetUniformLocation(program, "uDiskIncl");
    GLint loc_uPM  = gl_GetUniformLocation(program, "uPlayMode");
    GLint loc_uSlot = gl_GetUniformLocation(program, "uSlotSec");
    GLint loc_uPC   = gl_GetUniformLocation(program, "uPresetCount");
    GLint loc_uPT   = gl_GetUniformLocation(program, "uPresetTemp");
    GLint loc_uPI   = gl_GetUniformLocation(program, "uPresetIncl");
    GLint loc_uPR   = gl_GetUniformLocation(program, "uPresetRoll");
    GLint loc_uPN   = gl_GetUniformLocation(program, "uPresetInner");
    GLint loc_uPO   = gl_GetUniformLocation(program, "uPresetOuter");
    GLint loc_uPP   = gl_GetUniformLocation(program, "uPresetOpac");
    GLint loc_uPD   = gl_GetUniformLocation(program, "uPresetDopp");
    GLint loc_uPB   = gl_GetUniformLocation(program, "uPresetBeam");
    GLint loc_uPG   = gl_GetUniformLocation(program, "uPresetGain");
    GLint loc_uPCo  = gl_GetUniformLocation(program, "uPresetContr");
    GLint loc_uPW   = gl_GetUniformLocation(program, "uPresetWind");
    GLint loc_uPS   = gl_GetUniformLocation(program, "uPresetSpd");
    GLint loc_uPE   = gl_GetUniformLocation(program, "uPresetExpo");
    GLint loc_uPSt  = gl_GetUniformLocation(program, "uPresetStar");
    GLint locBorn   = gl_GetUniformLocation(program, "uBornProgress");
    GLint locFixedSz  = gl_GetUniformLocation(program, "uFixedSize");
    GLint locFixedLvl = gl_GetUniformLocation(program, "uFixedLevel");
    GLint locGrowEnabled = gl_GetUniformLocation(program, "uGrowEnabled");
    GLint locInitialSize = gl_GetUniformLocation(program, "uInitialSize");
    GLint locScreenSwallow = gl_GetUniformLocation(program, "uScreenSwallow");
    GLint locDistortion = gl_GetUniformLocation(program, "uDistortion");
    GLint locHomeX  = gl_GetUniformLocation(program, "uHomeX");
    GLint locHomeY  = gl_GetUniformLocation(program, "uHomeY");
    GLint locFollowMouse = gl_GetUniformLocation(program, "uFollowMouse");
    GLint locPhase  = gl_GetUniformLocation(program, "uRandPhase");
    GLint locPresetOff = gl_GetUniformLocation(program, "uPresetOffset");

    // ---- 随机化初始位置、轨迹和预设 ----
    // 一屏一黑洞: 子进程用 screenIdx 给种子加偏移，确保各屏黑洞行为独立。
    // 否则同秒启动的父子进程会拿到相同的 time(nullptr) → 相同的 rand() 序列 →
    // 相同的 home/phase/presetOff，两个屏的黑洞动作完全同步。
    unsigned seed = (unsigned)time(nullptr) + (unsigned)(screenIdx + 1) * 2654435761u;
    srand(seed);
    // 随机出生位置：避免边缘，范围 [0.15, 0.85]
    float randHomeX = 0.15f + 0.70f * (float)rand() / (float)RAND_MAX;
    float randHomeY = 0.15f + 0.70f * (float)rand() / (float)RAND_MAX;
    // 随机轨迹相位：0 ~ 2π
    float randPhase = 6.2831853f * (float)rand() / (float)RAND_MAX;
    // 随机预设偏移：0 ~ 60秒（覆盖多个预设周期）
    float randPresetOff = 60.0f * (float)rand() / (float)RAND_MAX;
    float homeX = cfg.randomPath ? randHomeX : 0.96f;
    float homeY = cfg.randomPath ? randHomeY : 0.04f;
    float cursorHomeX = homeX;
    float cursorHomeY = homeY;
    float phaseOffset = cfg.randomPath ? randPhase : 0.0f;
    float presetOffset = cfg.randomPath ? randPresetOff : 0.0f;
    if (debugLog) { fprintf(debugLog, "[Init] Spawn: randomPath=%d home=(%.2f,%.2f) phase=%.2f presetOff=%.1f seed=%u (screenIdx=%d)\n",
                            cfg.randomPath ? 1 : 0, homeX, homeY, phaseOffset, presetOffset, seed, screenIdx); fflush(debugLog); }

    gl_UseProgram(0);

    // ---- 预热捕获并获取多帧确保稳定 ----
    if (debugLog) { fprintf(debugLog, "[Init] Warming up %s capture...\n", useWGC ? "WGC" : "DXGI"); fflush(debugLog); }
    int stableFrames = 0;
    const int requiredStableFrames = 5;
    int warmupAttempts = 0;
    const int maxWarmupAttempts = 300;  // 跨屏要更久

    while (stableFrames < requiredStableFrames && warmupAttempts < maxWarmupAttempts) {
        bool gotPri = false;
        ID3D11Texture2D* framePri = useWGC ? WGC_GetFrame(wgcPri) : DXGI_GetFrame(dxgiPri);
        if (framePri) {
            gotPri = true;
            D3D11_TEXTURE2D_DESC desc; framePri->GetDesc(&desc);
            int fw = (int)desc.Width, fh = (int)desc.Height;
            int dstX = (cfg.displayMode == 1) ? 0 : (monPrimary.rc.left - winX);
            int dstY = (cfg.displayMode == 1) ? 0 : (monPrimary.rc.top - winY);
            D3D11_MAPPED_SUBRESOURCE mapped;
            if (useWGC ? WGC_CopyToStaging(wgcPri, framePri, mapped) : DXGI_CopyToStaging(dxgiPri, framePri, mapped)) {
                GLTex_UploadRegion(glTex, mapped.pData, (int)mapped.RowPitch, dstX, dstY, fw, fh);
                if (useWGC) WGC_UnmapStaging(wgcPri); else DXGI_UnmapStaging(dxgiPri);
                stableFrames++;
                unsigned char* pData = (unsigned char*)mapped.pData;
                int sum = 0;
                for (int i = 0; i < 100; i++) sum += pData[i];
                if (debugLog) { fprintf(debugLog, "[Warmup] Frame %d/%d pri(%dx%d) sum=%d\n", stableFrames, requiredStableFrames, fw, fh, sum); fflush(debugLog); }
            }
            framePri->Release();
            if (!useWGC) DXGI_ReleaseFrame(dxgiPri);
        }
        // 副屏帧（仅跨屏模式）
        if (crossScreen) {
            ID3D11Texture2D* frameSec = useWGC ? WGC_GetFrame(wgcSec) : DXGI_GetFrame(dxgiSec);
            if (frameSec) {
                D3D11_TEXTURE2D_DESC desc; frameSec->GetDesc(&desc);
                int fw = (int)desc.Width, fh = (int)desc.Height;
                int dstX = monSecondary.rc.left - winX;
                int dstY = monSecondary.rc.top - winY;
                D3D11_MAPPED_SUBRESOURCE mapped;
                if (useWGC ? WGC_CopyToStaging(wgcSec, frameSec, mapped) : DXGI_CopyToStaging(dxgiSec, frameSec, mapped)) {
                    GLTex_UploadRegion(glTex, mapped.pData, (int)mapped.RowPitch, dstX, dstY, fw, fh);
                    if (useWGC) WGC_UnmapStaging(wgcSec); else DXGI_UnmapStaging(dxgiSec);
                }
                frameSec->Release();
                if (!useWGC) DXGI_ReleaseFrame(dxgiSec);
            }
        }
        if (!gotPri) Sleep(16);
        warmupAttempts++;
        Win32GL_PollEvents(wgl);
    }

    if (stableFrames >= requiredStableFrames) {
        if (debugLog) { fprintf(debugLog, "[OK] %s warmup complete: %d frames\n", useWGC ? "WGC" : "DXGI", stableFrames); fflush(debugLog); }
    } else {
        if (debugLog) { fprintf(debugLog, "[WARN] Only %d frames after %d attempts\n", stableFrames, warmupAttempts); fflush(debugLog); }
    }

    // ---- 显示窗口（屏幕外初始化已完成，移入并显示） ----
    if (debugLog) { fprintf(debugLog, "[Init] Showing window...\n"); fflush(debugLog); }
    Win32GL_Show(wgl);
    Sleep(50);
    Win32GL_PollEvents(wgl);

    // 先渲染一帧保证窗口有内容（用预热阶段已经上传过的纹理，不再重新取帧）
    {
        int fbW=wgl.width, fbH=wgl.height;
        glViewport(0,0,fbW,fbH); glClearColor(0,0,0,1); glClear(GL_COLOR_BUFFER_BIT);
        gl_UseProgram(program);
        gl_ActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, GLTex_GetTexture(glTex));
        gl_Uniform1i(locCh0,0);
        gl_Uniform3f(locRes,(float)fbW,(float)fbH,0);
        gl_Uniform1f(locTime,0);
        gl_Uniform4f(locDate,0,0,0,(float)time(nullptr));
        gl_Uniform1f(loc_uHR,cfg.holeRadius); gl_Uniform1f(loc_uDG,cfg.diskGain);
        gl_Uniform1f(loc_uDT,cfg.diskTemp); gl_Uniform1f(loc_uEX,cfg.exposure);
        gl_Uniform1f(loc_uSP,cfg.spd); gl_Uniform1f(loc_uSG,cfg.starGain);
        gl_Uniform1f(loc_uDI,cfg.diskIncl);
        gl_Uniform1i(loc_uPM,cfg.playMode); gl_Uniform1f(loc_uSlot,cfg.slotSec);
        gl_Uniform1i(loc_uPC,cfg.presetCount);
        { float buf[64];
        for(int i=0;i<cfg.presetCount;i++)buf[i]=cfg.presets[i].temp; gl_Uniform1fv(loc_uPT,cfg.presetCount,buf);
        for(int i=0;i<cfg.presetCount;i++)buf[i]=cfg.presets[i].incl; gl_Uniform1fv(loc_uPI,cfg.presetCount,buf);
        for(int i=0;i<cfg.presetCount;i++)buf[i]=cfg.presets[i].roll; gl_Uniform1fv(loc_uPR,cfg.presetCount,buf);
        for(int i=0;i<cfg.presetCount;i++)buf[i]=cfg.presets[i].inner;gl_Uniform1fv(loc_uPN,cfg.presetCount,buf);
        for(int i=0;i<cfg.presetCount;i++)buf[i]=cfg.presets[i].outer;gl_Uniform1fv(loc_uPO,cfg.presetCount,buf);
        for(int i=0;i<cfg.presetCount;i++)buf[i]=cfg.presets[i].opac; gl_Uniform1fv(loc_uPP,cfg.presetCount,buf);
        for(int i=0;i<cfg.presetCount;i++)buf[i]=cfg.presets[i].dopp; gl_Uniform1fv(loc_uPD,cfg.presetCount,buf);
        for(int i=0;i<cfg.presetCount;i++)buf[i]=cfg.presets[i].beam; gl_Uniform1fv(loc_uPB,cfg.presetCount,buf);
        for(int i=0;i<cfg.presetCount;i++)buf[i]=cfg.presets[i].gain; gl_Uniform1fv(loc_uPG,cfg.presetCount,buf);
        for(int i=0;i<cfg.presetCount;i++)buf[i]=cfg.presets[i].contr;gl_Uniform1fv(loc_uPCo,cfg.presetCount,buf);
        for(int i=0;i<cfg.presetCount;i++)buf[i]=cfg.presets[i].wind; gl_Uniform1fv(loc_uPW,cfg.presetCount,buf);
        for(int i=0;i<cfg.presetCount;i++)buf[i]=cfg.presets[i].speed;gl_Uniform1fv(loc_uPS,cfg.presetCount,buf);
        for(int i=0;i<cfg.presetCount;i++)buf[i]=cfg.presets[i].expo; gl_Uniform1fv(loc_uPE,cfg.presetCount,buf);
        for(int i=0;i<cfg.presetCount;i++)buf[i]=cfg.presets[i].star; gl_Uniform1fv(loc_uPSt,cfg.presetCount,buf); }
        gl_Uniform1i(locFixedSz, cfg.fixedSize ? 1 : 0);
        gl_Uniform1f(locFixedLvl, cfg.fixedLevel);
        gl_Uniform1i(locGrowEnabled, cfg.growEnabled ? 1 : 0);
        gl_Uniform1f(locInitialSize, cfg.initialSize);
        gl_Uniform1i(locScreenSwallow, cfg.screenSwallow ? 1 : 0);
        gl_Uniform1f(locDistortion, cfg.distortion);
        gl_Uniform1f(locBorn, 0.01f);
        gl_Uniform1f(locHomeX, homeX);
        gl_Uniform1f(locHomeY, homeY);
        gl_Uniform1i(locFollowMouse, cfg.followMouse ? 1 : 0);
        gl_Uniform1f(locPhase, phaseOffset);
        gl_Uniform1f(locPresetOff, presetOffset);
        gl_BindVertexArray(vao); gl_DrawArrays(GL_TRIANGLE_STRIP,0,4); gl_BindVertexArray(0);
        gl_UseProgram(0); Win32GL_SwapBuffers(wgl);
    }

    // 启用分层模式（鼠标穿透）
    Win32GL_EnableLayered(wgl);
    // 不再隐藏系统光标 — WGC 已通过 IsCursorCaptureEnabled=false 禁用光标捕获，
    // 捕获的纹理不含光标，不会出现双重光标，系统光标始终保持正常可用

    if (debugLog) { fprintf(debugLog, "[OK] Ready, entering main loop\n"); fclose(debugLog); debugLog = nullptr; }

    // ---- Main loop ----
    double startTime = Win32GL_GetTime();
    double bornStart = startTime;
    const double BORN_DURATION = 0.8;
    const double DIE_DURATION = 0.5;
    float bornProgress = 0.01f;
    bool exiting = false;
    double exitStart = 0;
    int frames = 0; double lastFps = startTime;
    char title[128];

    if (!useWGC) {
        ID3D11Texture2D* f = DXGI_GetFrame(dxgiPri);
        if (f) { f->Release(); DXGI_ReleaseFrame(dxgiPri); }
        if (crossScreen) {
            ID3D11Texture2D* f2 = DXGI_GetFrame(dxgiSec);
            if (f2) { f2->Release(); DXGI_ReleaseFrame(dxgiSec); }
        }
    }

    while (true) {
        if (exiting) {
            // 退出渐出：只处理消息，不检查 shouldClose
            Win32GL_DrainMessages(wgl);
        } else {
            if (!Win32GL_PollEvents(wgl)) {
                exiting = true;
                exitStart = Win32GL_GetTime();
                wgl.active = true;  // 恢复active，让退出动画能渲染
            }
        }
        int fbW, fbH; Win32GL_GetFramebufferSize(wgl, &fbW, &fbH);
        glViewport(0, 0, fbW, fbH);

        // 退出时跳过捕获，避免卡顿
        if (!exiting) {
            // 主屏帧
            if (!useWGC) DXGI_ReleaseFrame(dxgiPri);
            ID3D11Texture2D* framePri = useWGC ? WGC_GetFrame(wgcPri) : DXGI_GetFrame(dxgiPri);
            if (framePri) {
                D3D11_TEXTURE2D_DESC desc; framePri->GetDesc(&desc);
                int fw = (int)desc.Width, fh = (int)desc.Height;
                int dstX = (cfg.displayMode == 1) ? 0 : (monPrimary.rc.left - winX);
                int dstY = (cfg.displayMode == 1) ? 0 : (monPrimary.rc.top - winY);
                D3D11_MAPPED_SUBRESOURCE mapped;
                if (useWGC ? WGC_CopyToStaging(wgcPri, framePri, mapped)
                           : DXGI_CopyToStaging(dxgiPri, framePri, mapped)) {
                    GLTex_UploadRegion(glTex, mapped.pData, (int)mapped.RowPitch, dstX, dstY, fw, fh);
                    if (useWGC) WGC_UnmapStaging(wgcPri); else DXGI_UnmapStaging(dxgiPri);
                }
                framePri->Release();
            }
            // 副屏帧（仅跨屏模式）
            if (crossScreen) {
                if (!useWGC) DXGI_ReleaseFrame(dxgiSec);
                ID3D11Texture2D* frameSec = useWGC ? WGC_GetFrame(wgcSec) : DXGI_GetFrame(dxgiSec);
                if (frameSec) {
                    D3D11_TEXTURE2D_DESC desc; frameSec->GetDesc(&desc);
                    int fw = (int)desc.Width, fh = (int)desc.Height;
                    int dstX = monSecondary.rc.left - winX;
                    int dstY = monSecondary.rc.top - winY;
                    D3D11_MAPPED_SUBRESOURCE mapped;
                    if (useWGC ? WGC_CopyToStaging(wgcSec, frameSec, mapped)
                               : DXGI_CopyToStaging(dxgiSec, frameSec, mapped)) {
                        GLTex_UploadRegion(glTex, mapped.pData, (int)mapped.RowPitch, dstX, dstY, fw, fh);
                        if (useWGC) WGC_UnmapStaging(wgcSec); else DXGI_UnmapStaging(dxgiSec);
                    }
                    frameSec->Release();
                }
            }
        }

        double now = Win32GL_GetTime();
        float t = (float)(now - startTime);
        float ep = (float)time(nullptr);
        float frameHomeX = homeX;
        float frameHomeY = homeY;
        if (cfg.followMouse) {
            float inertia = cfg.mouseInertia;
            if (inertia < 0.0f) inertia = 0.0f;
            if (inertia > 1.0f) inertia = 1.0f;

            POINT cursorPos;
            if (GetCursorPos(&cursorPos)) {
                float targetX = ((float)cursorPos.x - (float)wgl.targetX) / (float)((wgl.width > 0) ? wgl.width : 1);
                float targetY = ((float)cursorPos.y - (float)wgl.targetY) / (float)((wgl.height > 0) ? wgl.height : 1);
                if (targetX < 0.0f) targetX = 0.0f;
                if (targetX > 1.0f) targetX = 1.0f;
                if (targetY < 0.0f) targetY = 0.0f;
                if (targetY > 1.0f) targetY = 1.0f;

                if (inertia <= 0.0001f) {
                    cursorHomeX = targetX;
                    cursorHomeY = targetY;
                } else {
                    float followAlpha = 0.42f - 0.36f * inertia;
                    if (followAlpha < 0.06f) followAlpha = 0.06f;
                    if (followAlpha > 0.42f) followAlpha = 0.42f;
                    cursorHomeX += (targetX - cursorHomeX) * followAlpha;
                    cursorHomeY += (targetY - cursorHomeY) * followAlpha;
                }
            }
            frameHomeX = cursorHomeX;
            frameHomeY = cursorHomeY;

            if (inertia > 0.0001f) {
                float wanderRadius = 0.035f * inertia;
                float wanderX = cosf(t * 1.7f + phaseOffset) * wanderRadius;
                float wanderY = sinf(t * 1.3f + phaseOffset * 1.37f) * wanderRadius * 0.65f;
                frameHomeX += wanderX;
                frameHomeY += wanderY;
            }

            if (frameHomeX < 0.0f) frameHomeX = 0.0f;
            if (frameHomeX > 1.0f) frameHomeX = 1.0f;
            if (frameHomeY < 0.0f) frameHomeY = 0.0f;
            if (frameHomeY > 1.0f) frameHomeY = 1.0f;
        }

        // 黑洞生长/湮灭进度
        if (!exiting) {
            bornProgress = (float)((now - bornStart) / BORN_DURATION);
            if (bornProgress > 1.0f) bornProgress = 1.0f;
            if (bornProgress < 0.01f) bornProgress = 0.01f;
        } else {
            bornProgress = 1.0f - (float)((now - exitStart) / DIE_DURATION);
            if (bornProgress < 0.01f) {
                // 先隐藏窗口，让用户感知瞬间结束，后台再清理资源
                Win32GL_Hide(wgl);
                break;
            }
        }

        glClearColor(0,0,0,1); glClear(GL_COLOR_BUFFER_BIT);
        gl_UseProgram(program);

        gl_ActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, GLTex_GetTexture(glTex));
        gl_Uniform1i(locCh0, 0);
        gl_Uniform3f(locRes, (float)fbW, (float)fbH, 0.0f);
        gl_Uniform1f(locTime, t);
        gl_Uniform4f(locDate, 0,0,0,ep);

        gl_Uniform1f(loc_uHR, cfg.holeRadius);
        gl_Uniform1f(loc_uDG, cfg.diskGain);
        gl_Uniform1f(loc_uDT, cfg.diskTemp);
        gl_Uniform1f(loc_uEX, cfg.exposure);
        gl_Uniform1f(loc_uSP, cfg.spd);
        gl_Uniform1f(loc_uSG, cfg.starGain);
        gl_Uniform1f(loc_uDI, cfg.diskIncl);
        gl_Uniform1i(loc_uPM, cfg.playMode);
        gl_Uniform1f(loc_uSlot, cfg.slotSec);
        gl_Uniform1i(loc_uPC, cfg.presetCount);
        {
            float buf[64];
            for (int i = 0; i < cfg.presetCount; i++) buf[i] = cfg.presets[i].temp;
            gl_Uniform1fv(loc_uPT, cfg.presetCount, buf);
            for (int i = 0; i < cfg.presetCount; i++) buf[i] = cfg.presets[i].incl;
            gl_Uniform1fv(loc_uPI, cfg.presetCount, buf);
            for (int i = 0; i < cfg.presetCount; i++) buf[i] = cfg.presets[i].roll;
            gl_Uniform1fv(loc_uPR, cfg.presetCount, buf);
            for (int i = 0; i < cfg.presetCount; i++) buf[i] = cfg.presets[i].inner;
            gl_Uniform1fv(loc_uPN, cfg.presetCount, buf);
            for (int i = 0; i < cfg.presetCount; i++) buf[i] = cfg.presets[i].outer;
            gl_Uniform1fv(loc_uPO, cfg.presetCount, buf);
            for (int i = 0; i < cfg.presetCount; i++) buf[i] = cfg.presets[i].opac;
            gl_Uniform1fv(loc_uPP, cfg.presetCount, buf);
            for (int i = 0; i < cfg.presetCount; i++) buf[i] = cfg.presets[i].dopp;
            gl_Uniform1fv(loc_uPD, cfg.presetCount, buf);
            for (int i = 0; i < cfg.presetCount; i++) buf[i] = cfg.presets[i].beam;
            gl_Uniform1fv(loc_uPB, cfg.presetCount, buf);
            for (int i = 0; i < cfg.presetCount; i++) buf[i] = cfg.presets[i].gain;
            gl_Uniform1fv(loc_uPG, cfg.presetCount, buf);
            for (int i = 0; i < cfg.presetCount; i++) buf[i] = cfg.presets[i].contr;
            gl_Uniform1fv(loc_uPCo, cfg.presetCount, buf);
            for (int i = 0; i < cfg.presetCount; i++) buf[i] = cfg.presets[i].wind;
            gl_Uniform1fv(loc_uPW, cfg.presetCount, buf);
            for (int i = 0; i < cfg.presetCount; i++) buf[i] = cfg.presets[i].speed;
            gl_Uniform1fv(loc_uPS, cfg.presetCount, buf);
            for (int i = 0; i < cfg.presetCount; i++) buf[i] = cfg.presets[i].expo;
            gl_Uniform1fv(loc_uPE, cfg.presetCount, buf);
            for (int i = 0; i < cfg.presetCount; i++) buf[i] = cfg.presets[i].star;
            gl_Uniform1fv(loc_uPSt, cfg.presetCount, buf);
        }
        gl_Uniform1i(locFixedSz, cfg.fixedSize ? 1 : 0);
        gl_Uniform1f(locFixedLvl, cfg.fixedLevel);
        gl_Uniform1i(locGrowEnabled, cfg.growEnabled ? 1 : 0);
        gl_Uniform1f(locInitialSize, cfg.initialSize);
        gl_Uniform1i(locScreenSwallow, cfg.screenSwallow ? 1 : 0);
        gl_Uniform1f(locDistortion, cfg.distortion);
        gl_Uniform1f(locBorn, bornProgress);
        gl_Uniform1f(locHomeX, frameHomeX);
        gl_Uniform1f(locHomeY, frameHomeY);
        gl_Uniform1i(locFollowMouse, cfg.followMouse ? 1 : 0);
        gl_Uniform1f(locPhase, phaseOffset);
        gl_Uniform1f(locPresetOff, presetOffset);

        gl_BindVertexArray(vao);
        gl_DrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        gl_BindVertexArray(0);
        gl_UseProgram(0);

        Win32GL_SwapBuffers(wgl);

        frames++;
        if (now - lastFps >= 1.0) {
            snprintf(title, sizeof(title), "Black Hole [%d FPS]", frames);
            Win32GL_SetWindowTitle(wgl, title);
            frames=0; lastFps=now;
        }
    }

    GLTex_Shutdown(glTex);
    if (useWGC) { WGC_Release(wgcPri); if (crossScreen) WGC_Release(wgcSec); }
    else { DXGI_Release(dxgiPri); if (crossScreen) DXGI_Release(dxgiSec); }
    gl_DeleteProgram(program);
    gl_DeleteVertexArrays(1, &vao);
    gl_DeleteBuffers(1, &vbo);
    Win32GL_Shutdown(wgl);

    // 一屏一黑洞: 终止所有子渲染器 (Job Object 也会兜底)
    KillChildRenderers();
#else
    {
        Win32Window win;
        if (!Win32Window_Init(win, "Black Hole [D3D11]", 0, 0, 0, 0)) return 1;
        Win32Window_ShowSystemCursor(false);
        int fbW = win.width, fbH = win.height;
        WGCCapture wgc; DXGICapture dxgi; bool useWGC=true;
        if (!WGC_Init(wgc, nullptr)) { Win32Window_Shutdown(win); return 1; }
        D3D11Renderer r;
        if (!r.Init(win.hwnd, fbW, fbH, wgc.d3dDev, wgc.d3dCtx)) { WGC_Release(wgc); Win32Window_Shutdown(win); return 1; }
        double st = Win32Window_GetTime(); int fr=0; double lf=st; char tt[128];
        while (Win32Window_PollEvents(win)) {
            ID3D11Texture2D* frTex = WGC_GetFrame(wgc);
            if (!frTex) continue;
            TextureFrame tf = {}; tf.d3dTex=frTex; tf.valid=true;
            double nw = Win32Window_GetTime();
            float tm=(float)(nw-st), ep=(float)time(nullptr);
            BlackHoleUniforms u = {};
            u.iResolution[0]=(float)fbW; u.iResolution[1]=(float)fbH; u.iTime=tm; u.iDate[3]=ep;
            u.holeRadius=cfg.holeRadius; u.diskGain=cfg.diskGain; u.diskTemp=cfg.diskTemp; u.exposure=cfg.exposure; u.speed=cfg.spd; u.starGain=cfg.starGain; u.diskIncl=cfg.diskIncl; u.playMode=cfg.playMode; u.slotSec=cfg.slotSec; u.presetCount=cfg.presetCount;
            for(int i=0;i<cfg.presetCount&&i<64;i++){u.presetTemp[i]=cfg.presets[i].temp;u.presetIncl[i]=cfg.presets[i].incl;u.presetRoll[i]=cfg.presets[i].roll;u.presetInner[i]=cfg.presets[i].inner;u.presetOuter[i]=cfg.presets[i].outer;u.presetOpac[i]=cfg.presets[i].opac;u.presetDopp[i]=cfg.presets[i].dopp;u.presetBeam[i]=cfg.presets[i].beam;u.presetGain[i]=cfg.presets[i].gain;u.presetContr[i]=cfg.presets[i].contr;u.presetWind[i]=cfg.presets[i].wind;u.presetSpeed[i]=cfg.presets[i].speed;u.presetExpo[i]=cfg.presets[i].expo;u.presetStar[i]=cfg.presets[i].star;}
            r.Render(tf, u);
        }
        r.Shutdown(); if(useWGC)WGC_Release(wgc);else DXGI_Release(dxgi); Win32Window_ShowSystemCursor(true); Win32Window_Shutdown(win);
    }
#endif
    return 0;
}
