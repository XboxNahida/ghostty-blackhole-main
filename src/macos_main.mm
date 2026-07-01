#define GL_SILENCE_DEPRECATION
#define GLFW_INCLUDE_NONE

#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_COCOA
#include <GLFW/glfw3native.h>

#include <OpenGL/gl3.h>

#import <AppKit/AppKit.h>
#import <CoreGraphics/CoreGraphics.h>
#import <Foundation/Foundation.h>
#include <IOKit/IOKitLib.h>
#include <dispatch/dispatch.h>
#include <dlfcn.h>
#include <mach-o/dyld.h>
#include <objc/message.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

struct DiskPreset {
    float temp  = 5500.0f;
    float incl  = 1.50f;
    float roll  = 0.35f;
    float inner = 1.8f;
    float outer = 8.0f;
    float opac  = 0.90f;
    float dopp  = 0.60f;
    float beam  = 2.5f;
    float gain  = 2.2f;
    float contr = 1.6f;
    float wind  = 7.0f;
    float speed = 5.0f;
    float expo  = 1.40f;
    float star  = 0.0f;
};

struct BlackholeConfig {
    int   mode       = 1;     // 0=always, 1=idle
    int   idleSec    = 300;
    float holeRadius = -1.0f;
    float diskGain   = -1.0f;
    float diskTemp   = -1.0f;
    float exposure   = -1.0f;
    float spd        = -1.0f;
    float starGain   = -1.0f;
    float diskIncl   = -1.0f;

    int   presetCount = 0;
    DiskPreset presets[64];
    int   playMode        = 1;     // 0=sequence, 1=loop, 2=random
    float slotSec         = 5.25f;
    bool  videoAsIdle     = false; // kept for config compatibility; macOS does not inspect audio sessions yet
    bool  autoStart       = false; // kept for config compatibility; launch agent is not installed automatically
};

struct ScreenFrame {
    int width = 0;
    int height = 0;
    std::vector<unsigned char> rgba;
};

static const DiskPreset DEFAULT_PRESETS[16] = {
    {5500, 1.50f, 0.35f, 1.8f, 8.0f, 0.90f, 0.60f, 2.5f, 2.2f, 1.6f, 7.0f, 5.0f, 1.40f, 0.0f},
    {4500, 1.52f, 0.10f, 2.2f, 7.0f, 0.85f, 0.35f, 2.0f, 1.4f, 0.5f, 7.0f, 5.0f, 1.20f, 0.0f},
    {3800, 0.55f,-0.30f, 2.2f, 6.0f, 0.45f, 0.90f, 3.5f, 1.6f, 0.4f, 3.0f, 2.5f, 1.10f, 0.0f},
    {6500, 0.30f, 0.00f, 3.0f,10.0f, 0.50f, 0.80f, 2.5f, 1.0f, 1.1f, 7.0f, 5.0f, 1.00f, 0.0f},
    {15000,1.30f, 0.35f, 3.0f,14.0f, 0.35f, 1.00f, 4.0f, 1.2f, 1.3f, 8.0f, 5.0f, 0.80f, 0.0f},
    {18000,1.05f, 0.55f, 3.0f,16.0f, 0.30f, 1.00f, 5.0f, 1.0f, 1.5f, 9.0f, 6.0f, 0.75f, 0.0f},
    {5500, 1.50f, 0.35f, 1.8f, 8.0f, 0.00f, 1.00f, 2.5f, 0.0f, 1.6f, 7.0f, 5.0f, 1.00f, 0.6f},
    {5500, 1.50f, 0.35f, 1.8f, 8.0f, 0.90f, 0.60f, 2.5f, 2.2f, 1.6f, 7.0f, 5.0f, 1.40f, 0.0f},
    {3200, 1.45f, 0.60f, 2.0f, 9.0f, 0.95f, 0.20f, 1.5f, 3.0f, 2.0f, 5.0f, 4.0f, 1.60f, 0.0f},
    {8000, 1.20f,-0.50f, 2.5f, 7.0f, 0.70f, 0.75f, 2.8f, 1.8f, 1.4f, 8.0f, 5.5f, 1.30f, 0.0f},
    {2500, 1.55f, 0.20f, 1.6f, 6.0f, 0.60f, 0.10f, 1.2f, 2.6f, 1.8f, 4.0f, 3.0f, 1.50f, 0.0f},
    {12000,0.80f, 0.45f, 2.8f,12.0f, 0.40f, 0.95f, 3.5f, 1.5f, 1.2f, 8.5f, 5.0f, 0.90f, 0.0f},
    {5000, 0.10f, 0.00f, 2.0f, 9.0f, 0.85f, 0.50f, 2.0f, 1.3f, 1.5f, 6.0f, 4.5f, 1.10f, 0.0f},
    {22000,1.40f, 0.70f, 3.5f,18.0f, 0.25f, 1.00f, 4.5f, 0.9f, 1.7f,10.0f, 6.5f, 0.70f, 0.0f},
    {4200, 1.48f, 0.15f, 1.9f, 7.5f, 0.80f, 0.45f, 2.2f, 2.0f, 0.8f, 6.5f, 4.8f, 1.25f, 0.0f},
    {9000, 0.45f,-0.15f, 2.6f,11.0f, 0.55f, 0.85f, 3.0f, 1.1f, 1.3f, 7.5f, 5.2f, 1.05f, 0.0f},
};

