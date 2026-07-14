#include "gl_program.h"
#include "gl_funcs.h"

#ifndef BLACKHOLE_USE_D3D11

GLuint compileShader(GLenum type, const std::string& source, FILE* debugLog) {
    GLuint shader = gl_CreateShader(type);
    const char* src = source.c_str();
    gl_ShaderSource(shader, 1, &src, nullptr);
    gl_CompileShader(shader);
    GLint ok = 0; gl_GetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    char log[4096]; gl_GetShaderInfoLog(shader, sizeof(log), nullptr, log);
    if (log[0] && debugLog) {
        fprintf(debugLog, "[SHADER_ERROR][%s] %s\n", type==GL_VERTEX_SHADER?"vert":"frag", log);
        fflush(debugLog);
    }
    if (!ok) {
        if (debugLog) {
            fprintf(debugLog, "[FAIL] Shader compilation failed (type=%s)\n", type==GL_VERTEX_SHADER?"vert":"frag");
            fprintf(debugLog, "[SOURCE] First 500 chars:\n%s\n", source.substr(0, 500).c_str());
            fflush(debugLog);
        }
        gl_DeleteShader(shader);
        return 0;
    }
    if (debugLog) { fprintf(debugLog, "[OK] Shader compiled (type=%s, ID=%u)\n", type==GL_VERTEX_SHADER?"vert":"frag", shader); fflush(debugLog); }
    return shader;
}

GLuint createProgram(const std::string& vert, const std::string& frag, FILE* debugLog) {
    if (debugLog) { fprintf(debugLog, "[Init] Compiling vertex shader...\n"); fflush(debugLog); }
    GLuint vs = compileShader(GL_VERTEX_SHADER, vert, debugLog);
    if (!vs) return 0;
    if (debugLog) { fprintf(debugLog, "[Init] Compiling fragment shader...\n"); fflush(debugLog); }
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, frag, debugLog);
    if (!fs) { gl_DeleteShader(vs); return 0; }
    GLuint prog = gl_CreateProgram();
    gl_AttachShader(prog, vs); gl_AttachShader(prog, fs);
    gl_LinkProgram(prog);
    GLint ok = 0; gl_GetProgramiv(prog, GL_LINK_STATUS, &ok);
    char log[4096]; gl_GetProgramInfoLog(prog, sizeof(log), nullptr, log);
    if (log[0] && debugLog) {
        fprintf(debugLog, "[LINK_ERROR] %s\n", log);
        fflush(debugLog);
    }
    if (!ok) {
        if (debugLog) { fprintf(debugLog, "[FAIL] Program link failed\n"); fflush(debugLog); }
        gl_DeleteProgram(prog); gl_DeleteShader(vs); gl_DeleteShader(fs); return 0;
    }
    if (debugLog) { fprintf(debugLog, "[OK] Program linked (ID=%u)\n", prog); fflush(debugLog); }
    gl_DeleteShader(vs); gl_DeleteShader(fs);
    return prog;
}

#endif // BLACKHOLE_USE_D3D11
