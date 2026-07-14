// Windows implementation of GL function pointer loading.
// Uses wglGetProcAddress via Win32GL_GetProcAddress.
#include "gl_funcs.h"

#ifndef BLACKHOLE_USE_D3D11

// Declared as extern in gl_funcs.h; defined here.
#define DEFINE_GL_FUNC(name) PFN_##name##_PROC gl_##name = nullptr

DEFINE_GL_FUNC(CreateShader);
DEFINE_GL_FUNC(ShaderSource);
DEFINE_GL_FUNC(CompileShader);
DEFINE_GL_FUNC(GetShaderiv);
DEFINE_GL_FUNC(GetShaderInfoLog);
DEFINE_GL_FUNC(CreateProgram);
DEFINE_GL_FUNC(AttachShader);
DEFINE_GL_FUNC(LinkProgram);
DEFINE_GL_FUNC(GetProgramiv);
DEFINE_GL_FUNC(GetProgramInfoLog);
DEFINE_GL_FUNC(DeleteShader);
DEFINE_GL_FUNC(UseProgram);
DEFINE_GL_FUNC(GetUniformLocation);
DEFINE_GL_FUNC(Uniform3f);
DEFINE_GL_FUNC(Uniform1f);
DEFINE_GL_FUNC(Uniform1i);
DEFINE_GL_FUNC(ActiveTexture);
DEFINE_GL_FUNC(Uniform4f);
DEFINE_GL_FUNC(Uniform1fv);
DEFINE_GL_FUNC(GenVertexArrays);
DEFINE_GL_FUNC(GenBuffers);
DEFINE_GL_FUNC(BindVertexArray);
DEFINE_GL_FUNC(BindBuffer);
DEFINE_GL_FUNC(BufferData);
DEFINE_GL_FUNC(VertexAttribPointer);
DEFINE_GL_FUNC(EnableVertexAttribArray);
DEFINE_GL_FUNC(DrawArrays);
DEFINE_GL_FUNC(DeleteVertexArrays);
DEFINE_GL_FUNC(DeleteBuffers);
DEFINE_GL_FUNC(DeleteProgram);

// Forward declaration of the platform GL function loader.
// On Windows this is declared in win32_gl.h.
extern void* Win32GL_GetProcAddress(const char* name);

#define LOAD_GL_FUNC(name) do { \
    gl_##name = (PFN_##name##_PROC)Win32GL_GetProcAddress("gl" #name); \
    if (!gl_##name) { fprintf(stderr, "Failed to load gl" #name "\n"); return false; } \
} while(0)

bool loadGLFunctions() {
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

#endif // BLACKHOLE_USE_D3D11
