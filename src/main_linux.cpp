// blackhole-renderer  Linux OpenGL host for blackhole.glsl
// XDG Portal + PipeWire desktop background, with generated fallback.
// Usage:
//   ./blackhole-renderer --windowed --background generated \
//       --config blackhole_presets.txt --resources ./ --diagnostic

#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <ctime>
#include <cstring>
#include <cerrno>
#include <climits>
#include <csignal>
#include <limits>
#include <string>
#include <vector>
#include <unistd.h>
#include <QCoreApplication>
#include "portal_capture.h"
#define GLFW_EXPOSE_NATIVE_WAYLAND
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include "wayland_surface_export.h"

#include "render/gl_funcs.h"
#include "render/shader_source.h"
#include "render/gl_program.h"
#include "gui_config.h"

// ── Command-line flag helpers ──
static bool hasFlag(int argc, char* argv[], const char* name) {
    for (int i = 1; i < argc; i++)
        if (strcmp(argv[i], name) == 0) return true;
    return false;
}

static const char* flagValue(int argc, char* argv[], const char* name) {
    for (int i = 1; i < argc - 1; i++)
        if (strcmp(argv[i], name) == 0) return argv[i + 1];
    return nullptr;
}

// ── CLI validation ──
static void printUsage(const char* program) {
    fprintf(stderr, "Usage: %s --render [--windowed] --background desktop|generated\n", program);
    fprintf(stderr, "       [--config <path>] [--resources <dir>] [--display <N>] [--diagnostic]\n");
}

static bool isValidArg(const char* arg) {
    static const char* known[] = {
        "--render", "--windowed", "--diagnostic", "--resources", "--background",
        "--config", "--display", "--help", nullptr
    };
    for (int i = 0; known[i]; i++)
        if (strcmp(arg, known[i]) == 0) return true;
    return false;
}

static int validateArgs(int argc, char* argv[]) {
    for (int i = 1; i < argc; i++) {
        const char* arg = argv[i];
        if (!isValidArg(arg)) {
            fprintf(stderr, "ERROR: unknown argument: %s\n", argv[i]);
            printUsage(argv[0]);
            return 2;
        }
        const bool needsValue = strcmp(arg, "--resources") == 0 ||
                                strcmp(arg, "--background") == 0 ||
                                strcmp(arg, "--config") == 0 ||
                                strcmp(arg, "--display") == 0;
        if (needsValue) {
            if (i + 1 >= argc || strncmp(argv[i + 1], "--", 2) == 0) {
                fprintf(stderr, "ERROR: %s requires a value\n", arg);
                return 2;
            }
            if (strcmp(arg, "--display") == 0) {
                char* end = nullptr;
                errno = 0;
                long value = strtol(argv[i + 1], &end, 10);
                if (errno || !end || *end != '\0' || value < 0 || value > INT_MAX) {
                    fprintf(stderr, "ERROR: --display requires a non-negative integer\n");
                    return 2;
                }
            }
            ++i;
        }
    }
    return 0;
}

// ── Input state ──
static bool g_quit = false;
static const char* g_quitReason = "window close";
static double g_lastCursorX = 0.0, g_lastCursorY = 0.0;
static bool g_cursorInitialized = false;
static const double MOUSE_THRESHOLD = 10.0; // pixels
static double g_mouseInputArmTime = std::numeric_limits<double>::infinity();
static volatile sig_atomic_t g_exitSignal = 0;

static void terminationSignalHandler(int signalNumber) {
    g_exitSignal = signalNumber;
}

static void armMouseInputAfterStartup() {
    g_cursorInitialized = false;
    g_mouseInputArmTime = glfwGetTime() + 0.75;
}

static void keyCallback(GLFWwindow* win, int key, int, int action, int) {
    (void)win;
    if (action == GLFW_PRESS || action == GLFW_REPEAT) {
        fprintf(stderr, "[INPUT] keyboard key=%d action=%d\n", key, action);
        g_quitReason = "keyboard input";
        g_quit = true;
    }
}

