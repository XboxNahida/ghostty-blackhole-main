// blackhole standalone  Windows OpenGL host for blackhole.glsl
// v5: ImGui config panel + uniform-overridable shader params
// Build: Ctrl+Shift+B in VS Code

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

#ifndef DWMWA_BORDER_COLOR
#define DWMWA_BORDER_COLOR 34  // Windows 11 accent border (not in SDK 8.1)
#endif

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include <GL/gl.h>

#ifndef GL_COMPILE_STATUS
#include <GL/glcorearb.h>
#endif

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
    gl_##name = (PFN_##name##_PROC)glfwGetProcAddress("gl" #name); \
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

static std::string readFile(const char* path) {
    std::ifstream f(path, std::ios::in | std::ios::binary);
    if (!f) { fprintf(stderr, "Cannot open %s\n", path); return ""; }
    std::stringstream ss; ss << f.rdbuf(); return ss.str();
}

static GLuint compileShader(GLenum type, const std::string& source) {
    GLuint shader = gl_CreateShader(type);
    const char* src = source.c_str();
    gl_ShaderSource(shader, 1, &src, nullptr);
    gl_CompileShader(shader);
    GLint ok = 0; gl_GetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    char log[4096]; gl_GetShaderInfoLog(shader, sizeof(log), nullptr, log);
    if (log[0]) fprintf(stderr, "[%s] %s\n", type==GL_VERTEX_SHADER?"vert":"frag", log);
    if (!ok) { gl_DeleteShader(shader); return 0; }
    return shader;
}

static GLuint createProgram(const std::string& vert, const std::string& frag) {
    GLuint vs = compileShader(GL_VERTEX_SHADER, vert);
    if (!vs) return 0;
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, frag);
    if (!fs) { gl_DeleteShader(vs); return 0; }
    GLuint prog = gl_CreateProgram();
    gl_AttachShader(prog, vs); gl_AttachShader(prog, fs);
    gl_LinkProgram(prog);
    GLint ok = 0; gl_GetProgramiv(prog, GL_LINK_STATUS, &ok);
    char log[4096]; gl_GetProgramInfoLog(prog, sizeof(log), nullptr, log);
    if (log[0]) fprintf(stderr, "Link: %s\n", log);
    if (!ok) { gl_DeleteProgram(prog); gl_DeleteShader(vs); gl_DeleteShader(fs); return 0; }
    gl_DeleteShader(vs); gl_DeleteShader(fs);
    return prog;
}

static bool buildFragmentShader(std::string& out) {
    std::string header = readFile("shaders/frag_desktop_header.glsl");
    std::string body   = readFile("blackhole.glsl");
    if (header.empty() || body.empty()) return false;

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
                    "            float raw = iTime / max(uSlotSec, 0.5);\n"
                    "            f = smoothstep(1.0 - DEMO_XFADE, 1.0, fract(raw));\n"
                    "            i0 = int(min(raw, float(n) - 0.001));\n"
                    "            i1 = int(min(raw + 1.0, float(n) - 0.001));\n"
                    "        } else if (uPlayMode == 2) {\n"
                    "            float raw = iTime / max(uSlotSec, 0.5);\n"
                    "            f = smoothstep(1.0 - DEMO_XFADE, 1.0, fract(raw));\n"
                    "            int slot = int(raw);\n"
                    "            i0 = int(fract(sin(float(slot) * 127.1 + 311.7) * 43758.5453) * float(n));\n"
                    "            i1 = int(fract(sin(float(slot + 1) * 127.1 + 311.7) * 43758.5453) * float(n));\n"
                    "        } else {\n"
                    "            float raw = iTime / max(uSlotSec, 0.5);\n"
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

    out = header + "\n// ===== blackhole.glsl =====" + body +
          "\nvoid main() { vec4 c; vec2 fc = vec2(gl_FragCoord.x, iResolution.y - gl_FragCoord.y); mainImage(c, fc); fragColor = c; }\n";
    return true;
}

// Window subclass: minimal DWM interference (no repeated attribute flush)
static LRESULT CALLBACK OverlayWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR) {
    if (msg == WM_NCACTIVATE) return FALSE;
    if (msg == WM_MOUSEACTIVATE) return MA_NOACTIVATEANDEAT;
    if (msg == WM_NCCALCSIZE) return 0;
    return DefSubclassProc(hwnd, msg, wp, lp);
}