static void initDefaultPresets(BlackholeConfig& cfg) {
    cfg.presetCount = 16;
    for (int i = 0; i < cfg.presetCount; ++i) cfg.presets[i] = DEFAULT_PRESETS[i];
}

static std::string readFile(const std::filesystem::path& path) {
    std::ifstream f(path, std::ios::in | std::ios::binary);
    if (!f) return "";
    std::stringstream ss;
    ss << f.rdbuf();
    std::string content = ss.str();
    if (content.size() >= 3 &&
        static_cast<unsigned char>(content[0]) == 0xEF &&
        static_cast<unsigned char>(content[1]) == 0xBB &&
        static_cast<unsigned char>(content[2]) == 0xBF) {
        content = content.substr(3);
    }
    return content;
}

static std::string bundleResourcePath() {
    @autoreleasepool {
        NSString* path = [[NSBundle mainBundle] resourcePath];
        if (path) return std::string([path UTF8String]);
    }
    return "";
}

static std::filesystem::path executableDir() {
    char path[4096];
    uint32_t size = sizeof(path);
    if (_NSGetExecutablePath(path, &size) == 0) {
        return std::filesystem::weakly_canonical(std::filesystem::path(path)).parent_path();
    }
    return std::filesystem::current_path();
}

static bool enterResourceDirectory() {
    std::vector<std::filesystem::path> candidates;
    std::string bundlePath = bundleResourcePath();
    if (!bundlePath.empty()) candidates.emplace_back(bundlePath);
    candidates.emplace_back(std::filesystem::current_path());
    std::filesystem::path exe = executableDir();
    candidates.emplace_back(exe);
    candidates.emplace_back(exe.parent_path());
    candidates.emplace_back(exe.parent_path().parent_path());

    for (const auto& dir : candidates) {
        if (std::filesystem::exists(dir / "blackhole.glsl") &&
            std::filesystem::exists(dir / "shaders" / "frag_desktop_header.glsl")) {
            std::filesystem::current_path(dir);
            return true;
        }
    }
    return false;
}

static bool loadPresetsFromFile(BlackholeConfig& cfg) {
    FILE* f = fopen("blackhole_presets.txt", "r");
    if (!f) return false;

    char line[256];
    if (!fgets(line, sizeof(line), f)) {
        fclose(f);
        return false;
    }
    if (!strstr(line, "v4")) {
        fclose(f);
        return false;
    }
    if (!fgets(line, sizeof(line), f)) {
        fclose(f);
        return false;
    }

    int mode = 1, idle = 300, playMode = 1, videoAsIdle = 0, autoStart = 0;
    float slotSec = 5.25f;
    int scanned = sscanf(line, "%d %d %f %d %d %d",
                         &mode, &idle, &slotSec, &playMode, &videoAsIdle, &autoStart);
    if (scanned >= 3) {
        cfg.mode = mode;
        cfg.idleSec = idle;
        cfg.slotSec = slotSec;
        cfg.playMode = playMode;
        cfg.videoAsIdle = scanned >= 5 ? videoAsIdle != 0 : false;
        cfg.autoStart = scanned >= 6 ? autoStart != 0 : false;
        if (!fgets(line, sizeof(line), f)) {
            fclose(f);
            return false;
        }
    }

    int count = atoi(line);
    if (count < 1 || count > 64) {
        fclose(f);
        return false;
    }

    for (int i = 0; i < count; ++i) {
        if (!fgets(line, sizeof(line), f)) break; // name
        if (!fgets(line, sizeof(line), f)) break; // values
        DiskPreset& p = cfg.presets[i];
        sscanf(line, "%f %f %f %f %f %f %f %f %f %f %f %f %f %f",
               &p.temp, &p.incl, &p.roll, &p.inner, &p.outer,
               &p.opac, &p.dopp, &p.beam, &p.gain, &p.contr,
               &p.wind, &p.speed, &p.expo, &p.star);
    }

    cfg.presetCount = count;
    fclose(f);
    return true;
}

