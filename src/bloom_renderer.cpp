#include "bloom_renderer.h"

#include "win32_gl.h"

#include <algorithm>
#include <windows.h>
#include <GL/gl.h>
#ifndef GL_COMPILE_STATUS
#include <GL/glcorearb.h>
#endif

namespace {

template <typename T>
bool LoadGLProc(T& target, const char* name, FILE* debugLog) {
    target = reinterpret_cast<T>(Win32GL_GetProcAddress(name));
    if (target) return true;
    if (debugLog) {
        fprintf(debugLog, "[Bloom][FAIL] Missing OpenGL function: %s\n", name);
        fflush(debugLog);
    }
    return false;
}

struct BloomGL {
    PFNGLCREATESHADERPROC CreateShader = nullptr;
    PFNGLSHADERSOURCEPROC ShaderSource = nullptr;
    PFNGLCOMPILESHADERPROC CompileShader = nullptr;
    PFNGLGETSHADERIVPROC GetShaderiv = nullptr;
    PFNGLGETSHADERINFOLOGPROC GetShaderInfoLog = nullptr;
    PFNGLDELETESHADERPROC DeleteShader = nullptr;
    PFNGLCREATEPROGRAMPROC CreateProgram = nullptr;
    PFNGLATTACHSHADERPROC AttachShader = nullptr;
    PFNGLLINKPROGRAMPROC LinkProgram = nullptr;
    PFNGLGETPROGRAMIVPROC GetProgramiv = nullptr;
    PFNGLGETPROGRAMINFOLOGPROC GetProgramInfoLog = nullptr;
    PFNGLDELETEPROGRAMPROC DeleteProgram = nullptr;
    PFNGLUSEPROGRAMPROC UseProgram = nullptr;
    PFNGLGETUNIFORMLOCATIONPROC GetUniformLocation = nullptr;
    PFNGLUNIFORM1IPROC Uniform1i = nullptr;
    PFNGLUNIFORM2FPROC Uniform2f = nullptr;
    PFNGLACTIVETEXTUREPROC ActiveTexture = nullptr;
    PFNGLGENVERTEXARRAYSPROC GenVertexArrays = nullptr;
    PFNGLBINDVERTEXARRAYPROC BindVertexArray = nullptr;
    PFNGLDELETEVERTEXARRAYSPROC DeleteVertexArrays = nullptr;
    PFNGLGENFRAMEBUFFERSPROC GenFramebuffers = nullptr;
    PFNGLBINDFRAMEBUFFERPROC BindFramebuffer = nullptr;
    PFNGLFRAMEBUFFERTEXTURE2DPROC FramebufferTexture2D = nullptr;
    PFNGLCHECKFRAMEBUFFERSTATUSPROC CheckFramebufferStatus = nullptr;
    PFNGLDELETEFRAMEBUFFERSPROC DeleteFramebuffers = nullptr;

    bool Load(FILE* debugLog) {
        return LoadGLProc(CreateShader, "glCreateShader", debugLog)
            && LoadGLProc(ShaderSource, "glShaderSource", debugLog)
            && LoadGLProc(CompileShader, "glCompileShader", debugLog)
            && LoadGLProc(GetShaderiv, "glGetShaderiv", debugLog)
            && LoadGLProc(GetShaderInfoLog, "glGetShaderInfoLog", debugLog)
            && LoadGLProc(DeleteShader, "glDeleteShader", debugLog)
            && LoadGLProc(CreateProgram, "glCreateProgram", debugLog)
            && LoadGLProc(AttachShader, "glAttachShader", debugLog)
            && LoadGLProc(LinkProgram, "glLinkProgram", debugLog)
            && LoadGLProc(GetProgramiv, "glGetProgramiv", debugLog)
            && LoadGLProc(GetProgramInfoLog, "glGetProgramInfoLog", debugLog)
            && LoadGLProc(DeleteProgram, "glDeleteProgram", debugLog)
            && LoadGLProc(UseProgram, "glUseProgram", debugLog)
            && LoadGLProc(GetUniformLocation, "glGetUniformLocation", debugLog)
            && LoadGLProc(Uniform1i, "glUniform1i", debugLog)
            && LoadGLProc(Uniform2f, "glUniform2f", debugLog)
            && LoadGLProc(ActiveTexture, "glActiveTexture", debugLog)
            && LoadGLProc(GenVertexArrays, "glGenVertexArrays", debugLog)
            && LoadGLProc(BindVertexArray, "glBindVertexArray", debugLog)
            && LoadGLProc(DeleteVertexArrays, "glDeleteVertexArrays", debugLog)
            && LoadGLProc(GenFramebuffers, "glGenFramebuffers", debugLog)
            && LoadGLProc(BindFramebuffer, "glBindFramebuffer", debugLog)
            && LoadGLProc(FramebufferTexture2D, "glFramebufferTexture2D", debugLog)
            && LoadGLProc(CheckFramebufferStatus, "glCheckFramebufferStatus", debugLog)
            && LoadGLProc(DeleteFramebuffers, "glDeleteFramebuffers", debugLog);
    }
};

const char* kFullscreenVertex = R"GLSL(#version 330 core
out vec2 uv;
void main() {
    vec2 p = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));
    uv = p;
    gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
}
)GLSL";

