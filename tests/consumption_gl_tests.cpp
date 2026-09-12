// 直接运行生产 Bloom 的 GLSL，避免仅匹配源码字符串。
#define GLFW_INCLUDE_NONE
#include <windows.h>
#include <GLFW/glfw3.h>
// 单翻译单元测试复用私有 shader 构建函数；该类型不会跨模块传递。
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsubobject-linkage"
#include "../src/bloom_renderer.cpp"
#pragma GCC diagnostic pop
#include <cmath>
#include <vector>
#include <fstream>
#include <sstream>
#include "../src/formula_texture.h"

static std::string ReadText(const char* path) {
    std::ifstream file(path, std::ios::binary);
    std::ostringstream stream;
    stream << file.rdbuf();
    return stream.str();
}

static int TestConsumption(BloomRenderer& renderer) {
    const std::string effect = ReadText("shaders/consumption.glsl");
    if (effect.empty()) { std::fprintf(stderr, "FAIL missing consumption shader\n"); return 10; }
    std::string body = ReadText("blackhole.glsl");
    const size_t entry = body.find("void mainImage(");
    if (entry == std::string::npos) return 11;
    body.insert(entry, effect + "\n");
    const std::string source = ReadText("shaders/frag_desktop_header.glsl") + body + R"GLSL(
uniform float testTime;
uniform float testInclination;
uniform vec2 testCenter;
void main() {
    DiskLook look = LOOK_DEFAULT;
    look.incl = testInclination;
    look.roll = -0.35;
    fragColor = vec4(consumptionRender(vec2(gl_FragCoord.x, iResolution.y-gl_FragCoord.y)/iResolution.xy,
                                    testCenter, 0.11, look), 1.0);
}
)GLSL";
    GLuint program = CreateProgram(renderer, source.c_str());
    if (!program) return 12;
    GLuint formula = CreateFormulaTexture();
    if (!formula) return 13;
    constexpr int width = 640, height = 360;
    GLuint desktop = 0;
    glGenTextures(1, &desktop);
    glBindTexture(GL_TEXTURE_2D, desktop);
    std::vector<unsigned char> pixels(width * height * 4, 255);
    for (int y = 0; y < height; ++y) for (int x = 0; x < width; ++x) {
        const size_t p = static_cast<size_t>(y * width + x) * 4;
        const bool line = x % 80 < 2 || y % 40 < 2;
        pixels[p] = line ? 180 : 18;
        pixels[p+1] = line ? 200 : 24;
        pixels[p+2] = line ? 210 : 30;
    }
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    auto uniform1f = reinterpret_cast<PFNGLUNIFORM1FPROC>(glfwGetProcAddress("glUniform1f"));
    auto uniform3f = reinterpret_cast<PFNGLUNIFORM3FPROC>(glfwGetProcAddress("glUniform3f"));
    auto uniform4f = reinterpret_cast<PFNGLUNIFORM4FPROC>(glfwGetProcAddress("glUniform4f"));
    if (!uniform1f || !uniform3f || !uniform4f) return 14;
    std::vector<float> frame(width * height * 4);
    std::vector<float> exhausted;
    std::vector<float> mouseBaseline;
    double fullMouseDifference = 0.0;
    for (int index = 0; index < 10; ++index) {
        const float seconds[] = {0, 30, 72, 90, 90, 90, 72, 90, 90, 90};
        const float inclinations[] = {1.05f, 1.05f, 1.05f, 1.05f, 0.05f, 1.55f, 1.05f, 1.05f, 1.05f, 1.05f};
        Bloom_BeginScene(&renderer, width, height, true);
        renderer.gl.UseProgram(program);
        renderer.gl.ActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, desktop);
        renderer.gl.Uniform1i(renderer.gl.GetUniformLocation(program, "iChannel0"), 0);
        renderer.gl.ActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, formula);
        renderer.gl.Uniform1i(renderer.gl.GetUniformLocation(program, "uFormulaTexture"), 2);
        uniform3f(renderer.gl.GetUniformLocation(program, "iResolution"), width, height, 0);
        uniform1f(renderer.gl.GetUniformLocation(program, "uConsumeTime"), seconds[index]);
        uniform1f(renderer.gl.GetUniformLocation(program, "iTime"), seconds[index]);
        uniform1f(renderer.gl.GetUniformLocation(program, "testInclination"), inclinations[index]);
        const float mouseEnergy = index == 8 ? 1.0f : (index == 9 ? 0.01f : 0.0f);
        uniform4f(renderer.gl.GetUniformLocation(program, "uFlowMouse"), 0.70f, 0.50f, mouseEnergy, 0.0f);
        renderer.gl.Uniform2f(renderer.gl.GetUniformLocation(program, "uConsumeOrigin"), 0.5f, 0.5f);
        renderer.gl.Uniform2f(renderer.gl.GetUniformLocation(program, "testCenter"), 0.5f, 0.5f);
        renderer.gl.BindVertexArray(renderer.fullscreenVao);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glReadPixels(0, 0, width, height, GL_RGBA, GL_FLOAT, frame.data());
        float peak = 0.0f;
        for (float v : frame) { if (!std::isfinite(v)) return 15; peak = std::max(v, peak); }
        if (peak <= 1.0f) { std::fprintf(stderr, "FAIL no HDR light\n"); return 16; }
        if (index == 2) exhausted = frame;
        if (index == 7) mouseBaseline = frame;
        if (index >= 8) {
            double difference = 0.0;
            double distantDifference = 0.0;
            for (int y = 0; y < height; ++y) for (int x = 0; x < width; ++x) {
                const size_t p = static_cast<size_t>(y * width + x) * 4;
                const float dx = (static_cast<float>(x) / width - 0.70f) * width / height;
                const float dy = static_cast<float>(y) / height - 0.50f;
                for (int c = 0; c < 3; ++c) {
                    const double delta = std::abs(frame[p+c] - mouseBaseline[p+c]);
                    difference += delta;
                    if (dx*dx + dy*dy > 0.16f) distantDifference += delta;
                }
            }
            std::printf("Mouse energy %.2f: difference=%.3f distant=%.3f\n", mouseEnergy, difference, distantDifference);
            if (index == 8) {
                fullMouseDifference = difference;
                if (difference < 1.0 || distantDifference > difference * 0.02) return 19;
            } else if (difference >= fullMouseDifference * 0.2) return 20;
        }
        if (index == 6) {
            for (size_t p = 0; p < frame.size(); ++p)
                if (std::abs(frame[p] - exhausted[p]) > 0.005f) {
                    std::fprintf(stderr, "FAIL exhausted desktop returned\n"); return 17;
                }
        }
        Bloom_EndScene(&renderer, width, height, true);
        std::vector<unsigned char> rgb(width * height * 3);
        glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, rgb.data());
        std::ofstream file("build/consumption-frame-" + std::to_string(index) + ".ppm", std::ios::binary);
        file << "P6\n" << width << " " << height << "\n255\n";
        for (int y = height-1; y >= 0; --y)
            file.write(reinterpret_cast<char*>(rgb.data()+y*width*3), width*3);
        if (index == 5) {
            renderer.gl.ActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, desktop);
            std::fill(pixels.begin(), pixels.end(), 255);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        }
    }
    glDeleteTextures(1, &desktop);
    glDeleteTextures(1, &formula);
    renderer.gl.DeleteProgram(program);
    return glGetError() == GL_NO_ERROR ? 0 : 18;
}