static void writeDefaultConfigIfMissing() {
    if (std::filesystem::exists("blackhole_presets.txt")) return;
    BlackholeConfig cfg;
    initDefaultPresets(cfg);
    FILE* f = fopen("blackhole_presets.txt", "w");
    if (!f) return;
    fprintf(f, "# Blackhole Presets v4\n");
    fprintf(f, "%d %d %.3f %d %d %d\n", cfg.mode, cfg.idleSec, cfg.slotSec,
            cfg.playMode, static_cast<int>(cfg.videoAsIdle), static_cast<int>(cfg.autoStart));
    fprintf(f, "%d\n", cfg.presetCount);
    for (int i = 0; i < cfg.presetCount; ++i) {
        const DiskPreset& p = cfg.presets[i];
        fprintf(f, "Preset %d\n", i + 1);
        fprintf(f, "%.0f %.2f %.2f %.1f %.1f %.2f %.2f %.1f %.2f %.2f %.1f %.1f %.2f %.3f\n",
                p.temp, p.incl, p.roll, p.inner, p.outer,
                p.opac, p.dopp, p.beam, p.gain, p.contr,
                p.wind, p.speed, p.expo, p.star);
    }
    fclose(f);
}

static bool buildFragmentShader(std::string& out) {
    std::string header = readFile("shaders/frag_desktop_header.glsl");
    std::string body = readFile("blackhole.glsl");
    if (header.empty() || body.empty()) return false;

    // macOS exposes OpenGL 3.2+ through a core profile; gl_FragColor is not
    // available there, while the Windows compatibility context still accepts it.
    size_t fragColorDefine = header.find("#define fragColor gl_FragColor");
    if (fragColorDefine != std::string::npos) {
        header.replace(fragColorDefine, strlen("#define fragColor gl_FragColor"), "out vec4 fragColor;");
    }

    struct OverrideDef { const char* name; const char* uniform; } overrides[] = {
        {"HOLE_RADIUS", "uHoleRadius > 0.0 ? uHoleRadius :"},
        {"DISK_GAIN",   "uDiskGain > 0.0 ? uDiskGain :"},
        {"DISK_TEMP",   "uDiskTemp > 0.0 ? uDiskTemp :"},
        {"EXPOSURE",    "uExposure > 0.0 ? uExposure :"},
        {"DRIFT_SPEED", "uSpeed > 0.0 ? uSpeed :"},
        {"STAR_GAIN",   "uStarGain > 0.0 ? uStarGain :"},
        {"DISK_INCL",   "uDiskIncl > 0.0 ? uDiskIncl :"},
    };
    for (auto& o : overrides) {
        std::string prefix = std::string("const float ") + o.name + " = ";
        size_t pos = body.find(prefix);
        if (pos != std::string::npos) {
            size_t end = body.find(";", pos);
            if (end != std::string::npos) {
                std::string value = body.substr(pos + prefix.length(), end - pos - prefix.length());
                body.replace(pos, end - pos + 1,
                             std::string("float ") + o.name + " = " + o.uniform + " " + value + ";");
            }
        }
    }

    size_t demo = body.find("DiskLook demoLook()");
    if (demo != std::string::npos) {
        size_t open = body.find("{", demo);
        int depth = 0;
        size_t close = open;
        if (open != std::string::npos) {
            for (close = open; close < body.size(); ++close) {
                if (body[close] == '{') ++depth;
                if (body[close] == '}') {
                    --depth;
                    if (depth == 0) break;
                }
            }
        }
        if (close < body.size()) {
            std::string replacement =
                "DiskLook demoPreset(int i) {\n"
                "    return DiskLook(\n"
                "        uPresetTemp[i], uPresetIncl[i], uPresetRoll[i],\n"
                "        uPresetInner[i], uPresetOuter[i], uPresetOpac[i],\n"
                "        uPresetDopp[i], uPresetBeam[i], uPresetGain[i],\n"
                "        uPresetContr[i], uPresetWind[i], uPresetSpd[i],\n"
                "        uPresetExpo[i], uPresetStar[i]);\n"
                "}\n\n"
                "DiskLook demoLook() {\n"
                "    if (uPresetCount > 0) {\n"
                "        int n = int(clamp(float(uPresetCount), 1.0, float(MAX_PRESETS)));\n"
                "        float raw = (iTime + uPresetOffset) / max(uSlotSec, 0.5);\n"
                "        float f = smoothstep(1.0 - DEMO_XFADE, 1.0, fract(raw));\n"
                "        int i0;\n"
                "        int i1;\n"
                "        if (uPlayMode == 0) {\n"
                "            i0 = int(min(raw, float(n) - 0.001));\n"
                "            i1 = int(min(raw + 1.0, float(n) - 0.001));\n"
                "        } else if (uPlayMode == 2) {\n"
                "            int slot = int(raw);\n"
                "            i0 = int(fract(sin(float(slot) * 127.1 + 311.7) * 43758.5453) * float(n));\n"
                "            i1 = int(fract(sin(float(slot + 1) * 127.1 + 311.7) * 43758.5453) * float(n));\n"
                "        } else {\n"
                "            i0 = int(raw) % n;\n"
                "            i1 = (int(raw) + 1) % n;\n"
                "        }\n"
                "        return mixLook(demoPreset(i0), demoPreset(i1), f);\n"
                "    }\n"
                "    float u = mod(iTime, DEMO_SEC) / DEMO_SEC * float(DEMO_N);\n"
                "    int i = int(min(u, float(DEMO_N) - 0.001));\n"
                "    float f = smoothstep(1.0 - DEMO_XFADE, 1.0, fract(u));\n"
                "    return mixLook(DEMO_TOUR[i], DEMO_TOUR[(i + 1) % DEMO_N], f);\n"
                "}\n";
            body.replace(demo, close - demo + 1, replacement);
        }
    }

    size_t mode = body.find("#define SIZE_MODE MODE_TOKENS");
    if (mode != std::string::npos) {
        body.replace(mode, strlen("#define SIZE_MODE MODE_TOKENS"), "#define SIZE_MODE MODE_DEMO");
    }

    size_t grow = body.find("mod(iTime, DEMO_SEC) / DEMO_GROW_SEC");
    if (grow != std::string::npos) body.replace(grow, strlen("mod(iTime, DEMO_SEC) / DEMO_GROW_SEC"), "iTime / DEMO_GROW_SEC");

    size_t rh = body.find("float rh = HOLE_RADIUS * sz;");
    if (rh != std::string::npos) body.insert(rh, "    sz *= uBornProgress;\n");

    size_t workArea = body.find("const float WORK_AREA");
    if (workArea != std::string::npos) {
        size_t end = body.find(";", workArea);
        if (end != std::string::npos) body.replace(workArea, end - workArea + 1, "const float WORK_AREA = 0.0;");
    }

    size_t shield = body.find("float shield = vis * smoothstep(WORK_AREA");
    if (shield != std::string::npos) {
        size_t end = body.find(";", shield);
        if (end != std::string::npos) body.replace(shield, end - shield + 1, "float shield = vis;");
    }

    size_t homeX = body.find("const float TOKEN_HOME_X");
    if (homeX != std::string::npos) {
        size_t end = body.find(";", homeX);
        if (end != std::string::npos) body.replace(homeX, end - homeX + 1, "float TOKEN_HOME_X = uHomeX;");
    }
    size_t homeY = body.find("const float TOKEN_HOME_Y");
    if (homeY != std::string::npos) {
        size_t end = body.find(";", homeY);
        if (end != std::string::npos) body.replace(homeY, end - homeY + 1, "float TOKEN_HOME_Y = uHomeY;");
    }

    while (true) {
        size_t pos = body.find("lissa(t * TOKEN_CALM)");
        if (pos == std::string::npos) break;
        body.replace(pos, strlen("lissa(t * TOKEN_CALM)"), "lissa(t * TOKEN_CALM + uRandPhase)");
    }
    while (true) {
        size_t pos = body.find("lissa(t * TOKEN_RUSH)");
        if (pos == std::string::npos) break;
        body.replace(pos, strlen("lissa(t * TOKEN_RUSH)"), "lissa(t * TOKEN_RUSH + uRandPhase)");
    }
    while (true) {
        size_t pos = body.find("cos(t * 0.8)");
        if (pos == std::string::npos) break;
        body.replace(pos, strlen("cos(t * 0.8)"), "cos((t + uRandPhase) * 0.8)");
    }
    while (true) {
        size_t pos = body.find("sin(t * 1.0)");
        if (pos == std::string::npos) break;
        body.replace(pos, strlen("sin(t * 1.0)"), "sin((t + uRandPhase) * 1.0)");
    }

    size_t fade = body.find("const float DEMO_XFADE");
    if (fade != std::string::npos) {
        size_t end = body.find(";", fade);
        if (end != std::string::npos) body.replace(fade, end - fade + 1, "const float DEMO_XFADE = 0.65;");
    }

    out = header + "\n// ===== blackhole.glsl =====\n" + body +
          "\nvoid main() { vec4 c; vec2 fc = vec2(gl_FragCoord.x, iResolution.y - gl_FragCoord.y); mainImage(c, fc); fragColor = c; }\n";
    return true;
}

