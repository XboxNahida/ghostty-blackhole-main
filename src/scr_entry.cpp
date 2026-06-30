// scr_entry.cpp — Windows 原生屏保实现
// 处理 /s (全屏屏保) /p (预览) /c (配置) /a (密码)
// 独立于原有 blackhole.exe，不修改任何现有文件

#include "scr_entry.h"
#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include <cmath>
#include <ctime>
#include <string>
#include <fstream>
#include <sstream>
#include <cstring>
#include <windows.h>
#include <d3d11.h>

#include <GLFW/glfw3.h>
#include <GL/glcorearb.h>

#include "capture_wgc.h"
#include "capture_dxgi.h"
#include "gl_texture.h"
#include "win32_gl.h"
#include "gui_config.h"

// ========================= Logging (file scope) =========================
static FILE* g_log = nullptr;
static void ScrLogInit() {
    if (!g_log) {
        char path[MAX_PATH];
        GetTempPathA(MAX_PATH, path);
        strcat(path, "blackhole_scr.log");
        g_log = fopen(path, "w");
        if (g_log) setbuf(g_log, NULL);
    }
}
static HANDLE g_hLog = INVALID_HANDLE_VALUE;
static void ScrLog(const char* fmt, ...) {
    if (g_hLog == INVALID_HANDLE_VALUE) {
        g_hLog = CreateFileW(L"C:\\Temp\\blackhole_scr.log",
            GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    }
    if (g_hLog == INVALID_HANDLE_VALUE) return;
    char buf[2048];
    va_list ap; va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf)-2, fmt, ap);
    va_end(ap);
    if (n < 0) return;
    if (n >= (int)sizeof(buf)-2) n = sizeof(buf)-3;
    buf[n] = '\r'; buf[n+1] = '\n';
    DWORD written;
    WriteFile(g_hLog, buf, n+2, &written, nullptr);
    FlushFileBuffers(g_hLog);
}

// ========================= Crash diagnostics =========================
static LONG WINAPI ScrExceptionHandler(_EXCEPTION_POINTERS* ep) {
    ScrLog("[SCR] CRASH: exception code=0x%08X at %p\n",
           ep->ExceptionRecord->ExceptionCode,
           ep->ExceptionRecord->ExceptionAddress);
    return EXCEPTION_EXECUTE_HANDLER;
}

// ========================= OpenGL function loading =========================

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
    if (!gl_##name) { ScrLog("[SCR] Failed to load gl" #name "\n"); return false; } \
} while(0)

static bool loadGLFunctions() {
    LOAD_GL_FUNC(CreateShader);  LOAD_GL_FUNC(ShaderSource);
    LOAD_GL_FUNC(CompileShader); LOAD_GL_FUNC(GetShaderiv);
    LOAD_GL_FUNC(GetShaderInfoLog); LOAD_GL_FUNC(CreateProgram);
    LOAD_GL_FUNC(AttachShader);  LOAD_GL_FUNC(LinkProgram);
    LOAD_GL_FUNC(GetProgramiv);  LOAD_GL_FUNC(GetProgramInfoLog);
    LOAD_GL_FUNC(DeleteShader);  LOAD_GL_FUNC(UseProgram);
    LOAD_GL_FUNC(GetUniformLocation); LOAD_GL_FUNC(Uniform3f);
    LOAD_GL_FUNC(Uniform1f);     LOAD_GL_FUNC(Uniform1i);
    LOAD_GL_FUNC(ActiveTexture); LOAD_GL_FUNC(Uniform4f);
    LOAD_GL_FUNC(Uniform1fv);
    LOAD_GL_FUNC(GenVertexArrays); LOAD_GL_FUNC(GenBuffers);
    LOAD_GL_FUNC(BindVertexArray); LOAD_GL_FUNC(BindBuffer);
    LOAD_GL_FUNC(BufferData);    LOAD_GL_FUNC(VertexAttribPointer);
    LOAD_GL_FUNC(EnableVertexAttribArray); LOAD_GL_FUNC(DrawArrays);
    LOAD_GL_FUNC(DeleteVertexArrays); LOAD_GL_FUNC(DeleteBuffers);
    LOAD_GL_FUNC(DeleteProgram);
    return true;
}

// ========================= Shader build helpers =========================

static std::string getExeDir() {
    char p[MAX_PATH];
    GetModuleFileNameA(nullptr, p, MAX_PATH);
    char* s = strrchr(p, '\\');
    if (s) *s = 0;
    return std::string(p);
}

static std::string readFileEx(const char* relativePath) {
    ScrLog("[SCR] readFileEx('%s')\n", relativePath); 
    std::string base = getExeDir();
    std::string full = base + "\\" + relativePath;
    ScrLog("[SCR] resolved path: '%s'\n", full.c_str());
    {
        std::ifstream f(full, std::ios::in | std::ios::binary);
        if (f) { std::stringstream ss; ss << f.rdbuf(); return ss.str(); }
    }
    // 2. Fallback to cwd (development in build/ folder)
    {
        std::ifstream f(relativePath, std::ios::in | std::ios::binary);
        if (f) { std::stringstream ss; ss << f.rdbuf(); return ss.str(); }
    }
    ScrLog("[SCR] Cannot open %s (tried %s)\n", relativePath, full.c_str());
    return "";
}

static GLuint compileShaderScr(GLenum type, const std::string& source) {
    GLuint shader = gl_CreateShader(type);
    const GLchar* src = reinterpret_cast<const GLchar*>(source.c_str());
    gl_ShaderSource(shader, 1, &src, nullptr);
    gl_CompileShader(shader);
    GLint ok = 0; gl_GetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    GLchar log[4096]; gl_GetShaderInfoLog(shader, sizeof(log), nullptr, log);
    if (log[0]) ScrLog("[SCR][%s] %s\n", type == GL_VERTEX_SHADER ? "vert" : "frag", log);
    if (!ok) { gl_DeleteShader(shader); return 0; }
    return shader;
}

