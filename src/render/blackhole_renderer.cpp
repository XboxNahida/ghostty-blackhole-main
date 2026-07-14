#include "blackhole_renderer.h"
#include "gl_funcs.h"
#include "../bloom_renderer.h"
#include "../gui_config.h"
#include <cmath>
#include <ctime>
#include <algorithm>

#ifndef BLACKHOLE_USE_D3D11

bool BlackholeRenderer::init(GLuint prog, int width, int height, FILE* debugLog) {
    program = prog;

    // Full-screen quad
    float verts[] = { -1,-1, 1,-1, -1,1, 1,1 };
    gl_GenVertexArrays(1, &vao); gl_GenBuffers(1, &vbo);
    gl_BindVertexArray(vao);
    gl_BindBuffer(GL_ARRAY_BUFFER, vbo);
    gl_BufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
    gl_VertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2*sizeof(float), (void*)0);
    gl_EnableVertexAttribArray(0);
    gl_BindVertexArray(0);

    bloom = Bloom_Create(width, height, debugLog);
    if (!bloom && debugLog) {
        fprintf(debugLog, "[WARN] Bloom_Create returned null\n");
        fflush(debugLog);
    }

    cacheUniforms(prog);
    return true;
}

void BlackholeRenderer::cacheUniforms(GLuint prog) {
    locRes   = gl_GetUniformLocation(prog, "iResolution");
    locTime  = gl_GetUniformLocation(prog, "iTime");
    locMovementTime = gl_GetUniformLocation(prog, "uMovementTime");
    locDate  = gl_GetUniformLocation(prog, "iDate");
    locCh0   = gl_GetUniformLocation(prog, "iChannel0");
    loc_uHR  = gl_GetUniformLocation(prog, "uHoleRadius");
    loc_uDG  = gl_GetUniformLocation(prog, "uDiskGain");
    loc_uDT  = gl_GetUniformLocation(prog, "uDiskTemp");
    loc_uEX  = gl_GetUniformLocation(prog, "uExposure");
    loc_uSP  = gl_GetUniformLocation(prog, "uSpeed");
    loc_uSG  = gl_GetUniformLocation(prog, "uStarGain");
    loc_uDI  = gl_GetUniformLocation(prog, "uDiskIncl");
    loc_uPM  = gl_GetUniformLocation(prog, "uPlayMode");
    loc_uSlot = gl_GetUniformLocation(prog, "uSlotSec");
    loc_uPC   = gl_GetUniformLocation(prog, "uPresetCount");
    loc_uPT   = gl_GetUniformLocation(prog, "uPresetTemp");
    loc_uPI   = gl_GetUniformLocation(prog, "uPresetIncl");
    loc_uPR   = gl_GetUniformLocation(prog, "uPresetRoll");
    loc_uPN   = gl_GetUniformLocation(prog, "uPresetInner");
    loc_uPO   = gl_GetUniformLocation(prog, "uPresetOuter");
    loc_uPP   = gl_GetUniformLocation(prog, "uPresetOpac");
    loc_uPD   = gl_GetUniformLocation(prog, "uPresetDopp");
    loc_uPB   = gl_GetUniformLocation(prog, "uPresetBeam");
    loc_uPG   = gl_GetUniformLocation(prog, "uPresetGain");
    loc_uPCo  = gl_GetUniformLocation(prog, "uPresetContr");
    loc_uPW   = gl_GetUniformLocation(prog, "uPresetWind");
    loc_uPS   = gl_GetUniformLocation(prog, "uPresetSpd");
    loc_uPE   = gl_GetUniformLocation(prog, "uPresetExpo");
    loc_uPSt  = gl_GetUniformLocation(prog, "uPresetStar");
    locBorn   = gl_GetUniformLocation(prog, "uBornProgress");
    locFixedSz  = gl_GetUniformLocation(prog, "uFixedSize");
    locFixedLvl = gl_GetUniformLocation(prog, "uFixedLevel");
    locGrowEnabled = gl_GetUniformLocation(prog, "uGrowEnabled");
    locInitialSize = gl_GetUniformLocation(prog, "uInitialSize");
    locLightingEffect = gl_GetUniformLocation(prog, "uLightingEffect");
    locDistortion = gl_GetUniformLocation(prog, "uDistortion");
    locHomeX  = gl_GetUniformLocation(prog, "uHomeX");
    locHomeY  = gl_GetUniformLocation(prog, "uHomeY");
    locFollowMouse = gl_GetUniformLocation(prog, "uFollowMouse");
    locPhase  = gl_GetUniformLocation(prog, "uRandPhase");
    locPresetOff = gl_GetUniformLocation(prog, "uPresetOffset");
}