static bool isIdle(DWORD ms) {
    LASTINPUTINFO lii = { sizeof(LASTINPUTINFO) };
    return GetLastInputInfo(&lii) && (GetTickCount() - lii.dwTime) >= ms;
}

// ---- Main ----
int main(int argc, char* argv[]) {
    ShowWindow(GetConsoleWindow(), SW_HIDE);
    // Set working directory to project root
    {
        char p[MAX_PATH]; GetModuleFileNameA(nullptr, p, MAX_PATH);
        char* s = strrchr(p, '\\'); if (s) *s = 0;
        s = strrchr(p, '\\');
        if (s && (strcmp(s+1,"build")==0 || strcmp(s+1,"Build")==0)) *s = 0;
        SetCurrentDirectoryA(p);
    }

    if (!glfwInit()) { fprintf(stderr, "glfwInit failed\n"); return 1; }

    // ---- ImGui Config Panel ----
    BlackholeConfig cfg;
    if (!GUI_ShowConfigPanel(cfg)) { glfwTerminate(); return 0; }

    // ---- Create fullscreen black hole window ----
    glfwWindowHint(GLFW_FOCUS_ON_SHOW, GLFW_FALSE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_COMPAT_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_FALSE);
    glfwWindowHint(GLFW_DECORATED, GL_FALSE);
    glfwWindowHint(GLFW_RESIZABLE, GL_TRUE);
    glfwWindowHint(GLFW_MOUSE_PASSTHROUGH, GLFW_TRUE);

    GLFWmonitor* mon = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(mon);
    GLFWwindow* window = glfwCreateWindow(mode->width, mode->height, "Black Hole", nullptr, nullptr);
    glfwSetWindowPos(window, 0, 0);
    if (!window) { glfwTerminate(); return 1; }

    {
        HWND hwnd = glfwGetWin32Window(window);
        // Strip all frame/border styles
        DWORD style = GetWindowLong(hwnd, GWL_STYLE);
        style &= ~(WS_BORDER | WS_DLGFRAME | WS_THICKFRAME | WS_CAPTION | WS_SYSMENU);
        SetWindowLong(hwnd, GWL_STYLE, style);
        // Prevent DWM redirection bitmap (blocks accent layer cache)
        LONG ex = GetWindowLong(hwnd, GWL_EXSTYLE);
        ex |= WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_NOREDIRECTIONBITMAP;
        SetWindowLong(hwnd, GWL_EXSTYLE, ex);
        // Disable blur-behind (prevents DWM accent trigger)
        DWM_BLURBEHIND bb = {};
        bb.dwFlags = DWM_BB_ENABLE;
        bb.fEnable = FALSE;
        DwmEnableBlurBehindWindow(hwnd, &bb);
        // Accent border = transparent
        COLORREF bc = 0;
        DwmSetWindowAttribute(hwnd, DWMWA_BORDER_COLOR, &bc, sizeof(bc));
        DWMNCRENDERINGPOLICY ncrp = DWMNCRP_DISABLED;
        DwmSetWindowAttribute(hwnd, DWMWA_NCRENDERING_POLICY, &ncrp, sizeof(ncrp));
        SetWindowDisplayAffinity(hwnd, WDA_EXCLUDEFROMCAPTURE);
        SetWindowSubclass(hwnd, OverlayWndProc, 1, 0);
        SetWindowPos(hwnd, HWND_TOPMOST, 0,0,0,0, SWP_NOMOVE|SWP_NOSIZE|SWP_SHOWWINDOW);
        ShowWindow(hwnd, SW_SHOWNOACTIVATE);
        DwmFlush();  // Clear compositor accent layer cache
    }

    glfwMakeContextCurrent(window);
    setbuf(stderr, NULL);
    glfwSwapInterval(1);

    fprintf(stderr, "OpenGL %s, GLSL %s\n", glGetString(GL_VERSION), glGetString(GL_SHADING_LANGUAGE_VERSION));
    if (!loadGLFunctions()) { glfwTerminate(); return 1; }

    // ---- Capture (WGC default) ----
    WGCCapture wgc; DXGICapture dxgi;
    bool useWGC = true;
    int capW=0, capH=0; bool capOk;
    capOk = WGC_Init(wgc); capW=wgc.width; capH=wgc.height;
    if (!capOk) { glfwTerminate(); return 1; }

    GLTextureUpload glTex;
    if (!GLTex_Init(glTex, capW, capH)) { WGC_Release(wgc); glfwTerminate(); return 1; }

    // ---- Shader ----
    std::string vertSrc = readFile("shaders/vert.glsl");
    std::string fragSrc;
    if (vertSrc.empty() || !buildFragmentShader(fragSrc)) {
        GLTex_Shutdown(glTex); WGC_Release(wgc); glfwTerminate(); return 1;
    }
    GLuint program = createProgram(vertSrc, fragSrc);
    if (!program) { GLTex_Shutdown(glTex); WGC_Release(wgc); glfwTerminate(); return 1; }

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
    // GUI uniform locations (set once)
    GLint loc_uHR  = gl_GetUniformLocation(program, "uHoleRadius");
    GLint loc_uDG  = gl_GetUniformLocation(program, "uDiskGain");
    GLint loc_uDT  = gl_GetUniformLocation(program, "uDiskTemp");
    GLint loc_uEX  = gl_GetUniformLocation(program, "uExposure");
    GLint loc_uSP  = gl_GetUniformLocation(program, "uSpeed");
    GLint loc_uSG  = gl_GetUniformLocation(program, "uStarGain");
    GLint loc_uDI  = gl_GetUniformLocation(program, "uDiskIncl");
    GLint loc_uPM  = gl_GetUniformLocation(program, "uPlayMode");
    GLint loc_uSlot = gl_GetUniformLocation(program, "uSlotSec");
    // Preset uniform locations
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

    gl_UseProgram(0);

    // ---- Main loop ----
    double startTime = glfwGetTime();
    float idleStart = 0.0f;
    int frames = 0; double lastFps = startTime;
    char title[128];

    // DXGI pairing: acquire first frame before loop
    if (!useWGC) { ID3D11Texture2D* f = DXGI_GetFrame(dxgi); if (f) f->Release(); }

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(window, GL_TRUE);

        // Idle mode: unified pipeline, only opacity + FPS differ
        static bool idleWasVisible = false;
        bool userActive = (cfg.mode == 1) && !isIdle((DWORD)cfg.idleSec * 1000);
        if (cfg.mode == 1) {
            if (userActive) {
                glfwSetWindowOpacity(window, 0.0f);
                idleWasVisible = false;
            } else {
                glfwSetWindowOpacity(window, 1.0f);
                if (!idleWasVisible) idleStart = (float)(glfwGetTime() - startTime);
                idleWasVisible = true;
            }
        }

        int fbW, fbH; glfwGetFramebufferSize(window, &fbW, &fbH);
        glViewport(0, 0, fbW, fbH);

        // Capture
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

        double now = glfwGetTime();
        float t = (float)(now - startTime);
        float ep = (float)time(nullptr);


        glClearColor(0,0,0,1); glClear(GL_COLOR_BUFFER_BIT);
        gl_UseProgram(program);

        gl_ActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, GLTex_GetTexture(glTex));
        gl_Uniform1i(locCh0, 0);
        gl_Uniform3f(locRes, (float)fbW, (float)fbH, 0.0f);
        gl_Uniform1f(locTime, t - idleStart);
        gl_Uniform4f(locDate, 0,0,0,ep);

        // GUI parameters (set every frame 闂?cheap uniform calls)
        gl_Uniform1f(loc_uHR, cfg.holeRadius);
        gl_Uniform1f(loc_uDG, cfg.diskGain);
        gl_Uniform1f(loc_uDT, cfg.diskTemp);
        gl_Uniform1f(loc_uEX, cfg.exposure);
        gl_Uniform1f(loc_uSP, cfg.spd);
        gl_Uniform1f(loc_uSG, cfg.starGain);
        gl_Uniform1f(loc_uDI, cfg.diskIncl);
        gl_Uniform1i(loc_uPM, cfg.playMode);
        gl_Uniform1f(loc_uSlot, cfg.slotSec);
        // Upload preset uniforms
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

        gl_BindVertexArray(vao);
        gl_DrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        gl_BindVertexArray(0);
        gl_UseProgram(0);

        glfwSwapBuffers(window);

        if (userActive) Sleep(100);

        frames++;
        if (now - lastFps >= 1.0) {
            snprintf(title, sizeof(title), "Black Hole [%d FPS]", frames);
            glfwSetWindowTitle(window, title);
            frames=0; lastFps=now;
        }
    }

    GLTex_Shutdown(glTex);
    if (useWGC) WGC_Release(wgc); else DXGI_Release(dxgi);
    gl_DeleteProgram(program);
    gl_DeleteVertexArrays(1, &vao);
    gl_DeleteBuffers(1, &vbo);
    glfwTerminate();
    return 0;
}