static GLuint createProgramScr(const std::string& vert, const std::string& frag) {
    GLuint vs = compileShaderScr(GL_VERTEX_SHADER, vert);
    if (!vs) return 0;
    GLuint fs = compileShaderScr(GL_FRAGMENT_SHADER, frag);
    if (!fs) { gl_DeleteShader(vs); return 0; }
    GLuint prog = gl_CreateProgram();
    gl_AttachShader(prog, vs); gl_AttachShader(prog, fs);
    gl_LinkProgram(prog);
    GLint ok = 0; gl_GetProgramiv(prog, GL_LINK_STATUS, &ok);
    GLchar log[4096]; gl_GetProgramInfoLog(prog, sizeof(log), nullptr, log);
    if (log[0]) ScrLog("[SCR] Link: %s\n", log);
    if (!ok) { gl_DeleteProgram(prog); gl_DeleteShader(vs); gl_DeleteShader(fs); return 0; }
    gl_DeleteShader(vs); gl_DeleteShader(fs);
    return prog;
}

// 构建着色器源码（与 main.cpp 相同逻辑但独立实现）
static bool buildFragmentShaderScr(std::string& out) {
    ScrLog("[SCR] buildFragmentShaderScr: reading header\n"); 
    std::string header = readFileEx("shaders/frag_desktop_header.glsl");
    ScrLog("[SCR] header read: %d chars\n", (int)header.size());
    std::string body = readFileEx("blackhole.glsl");
    ScrLog("[SCR] blackhole read: %d chars\n", (int)body.size());
    if (header.empty() || body.empty()) return false;

    struct { const char* name; const char* uniform; } ov[] = {
        {"HOLE_RADIUS", "uHoleRadius > 0.0 ? uHoleRadius :"}, {"DISK_GAIN", "uDiskGain > 0.0 ? uDiskGain :"},
        {"DISK_TEMP", "uDiskTemp > 0.0 ? uDiskTemp :"}, {"EXPOSURE", "uExposure > 0.0 ? uExposure :"},
        {"DRIFT_SPEED", "uSpeed > 0.0 ? uSpeed :"}, {"STAR_GAIN", "uStarGain > 0.0 ? uStarGain :"},
        {"DISK_INCL", "uDiskIncl > 0.0 ? uDiskIncl :"},
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
                    "DiskLook demoPreset(int i) { return DiskLook(\n"
                    "    uPresetTemp[i], uPresetIncl[i], uPresetRoll[i],\n"
                    "    uPresetInner[i], uPresetOuter[i], uPresetOpac[i],\n"
                    "    uPresetDopp[i], uPresetBeam[i], uPresetGain[i],\n"
                    "    uPresetContr[i], uPresetWind[i], uPresetSpd[i],\n"
                    "    uPresetExpo[i], uPresetStar[i]); }\n"
                    "DiskLook demoLook() {\n"
                    "  if (uPresetCount > 0) {\n"
                    "    int n = int(clamp(float(uPresetCount), 1.0, float(MAX_PRESETS)));\n"
                    "    float f; int i0, i1;\n"
                    "    if (uPlayMode == 0) {\n"
                    "      float raw = iTime / max(uSlotSec, 0.5);\n"
                    "      f = smoothstep(1.0 - DEMO_XFADE, 1.0, fract(raw));\n"
                    "      i0 = int(min(raw, float(n) - 0.001)); i1 = int(min(raw + 1.0, float(n) - 0.001));\n"
                    "    } else if (uPlayMode == 2) {\n"
                    "      float raw = iTime / max(uSlotSec, 0.5);\n"
                    "      f = smoothstep(1.0 - DEMO_XFADE, 1.0, fract(raw));\n"
                    "      int slot = int(raw);\n"
                    "      i0 = int(fract(sin(float(slot)*127.1+311.7)*43758.5453)*float(n));\n"
                    "      i1 = int(fract(sin(float(slot+1)*127.1+311.7)*43758.5453)*float(n));\n"
                    "    } else {\n"
                    "      float raw = iTime / max(uSlotSec, 0.5);\n"
                    "      f = smoothstep(1.0 - DEMO_XFADE, 1.0, fract(raw));\n"
                    "      i0 = int(raw) % n; i1 = (int(raw)+1) % n;\n"
                    "    }\n"
                    "    return mixLook(demoPreset(i0), demoPreset(i1), f);\n"
                    "  } else {\n"
                    "    float u = mod(iTime, DEMO_SEC) / DEMO_SEC * float(DEMO_N);\n"
                    "    int i = int(min(u, float(DEMO_N)-0.001));\n"
                    "    float f = smoothstep(1.0 - DEMO_XFADE, 1.0, fract(u));\n"
                    "    return mixLook(DEMO_TOUR[i], DEMO_TOUR[(i+1)%DEMO_N], f);\n"
                    "  }\n"
                    "}\n";
                body.replace(dlo, dle - dlo + 1, newFunc);
            }
        }
    }

    size_t pos = body.find("#define SIZE_MODE MODE_TOKENS");
    if (pos != std::string::npos) body.replace(pos, 29, "#define SIZE_MODE MODE_DEMO");
    {
        size_t lp = body.find("mod(iTime, DEMO_SEC) / DEMO_GROW_SEC");
        if (lp != std::string::npos) body.replace(lp, 36, "iTime / DEMO_GROW_SEC");
    }

    out = header + "\n// ===== blackhole.glsl =====" + body +
          "\nvoid main(){vec4 c;vec2 fc=vec2(gl_FragCoord.x,iResolution.y-gl_FragCoord.y);mainImage(c,fc);fragColor=c;}\n";
    return true;
}

// ========================= Config helpers =========================

static std::string getConfigPath() {
    return getExeDir() + "\\blackhole_presets.txt";
}

static void ScrLoadConfig(BlackholeConfig& cfg) {
    char names[64][64] = {};
    if (!LoadPresetsFromFile(cfg, names)) InitDefaultPresets(cfg);
}