void BlackholeRenderer::drawAndBloom(int fbW, int fbH, const BlackholeConfig& cfg,
                                     unsigned int glTexID,
                                     float iTime, float moveTime,
                                     float homeX, float homeY,
                                     float phaseOffset, float presetOffset,
                                     float bornProgress,
                                     FILE* debugLog)
{
    // Clamp preset count to safe range before any uniform upload
    int pc = cfg.presetCount;
    if (pc < 0) pc = 0;
    if (pc > 64) pc = 64;

    const bool bloomActive = Bloom_BeginScene((BloomRenderer*)bloom, fbW, fbH, cfg.lightingEffect);
    (void)bloomActive; // used by Bloom_BeginScene internally; keep for future bloom toggling
    gl_UseProgram(program);
    gl_ActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, glTexID);
    gl_Uniform1i(locCh0, 0);
    gl_Uniform3f(locRes, (float)fbW, (float)fbH, 0);
    gl_Uniform1f(locTime, iTime);
    gl_Uniform1f(locMovementTime, moveTime);
    gl_Uniform4f(locDate, 0, 0, 0, (float)time(nullptr));
    gl_Uniform1f(loc_uHR, cfg.holeRadius);
    gl_Uniform1f(loc_uDG, cfg.diskGain);
    gl_Uniform1f(loc_uDT, cfg.diskTemp);
    gl_Uniform1f(loc_uEX, cfg.exposure);
    gl_Uniform1f(loc_uSP, cfg.spd);
    gl_Uniform1f(loc_uSG, cfg.starGain);
    gl_Uniform1f(loc_uDI, cfg.diskIncl);
    gl_Uniform1i(loc_uPM, cfg.playMode);
    gl_Uniform1f(loc_uSlot, cfg.slotSec);
    gl_Uniform1i(loc_uPC, pc);
    {
        float buf[64] = {};
        for (int i = 0; i < pc; i++) buf[i] = cfg.presets[i].temp;
        gl_Uniform1fv(loc_uPT, pc, buf);
        for (int i = 0; i < pc; i++) buf[i] = cfg.presets[i].incl;
        gl_Uniform1fv(loc_uPI, pc, buf);
        for (int i = 0; i < pc; i++) buf[i] = cfg.presets[i].roll;
        gl_Uniform1fv(loc_uPR, pc, buf);
        for (int i = 0; i < pc; i++) buf[i] = cfg.presets[i].inner;
        gl_Uniform1fv(loc_uPN, pc, buf);
        for (int i = 0; i < pc; i++) buf[i] = cfg.presets[i].outer;
        gl_Uniform1fv(loc_uPO, pc, buf);
        for (int i = 0; i < pc; i++) buf[i] = cfg.presets[i].opac;
        gl_Uniform1fv(loc_uPP, pc, buf);
        for (int i = 0; i < pc; i++) buf[i] = cfg.presets[i].dopp;
        gl_Uniform1fv(loc_uPD, pc, buf);
        for (int i = 0; i < pc; i++) buf[i] = cfg.presets[i].beam;
        gl_Uniform1fv(loc_uPB, pc, buf);
        for (int i = 0; i < pc; i++) buf[i] = cfg.presets[i].gain;
        gl_Uniform1fv(loc_uPG, pc, buf);
        for (int i = 0; i < pc; i++) buf[i] = cfg.presets[i].contr;
        gl_Uniform1fv(loc_uPCo, pc, buf);
        for (int i = 0; i < pc; i++) buf[i] = cfg.presets[i].wind;
        gl_Uniform1fv(loc_uPW, pc, buf);
        for (int i = 0; i < pc; i++) buf[i] = cfg.presets[i].speed;
        gl_Uniform1fv(loc_uPS, pc, buf);
        for (int i = 0; i < pc; i++) buf[i] = cfg.presets[i].expo;
        gl_Uniform1fv(loc_uPE, pc, buf);
        for (int i = 0; i < pc; i++) buf[i] = cfg.presets[i].star;
        gl_Uniform1fv(loc_uPSt, pc, buf);
    }
    gl_Uniform1f(locBorn, bornProgress);
    gl_Uniform1i(locFixedSz, cfg.fixedSize ? 1 : 0);
    gl_Uniform1f(locFixedLvl, cfg.fixedLevel);
    gl_Uniform1i(locGrowEnabled, cfg.growEnabled ? 1 : 0);
    gl_Uniform1f(locInitialSize, cfg.initialSize);
    gl_Uniform1i(locLightingEffect, cfg.lightingEffect ? 1 : 0);
    float lightingLensScale = cfg.lightingEffect ? 0.42f : 1.0f;
    gl_Uniform1f(locDistortion, cfg.distortion * lightingLensScale);
    gl_Uniform1f(locHomeX, homeX);
    gl_Uniform1f(locHomeY, homeY);
    gl_Uniform1i(locFollowMouse, cfg.followMouse ? 1 : 0);
    gl_Uniform1f(locPhase, phaseOffset);
    gl_Uniform1f(locPresetOff, presetOffset);

    gl_BindVertexArray(vao);
    gl_DrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    gl_BindVertexArray(0);

    Bloom_EndScene((BloomRenderer*)bloom, fbW, fbH, bloomActive);

    if (debugLog) {
        fprintf(debugLog, "[DEBUG] blackhole-renderer: fb=%dx%d, pc=%d\n", fbW, fbH, pc);
    }
}

void BlackholeRenderer::destroy() {
    // Bloom_Destroy requires lvalue reference; use a local variable
    BloomRenderer* localBloom = (BloomRenderer*)bloom;
    Bloom_Destroy(localBloom);
    bloom = nullptr;
    if (vao) { gl_DeleteVertexArrays(1, &vao); vao = 0; }
    if (vbo) { gl_DeleteBuffers(1, &vbo); vbo = 0; }
    if (program) { gl_DeleteProgram(program); program = 0; }
}

#endif // BLACKHOLE_USE_D3D11
