#pragma once
#include <cstdio>
#include <GL/gl.h>

struct BlackholeConfig; // forward decl from gui_config.h

/// Encapsulates the full-screen quad VAO/VBO, shader uniform locations,
/// Bloom renderer, and per-frame draw logic.
/// Platform-independent; relies on extern GL function pointers (gl_funcs.h).
struct BlackholeRenderer {
    GLuint program = 0;
    GLuint vao = 0, vbo = 0;
    void* bloom = nullptr; // BloomRenderer* (opaque to avoid Bloom header dependency)

    // Cached uniform locations
    GLint locRes = -1;
    GLint locTime = -1;
    GLint locMovementTime = -1;
    GLint locDate = -1;
    GLint locCh0 = -1;
    GLint loc_uHR = -1, loc_uDG = -1, loc_uDT = -1, loc_uEX = -1;
    GLint loc_uSP = -1, loc_uSG = -1, loc_uDI = -1;
    GLint loc_uPM = -1, loc_uSlot = -1, loc_uPC = -1;
    GLint loc_uPT = -1, loc_uPI = -1, loc_uPR = -1, loc_uPN = -1;
    GLint loc_uPO = -1, loc_uPP = -1, loc_uPD = -1, loc_uPB = -1;
    GLint loc_uPG = -1, loc_uPCo = -1, loc_uPW = -1, loc_uPS = -1;
    GLint loc_uPE = -1, loc_uPSt = -1;
    GLint locBorn = -1;
    GLint locFixedSz = -1, locFixedLvl = -1, locGrowEnabled = -1, locInitialSize = -1;
    GLint locLightingEffect = -1, locDistortion = -1;
    GLint locHomeX = -1, locHomeY = -1, locFollowMouse = -1;
    GLint locPhase = -1, locPresetOff = -1;

    /// Initialize VAO, VBO, and Bloom.  Must be called after program creation.
    /// width/height are the framebuffer dimensions for Bloom.
    bool init(GLuint prog, int width, int height, FILE* debugLog);

    /// Cache all uniform locations for the given program.
    void cacheUniforms(GLuint prog);

    /// Set all uniforms from config and draw the full-screen quad.
    /// Handles Bloom begin/end internally.
    /// bornProgress: transient per-frame black hole birth/animation progress [0,1].
    void drawAndBloom(int fbW, int fbH, const BlackholeConfig& cfg,
                      unsigned int glTexID,
                      float iTime, float moveTime,
                      float homeX, float homeY,
                      float phaseOffset, float presetOffset,
                      float bornProgress,
                      FILE* debugLog);

    /// Destroy Bloom and delete GL resources.
    void destroy();
};