static void ScrSaveConfig(const BlackholeConfig& cfg) {
    char names[64][64] = {};
    SavePresetsToFile(cfg, names);
}

// ========================= Fullscreen helper window =========================

namespace {
    struct ScrGLWin {
        HWND hwnd = nullptr; HDC hdc = nullptr; HGLRC hglrc = nullptr;
        int width = 0, height = 0; bool active = false;
    };

    bool InitScrGLWindow(ScrGLWin& g) {
        g.width = GetSystemMetrics(SM_CXSCREEN);
        g.height = GetSystemMetrics(SM_CYSCREEN);
        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(wc); wc.style = CS_OWNDC;
        wc.lpfnWndProc = DefWindowProcW;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.lpszClassName = L"BHScrGL";
        wc.hIcon = LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(100));
        static bool reg = false;
        if (!reg) { RegisterClassExW(&wc); reg = true; }
        g.hwnd = CreateWindowExW(WS_EX_TOPMOST, L"BHScrGL", L"", WS_POPUP,
            0, 0, g.width, g.height, nullptr, nullptr, wc.hInstance, nullptr);
        if (!g.hwnd) return false;
        g.hdc = GetDC(g.hwnd);
        if (!g.hdc) { DestroyWindow(g.hwnd); g.hwnd = nullptr; return false; }
        PIXELFORMATDESCRIPTOR pfd = {};
        pfd.nSize = sizeof(pfd); pfd.nVersion = 1;
        pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
        pfd.iPixelType = PFD_TYPE_RGBA; pfd.cColorBits = 32;
        int pf = ChoosePixelFormat(g.hdc, &pfd);
        if (!pf || !SetPixelFormat(g.hdc, pf, &pfd)) {
            ReleaseDC(g.hwnd, g.hdc); DestroyWindow(g.hwnd); g.hwnd = nullptr; g.hdc = nullptr; return false;
        }
        HGLRC dummy = wglCreateContext(g.hdc);
        if (!dummy) { ReleaseDC(g.hwnd, g.hdc); DestroyWindow(g.hwnd); g.hwnd = nullptr; g.hdc = nullptr; return false; }
        wglMakeCurrent(g.hdc, dummy);
        typedef HGLRC(WINAPI *PFWGLCREATECTXATTRIBS)(HDC, HGLRC, const int*);
        auto wglCreateCtxAttribs = (PFWGLCREATECTXATTRIBS)Win32GL_GetProcAddress("wglCreateContextAttribsARB");
        if (wglCreateCtxAttribs) {
            int attribs[] = { 0x2091, 3, 0x2092, 3, 0x9126, 0x0002, 0 };
            g.hglrc = wglCreateCtxAttribs(g.hdc, 0, attribs);
        }
        if (!g.hglrc) g.hglrc = wglCreateContext(g.hdc);
        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(dummy);
        if (!g.hglrc) { ReleaseDC(g.hwnd, g.hdc); DestroyWindow(g.hwnd); g.hwnd = nullptr; g.hdc = nullptr; return false; }
        wglMakeCurrent(g.hdc, g.hglrc);
        ShowWindow(g.hwnd, SW_SHOW);
        // CRITICAL: exclude from screen capture so WGC captures the DESKTOP, not our window
        typedef BOOL (WINAPI *PFN_SetWindowDisplayAffinity)(HWND, DWORD);
        auto pfnSetWindowDisplayAffinity = (PFN_SetWindowDisplayAffinity)GetProcAddress(GetModuleHandleA("user32.dll"), "SetWindowDisplayAffinity");
        if (pfnSetWindowDisplayAffinity) pfnSetWindowDisplayAffinity(g.hwnd, 0x00000011); // WDA_EXCLUDEFROMCAPTURE
        g.active = true;
        return true;
    }

    void ShutdownScrGLWindow(ScrGLWin& g) {
        if (!g.active) return;
        wglMakeCurrent(nullptr, nullptr);
        if (g.hglrc) wglDeleteContext(g.hglrc);
        if (g.hdc) ReleaseDC(g.hwnd, g.hdc);
        if (g.hwnd) DestroyWindow(g.hwnd);
        memset(&g, 0, sizeof(g));
    }
}

// ========================= /s Fullscreen =========================