const char* kBlurFragment = R"GLSL(#version 330 core
in vec2 uv;
out vec4 fragColor;
uniform sampler2D sceneTexture;
uniform vec2 blurDirection;
uniform int extractBright;

void main() {
    float weight[5] = float[](0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);
    vec3 source = texture(sceneTexture, uv).rgb;
    if (extractBright > 0) source = max(source - vec3(1.05), vec3(0.0));
    vec2 texel = 1.0 / vec2(textureSize(sceneTexture, 0));
    vec3 result = source * weight[0];
    for (int i = 1; i < 5; ++i) {
        vec2 offset = blurDirection * texel * float(i);
        result += texture(sceneTexture, uv + offset).rgb * weight[i];
        result += texture(sceneTexture, uv - offset).rgb * weight[i];
    }
    fragColor = vec4(result, 1.0);
}
)GLSL";

const char* kCompositeFragment = R"GLSL(#version 330 core
in vec2 uv;
out vec4 fragColor;
uniform sampler2D sceneTexture;
uniform sampler2D bloomTexture;

void main() {
    vec3 scene = texture(sceneTexture, uv).rgb;
    vec3 bloom = texture(bloomTexture, uv).rgb;
    vec3 preservedScene = clamp(scene, vec3(0.0), vec3(1.0));
    vec3 hdrGlow = max(scene - vec3(1.0), vec3(0.0)) + bloom * 1.35;
    vec3 glowResponse = vec3(1.0) - exp(-hdrGlow * 1.05);
    vec3 mapped = preservedScene + (vec3(1.0) - preservedScene) * glowResponse;
    fragColor = vec4(mapped, 1.0);
}
)GLSL";

} // namespace

struct BloomRenderer {
    BloomGL gl;
    FILE* debugLog = nullptr;
    GLuint sceneFbo = 0;
    GLuint sceneTexture = 0;
    GLuint pingFbo[2] = {0, 0};
    GLuint pingTexture[2] = {0, 0};
    GLuint fullscreenVao = 0;
    GLuint blurProgram = 0;
    GLuint compositeProgram = 0;
    GLint blurSceneLoc = -1;
    GLint blurDirectionLoc = -1;
    GLint extractBrightLoc = -1;
    GLint compositeSceneLoc = -1;
    GLint bloomTextureLoc = -1;
    int width = 0;
    int height = 0;
    int bloomWidth = 0;
    int bloomHeight = 0;
    bool ready = false;
};

