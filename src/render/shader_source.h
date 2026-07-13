#pragma once
#include <string>
#include <cstdio>

/// Read a file into a string.
/// Strips UTF-8 BOM and normalizes CRLF -> LF.
/// Returns empty string on failure.
std::string readFile(const char* path);

/// A single string patch to apply to shader source.
/// critical=true: failure to find anchor returns false.
/// critical=false: missing anchor is silently skipped.
struct ShaderPatch {
    const char* anchor;      ///< text to find
    const char* replacement; ///< replacement text
    const char* description; ///< human-readable name for diagnostics
    bool critical;
};

/// Apply a list of ShaderPatches to a string.
/// Returns false if any critical patch fails to find its anchor.
/// When debugLog is non-null, writes diagnostics for all missing anchors.
bool applyPatches(std::string& body, const ShaderPatch* patches, int count,
                  FILE* debugLog = nullptr);

/// Core shader assembly: given header and body source strings, apply all
/// patches and produce the final fragment shader in `out`.
/// This is the testable production path; buildFragmentShader() is a wrapper
/// that reads files and delegates here.
/// Returns false if any critical patch anchor is missing.
bool buildFragmentShaderFromSources(const std::string& header,
                                    const std::string& body,
                                    std::string& out,
                                    FILE* debugLog = nullptr);

/// Build the fragment shader by reading "shaders/frag_desktop_header.glsl"
/// and "blackhole.glsl" from disk, then delegating to
/// buildFragmentShaderFromSources().
/// Returns false if shader files cannot be read or a critical patch fails.
bool buildFragmentShader(std::string& out, FILE* debugLog = nullptr);
