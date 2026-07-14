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
#include <algorithm>
#include <utility>
#include <windows.h>
#include <d3d11.h>
#include <dwmapi.h>
#include <wtsapi32.h>

#include "capture_wgc.h"
#include "capture_dxgi.h"
#include "gl_texture.h"
#include "bloom_renderer.h"
#include "foreground_window.h"
#include "game_detection.h"
#include "media_session.h"
#include "gui_config.h"
#include "movement_settings.h"
#include "win32_gl.h"
#include "monitors.h"
#include "render/gl_funcs.h"
#include "render/shader_source.h"
#include "render/gl_program.h"
#include "render/blackhole_renderer.h"
#ifdef BLACKHOLE_USE_D3D11
#include "d3d11_renderer.h"
#include "win32_window.h"
#endif

#ifndef DWMWA_BORDER_COLOR
#define DWMWA_BORDER_COLOR 34  // Windows 11 accent border (not in SDK 8.1)
#endif
#ifndef VK_R
#define VK_R 0x52
#endif
#ifndef PROC_THREAD_ATTRIBUTE_JOB_LIST
#define PROC_THREAD_ATTRIBUTE_JOB_LIST \
    ProcThreadAttributeValue(13, FALSE, TRUE, FALSE)
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

    // 桌面、系统外壳和黑洞自身都不能进入全屏视频/游戏判断。
    if (Foreground_Classify(fg) != ForegroundKind::Application) {
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

    // Method 1b: 当前显示器上的无边框全屏游戏。
    if (Foreground_IsBorderlessFullscreen(fg)) {
        return true;
    }

    // Get foreground process name
    DWORD pid = 0;
    GetWindowThreadProcessId(fg, &pid);
    if (!pid) return false;
    char pname[260]; GetProcessName(pid, pname, sizeof(pname));
    if (!pname[0]) return false;

    if (GameDetection_IsKnownGameProcess(pid)) return true;

    // Check if process is a known video player or browser
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

    const MediaSessionSnapshot mediaSession = MediaSession_QueryCurrent();
    if (mediaSession.state == MediaPlaybackState::Playing &&
        MediaSession_SourceMatchesProcess(mediaSession.sourceAppId, pname)) {
        return true;
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
static HANDLE g_hMonitorJob = nullptr;
static std::vector<PROCESS_INFORMATION> g_childRenderers;

static void EnsureChildJob() {
    if (g_hChildJob) return;
    g_hChildJob = CreateJobObjectA(nullptr, nullptr);
    if (!g_hChildJob) return;
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION info = {};
    info.BasicLimitInformation.LimitFlags =
        JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (!SetInformationJobObject(g_hChildJob, JobObjectExtendedLimitInformation,
                                 &info, sizeof(info))) {
        CloseHandle(g_hChildJob);
        g_hChildJob = nullptr;
    }
}

static void EnsureMonitorJob() {
    if (g_hMonitorJob) return;
    g_hMonitorJob = CreateJobObjectA(nullptr, nullptr);
    if (!g_hMonitorJob) return;
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION info = {};
    info.BasicLimitInformation.LimitFlags =
        JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (!SetInformationJobObject(g_hMonitorJob, JobObjectExtendedLimitInformation,
                                 &info, sizeof(info))) {
        CloseHandle(g_hMonitorJob);
        g_hMonitorJob = nullptr;
    }
}

static bool CreateProcessInJob(char* cmd, HANDLE job, PROCESS_INFORMATION& pi) {
    SIZE_T attributeListSize = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &attributeListSize);
    auto* attributeList = static_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(
        HeapAlloc(GetProcessHeap(), 0, attributeListSize));
    if (!attributeList) return false;

    bool created = false;
    if (InitializeProcThreadAttributeList(attributeList, 1, 0, &attributeListSize)) {
        HANDLE jobList[] = { job };
        if (UpdateProcThreadAttribute(attributeList, 0, PROC_THREAD_ATTRIBUTE_JOB_LIST,
                                      jobList, sizeof(jobList), nullptr, nullptr)) {
            STARTUPINFOEXA si = {};
            si.StartupInfo.cb = sizeof(si);
            si.StartupInfo.dwFlags = STARTF_USESHOWWINDOW;
            si.StartupInfo.wShowWindow = SW_HIDE;
            si.lpAttributeList = attributeList;
            pi = {};
            created = CreateProcessA(nullptr, cmd, nullptr, nullptr, FALSE,
                                     EXTENDED_STARTUPINFO_PRESENT, nullptr, nullptr,
                                     &si.StartupInfo, &pi) != FALSE;
        }
        DeleteProcThreadAttributeList(attributeList);
    }
    HeapFree(GetProcessHeap(), 0, attributeList);
    return created;
}

static void SpawnChildRenderer(const char* selfPath, int screenIdx) {
    EnsureChildJob();
    if (!g_hChildJob) return;
    char cmd[MAX_PATH + 32];
    snprintf(cmd, sizeof(cmd), "\"%s\" --render --screen %d", selfPath, screenIdx);
    PROCESS_INFORMATION pi = {};
    if (CreateProcessInJob(cmd, g_hChildJob, pi)) {
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
        WaitForSingleObject(pi.hProcess, 2000);
        CloseHandle(pi.hProcess);
    }
    g_childRenderers.clear();
    if (g_hChildJob) {
        CloseHandle(g_hChildJob);
        g_hChildJob = nullptr;
    }
}

static void MonitorSpawn(const char* selfPath) {
    if (g_pi.hProcess) return;
    EnsureMonitorJob();
    if (!g_hMonitorJob) return;
    char cmd[MAX_PATH + 16];
    snprintf(cmd, sizeof(cmd), "\"%s\" --render", selfPath);
    if (CreateProcessInJob(cmd, g_hMonitorJob, g_pi)) {
        CloseHandle(g_pi.hThread);
        g_pi.hThread = NULL;
    }
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
    if (g_pi.hProcess) {
        HWND hwnd = FindWindowByPID(g_pi.dwProcessId);
        if (hwnd) PostMessage(hwnd, WM_CLOSE, 0, 0);
        WaitForSingleObject(g_pi.hProcess, 2000);
        CloseHandle(g_pi.hProcess);
        g_pi.hProcess = NULL;
    }
    if (g_hMonitorJob) {
        CloseHandle(g_hMonitorJob);
        g_hMonitorJob = nullptr;
    }
}

static bool MonitorRunning() {
    if (!g_pi.hProcess) return false;
    DWORD code;
    if (GetExitCodeProcess(g_pi.hProcess, &code) && code == STILL_ACTIVE)
        return true;
    CloseHandle(g_pi.hProcess);
    g_pi.hProcess = NULL;
    if (g_hMonitorJob) {
        CloseHandle(g_hMonitorJob);
        g_hMonitorJob = nullptr;
    }
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

    HANDLE controlMutex = nullptr;
    if (!isRenderer) {
        const char* mutexName = isConfig
            ? "Local\\BlakholeRendererConfig"
            : "Local\\BlakholeRendererControl";
        controlMutex = CreateMutexA(nullptr, TRUE, mutexName);
        if (!controlMutex) return 1;
        if (GetLastError() == ERROR_ALREADY_EXISTS) {
            CloseHandle(controlMutex);
            return 0;
        }
    }

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

    BlackholeRenderer renderer;
    if (!renderer.init(program, wgl.width, wgl.height, debugLog)) {
        if (debugLog) { fprintf(debugLog, "[FAIL] BlackholeRenderer init failed\n"); fclose(debugLog); }
        GLTex_Shutdown(glTex);
        if (useWGC) { WGC_Release(wgcPri); if (crossScreen) WGC_Release(wgcSec); }
        else { DXGI_Release(dxgiPri); if (crossScreen) DXGI_Release(dxgiSec); }
        Win32GL_Shutdown(wgl); return 1;
    }

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
    MovementSpawn spawn = ResolveMovementSpawn(
        cfg.spawnPosition, randHomeX, randHomeY, randPhase, randPresetOff);
    float homeX = spawn.x;
    float homeY = spawn.y;
    float cursorHomeX = homeX;
    float cursorHomeY = homeY;
    float mouseVelX = 0.0f;
    float mouseVelY = 0.0f;
    float phaseOffset = spawn.phaseOffset;
    float presetOffset = spawn.presetOffset;
    if (debugLog) { fprintf(debugLog, "[Init] Spawn: spawnPosition=%d movementSpeed=%.2f home=(%.2f,%.2f) phase=%.2f presetOff=%.1f seed=%u (screenIdx=%d)\n",
                            cfg.spawnPosition, cfg.movementSpeed, homeX, homeY, phaseOffset, presetOffset, seed, screenIdx); fflush(debugLog); }

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
    bool recordingCaptureRuntimeAllowed = cfg.allowRecordingCapture;
    bool recordingCaptureFrozen = recordingCaptureRuntimeAllowed;
    Win32GL_SetCaptureExcluded(wgl, !recordingCaptureRuntimeAllowed);
    if (debugLog) {
        fprintf(debugLog, "[Init] Recording capture: %s (Ctrl+Alt+R toggles at runtime)\n",
                recordingCaptureRuntimeAllowed ? "allowed with frozen desktop texture" : "excluded by display affinity");
        fflush(debugLog);
    }
    Sleep(50);
    Win32GL_PollEvents(wgl);

        // First frame — let renderer handle it
    renderer.drawAndBloom(
        wgl.width, wgl.height, cfg, GLTex_GetTexture(glTex),
        0.0f, 0.0f, homeX, homeY,
        phaseOffset, presetOffset, 0.01f, debugLog);
    Win32GL_SwapBuffers(wgl);


    // 启用分层模式（鼠标穿透）
    Win32GL_EnableLayered(wgl);
    bool recordingHotkeyRegistered = false;
    if (screenIdx < 0) {
        recordingHotkeyRegistered = RegisterHotKey(
            wgl.hwnd, WIN32GL_RECORDING_HOTKEY_ID,
            MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 'R') != FALSE;
        if (!recordingHotkeyRegistered && debugLog) {
            fprintf(debugLog, "[RecordingCapture] RegisterHotKey failed: %lu\n", GetLastError());
            fflush(debugLog);
        }
    }
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

        double now = Win32GL_GetTime();
        if (!exiting && Win32GL_TakeRecordingHotkey(wgl)) {
            recordingCaptureRuntimeAllowed = !recordingCaptureRuntimeAllowed;
            recordingCaptureFrozen = recordingCaptureRuntimeAllowed;
            Win32GL_SetCaptureExcluded(wgl, !recordingCaptureRuntimeAllowed);
            fprintf(stderr, "[RecordingCapture] %s via Ctrl+Alt+R\n",
                    recordingCaptureRuntimeAllowed ? "allowed and frozen" : "excluded and live");
        }

        bool captureUpdateDue = !recordingCaptureFrozen;

        // 退出时跳过捕获，避免卡顿；录屏模式下完全冻结窗口显示前的桌面纹理，避免自捕获递归
        if (!exiting && captureUpdateDue) {
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

        float t = (float)(now - startTime);
        float ep = (float)time(nullptr);
        float frameHomeX = homeX;
        float frameHomeY = homeY;
        if (cfg.followMouse) {
            float inertia = cfg.mouseInertia;
            if (inertia < 0.0f) inertia = 0.0f;
            if (inertia > 1.0f) inertia = 1.0f;
            float mouseTargetX = cursorHomeX;
            float mouseTargetY = cursorHomeY;
            float allowedRadius = 0.018f + 0.057f * inertia;

            POINT cursorPos;
            if (GetCursorPos(&cursorPos)) {
                float targetX = ((float)cursorPos.x - (float)wgl.targetX) / (float)((wgl.width > 0) ? wgl.width : 1);
                float targetY = ((float)cursorPos.y - (float)wgl.targetY) / (float)((wgl.height > 0) ? wgl.height : 1);
                if (targetX < 0.0f) targetX = 0.0f;
                if (targetX > 1.0f) targetX = 1.0f;
                if (targetY < 0.0f) targetY = 0.0f;
                if (targetY > 1.0f) targetY = 1.0f;
                mouseTargetX = targetX;
                mouseTargetY = targetY;

                if (inertia <= 0.0001f) {
                    cursorHomeX = targetX;
                    cursorHomeY = targetY;
                    mouseVelX = 0.0f;
                    mouseVelY = 0.0f;
                } else {
                    float gravityStrength = cfg.limitMouseOvershoot
                        ? (0.0000025f + 0.0000065f * (1.0f - inertia))
                        : (0.0000045f + 0.0000105f * (1.0f - inertia));
                    float gravitySoftening = cfg.limitMouseOvershoot
                        ? (0.030f + 0.045f * inertia)
                        : (0.024f + 0.035f * inertia);
                    float settleRadius = 0.0045f + 0.0065f * inertia;
                    float settleSpeed = 0.00022f + 0.00018f * inertia;
                    float gravityDeadZone = settleRadius * 1.85f;
                    float maxGravityAccel = 0.00075f + 0.00055f * (1.0f - inertia);
                    float maxGravitySpeed = 0.0065f + 0.0035f * (1.0f - inertia);
                    float farReturnStrength = cfg.limitMouseOvershoot
                        ? (0.00055f + 0.00035f * (1.0f - inertia))
                        : (0.00080f + 0.00065f * (1.0f - inertia));
                    float driftKeep = 0.9960f - 0.0050f * (1.0f - inertia);
                    float maxSeparation = 0.26f + 0.30f * inertia;
                    float worldMargin = 0.04f;

                    float toMouseX = targetX - cursorHomeX;
                    float toMouseY = targetY - cursorHomeY;
                    float mouseDist = sqrtf(toMouseX * toMouseX + toMouseY * toMouseY);
                    float currentSpeed = sqrtf(mouseVelX * mouseVelX + mouseVelY * mouseVelY);

                    if (mouseDist < settleRadius && currentSpeed < settleSpeed) {
                        cursorHomeX = targetX;
                        cursorHomeY = targetY;
                        mouseVelX = 0.0f;
                        mouseVelY = 0.0f;
                    } else if (mouseDist > gravityDeadZone) {
                        float dirX = toMouseX / mouseDist;
                        float dirY = toMouseY / mouseDist;
                        float softenedDist = sqrtf(mouseDist * mouseDist + gravitySoftening * gravitySoftening);
                        float gravityAccel = gravityStrength / (softenedDist * softenedDist);
                        if (cfg.limitMouseOvershoot && gravityAccel > maxGravityAccel) gravityAccel = maxGravityAccel;
                        if (mouseDist > maxSeparation) {
                            gravityAccel += (mouseDist - maxSeparation) * farReturnStrength;
                        }
                        mouseVelX += dirX * gravityAccel;
                        mouseVelY += dirY * gravityAccel;
                    }

                    mouseVelX *= driftKeep;
                    mouseVelY *= driftKeep;

                    float gravitySpeed = sqrtf(mouseVelX * mouseVelX + mouseVelY * mouseVelY);
                    if (cfg.limitMouseOvershoot && gravitySpeed > maxGravitySpeed && gravitySpeed > 0.000001f) {
                        float speedScale = maxGravitySpeed / gravitySpeed;
                        mouseVelX *= speedScale;
                        mouseVelY *= speedScale;
                    }

                    cursorHomeX += mouseVelX;
                    cursorHomeY += mouseVelY;

                    float afterX = cursorHomeX - targetX;
                    float afterY = cursorHomeY - targetY;
                    float afterDist = sqrtf(afterX * afterX + afterY * afterY);
                    if (cfg.limitMouseOvershoot && afterDist > maxSeparation && afterDist > 0.000001f) {
                        float limitScale = maxSeparation / afterDist;
                        cursorHomeX = targetX + afterX * limitScale;
                        cursorHomeY = targetY + afterY * limitScale;

                        float outwardX = afterX / afterDist;
                        float outwardY = afterY / afterDist;
                        float outwardVel = mouseVelX * outwardX + mouseVelY * outwardY;
                        if (outwardVel > 0.0f) {
                            mouseVelX -= outwardX * outwardVel * 0.85f;
                            mouseVelY -= outwardY * outwardVel * 0.85f;
                        }
                    }

                    if (cfg.limitMouseOvershoot) {
                        if (cursorHomeX < -worldMargin) {
                            cursorHomeX = -worldMargin;
                            if (mouseVelX < 0.0f) mouseVelX *= -0.25f;
                        } else if (cursorHomeX > 1.0f + worldMargin) {
                            cursorHomeX = 1.0f + worldMargin;
                            if (mouseVelX > 0.0f) mouseVelX *= -0.25f;
                        }
                        if (cursorHomeY < -worldMargin) {
                            cursorHomeY = -worldMargin;
                            if (mouseVelY < 0.0f) mouseVelY *= -0.25f;
                        } else if (cursorHomeY > 1.0f + worldMargin) {
                            cursorHomeY = 1.0f + worldMargin;
                            if (mouseVelY > 0.0f) mouseVelY *= -0.25f;
                        }
                    }
                }
            }
            frameHomeX = cursorHomeX;
            frameHomeY = cursorHomeY;

            if (inertia > 0.0001f) {
                float wanderRadius = (0.018f + 0.057f * inertia) * 0.45f;
                float wanderX = cosf(t * 0.42f + phaseOffset) * wanderRadius;
                float wanderY = sinf(t * 0.33f + phaseOffset * 1.37f) * wanderRadius * 0.65f;
                frameHomeX += wanderX;
                frameHomeY += wanderY;
            }

            if (inertia > 0.0001f) {
                float frameDx = frameHomeX - mouseTargetX;
                float frameDy = frameHomeY - mouseTargetY;
                float frameDist = sqrtf(frameDx * frameDx + frameDy * frameDy);
                float maxSeparation = 0.26f + 0.30f * inertia;
                if (cfg.limitMouseOvershoot && frameDist > maxSeparation && frameDist > 0.000001f) {
                    float frameScale = maxSeparation / frameDist;
                    frameHomeX = mouseTargetX + frameDx * frameScale;
                    frameHomeY = mouseTargetY + frameDy * frameScale;
                }
            }

            if (cfg.limitMouseOvershoot) {
                float renderMargin = 0.0f;
                if (frameHomeX < -renderMargin) frameHomeX = -renderMargin;
                if (frameHomeX > 1.0f + renderMargin) frameHomeX = 1.0f + renderMargin;
                if (frameHomeY < -renderMargin) frameHomeY = -renderMargin;
                if (frameHomeY > 1.0f + renderMargin) frameHomeY = 1.0f + renderMargin;
            }
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

                renderer.drawAndBloom(fbW, fbH, cfg, GLTex_GetTexture(glTex),
                              t, t * cfg.movementSpeed,
                              frameHomeX, frameHomeY,
                              phaseOffset, presetOffset, bornProgress, debugLog);

        Win32GL_SwapBuffers(wgl);

        frames++;
        if (now - lastFps >= 1.0) {
            snprintf(title, sizeof(title), "Black Hole [%d FPS]", frames);
            Win32GL_SetWindowTitle(wgl, title);
            frames=0; lastFps=now;
        }
    }

    renderer.destroy();
    GLTex_Shutdown(glTex);
    if (useWGC) { WGC_Release(wgcPri); if (crossScreen) WGC_Release(wgcSec); }
    else { DXGI_Release(dxgiPri); if (crossScreen) DXGI_Release(dxgiSec); }
    if (recordingHotkeyRegistered) {
        UnregisterHotKey(wgl.hwnd, WIN32GL_RECORDING_HOTKEY_ID);
    }
    Win32GL_Shutdown(wgl);

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
    // 一屏一黑洞: 请求子渲染器关闭，Job Object 负责超时兜底。
    KillChildRenderers();
    return 0;
}