static void mouseButtonCallback(GLFWwindow* win, int, int action, int) {
    (void)win;
    if (action == GLFW_PRESS && glfwGetTime() >= g_mouseInputArmTime) {
        fprintf(stderr, "[INPUT] mouse button action=%d time=%.3f armTime=%.3f\n",
                action, glfwGetTime(), g_mouseInputArmTime);
        g_quitReason = "mouse button input";
        g_quit = true;
    }
}

static void cursorPosCallback(GLFWwindow* win, double x, double y) {
    (void)win;
    if (!g_cursorInitialized || glfwGetTime() < g_mouseInputArmTime) {
        g_lastCursorX = x;
        g_lastCursorY = y;
        g_cursorInitialized = true;
        return;
    }
    double dx = x - g_lastCursorX;
    double dy = y - g_lastCursorY;
    double dist = std::sqrt(dx * dx + dy * dy);
    if (dist >= MOUSE_THRESHOLD) {
        fprintf(stderr, "[INPUT] mouse movement distance=%.2f from=(%.1f,%.1f) to=(%.1f,%.1f)\n",
                dist, g_lastCursorX, g_lastCursorY, x, y);
        g_quitReason = "mouse movement";
        g_quit = true;
    }
}

static void windowFocusCallback(GLFWwindow*, int focused) {
    fprintf(stderr, "[WINDOW] focus=%d time=%.3f\n", focused, glfwGetTime());
}

static void windowIconifyCallback(GLFWwindow*, int iconified) {
    fprintf(stderr, "[WINDOW] iconified=%d time=%.3f\n", iconified, glfwGetTime());
}

static void windowCloseCallback(GLFWwindow* win) {
    fprintf(stderr, "[WINDOW] close requested time=%.3f\n", glfwGetTime());
    g_quitReason = "window close callback";
    glfwSetWindowShouldClose(win, GLFW_TRUE);
}