static GLuint compileShader(GLenum type, const std::string& source) {
    GLuint shader = glCreateShader(type);
    const char* src = source.c_str();
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    GLint ok = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[4096] = {};
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        fprintf(stderr, "Shader compile failed: %s\n", log);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

static GLuint createProgram(const std::string& vert, const std::string& frag) {
    GLuint vs = compileShader(GL_VERTEX_SHADER, vert);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, frag);
    if (!vs || !fs) return 0;

    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);

    GLint ok = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[4096] = {};
        glGetProgramInfoLog(program, sizeof(log), nullptr, log);
        fprintf(stderr, "Program link failed: %s\n", log);
        glDeleteProgram(program);
        program = 0;
    }

    glDeleteShader(vs);
    glDeleteShader(fs);
    return program;
}

static void makeFallbackFrame(ScreenFrame& frame, int width, int height) {
    frame.width = std::max(1, width);
    frame.height = std::max(1, height);
    frame.rgba.resize(static_cast<size_t>(frame.width) * frame.height * 4);
    for (int y = 0; y < frame.height; ++y) {
        for (int x = 0; x < frame.width; ++x) {
            float u = static_cast<float>(x) / std::max(1, frame.width - 1);
            float v = static_cast<float>(y) / std::max(1, frame.height - 1);
            size_t i = (static_cast<size_t>(y) * frame.width + x) * 4;
            frame.rgba[i + 0] = static_cast<unsigned char>(18 + 32 * u);
            frame.rgba[i + 1] = static_cast<unsigned char>(18 + 26 * v);
            frame.rgba[i + 2] = static_cast<unsigned char>(24 + 36 * (1.0f - u));
            frame.rgba[i + 3] = 255;
        }
    }
}

