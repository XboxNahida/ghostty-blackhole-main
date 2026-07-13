// blackhole-renderer  Linux OpenGL host for blackhole.glsl
// MVP: generated background only (no desktop capture, no Bloom).
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
#include <string>
#include <vector>
#include <unistd.h>
#include <GLFW/glfw3.h>

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
    fprintf(stderr, "Usage: %s --render [--windowed] --background generated\n", program);
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
static double g_lastCursorX = 0.0, g_lastCursorY = 0.0;
static bool g_cursorInitialized = false;
static const double MOUSE_THRESHOLD = 10.0; // pixels

static void keyCallback(GLFWwindow* win, int key, int, int action, int) {
    (void)win;
    if (action == GLFW_PRESS || action == GLFW_REPEAT)
        g_quit = true;
}

static void mouseButtonCallback(GLFWwindow* win, int, int action, int) {
    (void)win;
    if (action == GLFW_PRESS)
        g_quit = true;
}

static void cursorPosCallback(GLFWwindow* win, double x, double y) {
    (void)win;
    if (!g_cursorInitialized) {
        g_lastCursorX = x;
        g_lastCursorY = y;
        g_cursorInitialized = true;
        return;
    }
    double dx = x - g_lastCursorX;
    double dy = y - g_lastCursorY;
    double dist = std::sqrt(dx * dx + dy * dy);
    if (dist >= MOUSE_THRESHOLD) {
        g_quit = true;
    }
}

// ── Main ──
int main(int argc, char* argv[]) {
    bool windowed   = hasFlag(argc, argv, "--windowed");
    bool diagnostic = hasFlag(argc, argv, "--diagnostic");
    FILE* log = diagnostic ? stderr : nullptr;

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

    // ── Validate --background (DS-03: must be "generated") ──
    {
        const char* bg = flagValue(argc, argv, "--background");
        if (!bg || strcmp(bg, "generated") != 0) {
            fprintf(stderr, "ERROR: --background must be 'generated' (DS-03)\n");
            return 2;
        }
    }

    // ── Validate --render (required for DS-03) ──
    if (!hasFlag(argc, argv, "--render")) {
        fprintf(stderr, "ERROR: --render is required\n");
        return 2;
    }

    // ── GLFW init ──
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
    glfwShowWindow(win);

    // ── Determine actual framebuffer size and set viewport ──
    int fbW, fbH;
    glfwGetFramebufferSize(win, &fbW, &fbH);
    if (fbW > 0 && fbH > 0)
        glViewport(0, 0, fbW, fbH);

    // ── Render loop ──
    double startTime = glfwGetTime();
    int frames = 0;
    double lastFps = startTime;

    while (!glfwWindowShouldClose(win) && !g_quit) {
        glfwPollEvents();

        double now = glfwGetTime();
        float t = (float)(now - startTime);
        float ep = (float)time(nullptr);

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
    gl_DeleteProgram(program);
    gl_DeleteVertexArrays(1, &vao);
    gl_DeleteBuffers(1, &vbo);
    glDeleteTextures(1, &texID);
    glfwDestroyWindow(win);
    glfwTerminate();

    if (log) {
        fprintf(log, "[OK] blackhole-renderer exiting cleanly\n");
        fflush(log);
    }
    return EXIT_SUCCESS;
}