void* Win32GL_GetProcAddress(const char* name) {
    return reinterpret_cast<void*>(glfwGetProcAddress(name));
}

int main() {
    if (!glfwInit()) return 2;
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    GLFWwindow* window = glfwCreateWindow(640, 360, "GPU regression", nullptr, nullptr);
    if (!window) return 3;
    glfwMakeContextCurrent(window);
    std::printf("GPU: %s\n", glGetString(GL_RENDERER));
    BloomRenderer* bloom = Bloom_Create(128, 128, stderr);
    if (!bloom) return 4;
    for (float level : {0.0f, 0.25f, 0.5f, 1.0f}) {
        if (!Bloom_BeginScene(bloom, 128, 128, true)) return 5;
        glClearColor(level, level, level, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        Bloom_EndScene(bloom, 128, 128, true);
        float pixel[4] = {};
        glReadPixels(64, 64, 1, 1, GL_RGBA, GL_FLOAT, pixel);
        if (!std::isfinite(pixel[0]) || std::abs(pixel[0] - level) > 0.008f) {
            std::fprintf(stderr, "FAIL ordinary desktop glows: input=%.3f output=%.3f\n", level, pixel[0]);
            return 6;
        }
    }
    Bloom_BeginScene(bloom, 128, 128, true);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_SCISSOR_TEST);
    glScissor(60, 60, 8, 8);
    glClearColor(5, 4, 2, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_SCISSOR_TEST);
    Bloom_EndScene(bloom, 128, 128, true);
    float halo[4] = {};
    glReadPixels(70, 64, 1, 1, GL_RGBA, GL_FLOAT, halo);
    if (!(halo[0] > 0.01f && halo[0] < 0.9f) || glGetError() != GL_NO_ERROR) return 7;
    const int consumptionResult = TestConsumption(*bloom);
    Bloom_Destroy(bloom);
    glfwDestroyWindow(window);
    glfwTerminate();
    if (consumptionResult) return consumptionResult;
    std::puts("BLOOM_AND_CONSUMPTION_GPU_OK");
    return 0;
}