static bool captureMainDisplay(ScreenFrame& frame) {
    static bool didLoadScreenCaptureKit = false;
    static bool hasScreenCaptureKit = false;
    if (!didLoadScreenCaptureKit) {
        hasScreenCaptureKit = dlopen(
            "/System/Library/Frameworks/ScreenCaptureKit.framework/ScreenCaptureKit",
            RTLD_LAZY | RTLD_LOCAL) != nullptr;
        didLoadScreenCaptureKit = true;
    }
    if (!hasScreenCaptureKit) return false;

    Class manager = NSClassFromString(@"SCScreenshotManager");
    SEL selector = NSSelectorFromString(@"captureImageInRect:completionHandler:");
    if (!manager || ![manager respondsToSelector:selector]) return false;

    NSScreen* screen = [NSScreen mainScreen];
    if (!screen) return false;
    NSRect screenFrame = [screen frame];
    CGRect captureRect = CGRectMake(
        screenFrame.origin.x,
        screenFrame.origin.y,
        screenFrame.size.width,
        screenFrame.size.height);

    dispatch_semaphore_t sema = dispatch_semaphore_create(0);
    __block CGImageRef image = nullptr;
    using CaptureImageFn = void (*)(id, SEL, CGRect, void (^)(CGImageRef, NSError*));
    auto captureImage = reinterpret_cast<CaptureImageFn>(objc_msgSend);
    captureImage(manager, selector, captureRect, ^(CGImageRef capturedImage, NSError* error) {
        if (capturedImage && !error) image = CGImageRetain(capturedImage);
        dispatch_semaphore_signal(sema);
    });

    dispatch_time_t timeout = dispatch_time(DISPATCH_TIME_NOW, 2 * NSEC_PER_SEC);
    if (dispatch_semaphore_wait(sema, timeout) != 0 || !image) return false;

    if (!image) return false;

    size_t width = CGImageGetWidth(image);
    size_t height = CGImageGetHeight(image);
    if (width == 0 || height == 0) {
        CGImageRelease(image);
        return false;
    }

    frame.width = static_cast<int>(width);
    frame.height = static_cast<int>(height);
    frame.rgba.assign(width * height * 4, 0);

    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
    CGContextRef ctx = CGBitmapContextCreate(
        frame.rgba.data(),
        width,
        height,
        8,
        width * 4,
        colorSpace,
        kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big);

    if (!ctx) {
        CGColorSpaceRelease(colorSpace);
        CGImageRelease(image);
        return false;
    }

    CGContextDrawImage(ctx, CGRectMake(0, 0, width, height), image);
    CGContextRelease(ctx);
    CGColorSpaceRelease(colorSpace);
    CGImageRelease(image);
    return true;
}