int ScrDoFullscreen(const BlackholeConfig& cfg) {
    ScrLog("[SCR] Fullscreen mode starting...\n");
    ScrGLWin glw;
    if (!InitScrGLWindow(glw)) { ScrLog("[SCR] InitScrGLWindow failed\n"); return 1; }
    ScrLog("[SCR] GL window OK\n");
    if (!loadGLFunctions()) { ScrLog("[SCR] loadGLFunctions failed\n"); ShutdownScrGLWindow(glw); return 1; }
    ScrLog("[SCR] GL functions loaded\n");

    WGCCapture wgc; DXGICapture dxgi;
    bool useWGC = true;
    int capW = 0, capH = 0;
    if (!WGC_Init(wgc)) {
        ScrLog("[SCR] WGC failed, trying DXGI\n");
        useWGC = false;
        if (!DXGI_Init(dxgi)) { ScrLog("[SCR] DXGI also failed\n"); ShutdownScrGLWindow(glw); return 1; }
        capW = dxgi.width; capH = dxgi.height;
    } else { capW = wgc.width; capH = wgc.height; }
    ScrLog("[SCR] Capture ready: %dx%d (useWGC=%d)\n", capW, capH, useWGC);

    GLTextureUpload glTex;
    if (!GLTex_Init(glTex, capW, capH)) {
        ScrLog("[SCR] Tex_Init failed\n");
        if (useWGC) WGC_Release(wgc); else DXGI_Release(dxgi);
        ShutdownScrGLWindow(glw); return 1;
    }
    ScrLog("[SCR] Texture ready\n");
    ScrLog("[SCR] Reading shaders/vert.glsl...\n");
    std::string vertSrc = readFileEx("shaders/vert.glsl");
    ScrLog("[SCR] vert.glsl read: %d chars\n", (int)vertSrc.size());
    std::string fragSrc;
    if (vertSrc.empty() || !buildFragmentShaderScr(fragSrc)) {
        ScrLog("[SCR] Shader build failed (vert=%d chars, frag=%d chars)\n", (int)vertSrc.size(), (int)fragSrc.size());
        GLTex_Shutdown(glTex); if (useWGC) WGC_Release(wgc); else DXGI_Release(dxgi);
        ShutdownScrGLWindow(glw); return 1;
    }
    ScrLog("[SCR] Shader built (%d chars)\n", (int)fragSrc.size());
    GLuint program = createProgramScr(vertSrc, fragSrc);
    if (!program) {
        ScrLog("[SCR] Shader compile/link failed\n");
        GLTex_Shutdown(glTex); if (useWGC) WGC_Release(wgc); else DXGI_Release(dxgi);
        ShutdownScrGLWindow(glw); return 1;
    }
    ScrLog("[SCR] Shader program linked OK\n");

    float verts[] = { -1,-1, 1,-1, -1,1, 1,1 };
    GLuint vao, vbo;
    gl_GenVertexArrays(1, &vao); gl_GenBuffers(1, &vbo);
    gl_BindVertexArray(vao); gl_BindBuffer(GL_ARRAY_BUFFER, vbo);
    gl_BufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
    gl_VertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    gl_EnableVertexAttribArray(0); gl_BindVertexArray(0);

    // Core uniforms (the shader built-in demo handles everything else with good defaults)
    GLint locRes  = gl_GetUniformLocation(program, "iResolution");
    GLint locTime = gl_GetUniformLocation(program, "iTime");
    GLint locDate = gl_GetUniformLocation(program, "iDate");
    GLint locCh0  = gl_GetUniformLocation(program, "iChannel0");

    double startTime = Win32GL_GetTime();
    POINT ptFirst; GetCursorPos(&ptFirst); bool firstMouse = true;
    bool running = true;

    typedef BOOL(WINAPI *PFWGLSWAPINTERVAL)(int);
    auto wglSwapInterval = (PFWGLSWAPINTERVAL)Win32GL_GetProcAddress("wglSwapIntervalEXT");
    if (wglSwapInterval) wglSwapInterval(1);

    while (running) {
        MSG msg;
        while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE)) {
            // Ignore system-generated WM_CLOSE (sent by DWM/desktop changes)
            // Only exit on explicit USER input: keyboard, mouse click, or mouse wheel
            if (msg.message == WM_KEYDOWN || msg.message == WM_SYSKEYDOWN ||
                msg.message == WM_LBUTTONDOWN || msg.message == WM_RBUTTONDOWN ||
                msg.message == WM_MBUTTONDOWN || msg.message == WM_MOUSEWHEEL ||
                msg.message == WM_QUIT) {
                ScrLog("[SCR] Exit trigger: msg=0x%04X\n", msg.message);
                running = false; break;
            }
            TranslateMessage(&msg); DispatchMessageA(&msg);
        }
        if (!running) break;

        POINT pt; GetCursorPos(&pt);
        int dx = pt.x - ptFirst.x, dy = pt.y - ptFirst.y;
        if (!firstMouse && (dx*dx + dy*dy > 400)) { running = false; break; }
        firstMouse = false;

        int fbW = glw.width, fbH = glw.height;
        glViewport(0, 0, fbW, fbH);

        if (!useWGC) DXGI_ReleaseFrame(dxgi);
        ID3D11Texture2D* frame = useWGC ? WGC_GetFrame(wgc) : DXGI_GetFrame(dxgi);
        if (frame) {
            D3D11_TEXTURE2D_DESC desc; frame->GetDesc(&desc);
            int fw = (int)desc.Width, fh = (int)desc.Height;
            if (fw != glTex.width || fh != glTex.height) GLTex_Resize(glTex, fw, fh);
            if (fw == glTex.width && fh == glTex.height) {
                D3D11_MAPPED_SUBRESOURCE mapped;
                if ((useWGC ? WGC_CopyToStaging(wgc, frame, mapped) : DXGI_CopyToStaging(dxgi, frame, mapped))) {
                    GLTex_Upload(glTex, mapped.pData, (int)mapped.RowPitch);
                    if (useWGC) WGC_UnmapStaging(wgc); else DXGI_UnmapStaging(dxgi);
                }
            }
            frame->Release();
        }

        double now = Win32GL_GetTime();
        float t = (float)(now - startTime), ep = (float)time(nullptr);
        glClearColor(0,0,0,1); glClear(GL_COLOR_BUFFER_BIT);
        gl_UseProgram(program);
        gl_ActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, GLTex_GetTexture(glTex));
        gl_Uniform1i(locCh0, 0);
        gl_Uniform3f(locRes, (float)fbW, (float)fbH, 0.0f);
        gl_Uniform1f(locTime, t); gl_Uniform4f(locDate, 0,0,0, ep);

        // Set shader parameters for proper black hole rendering
        // These override the defaults in the shader header
        GLint loc_uHR = gl_GetUniformLocation(program, "uHoleRadius");
        GLint loc_uDG = gl_GetUniformLocation(program, "uDiskGain");
        GLint loc_uDT = gl_GetUniformLocation(program, "uDiskTemp");
        GLint loc_uEX = gl_GetUniformLocation(program, "uExposure");
        GLint loc_uSP = gl_GetUniformLocation(program, "uSpeed");
        GLint loc_uSG = gl_GetUniformLocation(program, "uStarGain");
        GLint loc_uDI = gl_GetUniformLocation(program, "uDiskIncl");

        // Override defaults for proper gravitational lensing effect
        if (loc_uHR >= 0) gl_Uniform1f(loc_uHR, 0.02f);   // original: HOLE_RADIUS
        if (loc_uDG >= 0) gl_Uniform1f(loc_uDG, 2.2f);     // original: DISK_GAIN
        if (loc_uDT >= 0) gl_Uniform1f(loc_uDT, 5500.0f);  // original: DISK_TEMP
        if (loc_uEX >= 0) gl_Uniform1f(loc_uEX, 1.4f);     // original: EXPOSURE
        if (loc_uSP >= 0) gl_Uniform1f(loc_uSP, 1.0f);     // original: DRIFT_SPEED
        if (loc_uSG >= 0) gl_Uniform1f(loc_uSG, 0.0f);     // original: STAR_GAIN
        if (loc_uDI >= 0) gl_Uniform1f(loc_uDI, 1.5f);     // original: DISK_INCL

        // CRITICAL: set uPresetCount=1 to use uniform-based DiskLook instead of demo tour
        GLint loc_uPC = gl_GetUniformLocation(program, "uPresetCount");
        GLint loc_uPM = gl_GetUniformLocation(program, "uPlayMode");
        GLint loc_uSlot = gl_GetUniformLocation(program, "uSlotSec");
        GLint loc_uPT = gl_GetUniformLocation(program, "uPresetTemp");
        GLint loc_uPI = gl_GetUniformLocation(program, "uPresetIncl");
        GLint loc_uPR = gl_GetUniformLocation(program, "uPresetRoll");
        GLint loc_uPN = gl_GetUniformLocation(program, "uPresetInner");
        GLint loc_uPO = gl_GetUniformLocation(program, "uPresetOuter");
        GLint loc_uPP = gl_GetUniformLocation(program, "uPresetOpac");
        GLint loc_uPD = gl_GetUniformLocation(program, "uPresetDopp");
        GLint loc_uPB = gl_GetUniformLocation(program, "uPresetBeam");
        GLint loc_uPG = gl_GetUniformLocation(program, "uPresetGain");
        GLint loc_uPCo = gl_GetUniformLocation(program, "uPresetContr");
        GLint loc_uPW = gl_GetUniformLocation(program, "uPresetWind");
        GLint loc_uPS = gl_GetUniformLocation(program, "uPresetSpd");
        GLint loc_uPE = gl_GetUniformLocation(program, "uPresetExpo");
        GLint loc_uPSt = gl_GetUniformLocation(program, "uPresetStar");

        if (loc_uPC >= 0) gl_Uniform1i(loc_uPC, 1);           // 1 preset = use uniform values
        if (loc_uPM >= 0) gl_Uniform1i(loc_uPM, 0);           // sequential mode
        if (loc_uSlot >= 0) gl_Uniform1f(loc_uSlot, 10000.0f); // very long slot time

        // Configure the single preset with desired values
        if (loc_uPT >= 0) { float v = 12000.0f; gl_Uniform1fv(loc_uPT, 1, &v); }
        if (loc_uPI >= 0) { float v = 1.52f;    gl_Uniform1fv(loc_uPI, 1, &v); }
        if (loc_uPR >= 0) { float v = 0.35f;    gl_Uniform1fv(loc_uPR, 1, &v); }
        if (loc_uPN >= 0) { float v = 1.8f;     gl_Uniform1fv(loc_uPN, 1, &v); }
        if (loc_uPO >= 0) { float v = 8.0f;     gl_Uniform1fv(loc_uPO, 1, &v); }
        if (loc_uPP >= 0) { float v = 0.9f;     gl_Uniform1fv(loc_uPP, 1, &v); }
        if (loc_uPD >= 0) { float v = 0.6f;     gl_Uniform1fv(loc_uPD, 1, &v); }
        if (loc_uPB >= 0) { float v = 2.5f;     gl_Uniform1fv(loc_uPB, 1, &v); }
        if (loc_uPG >= 0) { float v = 2.2f;     gl_Uniform1fv(loc_uPG, 1, &v); }
        if (loc_uPCo >= 0) { float v = 1.6f;    gl_Uniform1fv(loc_uPCo, 1, &v); }
        if (loc_uPW >= 0) { float v = 7.0f;     gl_Uniform1fv(loc_uPW, 1, &v); }
        if (loc_uPS >= 0) { float v = 5.0f;     gl_Uniform1fv(loc_uPS, 1, &v); }
        if (loc_uPE >= 0) { float v = 1.4f;     gl_Uniform1fv(loc_uPE, 1, &v); }
        if (loc_uPSt >= 0) { float v = 0.0f;    gl_Uniform1fv(loc_uPSt, 1, &v); }

        gl_BindVertexArray(vao); gl_DrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        gl_BindVertexArray(0); gl_UseProgram(0);
        SwapBuffers(glw.hdc);
    }

    ScrLog("[SCR] Exiting fullscreen mode\n");
    GLTex_Shutdown(glTex);
    if (useWGC) WGC_Release(wgc); else DXGI_Release(dxgi);
    gl_DeleteProgram(program);
    gl_DeleteVertexArrays(1, &vao); gl_DeleteBuffers(1, &vbo);
    ShutdownScrGLWindow(glw);
    return 0;
}

