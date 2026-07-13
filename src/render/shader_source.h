#pragma once
#include <string>
#include <cstdio>

/// Read a file into a string.
/// Strips UTF-8 BOM and normalizes CRLF -> LF.
/// Returns empty string on failure.
std::string readFile(const char* path);

/// Build the fragment shader source by combining frag_desktop_header.glsl
/// and blackhole.glsl, injecting uniforms and applying string replacements
/// for movement time, overridable constants, demo presets, etc.
/// Returns false if required shader files cannot be read.
bool buildFragmentShader(std::string& out, FILE* debugLog = nullptr);