static double idleSeconds() {
    io_iterator_t iter = IO_OBJECT_NULL;
    kern_return_t kr = IOServiceGetMatchingServices(kIOMainPortDefault, IOServiceMatching("IOHIDSystem"), &iter);
    if (kr != KERN_SUCCESS) return 0.0;

    io_registry_entry_t entry = IOIteratorNext(iter);
    IOObjectRelease(iter);
    if (!entry) return 0.0;

    CFTypeRef value = IORegistryEntryCreateCFProperty(entry, CFSTR("HIDIdleTime"), kCFAllocatorDefault, 0);
    IOObjectRelease(entry);
    if (!value) return 0.0;

    uint64_t nanos = 0;
    if (CFGetTypeID(value) == CFNumberGetTypeID()) {
        CFNumberGetValue((CFNumberRef)value, kCFNumberSInt64Type, &nanos);
    }
    CFRelease(value);
    return static_cast<double>(nanos) / 1000000000.0;
}

static bool isIdleFor(const BlackholeConfig& cfg) {
    return idleSeconds() >= std::max(1, cfg.idleSec);
}

static void configureMacWindow(GLFWwindow* window) {
    @autoreleasepool {
        [NSApp setActivationPolicy:NSApplicationActivationPolicyAccessory];
        NSWindow* nsWindow = glfwGetCocoaWindow(window);
        if (!nsWindow) return;
        [nsWindow setLevel:NSScreenSaverWindowLevel];
        [nsWindow setCollectionBehavior:
            NSWindowCollectionBehaviorCanJoinAllSpaces |
            NSWindowCollectionBehaviorFullScreenAuxiliary |
            NSWindowCollectionBehaviorStationary |
            NSWindowCollectionBehaviorIgnoresCycle];
        [nsWindow setIgnoresMouseEvents:YES];
        [nsWindow setOpaque:NO];
        [nsWindow setBackgroundColor:[NSColor clearColor]];
        [nsWindow orderFrontRegardless];
    }
}

static void uploadTexture(GLuint texture, const ScreenFrame& frame) {
    glBindTexture(GL_TEXTURE_2D, texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, frame.width, frame.height, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, frame.rgba.data());
}

static void updateUniformArray(GLint loc, const BlackholeConfig& cfg, float DiskPreset::*member) {
    if (loc < 0 || cfg.presetCount <= 0) return;
    float values[64] = {};
    int count = std::min(cfg.presetCount, 64);
    for (int i = 0; i < count; ++i) values[i] = cfg.presets[i].*member;
    glUniform1fv(loc, count, values);
}