// ========================= /p Preview =========================

namespace {

    struct PrevCtx {
        HDC hdc = nullptr; HGLRC hctx = nullptr;
        int width = 0, height = 0;
        GLuint program = 0, vao = 0, vbo = 0;
        GLint locRes, locTime, locDate, locCh0;
        double startTime = 0;
        HWND hParent = nullptr;
        UINT_PTR timerId = 0;
        // Desktop capture state
        WGCCapture wgc; DXGICapture dxgi; bool useWGC = true;
        GLTextureUpload glTex; bool texReady = false;
    };
    static PrevCtx* g_pCtx = nullptr;

    static void PrevRender(PrevCtx* ctx) {
        if (!ctx || !ctx->hdc || !ctx->hctx || !ctx->program) return;
        glViewport(0, 0, ctx->width, ctx->height);

        // Capture desktop and upload to iChannel0
        if (ctx->texReady) {
            if (!ctx->useWGC) DXGI_ReleaseFrame(ctx->dxgi);
            ID3D11Texture2D* frame = ctx->useWGC ? WGC_GetFrame(ctx->wgc) : DXGI_GetFrame(ctx->dxgi);
            if (frame) {
                D3D11_TEXTURE2D_DESC desc; frame->GetDesc(&desc);
                int fw = (int)desc.Width, fh = (int)desc.Height;
                if (fw != ctx->glTex.width || fh != ctx->glTex.height)
                    GLTex_Resize(ctx->glTex, fw, fh);
                if (fw == ctx->glTex.width && fh == ctx->glTex.height) {
                    D3D11_MAPPED_SUBRESOURCE mapped;
                    if ((ctx->useWGC ? WGC_CopyToStaging(ctx->wgc, frame, mapped)
                                     : DXGI_CopyToStaging(ctx->dxgi, frame, mapped))) {
                        GLTex_Upload(ctx->glTex, mapped.pData, (int)mapped.RowPitch);
                        if (ctx->useWGC) WGC_UnmapStaging(ctx->wgc);
                        else DXGI_UnmapStaging(ctx->dxgi);
                    }
                }
                frame->Release();
            }
        }

        double now = Win32GL_GetTime();
        float t = (float)(now - ctx->startTime), ep = (float)time(nullptr);
        glClearColor(0.02f, 0.02f, 0.05f, 1); glClear(GL_COLOR_BUFFER_BIT);
        gl_UseProgram(ctx->program);
        if (ctx->texReady) {
            gl_ActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, GLTex_GetTexture(ctx->glTex));
            if (ctx->locCh0 >= 0) gl_Uniform1i(ctx->locCh0, 0);
        }
        gl_Uniform3f(ctx->locRes, (float)ctx->width, (float)ctx->height, 0.0f);
        gl_Uniform1f(ctx->locTime, t); gl_Uniform4f(ctx->locDate, 0,0,0, ep);

