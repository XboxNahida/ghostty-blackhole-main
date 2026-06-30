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

#include "capture_wgc.h"
#include "capture_dxgi.h"
#include "gl_texture.h"
#include "gui_config.h"
#include "win32_gl.h"
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

    // Remove time wrapping from hole size: grow to full and stay there
    {
        size_t lp = body.find("mod(iTime, DEMO_SEC) / DEMO_GROW_SEC");
        if (lp != std::string::npos)
            body.replace(lp, 36, "iTime / DEMO_GROW_SEC");
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

    // 直接写入调试文件
    FILE* debugLog = fopen("blackhole_debug.txt", "w");
    if (debugLog) {
        fprintf(debugLog, "========== BLACKHOLE START ==========\n");
        fprintf(debugLog, "[Init] isRenderer=%d, argc=%d\n", isRenderer, argc);
        if (argc >= 2) fprintf(debugLog, "[Init] argv[1]='%s'\n", argv[1]);
        fflush(debugLog);
    }

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
        cfg.mode = 0;
    } else if (isConfig) {
        // === CONFIG ONLY: show config panel, save and exit ===
        if (!glfwInit()) { fprintf(stderr, "glfwInit failed\n"); return 1; }
        if (!GUI_ShowConfigPanel(cfg)) { glfwTerminate(); return 0; }
        // Save config and exit
        char names[64][64] = {};
        SavePresetsToFile(cfg, names);
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
            if (debugLog) { fprintf(debugLog, "[Monitor] loaded idleSec=%d mode=%d\n", cfg.idleSec, cfg.mode); fflush(debugLog); }
        } else {
            // Normal launch: show config panel first
            if (!glfwInit()) { fprintf(stderr, "glfwInit failed\n"); return 1; }
            if (!GUI_ShowConfigPanel(cfg)) { glfwTerminate(); return 0; }
            char names[64][64] = {};
            SavePresetsToFile(cfg, names);
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
            if (m == WM_TIMER && w == 1) {
                auto* pSelf = (char*)GetWindowLongPtrA(h, GWLP_USERDATA);
                auto* pCfg  = (BlackholeConfig*)(pSelf + MAX_PATH);
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

        // Start renderer immediately in mode 0
        if (cfg.mode == 0) MonitorSpawn(selfPath);

        SetTimer(monHwnd, 1, 1000, NULL);  // 1s detection for gaming responsiveness
        fprintf(stderr, "[Monitor] mode=%d idleSec=%d (tray icon ready)\n", cfg.mode, cfg.idleSec);

        MSG msg;
        while (GetMessageA(&msg, NULL, 0, 0)) { TranslateMessage(&msg); DispatchMessageA(&msg); }

        KillTimer(monHwnd, 1);
        Shell_NotifyIconA(NIM_DELETE, &nid);
        MonitorKill();
        return 0;
    }


    // === OpenGL/WGL ===
#ifndef BLACKHOLE_USE_D3D11
    // ---- Create fullscreen black hole window via Win32 + WGL ----
    char winTitle[64];
    snprintf(winTitle, sizeof(winTitle), "BH_%u", GetCurrentProcessId());
    Win32GL wgl;
    if (debugLog) { fprintf(debugLog, "[Init] Creating window...\n"); fflush(debugLog); }
    if (!Win32GL_Init(wgl, winTitle, 0, 0)) {
        if (debugLog) { fprintf(debugLog, "[FAIL] Win32GL_Init failed!\n"); fclose(debugLog); }
        return 1;
    }
    if (debugLog) { fprintf(debugLog, "[OK] Window created: %dx%d\n", wgl.width, wgl.height); fflush(debugLog); }

    setbuf(stderr, NULL);

    if (debugLog) { fprintf(debugLog, "[Init] Loading GL functions...\n"); fflush(debugLog); }
    if (!loadGLFunctions()) {
        if (debugLog) { fprintf(debugLog, "[FAIL] loadGLFunctions failed!\n"); fclose(debugLog); }
        Win32GL_Shutdown(wgl); return 1;
    }
    if (debugLog) { fprintf(debugLog, "[OK] GL functions loaded\n"); fflush(debugLog); }

    // ---- Capture (WGC default) ----
    WGCCapture wgc; DXGICapture dxgi;
    bool useWGC = true;
    int capW=0, capH=0; bool capOk;
    if (debugLog) { fprintf(debugLog, "[Init] Initializing WGC capture...\n"); fflush(debugLog); }
    capOk = WGC_Init(wgc); capW=wgc.width; capH=wgc.height;
    if (!capOk) {
        if (debugLog) { fprintf(debugLog, "[FAIL] WGC_Init failed!\n"); fclose(debugLog); }
        Win32GL_Shutdown(wgl); return 1;
    }
    if (debugLog) { fprintf(debugLog, "[OK] WGC initialized: %dx%d\n", capW, capH); fflush(debugLog); }

    GLTextureUpload glTex;
    if (debugLog) { fprintf(debugLog, "[Init] Creating GL texture...\n"); fflush(debugLog); }
    if (!GLTex_Init(glTex, capW, capH)) {
        if (debugLog) { fprintf(debugLog, "[FAIL] GLTex_Init failed!\n"); fclose(debugLog); }
        WGC_Release(wgc); Win32GL_Shutdown(wgl); return 1;
    }
    if (debugLog) { fprintf(debugLog, "[OK] GL texture created: ID=%u\n", glTex.tex); fflush(debugLog); }

    // ---- Shader ----
    if (debugLog) { fprintf(debugLog, "[Init] Loading shaders...\n"); fflush(debugLog); }
    std::string vertSrc = readFile("shaders/vert.glsl");
    std::string fragSrc;
    if (vertSrc.empty()) {
        if (debugLog) { fprintf(debugLog, "[FAIL] vert.glsl not found or empty!\n"); fclose(debugLog); }
        GLTex_Shutdown(glTex); WGC_Release(wgc); Win32GL_Shutdown(wgl); return 1;
    }
    if (debugLog) { fprintf(debugLog, "[OK] vert.glsl loaded (%zu bytes)\n", vertSrc.size()); fflush(debugLog); }
    
    if (!buildFragmentShader(fragSrc, debugLog)) {
        if (debugLog) { fprintf(debugLog, "[FAIL] buildFragmentShader failed!\n"); fclose(debugLog); }
        GLTex_Shutdown(glTex); WGC_Release(wgc); Win32GL_Shutdown(wgl); return 1;
    }
    if (debugLog) { fprintf(debugLog, "[OK] Fragment shader built (%zu bytes)\n", fragSrc.size()); fflush(debugLog); }
    
    GLuint program = createProgram(vertSrc, fragSrc, debugLog);
    if (!program) { 
        if (debugLog) { fprintf(debugLog, "[CRITICAL] Shader program creation FAILED!\n"); fclose(debugLog); }
        GLTex_Shutdown(glTex); WGC_Release(wgc); Win32GL_Shutdown(wgl); return 1; 
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
    GLint locHomeX  = gl_GetUniformLocation(program, "uHomeX");
    GLint locHomeY  = gl_GetUniformLocation(program, "uHomeY");
    GLint locPhase  = gl_GetUniformLocation(program, "uRandPhase");
    GLint locPresetOff = gl_GetUniformLocation(program, "uPresetOffset");

    // ---- 随机化初始位置、轨迹和预设 ----
    srand((unsigned)time(nullptr));
    // 随机出生位置：避免边缘，范围 [0.15, 0.85]
    float randHomeX = 0.15f + 0.70f * (float)rand() / (float)RAND_MAX;
    float randHomeY = 0.15f + 0.70f * (float)rand() / (float)RAND_MAX;
    // 随机轨迹相位：0 ~ 2π
    float randPhase = 6.2831853f * (float)rand() / (float)RAND_MAX;
    // 随机预设偏移：0 ~ 60秒（覆盖多个预设周期）
    float randPresetOff = 60.0f * (float)rand() / (float)RAND_MAX;
    if (debugLog) { fprintf(debugLog, "[Init] Random spawn: home=(%.2f,%.2f) phase=%.2f presetOff=%.1f\n",
                            randHomeX, randHomeY, randPhase, randPresetOff); fflush(debugLog); }

    gl_UseProgram(0);

    // ---- 预热 WGC 并获取多帧确保稳定 ----
    if (debugLog) { fprintf(debugLog, "[Init] Warming up WGC capture...\n"); fflush(debugLog); }
    int stableFrames = 0;
    const int requiredStableFrames = 5;
    int warmupAttempts = 0;
    const int maxWarmupAttempts = 150;
    
    while (stableFrames < requiredStableFrames && warmupAttempts < maxWarmupAttempts) {
        ID3D11Texture2D* frame = WGC_GetFrame(wgc);
        if (frame) {
            D3D11_TEXTURE2D_DESC desc; frame->GetDesc(&desc);
            int fw = (int)desc.Width, fh = (int)desc.Height;
            if (fw == glTex.width && fh == glTex.height) {
                D3D11_MAPPED_SUBRESOURCE mapped;
                if (WGC_CopyToStaging(wgc, frame, mapped)) {
                    GLTex_Upload(glTex, mapped.pData, (int)mapped.RowPitch);
                    WGC_UnmapStaging(wgc);
                    stableFrames++;
                    unsigned char* pData = (unsigned char*)mapped.pData;
                    int sum = 0;
                    for (int i = 0; i < 100; i++) sum += pData[i];
                    if (debugLog) { fprintf(debugLog, "[Warmup] Frame %d/%d (%dx%d) sum=%d\n", stableFrames, requiredStableFrames, fw, fh, sum); fflush(debugLog); }
                }
            }
            frame->Release();
        } else {
            Sleep(16);
        }
        warmupAttempts++;
        Win32GL_PollEvents(wgl);
    }
    
    if (stableFrames >= requiredStableFrames) {
        if (debugLog) { fprintf(debugLog, "[OK] WGC warmup complete: %d frames\n", stableFrames); fflush(debugLog); }
    } else {
        if (debugLog) { fprintf(debugLog, "[WARN] Only %d frames after %d attempts\n", stableFrames, warmupAttempts); fflush(debugLog); }
    }

    // ---- 显示窗口（屏幕外初始化已完成，移入并显示） ----
    if (debugLog) { fprintf(debugLog, "[Init] Showing window...\n"); fflush(debugLog); }
    Win32GL_Show(wgl);
    Sleep(50);
    Win32GL_PollEvents(wgl);

    // 先渲染一帧保证窗口有内容
    {
        ID3D11Texture2D* frame = WGC_GetFrame(wgc);
        if (frame) {
            D3D11_TEXTURE2D_DESC desc; frame->GetDesc(&desc);
            if ((int)desc.Width==glTex.width && (int)desc.Height==glTex.height) {
                D3D11_MAPPED_SUBRESOURCE mapped;
                if (WGC_CopyToStaging(wgc, frame, mapped))
                    { GLTex_Upload(glTex, mapped.pData, (int)mapped.RowPitch); WGC_UnmapStaging(wgc); }
            }
            frame->Release();
        }
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
        gl_Uniform1f(locBorn, 0.01f);
        gl_Uniform1f(locHomeX, randHomeX);
        gl_Uniform1f(locHomeY, randHomeY);
        gl_Uniform1f(locPhase, randPhase);
        gl_Uniform1f(locPresetOff, randPresetOff);
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

    if (!useWGC) { ID3D11Texture2D* f = DXGI_GetFrame(dxgi); if (f) f->Release(); }

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
            if (!useWGC) DXGI_ReleaseFrame(dxgi);
            ID3D11Texture2D* frame = useWGC ? WGC_GetFrame(wgc) : DXGI_GetFrame(dxgi);

            if (frame) {
                D3D11_TEXTURE2D_DESC desc; frame->GetDesc(&desc);
                int fw=(int)desc.Width, fh=(int)desc.Height;
                if (fw!=glTex.width || fh!=glTex.height) GLTex_Resize(glTex, fw, fh);
                if (fw==glTex.width && fh==glTex.height) {
                    D3D11_MAPPED_SUBRESOURCE mapped;
                    if ((useWGC ? WGC_CopyToStaging(wgc,frame,mapped) : DXGI_CopyToStaging(dxgi,frame,mapped))) {
                        GLTex_Upload(glTex, mapped.pData, (int)mapped.RowPitch);
                        if (useWGC) WGC_UnmapStaging(wgc); else DXGI_UnmapStaging(dxgi);
                    }
                }
                frame->Release();
            }
        }

        double now = Win32GL_GetTime();
        float t = (float)(now - startTime);
        float ep = (float)time(nullptr);

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
        gl_Uniform1f(locBorn, bornProgress);
        gl_Uniform1f(locHomeX, randHomeX);
        gl_Uniform1f(locHomeY, randHomeY);
        gl_Uniform1f(locPhase, randPhase);
        gl_Uniform1f(locPresetOff, randPresetOff);

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
    if (useWGC) WGC_Release(wgc); else DXGI_Release(dxgi);
    gl_DeleteProgram(program);
    gl_DeleteVertexArrays(1, &vao);
    gl_DeleteBuffers(1, &vbo);
    Win32GL_Shutdown(wgl);
#else
    {
        Win32Window win;
        if (!Win32Window_Init(win, "Black Hole [D3D11]", 0, 0, 0, 0)) return 1;
        Win32Window_ShowSystemCursor(false);
        int fbW = win.width, fbH = win.height;
        WGCCapture wgc; DXGICapture dxgi; bool useWGC=true;
        if (!WGC_Init(wgc)) { Win32Window_Shutdown(win); return 1; }
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