static int runRenderer(const BlackholeConfig& cfg, bool exitWhenUserReturns) {
    if (!glfwInit()) {
        fprintf(stderr, "glfwInit failed\n");
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
    glfwWindowHint(GLFW_FLOATING, GLFW_TRUE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE);

    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = monitor ? glfwGetVideoMode(monitor) : nullptr;
    int winW = mode ? mode->width : 1440;
    int winH = mode ? mode->height : 900;
    int posX = 0, posY = 0;
    if (monitor) glfwGetMonitorPos(monitor, &posX, &posY);

    GLFWwindow* window = glfwCreateWindow(winW, winH, "Black Hole", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        return 1;
    }
    glfwSetWindowPos(window, posX, posY);
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    configureMacWindow(window);

    std::string vert = readFile("shaders/vert.glsl");
    std::string frag;
    if (vert.empty() || !buildFragmentShader(frag)) {
        fprintf(stderr, "Failed to load shader resources\n");
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    GLuint program = createProgram(vert, frag);
    if (!program) {
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    float quad[] = {-1.0f,-1.0f, 1.0f,-1.0f, -1.0f,1.0f, 1.0f,1.0f};
    GLuint vao = 0, vbo = 0;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);

    ScreenFrame frame;
    if (!captureMainDisplay(frame)) {
        fprintf(stderr, "Screen capture unavailable; using fallback background. Grant Screen Recording permission for live desktop lensing.\n");
        makeFallbackFrame(frame, winW, winH);
    }

    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    uploadTexture(texture, frame);

    GLint locRes = glGetUniformLocation(program, "iResolution");
    GLint locTime = glGetUniformLocation(program, "iTime");
    GLint locDate = glGetUniformLocation(program, "iDate");
    GLint locCh0 = glGetUniformLocation(program, "iChannel0");
    GLint locBorn = glGetUniformLocation(program, "uBornProgress");
    GLint locHomeX = glGetUniformLocation(program, "uHomeX");
    GLint locHomeY = glGetUniformLocation(program, "uHomeY");
    GLint locPhase = glGetUniformLocation(program, "uRandPhase");
    GLint locPresetOff = glGetUniformLocation(program, "uPresetOffset");

    GLint locHoleRadius = glGetUniformLocation(program, "uHoleRadius");
    GLint locDiskGain = glGetUniformLocation(program, "uDiskGain");
    GLint locDiskTemp = glGetUniformLocation(program, "uDiskTemp");
    GLint locExposure = glGetUniformLocation(program, "uExposure");
    GLint locSpeed = glGetUniformLocation(program, "uSpeed");
    GLint locStarGain = glGetUniformLocation(program, "uStarGain");
    GLint locDiskIncl = glGetUniformLocation(program, "uDiskIncl");
    GLint locPlayMode = glGetUniformLocation(program, "uPlayMode");
    GLint locSlotSec = glGetUniformLocation(program, "uSlotSec");
    GLint locPresetCount = glGetUniformLocation(program, "uPresetCount");
    GLint locPresetTemp = glGetUniformLocation(program, "uPresetTemp");
    GLint locPresetIncl = glGetUniformLocation(program, "uPresetIncl");
    GLint locPresetRoll = glGetUniformLocation(program, "uPresetRoll");
    GLint locPresetInner = glGetUniformLocation(program, "uPresetInner");
    GLint locPresetOuter = glGetUniformLocation(program, "uPresetOuter");
    GLint locPresetOpac = glGetUniformLocation(program, "uPresetOpac");
    GLint locPresetDopp = glGetUniformLocation(program, "uPresetDopp");
    GLint locPresetBeam = glGetUniformLocation(program, "uPresetBeam");
    GLint locPresetGain = glGetUniformLocation(program, "uPresetGain");
    GLint locPresetContr = glGetUniformLocation(program, "uPresetContr");
    GLint locPresetWind = glGetUniformLocation(program, "uPresetWind");
    GLint locPresetSpd = glGetUniformLocation(program, "uPresetSpd");
    GLint locPresetExpo = glGetUniformLocation(program, "uPresetExpo");
    GLint locPresetStar = glGetUniformLocation(program, "uPresetStar");

    srand(static_cast<unsigned>(time(nullptr)));
    float randHomeX = 0.15f + 0.70f * static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
    float randHomeY = 0.15f + 0.70f * static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
    float randPhase = 6.2831853f * static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
    float randPresetOff = 60.0f * static_cast<float>(rand()) / static_cast<float>(RAND_MAX);

    auto start = std::chrono::steady_clock::now();
    auto bornStart = start;
    auto lastCapture = start - std::chrono::seconds(1);
    bool exiting = false;
    auto exitStart = start;
    constexpr double birthDuration = 0.8;
    constexpr double dieDuration = 0.5;

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        auto now = std::chrono::steady_clock::now();
        double seconds = std::chrono::duration<double>(now - start).count();

        if (!exiting && exitWhenUserReturns && !isIdleFor(cfg)) {
            exiting = true;
            exitStart = now;
        }
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            exiting = true;
            exitStart = now;
        }

        if (!exiting && now - lastCapture >= std::chrono::milliseconds(250)) {
            ScreenFrame next;
            if (captureMainDisplay(next)) {
                frame = std::move(next);
                uploadTexture(texture, frame);
            }
            lastCapture = now;
        }

        double phaseSeconds = std::chrono::duration<double>(now - (exiting ? exitStart : bornStart)).count();
        float bornProgress = exiting
            ? 1.0f - static_cast<float>(phaseSeconds / dieDuration)
            : static_cast<float>(phaseSeconds / birthDuration);
        bornProgress = std::clamp(bornProgress, 0.01f, 1.0f);
        if (exiting && bornProgress <= 0.02f) break;

        int fbW = 0, fbH = 0;
        glfwGetFramebufferSize(window, &fbW, &fbH);
        glViewport(0, 0, fbW, fbH);
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(program);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture);
        glUniform1i(locCh0, 0);
        glUniform3f(locRes, static_cast<float>(fbW), static_cast<float>(fbH), 0.0f);
        glUniform1f(locTime, static_cast<float>(seconds));
        glUniform4f(locDate, 0.0f, 0.0f, 0.0f, static_cast<float>(time(nullptr)));
        glUniform1f(locBorn, bornProgress);
        glUniform1f(locHomeX, randHomeX);
        glUniform1f(locHomeY, randHomeY);
        glUniform1f(locPhase, randPhase);
        glUniform1f(locPresetOff, randPresetOff);

        glUniform1f(locHoleRadius, cfg.holeRadius);
        glUniform1f(locDiskGain, cfg.diskGain);
        glUniform1f(locDiskTemp, cfg.diskTemp);
        glUniform1f(locExposure, cfg.exposure);
        glUniform1f(locSpeed, cfg.spd);
        glUniform1f(locStarGain, cfg.starGain);
        glUniform1f(locDiskIncl, cfg.diskIncl);
        glUniform1i(locPlayMode, cfg.playMode);
        glUniform1f(locSlotSec, cfg.slotSec);
        glUniform1i(locPresetCount, cfg.presetCount);
        updateUniformArray(locPresetTemp, cfg, &DiskPreset::temp);
        updateUniformArray(locPresetIncl, cfg, &DiskPreset::incl);
        updateUniformArray(locPresetRoll, cfg, &DiskPreset::roll);
        updateUniformArray(locPresetInner, cfg, &DiskPreset::inner);
        updateUniformArray(locPresetOuter, cfg, &DiskPreset::outer);
        updateUniformArray(locPresetOpac, cfg, &DiskPreset::opac);
        updateUniformArray(locPresetDopp, cfg, &DiskPreset::dopp);
        updateUniformArray(locPresetBeam, cfg, &DiskPreset::beam);
        updateUniformArray(locPresetGain, cfg, &DiskPreset::gain);
        updateUniformArray(locPresetContr, cfg, &DiskPreset::contr);
        updateUniformArray(locPresetWind, cfg, &DiskPreset::wind);
        updateUniformArray(locPresetSpd, cfg, &DiskPreset::speed);
        updateUniformArray(locPresetExpo, cfg, &DiskPreset::expo);
        updateUniformArray(locPresetStar, cfg, &DiskPreset::star);

        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glBindVertexArray(0);
        glUseProgram(0);

        glfwSwapBuffers(window);
    }

    glDeleteTextures(1, &texture);
    glDeleteProgram(program);
    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