namespace {

GLuint CompileShader(BloomRenderer& bloom, GLenum type, const char* source) {
    GLuint shader = bloom.gl.CreateShader(type);
    bloom.gl.ShaderSource(shader, 1, &source, nullptr);
    bloom.gl.CompileShader(shader);
    GLint ok = GL_FALSE;
    bloom.gl.GetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (ok == GL_TRUE) return shader;

    char log[4096] = {};
    bloom.gl.GetShaderInfoLog(shader, sizeof(log), nullptr, log);
    if (bloom.debugLog) {
        fprintf(bloom.debugLog, "[Bloom][FAIL] Shader compile: %s\n", log);
        fflush(bloom.debugLog);
    }
    bloom.gl.DeleteShader(shader);
    return 0;
}

GLuint CreateProgram(BloomRenderer& bloom, const char* fragmentSource) {
    GLuint vertex = CompileShader(bloom, GL_VERTEX_SHADER, kFullscreenVertex);
    if (!vertex) return 0;
    GLuint fragment = CompileShader(bloom, GL_FRAGMENT_SHADER, fragmentSource);
    if (!fragment) {
        bloom.gl.DeleteShader(vertex);
        return 0;
    }

    GLuint program = bloom.gl.CreateProgram();
    bloom.gl.AttachShader(program, vertex);
    bloom.gl.AttachShader(program, fragment);
    bloom.gl.LinkProgram(program);
    bloom.gl.DeleteShader(vertex);
    bloom.gl.DeleteShader(fragment);

    GLint ok = GL_FALSE;
    bloom.gl.GetProgramiv(program, GL_LINK_STATUS, &ok);
    if (ok == GL_TRUE) return program;

    char log[4096] = {};
    bloom.gl.GetProgramInfoLog(program, sizeof(log), nullptr, log);
    if (bloom.debugLog) {
        fprintf(bloom.debugLog, "[Bloom][FAIL] Program link: %s\n", log);
        fflush(bloom.debugLog);
    }
    bloom.gl.DeleteProgram(program);
    return 0;
}

void DestroyTargets(BloomRenderer& bloom) {
    bloom.gl.BindFramebuffer(GL_FRAMEBUFFER, 0);
    if (bloom.sceneFbo) bloom.gl.DeleteFramebuffers(1, &bloom.sceneFbo);
    if (bloom.pingFbo[0] || bloom.pingFbo[1]) bloom.gl.DeleteFramebuffers(2, bloom.pingFbo);
    if (bloom.sceneTexture) glDeleteTextures(1, &bloom.sceneTexture);
    if (bloom.pingTexture[0] || bloom.pingTexture[1]) glDeleteTextures(2, bloom.pingTexture);
    bloom.sceneFbo = 0;
    bloom.sceneTexture = 0;
    bloom.pingFbo[0] = bloom.pingFbo[1] = 0;
    bloom.pingTexture[0] = bloom.pingTexture[1] = 0;
    bloom.ready = false;
}

bool CreateTarget(BloomRenderer& bloom, GLuint& fbo, GLuint& texture, int width, int height) {
    bloom.gl.GenFramebuffers(1, &fbo);
    bloom.gl.BindFramebuffer(GL_FRAMEBUFFER, fbo);
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    bloom.gl.FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
    GLenum status = bloom.gl.CheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status == GL_FRAMEBUFFER_COMPLETE) return true;
    if (bloom.debugLog) {
        fprintf(bloom.debugLog, "[Bloom][FAIL] FBO incomplete: 0x%04x\n", static_cast<unsigned>(status));
        fflush(bloom.debugLog);
    }
    return false;
}

bool ResizeTargets(BloomRenderer& bloom, int width, int height) {
    if (width <= 0 || height <= 0) return false;
    DestroyTargets(bloom);
    bloom.width = width;
    bloom.height = height;
    bloom.bloomWidth = std::max(1, width / 2);
    bloom.bloomHeight = std::max(1, height / 2);

    if (!CreateTarget(bloom, bloom.sceneFbo, bloom.sceneTexture, width, height)
        || !CreateTarget(bloom, bloom.pingFbo[0], bloom.pingTexture[0], bloom.bloomWidth, bloom.bloomHeight)
        || !CreateTarget(bloom, bloom.pingFbo[1], bloom.pingTexture[1], bloom.bloomWidth, bloom.bloomHeight)) {
        DestroyTargets(bloom);
        return false;
    }
    bloom.gl.BindFramebuffer(GL_FRAMEBUFFER, 0);
    bloom.ready = true;
    return true;
}

} // namespace

