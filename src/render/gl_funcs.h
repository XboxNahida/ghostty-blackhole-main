#pragma once
// Cross-platform GL function pointer declarations for the shared render core.
// The actual loading is platform-specific:
//   Windows: Win32GL_GetProcAddress (gl_funcs_win32.cpp)
//   Linux:   glfwGetProcAddress       (gl_funcs_linux.cpp)
// These pointers are used by the platform-neutral shader/render code.

#include <cstdio>
#include <GL/gl.h>

// Calling convention: WINAPI (__stdcall) on Windows, standard cdecl elsewhere.
#ifndef _WIN32
#define WINAPI
#endif

// Guard: on Windows the D3D11 path doesn't need these.
// The define is never set in the current build; kept for source compatibility.
#ifndef BLACKHOLE_USE_D3D11

#define DECL_GL_FUNC(ret, name, args) \
    typedef ret (WINAPI *PFN_##name##_PROC) args; \
    extern PFN_##name##_PROC gl_##name

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

/// Load all GL function pointers.
/// Returns false on any failure. Call once after a GL context is current.
/// Platform-specific implementations live in gl_funcs_win32.cpp / gl_funcs_linux.cpp.
bool loadGLFunctions();

#endif // BLACKHOLE_USE_D3D11
