#pragma once
#include <string>
#include <cstdio>

/// Read a file into a string.
/// Strips UTF-8 BOM and normalizes CRLF -> LF.
/// Returns empty string on failure.
std::string readFile(const char* path);

/// Helper: a single string patch to apply to shader source.
/// critical=true: failure to find anchor returns false from buildFragmentShader.
/// critical=false: missing anchor is silently skipped (optional cosmetic patch).
struct ShaderPatch {
    const char* anchor;      ///< text to find
    const char* replacement; ///< replacement text (nullptr = insert before anchor)
    const char* description; ///< human-readable name for diagnostics
    bool critical;
};

/// Apply a list of ShaderPatches to a string.
/// Returns false if any critical patch fails to find its anchor.
/// When debugLog is non-null, writes diagnostics for all missing anchors.
bool applyPatches(std::string& body, const ShaderPatch* patches, int count,
                  FILE* debugLog = nullptr);

/// Build the fragment shader source by combining frag_desktop_header.glsl
/// and blackhole.glsl, injecting uniforms and applying string replacements
/// for movement time, overridable constants, demo presets, etc.
/// Returns false if required shader files cannot be read.
bool buildFragmentShader(std::string& out, FILE* debugLog = nullptr);