BloomRenderer* Bloom_Create(int width, int height, FILE* debugLog) {
    BloomRenderer* bloom = new BloomRenderer();
    bloom->debugLog = debugLog;
    if (!bloom->gl.Load(debugLog)) {
        delete bloom;
        return nullptr;
    }

    bloom->blurProgram = CreateProgram(*bloom, kBlurFragment);
    bloom->compositeProgram = CreateProgram(*bloom, kCompositeFragment);
    if (!bloom->blurProgram || !bloom->compositeProgram) {
        Bloom_Destroy(bloom);
        return nullptr;
    }

    bloom->blurSceneLoc = bloom->gl.GetUniformLocation(bloom->blurProgram, "sceneTexture");
    bloom->blurDirectionLoc = bloom->gl.GetUniformLocation(bloom->blurProgram, "blurDirection");
    bloom->extractBrightLoc = bloom->gl.GetUniformLocation(bloom->blurProgram, "extractBright");
    bloom->compositeSceneLoc = bloom->gl.GetUniformLocation(bloom->compositeProgram, "sceneTexture");
    bloom->bloomTextureLoc = bloom->gl.GetUniformLocation(bloom->compositeProgram, "bloomTexture");
    bloom->gl.GenVertexArrays(1, &bloom->fullscreenVao);

    if (!ResizeTargets(*bloom, width, height)) {
        Bloom_Destroy(bloom);
        return nullptr;
    }
    if (debugLog) {
        fprintf(debugLog, "[Bloom][OK] HDR targets ready: %dx%d, blur=%dx%d\n",
                bloom->width, bloom->height, bloom->bloomWidth, bloom->bloomHeight);
        fflush(debugLog);
    }
    return bloom;
}

bool Bloom_BeginScene(BloomRenderer* bloom, int width, int height, bool enabled) {
    if (!bloom || !enabled) {
        if (bloom) bloom->gl.BindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, width, height);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        return false;
    }
    if ((!bloom->ready || bloom->width != width || bloom->height != height)
        && !ResizeTargets(*bloom, width, height)) {
        bloom->gl.BindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, width, height);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        return false;
    }

    bloom->gl.BindFramebuffer(GL_FRAMEBUFFER, bloom->sceneFbo);
    glViewport(0, 0, width, height);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    return true;
}

void Bloom_EndScene(BloomRenderer* bloom, int width, int height, bool enabled) {
    if (!bloom || !enabled || !bloom->ready) return;

    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    bloom->gl.BindVertexArray(bloom->fullscreenVao);
    bloom->gl.UseProgram(bloom->blurProgram);
    bloom->gl.Uniform1i(bloom->blurSceneLoc, 0);

    constexpr int kBlurPasses = 10;
    for (int pass = 0; pass < kBlurPasses; ++pass) {
        const int target = pass & 1;
        const GLuint source = (pass == 0) ? bloom->sceneTexture : bloom->pingTexture[1 - target];
        bloom->gl.BindFramebuffer(GL_FRAMEBUFFER, bloom->pingFbo[target]);
        glViewport(0, 0, bloom->bloomWidth, bloom->bloomHeight);
        bloom->gl.ActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, source);
        bloom->gl.Uniform2f(bloom->blurDirectionLoc, target == 0 ? 1.0f : 0.0f, target == 0 ? 0.0f : 1.0f);
        bloom->gl.Uniform1i(bloom->extractBrightLoc, pass == 0 ? 1 : 0);
        glDrawArrays(GL_TRIANGLES, 0, 3);
    }

    bloom->gl.BindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, width, height);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    bloom->gl.UseProgram(bloom->compositeProgram);
    bloom->gl.ActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, bloom->sceneTexture);
    bloom->gl.Uniform1i(bloom->compositeSceneLoc, 0);
    bloom->gl.ActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, bloom->pingTexture[(kBlurPasses - 1) & 1]);
    bloom->gl.Uniform1i(bloom->bloomTextureLoc, 1);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    bloom->gl.ActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, 0);
    bloom->gl.ActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);
    bloom->gl.UseProgram(0);
    bloom->gl.BindVertexArray(0);
}

void Bloom_Destroy(BloomRenderer*& bloom) {
    if (!bloom) return;
    DestroyTargets(*bloom);
    if (bloom->fullscreenVao) bloom->gl.DeleteVertexArrays(1, &bloom->fullscreenVao);
    if (bloom->blurProgram) bloom->gl.DeleteProgram(bloom->blurProgram);
    if (bloom->compositeProgram) bloom->gl.DeleteProgram(bloom->compositeProgram);
    delete bloom;
    bloom = nullptr;
}