// ── Main ──
int main(int argc, char* argv[]) {
    bool windowed   = hasFlag(argc, argv, "--windowed");
    bool diagnostic = hasFlag(argc, argv, "--diagnostic");
    FILE* log = diagnostic ? stderr : nullptr;
    std::signal(SIGTERM, terminationSignalHandler);
    std::signal(SIGINT, terminationSignalHandler);

    // Validate CLI before any side effects
    {
        int ret = validateArgs(argc, argv);
        if (ret != 0) return ret;
    }
    if (hasFlag(argc, argv, "--help")) {
        printUsage(argv[0]);
        return 0;
    }

    const char* resourcesDir = flagValue(argc, argv, "--resources");
    if (resourcesDir && resourcesDir[0]) {
        if (chdir(resourcesDir) != 0) {
            fprintf(stderr, "FAILED: chdir to %s\n", resourcesDir);
            return EXIT_FAILURE;
        }
    }

    const char* background = flagValue(argc, argv, "--background");
    {
        if (!background || (strcmp(background, "generated") != 0 && strcmp(background, "desktop") != 0)) {
            fprintf(stderr, "ERROR: --background must be 'desktop' or 'generated'\n");
            return 2;
        }
    }

    // ── Validate --render (required for DS-03) ──
    if (!hasFlag(argc, argv, "--render")) {
        fprintf(stderr, "ERROR: --render is required\n");
        return 2;
    }

    QCoreApplication qtApplication(argc, argv);
    PortalCapture portal;
    const bool desktopCapture = strcmp(background, "desktop") == 0;
    bool portalFailed = false;
    QObject::connect(&portal, &PortalCapture::captureFailed, [&](const QString &reason) {
        portalFailed = true;
        fprintf(stderr, "[PORTAL] capture unavailable, using generated fallback: %s\n",
                reason.toUtf8().constData());
        armMouseInputAfterStartup();
    });
    QObject::connect(&portal, &PortalCapture::captureStarted, [&] {
        fprintf(stderr, "[PORTAL] PipeWire desktop stream started\n");
        armMouseInputAfterStartup();
    });
    // ── GLFW init ──
    // xdg-foreign must export the actual xdg_toplevel surface.  GLFW's
    // libdecor backend exposes its child content surface via the native API,
    // which Mutter correctly rejects as having an invalid role.
    glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_WAYLAND);
    glfwInitHint(GLFW_WAYLAND_LIBDECOR, GLFW_WAYLAND_DISABLE_LIBDECOR);
    if (!glfwInit()) {
        fprintf(stderr, "FAILED: glfwInit\n");
        return EXIT_FAILURE;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE); // shown after init

    // Determine window size and monitor
    int winW = 1280, winH = 720;
    GLFWmonitor* monitor = nullptr;
    if (!windowed) {
        const char* dispStr = flagValue(argc, argv, "--display");
        int dispIdx = dispStr ? static_cast<int>(strtol(dispStr, nullptr, 10)) : 0;
        int count = 0;
        GLFWmonitor** monitors = glfwGetMonitors(&count);
        if (dispIdx >= 0 && dispIdx < count) {
            monitor = monitors[dispIdx];
        } else if (dispStr) {
            fprintf(stderr, "ERROR: --display index %d out of range (available: %d)\n", dispIdx, count);
            glfwTerminate();
            return 2;
        } else {
            monitor = glfwGetPrimaryMonitor();
        }
        if (!monitor) {
            fprintf(stderr, "FAILED: no monitor available, falling back to windowed\n");
            windowed = true;
        } else {
            const GLFWvidmode* mode = glfwGetVideoMode(monitor);
            winW = mode->width;
            winH = mode->height;
            glfwWindowHint(GLFW_RED_BITS, mode->redBits);
            glfwWindowHint(GLFW_GREEN_BITS, mode->greenBits);
            glfwWindowHint(GLFW_BLUE_BITS, mode->blueBits);
            glfwWindowHint(GLFW_REFRESH_RATE, mode->refreshRate);
        }
    }

    GLFWwindow* win = glfwCreateWindow(winW, winH,
        "Black Hole [Linux]", monitor, nullptr);
    if (!win) {
        fprintf(stderr, "FAILED: glfwCreateWindow\n");
        glfwTerminate();
        return EXIT_FAILURE;
    }

    glfwMakeContextCurrent(win);
    glfwSwapInterval(1); // vsync
    WaylandSurfaceExport surfaceExport;

    // ── Log GL info ──
    {
        const char* vendor   = (const char*)glGetString(GL_VENDOR);
        const char* renderer = (const char*)glGetString(GL_RENDERER);
        const char* version  = (const char*)glGetString(GL_VERSION);
        if (log) {
            fprintf(log, "[GL] vendor:   %s\n", vendor ? vendor : "?");
            fprintf(log, "[GL] renderer: %s\n", renderer ? renderer : "?");
            fprintf(log, "[GL] version:  %s\n", version ? version : "?");
            fflush(log);
        }
        fprintf(stdout, "GL vendor=%s renderer=%s version=%s\n",
                vendor ? vendor : "?",
                renderer ? renderer : "?",
                version ? version : "?");
    }

    // ── Load GL function pointers ──
    if (!loadGLFunctions()) {
        fprintf(stderr, "FAILED: loadGLFunctions\n");
        glfwDestroyWindow(win);
        glfwTerminate();
        return EXIT_FAILURE;
    }

    // ── Load config (use --config path if provided) ──
    BlackholeConfig cfg;
    {
        const char* configPath = flagValue(argc, argv, "--config");
        char names[64][64];
        if (configPath) {
            if (!LoadPresetsFromFile(cfg, names, configPath)) {
                fprintf(stderr, "FAILED: cannot load config %s\n", configPath);
                glfwDestroyWindow(win);
                glfwTerminate();
                return EXIT_FAILURE;
            }
        } else {
            if (!LoadPresetsFromFile(cfg, names))
                InitDefaultPresets(cfg);
        }
        cfg.mode = 0;
    }

    // ── Build fragment shader ──
    std::string vertSrc = readFile("shaders/vert.glsl");
    std::string fragSrc;
    if (vertSrc.empty() || !buildFragmentShader(fragSrc, log)) {
        fprintf(stderr, "FAILED: shader assembly\n");
        glfwDestroyWindow(win);
        glfwTerminate();
        return EXIT_FAILURE;
    }

    GLuint program = createProgram(vertSrc, fragSrc, log);
    if (!program) {
        fprintf(stderr, "FAILED: shader program creation\n");
        glfwDestroyWindow(win);
        glfwTerminate();
        return EXIT_FAILURE;
    }

    // ── Full-screen quad VAO/VBO ──
    float verts[] = { -1,-1, 1,-1, -1,1, 1,1 };
    GLuint vao, vbo;
    gl_GenVertexArrays(1, &vao);
    gl_GenBuffers(1, &vbo);
    gl_BindVertexArray(vao);
    gl_BindBuffer(GL_ARRAY_BUFFER, vbo);
    gl_BufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
    gl_VertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2*sizeof(float), (void*)0);
    gl_EnableVertexAttribArray(0);
    gl_BindVertexArray(0);

    // ── Generated background texture (1x1 black) ──
    GLuint texID = 0;
    glGenTextures(1, &texID);
    glBindTexture(GL_TEXTURE_2D, texID);
    unsigned char black[] = {0, 0, 0, 255};
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, black);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    int textureWidth = 1, textureHeight = 1;

    // ── Cache uniform locations ──
    GLint locRes  = gl_GetUniformLocation(program, "iResolution");
    GLint locTime = gl_GetUniformLocation(program, "iTime");
    GLint locDate = gl_GetUniformLocation(program, "iDate");
    GLint locCh0  = gl_GetUniformLocation(program, "iChannel0");
    GLint locBorn = gl_GetUniformLocation(program, "uBornProgress");
    GLint locLight = gl_GetUniformLocation(program, "uLightingEffect");
    GLint locDist = gl_GetUniformLocation(program, "uDistortion");
    GLint locHoleR = gl_GetUniformLocation(program, "uHoleRadius");
    GLint locGain  = gl_GetUniformLocation(program, "uDiskGain");
    GLint locTemp  = gl_GetUniformLocation(program, "uDiskTemp");
    GLint locExpo  = gl_GetUniformLocation(program, "uExposure");
    GLint locSpeed = gl_GetUniformLocation(program, "uSpeed");
    GLint locStar  = gl_GetUniformLocation(program, "uStarGain");
    GLint locIncl  = gl_GetUniformLocation(program, "uDiskIncl");
    GLint locPresetCount = gl_GetUniformLocation(program, "uPresetCount");
    GLint locSlotSec = gl_GetUniformLocation(program, "uSlotSec");
    GLint locPlayMode = gl_GetUniformLocation(program, "uPlayMode");

    struct PresetUniform { const char* name; float DiskPreset::*field; };
    const PresetUniform presetUniforms[] = {
        {"uPresetTemp", &DiskPreset::temp}, {"uPresetIncl", &DiskPreset::incl},
        {"uPresetRoll", &DiskPreset::roll}, {"uPresetInner", &DiskPreset::inner},
        {"uPresetOuter", &DiskPreset::outer}, {"uPresetOpac", &DiskPreset::opac},
        {"uPresetDopp", &DiskPreset::dopp}, {"uPresetBeam", &DiskPreset::beam},
        {"uPresetGain", &DiskPreset::gain}, {"uPresetContr", &DiskPreset::contr},
        {"uPresetWind", &DiskPreset::wind}, {"uPresetSpd", &DiskPreset::speed},
        {"uPresetExpo", &DiskPreset::expo}, {"uPresetStar", &DiskPreset::star},
    };
    gl_UseProgram(program);
    const int presetCount = cfg.presetCount < 1 ? 1 : (cfg.presetCount > 64 ? 64 : cfg.presetCount);
    gl_Uniform1i(locPresetCount, presetCount);
    gl_Uniform1f(locSlotSec, cfg.slotSec);
    gl_Uniform1i(locPlayMode, cfg.playMode);
    for (const auto& uniform : presetUniforms) {
        float values[64];
        for (int i = 0; i < presetCount; ++i) values[i] = cfg.presets[i].*(uniform.field);
        gl_Uniform1fv(gl_GetUniformLocation(program, uniform.name), presetCount, values);
    }
    gl_UseProgram(0);
    if (log) {
        fprintf(log, "[CONFIG] presets=%d slot=%.3f playMode=%d firstTemp=%.1f\n",
                presetCount, cfg.slotSec, cfg.playMode, cfg.presets[0].temp);
    }

    // ── Show window ──
    glfwSetKeyCallback(win, keyCallback);
    glfwSetMouseButtonCallback(win, mouseButtonCallback);
    glfwSetCursorPosCallback(win, cursorPosCallback);
    glfwSetWindowFocusCallback(win, windowFocusCallback);
    glfwSetWindowIconifyCallback(win, windowIconifyCallback);
    glfwSetWindowCloseCallback(win, windowCloseCallback);
    if (!desktopCapture)
        armMouseInputAfterStartup();
    glfwShowWindow(win);
    fprintf(stderr,
            "[WINDOW] show requested fullscreen=%d size=%dx%d focused=%d visible=%d iconified=%d\n",
            monitor != nullptr, winW, winH,
            glfwGetWindowAttrib(win, GLFW_FOCUSED),
            glfwGetWindowAttrib(win, GLFW_VISIBLE),
            glfwGetWindowAttrib(win, GLFW_ICONIFIED));
    if (desktopCapture) {
        // Showing the window assigns its xdg_toplevel role on Wayland; Mutter
        // rejects an xdg-foreign export made before this point.
        glfwPollEvents();
        QObject::connect(&surfaceExport, &WaylandSurfaceExport::exported,
                         &portal, [&portal](const QString &parent) { portal.startCapture(parent); });
        QObject::connect(&surfaceExport, &WaylandSurfaceExport::exportFailed,
                         &portal, [&portalFailed](const QString &reason) {
            portalFailed = true;
            fprintf(stderr, "[PORTAL] parent export failed, using generated fallback: %s\n",
                    reason.toUtf8().constData());
            armMouseInputAfterStartup();
        });
        if (!surfaceExport.init(glfwGetWaylandDisplay(), glfwGetWaylandWindow(win))) {
            portalFailed = true;
            fprintf(stderr, "[PORTAL] xdg-foreign v2 is unavailable, using generated fallback\n");
            armMouseInputAfterStartup();
        } else {
            surfaceExport.exportSurface();
        }
    }

    // ── Determine actual framebuffer size and set viewport ──
    int fbW, fbH;
    glfwGetFramebufferSize(win, &fbW, &fbH);
    if (fbW > 0 && fbH > 0)
        glViewport(0, 0, fbW, fbH);

    if (log) {
        fprintf(log, "[OK] Ready, entering main loop\n");
        fflush(log);
    }

    // ── Render loop ──
    double startTime = glfwGetTime();
    int frames = 0;
    double lastFps = startTime;
    bool delayedActivationRequested = false;

    while (!glfwWindowShouldClose(win) && !g_quit) {
        if (g_exitSignal != 0) {
            static char signalReason[64];
            snprintf(signalReason, sizeof(signalReason), "external signal %d", int(g_exitSignal));
            g_quitReason = signalReason;
            g_quit = true;
            fprintf(stderr, "[SIGNAL] received signal=%d\n", int(g_exitSignal));
            break;
        }
        glfwPollEvents();
        QCoreApplication::processEvents(QEventLoop::AllEvents, 2);

        PortalCapture::Frame captured;
        if (desktopCapture && portal.takeLatestFrame(captured)) {
            glBindTexture(GL_TEXTURE_2D, texID);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            if (captured.width != textureWidth || captured.height != textureHeight) {
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, captured.width, captured.height, 0,
                             GL_BGRA, GL_UNSIGNED_BYTE, captured.pixels.constData());
                textureWidth = captured.width; textureHeight = captured.height;
                fprintf(stderr, "[PORTAL] rendering captured desktop frames at %dx%d\n",
                        textureWidth, textureHeight);
            } else {
                glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, textureWidth, textureHeight,
                                GL_BGRA, GL_UNSIGNED_BYTE, captured.pixels.constData());
            }
        }

        double now = glfwGetTime();
        float t = (float)(now - startTime);
        float ep = (float)time(nullptr);

        // The button release that launched this child can return focus to the
        // Qt settings window after the fullscreen surface has already mapped.
        // Re-request activation once that originating click has completed.
        if (!windowed && !delayedActivationRequested && t >= 0.50f) {
            delayedActivationRequested = true;
            fprintf(stderr, "[WINDOW] requesting delayed fullscreen activation time=%.3f focused=%d\n",
                    now, glfwGetWindowAttrib(win, GLFW_FOCUSED));
            glfwFocusWindow(win);
        }

        // Handle framebuffer size changes
        int newFbW, newFbH;
        glfwGetFramebufferSize(win, &newFbW, &newFbH);
        if (newFbW != fbW || newFbH != fbH) {
            fbW = newFbW;
            fbH = newFbH;
            if (fbW > 0 && fbH > 0)
                glViewport(0, 0, fbW, fbH);
        }

        // Skip drawing if framebuffer is zero-sized
        if (fbW == 0 || fbH == 0) {
            glfwSwapBuffers(win);
            continue;
        }

        gl_UseProgram(program);
        gl_ActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texID);
        gl_Uniform1i(locCh0, 0);
        gl_Uniform3f(locRes, (float)fbW, (float)fbH, 0);
        gl_Uniform1f(locTime, t);
        gl_Uniform4f(locDate, 0, 0, 0, ep);

        // Upload config uniforms from BlackholeConfig
        gl_Uniform1f(locBorn, 1.0f);
        gl_Uniform1i(locLight, cfg.lightingEffect ? 1 : 0);
        gl_Uniform1f(locDist, cfg.distortion);
        if (cfg.holeRadius >= 0.0f) gl_Uniform1f(locHoleR, cfg.holeRadius);
        if (cfg.diskGain   >= 0.0f) gl_Uniform1f(locGain, cfg.diskGain);
        if (cfg.diskTemp   >= 0.0f) gl_Uniform1f(locTemp, cfg.diskTemp);
        if (cfg.exposure   >= 0.0f) gl_Uniform1f(locExpo, cfg.exposure);
        if (cfg.spd        >= 0.0f) gl_Uniform1f(locSpeed, cfg.spd);
        if (cfg.starGain   >= 0.0f) gl_Uniform1f(locStar, cfg.starGain);
        if (cfg.diskIncl   >= 0.0f) gl_Uniform1f(locIncl, cfg.diskIncl);

        gl_BindVertexArray(vao);
        gl_DrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        gl_BindVertexArray(0);
        gl_UseProgram(0);

        glfwSwapBuffers(win);

        // FPS counter
        frames++;
        if (now - lastFps >= 1.0) {
            char title[128];
            snprintf(title, sizeof(title), "Black Hole [Linux] [%d FPS]", frames);
            glfwSetWindowTitle(win, title);
            frames = 0;
            lastFps = now;
        }
    }

    // ── Cleanup ──
    portal.stopCapture();
    gl_DeleteProgram(program);
    gl_DeleteVertexArrays(1, &vao);
    gl_DeleteBuffers(1, &vbo);
    glDeleteTextures(1, &texID);
    glfwDestroyWindow(win);
    glfwTerminate();

    if (log) {
        fprintf(log, "[OK] blackhole-renderer exiting cleanly: %s\n", g_quitReason);
        fflush(log);
    }
    return EXIT_SUCCESS;
}