        // Set shader parameters for proper black hole rendering
        GLint loc_uHR = gl_GetUniformLocation(ctx->program, "uHoleRadius");
        GLint loc_uDG = gl_GetUniformLocation(ctx->program, "uDiskGain");
        GLint loc_uDT = gl_GetUniformLocation(ctx->program, "uDiskTemp");
        GLint loc_uEX = gl_GetUniformLocation(ctx->program, "uExposure");
        GLint loc_uSP = gl_GetUniformLocation(ctx->program, "uSpeed");
        GLint loc_uSG = gl_GetUniformLocation(ctx->program, "uStarGain");
        GLint loc_uDI = gl_GetUniformLocation(ctx->program, "uDiskIncl");
        if (loc_uHR >= 0) gl_Uniform1f(loc_uHR, 0.02f);
        if (loc_uDG >= 0) gl_Uniform1f(loc_uDG, 2.2f);
        if (loc_uDT >= 0) gl_Uniform1f(loc_uDT, 5500.0f);
        if (loc_uEX >= 0) gl_Uniform1f(loc_uEX, 1.4f);
        if (loc_uSP >= 0) gl_Uniform1f(loc_uSP, 1.0f);
        if (loc_uSG >= 0) gl_Uniform1f(loc_uSG, 0.0f);
        if (loc_uDI >= 0) gl_Uniform1f(loc_uDI, 1.5f);

