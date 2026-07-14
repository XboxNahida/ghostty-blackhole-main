#pragma once
#include <string>
#include <cstdio>
#include <GL/gl.h>

/// Compile a single GL shader from source.
/// Returns 0 on failure. Logs to debugLog when non-null.
GLuint compileShader(GLenum type, const std::string& source, FILE* debugLog = nullptr);

/// Create a GL program by compiling vertex and fragment sources and linking.
/// Returns 0 on failure.
GLuint createProgram(const std::string& vert, const std::string& frag, FILE* debugLog = nullptr);