static void printHelp(const char* argv0) {
    printf("Black Hole macOS\n");
    printf("Usage: %s [--render|--monitor|--config|--help]\n", argv0);
    printf("  --render   Show the black hole immediately.\n");
    printf("  --monitor  Wait for idleSec in blackhole_presets.txt, then show until activity returns.\n");
    printf("  --config   Create blackhole_presets.txt if missing and print its path.\n");
}

int main(int argc, char* argv[]) {
    @autoreleasepool {
        [NSApplication sharedApplication];
    }

    if (!enterResourceDirectory()) {
        fprintf(stderr, "Could not find blackhole.glsl and shaders/. Run from the repo root or the .app bundle.\n");
        return 1;
    }

    BlackholeConfig cfg;
    if (!loadPresetsFromFile(cfg)) initDefaultPresets(cfg);

    bool render = argc >= 2 && strcmp(argv[1], "--render") == 0;
    bool monitor = argc >= 2 && strcmp(argv[1], "--monitor") == 0;
    bool config = argc >= 2 && strcmp(argv[1], "--config") == 0;
    bool help = argc >= 2 && strcmp(argv[1], "--help") == 0;

    if (help) {
        printHelp(argv[0]);
        return 0;
    }
    if (config) {
        writeDefaultConfigIfMissing();
        printf("%s\n", std::filesystem::absolute("blackhole_presets.txt").string().c_str());
        return 0;
    }
    if (render) {
        return runRenderer(cfg, false);
    }

    if (!monitor && cfg.mode == 0) {
        return runRenderer(cfg, false);
    }

    fprintf(stderr, "Black Hole macOS monitor: waiting for %d seconds idle. Press Ctrl-C to exit.\n", cfg.idleSec);
    while (true) {
        if (isIdleFor(cfg)) {
            int code = runRenderer(cfg, true);
            if (code != 0) return code;
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}