        // CRITICAL: set uPresetCount=1 to use uniform-based DiskLook instead of demo tour
        GLint loc_uPC = gl_GetUniformLocation(ctx->program, "uPresetCount");
        GLint loc_uPM = gl_GetUniformLocation(ctx->program, "uPlayMode");
        GLint loc_uSlot = gl_GetUniformLocation(ctx->program, "uSlotSec");
        GLint loc_uPT = gl_GetUniformLocation(ctx->program, "uPresetTemp");
        GLint loc_uPI = gl_GetUniformLocation(ctx->program, "uPresetIncl");
        GLint loc_uPR = gl_GetUniformLocation(ctx->program, "uPresetRoll");
        GLint loc_uPN = gl_GetUniformLocation(ctx->program, "uPresetInner");
        GLint loc_uPO = gl_GetUniformLocation(ctx->program, "uPresetOuter");
        GLint loc_uPP = gl_GetUniformLocation(ctx->program, "uPresetOpac");
        GLint loc_uPD = gl_GetUniformLocation(ctx->program, "uPresetDopp");
        GLint loc_uPB = gl_GetUniformLocation(ctx->program, "uPresetBeam");
        GLint loc_uPG = gl_GetUniformLocation(ctx->program, "uPresetGain");
        GLint loc_uPCo = gl_GetUniformLocation(ctx->program, "uPresetContr");
        GLint loc_uPW = gl_GetUniformLocation(ctx->program, "uPresetWind");
        GLint loc_uPS = gl_GetUniformLocation(ctx->program, "uPresetSpd");
        GLint loc_uPE = gl_GetUniformLocation(ctx->program, "uPresetExpo");
        GLint loc_uPSt = gl_GetUniformLocation(ctx->program, "uPresetStar");
        if (loc_uPC >= 0) gl_Uniform1i(loc_uPC, 1);
        if (loc_uPM >= 0) gl_Uniform1i(loc_uPM, 0);
        if (loc_uSlot >= 0) gl_Uniform1f(loc_uSlot, 10000.0f);
        if (loc_uPT >= 0) { float v = 5500.0f; gl_Uniform1fv(loc_uPT, 1, &v); }
        if (loc_uPI >= 0) { float v = 1.5f;    gl_Uniform1fv(loc_uPI, 1, &v); }
        if (loc_uPR >= 0) { float v = 0.35f;    gl_Uniform1fv(loc_uPR, 1, &v); }
        if (loc_uPN >= 0) { float v = 1.8f;     gl_Uniform1fv(loc_uPN, 1, &v); }
        if (loc_uPO >= 0) { float v = 8.0f;     gl_Uniform1fv(loc_uPO, 1, &v); }
        if (loc_uPP >= 0) { float v = 0.9f;     gl_Uniform1fv(loc_uPP, 1, &v); }
        if (loc_uPD >= 0) { float v = 0.6f;     gl_Uniform1fv(loc_uPD, 1, &v); }
        if (loc_uPB >= 0) { float v = 2.5f;     gl_Uniform1fv(loc_uPB, 1, &v); }
        if (loc_uPG >= 0) { float v = 2.2f;     gl_Uniform1fv(loc_uPG, 1, &v); }
        if (loc_uPCo >= 0) { float v = 1.6f;    gl_Uniform1fv(loc_uPCo, 1, &v); }
        if (loc_uPW >= 0) { float v = 7.0f;     gl_Uniform1fv(loc_uPW, 1, &v); }
        if (loc_uPS >= 0) { float v = 5.0f;     gl_Uniform1fv(loc_uPS, 1, &v); }
        if (loc_uPE >= 0) { float v = 1.4f;     gl_Uniform1fv(loc_uPE, 1, &v); }
        if (loc_uPSt >= 0) { float v = 0.0f;    gl_Uniform1fv(loc_uPSt, 1, &v); }
        gl_BindVertexArray(ctx->vao); gl_DrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        gl_BindVertexArray(0); gl_UseProgram(0);
        SwapBuffers(ctx->hdc);
    }

    static LRESULT CALLBACK PrevWndProc(HWND hWnd, UINT msg, WPARAM w, LPARAM l) {
        if (msg == WM_PAINT && g_pCtx) { PrevRender(g_pCtx); ValidateRect(hWnd, NULL); return 0; }
        if (msg == WM_TIMER && g_pCtx) { PrevRender(g_pCtx); return 0; }
        if (msg == WM_DESTROY && g_pCtx) { KillTimer(hWnd, g_pCtx->timerId); return 0; }
        return DefWindowProc(hWnd, msg, w, l);
    }

    static bool createModernGLContext(HWND hWnd, HDC& hdc, HGLRC& hglrc) {
        hdc = GetDC(hWnd);
        PIXELFORMATDESCRIPTOR pfd = {};
        pfd.nSize = sizeof(pfd); pfd.nVersion = 1;
        pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
        pfd.iPixelType = PFD_TYPE_RGBA; pfd.cColorBits = 24;
        int pf = ChoosePixelFormat(hdc, &pfd);
        SetPixelFormat(hdc, pf, &pfd);
        HGLRC legacy = wglCreateContext(hdc);
        if (!legacy) return false;
        wglMakeCurrent(hdc, legacy);
        typedef HGLRC(WINAPI *PFWGLCREATECTXATTRIBS)(HDC, HGLRC, const int*);
        auto wglCreateCtxAttribs = (PFWGLCREATECTXATTRIBS)Win32GL_GetProcAddress("wglCreateContextAttribsARB");
        if (!wglCreateCtxAttribs) { hglrc = legacy; return true; }
        int attribs[] = { 0x2091, 3, 0x2092, 3, 0x9126, 0x0002, 0 };
        HGLRC modern = wglCreateCtxAttribs(hdc, 0, attribs);
        if (modern) {
            wglMakeCurrent(nullptr, nullptr); wglDeleteContext(legacy);
            wglMakeCurrent(hdc, modern); hglrc = modern; return true;
        }
        wglMakeCurrent(hdc, legacy); hglrc = legacy; return true;
    }
}

