#include "blackhole_renderer.h"
#include "gl_funcs.h"
#include "../bloom_renderer.h"
#include "../gui_config.h"
#include <cmath>
#include <ctime>

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
                                     bool bloomActive, FILE* debugLog)
{
    const bool bloomRunning = Bloom_BeginScene((BloomRenderer*)bloom, fbW, fbH, cfg.lightingEffect);
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
    gl_Uniform1i(loc_uPC, cfg.presetCount);
    {
        float buf[64];
        for (int i = 0; i < cfg.presetCount; i++) buf[i] = cfg.presets[i].temp;
        gl_Uniform1fv(loc_uPT, cfg.presetCount, buf);
        for (int i = 0; i < cfg.presetCount; i++) buf[i] = cfg.presets[i].incl;
        gl_Uniform1fv(loc_uPI, cfg.presetCount, buf);
        for (int i = 0; i < cfg.presetCount; i++) buf[i] = cfg.presets[i].roll;
        gl_Uniform1fv(loc_uPR, cfg.presetCount, buf);
        for (int i = 0; i < cfg.presetCount; i++) buf[i] = cfg.presets[i].inner;
        gl_Uniform1fv(loc_uPN, cfg.presetCount, buf);
        for (int i = 0; i < cfg.presetCount; i++) buf[i] = cfg.presets[i].outer;
        gl_Uniform1fv(loc_uPO, cfg.presetCount, buf);
        for (int i = 0; i < cfg.presetCount; i++) buf[i] = cfg.presets[i].opac;
        gl_Uniform1fv(loc_uPP, cfg.presetCount, buf);
        for (int i = 0; i < cfg.presetCount; i++) buf[i] = cfg.presets[i].dopp;
        gl_Uniform1fv(loc_uPD, cfg.presetCount, buf);
        for (int i = 0; i < cfg.presetCount; i++) buf[i] = cfg.presets[i].beam;
        gl_Uniform1fv(loc_uPB, cfg.presetCount, buf);
        for (int i = 0; i < cfg.presetCount; i++) buf[i] = cfg.presets[i].gain;
        gl_Uniform1fv(loc_uPG, cfg.presetCount, buf);
        for (int i = 0; i < cfg.presetCount; i++) buf[i] = cfg.presets[i].contr;
        gl_Uniform1fv(loc_uPCo, cfg.presetCount, buf);
        for (int i = 0; i < cfg.presetCount; i++) buf[i] = cfg.presets[i].wind;
        gl_Uniform1fv(loc_uPW, cfg.presetCount, buf);
        for (int i = 0; i < cfg.presetCount; i++) buf[i] = cfg.presets[i].spd;
        gl_Uniform1fv(loc_uPS, cfg.presetCount, buf);
        for (int i = 0; i < cfg.presetCount; i++) buf[i] = cfg.presets[i].expo;
        gl_Uniform1fv(loc_uPE, cfg.presetCount, buf);
        for (int i = 0; i < cfg.presetCount; i++) buf[i] = cfg.presets[i].star;
        gl_Uniform1fv(loc_uPSt, cfg.presetCount, buf);
    }
    gl_Uniform1f(locBorn, cfg.bornProgress);
    gl_Uniform1f(locFixedSz, cfg.fixedSize);
    gl_Uniform1f(locFixedLvl, cfg.fixedLevel);
    gl_Uniform1f(locGrowEnabled, cfg.growEnabled ? 1.0f : 0.0f);
    gl_Uniform1f(locInitialSize, cfg.initialSize);
    gl_Uniform1f(locLightingEffect, cfg.lightingEffect ? 1.0f : 0.0f);
    gl_Uniform1f(locDistortion, cfg.distortion);
    gl_Uniform1f(locHomeX, homeX);
    gl_Uniform1f(locHomeY, homeY);
    gl_Uniform1f(locFollowMouse, cfg.followMouse ? 1.0f : 0.0f);
    gl_Uniform1f(locPhase, phaseOffset);
    gl_Uniform1f(locPresetOff, presetOffset);

    gl_BindVertexArray(vao);
    gl_DrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    gl_BindVertexArray(0);

    Bloom_EndScene((BloomRenderer*)bloom, fbW, fbH, bloomRunning);
}

void BlackholeRenderer::destroy() {
    Bloom_Destroy((BloomRenderer*)bloom);
    bloom = nullptr;
    if (vao) { gl_DeleteVertexArrays(1, &vao); vao = 0; }
    if (vbo) { gl_DeleteBuffers(1, &vbo); vbo = 0; }
    if (program) { gl_DeleteProgram(program); program = 0; }
}

#endif // BLACKHOLE_USE_D3D11