int ScrDoPreview(HWND hParent) {
    ScrLog("[SCR] Preview: parent=%p\n", (void*)hParent);
    RECT rc; GetClientRect(hParent, &rc);
    int w = rc.right - rc.left, h = rc.bottom - rc.top;
    if (w <= 0 || h <= 0) { w = 200; h = 150; }

    static bool clsReg = false;
    static const char* CLSNAME = "BHScrPrev";
    if (!clsReg) {
        WNDCLASSEXA wc = {};
        wc.cbSize = sizeof(wc); wc.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = PrevWndProc; wc.hInstance = GetModuleHandle(nullptr);
        wc.lpszClassName = CLSNAME;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        RegisterClassExA(&wc); clsReg = true;
    }

    HWND hChild = CreateWindowExA(0, CLSNAME, "Preview",
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
        0, 0, w, h, hParent, NULL, GetModuleHandle(nullptr), nullptr);
    if (!hChild) { ScrLog("[SCR] CreateWindowEx failed\n"); return 1; }
    ShowWindow(hChild, SW_SHOW);

    PrevCtx* ctx = new PrevCtx();
    g_pCtx = ctx;
    ctx->hParent = hParent; ctx->width = w; ctx->height = h;

    if (!createModernGLContext(hChild, ctx->hdc, ctx->hctx)) {
        ScrLog("[SCR] GL context failed\n");
        DestroyWindow(hChild); delete ctx; g_pCtx = nullptr; return 1;
    }
    if (!loadGLFunctions()) {
        ScrLog("[SCR] loadGLFunctions failed\n");
        wglMakeCurrent(nullptr, nullptr); wglDeleteContext(ctx->hctx);
        ReleaseDC(hChild, ctx->hdc); DestroyWindow(hChild); delete ctx; g_pCtx = nullptr; return 1;
    }

    std::string vertSrc = readFileEx("shaders/vert.glsl");
    std::string fragSrc;
    if (vertSrc.empty() || !buildFragmentShaderScr(fragSrc)) {
        ScrLog("[SCR] Shader source build failed\n");
        wglMakeCurrent(nullptr, nullptr); wglDeleteContext(ctx->hctx);
        ReleaseDC(hChild, ctx->hdc); DestroyWindow(hChild); delete ctx; g_pCtx = nullptr; return 1;
    }
    ctx->program = createProgramScr(vertSrc, fragSrc);
    if (!ctx->program) {
        ScrLog("[SCR] Shader compile failed\n");
        wglMakeCurrent(nullptr, nullptr); wglDeleteContext(ctx->hctx);
        ReleaseDC(hChild, ctx->hdc); DestroyWindow(hChild); delete ctx; g_pCtx = nullptr; return 1;
    }

    float verts2[] = { -1,-1, 1,-1, -1,1, 1,1 };
    gl_GenVertexArrays(1, &ctx->vao); gl_GenBuffers(1, &ctx->vbo);
    gl_BindVertexArray(ctx->vao); gl_BindBuffer(GL_ARRAY_BUFFER, ctx->vbo);
    gl_BufferData(GL_ARRAY_BUFFER, sizeof(verts2), verts2, GL_STATIC_DRAW);
    gl_VertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    gl_EnableVertexAttribArray(0); gl_BindVertexArray(0);

    ctx->locRes = gl_GetUniformLocation(ctx->program, "iResolution");
    ctx->locTime = gl_GetUniformLocation(ctx->program, "iTime");
    ctx->locDate = gl_GetUniformLocation(ctx->program, "iDate");
    ctx->locCh0 = gl_GetUniformLocation(ctx->program, "iChannel0");

    // Initialize WGC capture for desktop texture
    ctx->useWGC = true;
    if (!WGC_Init(ctx->wgc)) {
        ScrLog("[SCR] Preview: WGC failed, trying DXGI\n");
        ctx->useWGC = false;
        if (!DXGI_Init(ctx->dxgi)) {
            ScrLog("[SCR] Preview: DXGI also failed\n");
            // Keep texReady = false, shader will use black background
        } else {
            ctx->texReady = true;
            if (!GLTex_Init(ctx->glTex, ctx->dxgi.width, ctx->dxgi.height)) {
                ScrLog("[SCR] Preview: GLTex_Init failed (DXGI)\n");
                ctx->texReady = false;
            }
        }
    } else {
        ctx->texReady = true;
        if (!GLTex_Init(ctx->glTex, ctx->wgc.width, ctx->wgc.height)) {
            ScrLog("[SCR] Preview: GLTex_Init failed (WGC)\n");
            ctx->texReady = false;
        }
    }
    ScrLog("[SCR] Preview capture: %s (texReady=%d)\n", ctx->useWGC ? "WGC" : "DXGI", ctx->texReady);

    ctx->startTime = Win32GL_GetTime();
    ctx->timerId = SetTimer(hChild, 1, 33, NULL);

    ScrLog("[SCR] Preview running: w=%d h=%d\n", w, h);

    MSG msg;
    while (IsWindow(hParent) && GetMessageA(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg); DispatchMessageA(&msg);
    }

    ScrLog("[SCR] Preview exiting\n");
    KillTimer(hChild, ctx->timerId);
    if (ctx->texReady) {
        GLTex_Shutdown(ctx->glTex);
        if (ctx->useWGC) WGC_Release(ctx->wgc);
        else DXGI_Release(ctx->dxgi);
    }
    gl_DeleteProgram(ctx->program);
    gl_DeleteVertexArrays(1, &ctx->vao); gl_DeleteBuffers(1, &ctx->vbo);
    wglMakeCurrent(nullptr, nullptr); wglDeleteContext(ctx->hctx);
    ReleaseDC(hChild, ctx->hdc); DestroyWindow(hChild);
    delete ctx; g_pCtx = nullptr;
    return 0;
}

// ========================= /c Config =========================

int ScrDoConfig() {
    ScrLog("[SCR] Config mode\n");
    BlackholeConfig cfg;
    ScrLoadConfig(cfg);
    if (!glfwInit()) { ScrLog("[SCR] glfwInit failed\n"); return 1; }
    if (!GUI_ShowConfigPanel(cfg)) { glfwTerminate(); return 0; }
    ScrSaveConfig(cfg);
    glfwTerminate();
    return 0;
}

// ========================= /a Password =========================

int ScrDoPassword(HWND hParent) {
    MessageBoxA(hParent,
        "Windows 10 and later no longer support screensaver passwords.\n"
        "Use Windows lock screen settings instead.",
        "Black Hole Screensaver", MB_OK | MB_ICONINFORMATION);
    return 0;
}

// ========================= WinMain =========================

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR cmdLine, int) {
    SetUnhandledExceptionFilter(ScrExceptionHandler);
    ScrLog("[SCR] WinMain reached, cmdLine='%s', exeDir='%s'", cmdLine, getExeDir().c_str());
    ShowWindow(GetConsoleWindow(), SW_HIDE);
    SetProcessDPIAware();
    ScrLog("[SCR] Starting, cmdLine='%s', exeDir='%s'\n", cmdLine, getExeDir().c_str());

    char* cmd = cmdLine;
    while (*cmd == ' ') cmd++;

    if (*cmd == '\0' || _stricmp(cmd, "/s") == 0 || _stricmp(cmd, "/S") == 0 ||
        _stricmp(cmd, "-s") == 0) {
        BlackholeConfig cfg; ScrLoadConfig(cfg); return ScrDoFullscreen(cfg);
    }
    if (_strnicmp(cmd, "/p", 2) == 0 || _strnicmp(cmd, "/P", 2) == 0) {
        HWND hParent = nullptr;
        char* num = cmd + 2;
        while (*num == ' ') num++;
        if (*num >= '0' && *num <= '9')
            hParent = (HWND)(UINT_PTR)strtoul(num, nullptr, 10);
        if (!hParent || !IsWindow(hParent)) {
            ScrLog("[SCR] Invalid /p hwnd\n"); return 1;
        }
        return ScrDoPreview(hParent);
    }
    if (_strnicmp(cmd, "/c", 2) == 0 || _strnicmp(cmd, "/C", 2) == 0) {
        return ScrDoConfig();
    }
    if (_strnicmp(cmd, "/a", 2) == 0 || _strnicmp(cmd, "/A", 2) == 0) {
        HWND hParent = nullptr;
        char* num = cmd + 2;
        while (*num == ' ') num++;
        if (*num >= '0' && *num <= '9')
            hParent = (HWND)(UINT_PTR)strtoul(num, nullptr, 10);
        return ScrDoPassword(hParent);
    }

    ScrLog("[SCR] Unknown args, defaulting to /s\n");
    BlackholeConfig cfg; ScrLoadConfig(cfg); return ScrDoFullscreen(cfg);
}

