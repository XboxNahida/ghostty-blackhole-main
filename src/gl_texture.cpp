// gl_texture.cpp  Synchronous D3D11-staging -> OpenGL texture upload
// No PBO, no async DMA 闂?simple, reliable glTexSubImage2D.
#include "gl_texture.h"
#include <cstdio>
#include <cstring>
#include <cstddef>
#include <vector>

// ---- OpenGL 1.2+ constants not in minimal gl.h ----
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE        0x812F
#endif
#ifndef GL_BGRA_EXT
#define GL_BGRA_EXT             0x80E1
#endif
#ifndef GL_RGBA
#define GL_RGBA                0x8058
#endif

// ---- Public API ----

bool GLTex_Init(GLTextureUpload& gt, int width, int height) {
    gt.active = false;
    gt.width  = width;
    gt.height = height;

    glGenTextures(1, &gt.tex);
    glBindTexture(GL_TEXTURE_2D, gt.tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    std::vector<unsigned char> defaultData(width * height * 4, 128);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0,
                 GL_BGRA_EXT, GL_UNSIGNED_BYTE, defaultData.data());
    glBindTexture(GL_TEXTURE_2D, 0);

    gt.active = true;
    fprintf(stderr, "[Tex] Texture ready: %dx%d (initialized with default content)\n", width, height);
    return true;
}

void GLTex_Upload(GLTextureUpload& gt, const void* data, int stride) {
    if (!gt.active || !data) return;

    glBindTexture(GL_TEXTURE_2D, gt.tex);

    if (stride == gt.width * 4) {
        // Fast path: tightly packed, single glTexSubImage2D
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, gt.width, gt.height,
                        GL_BGRA_EXT, GL_UNSIGNED_BYTE, data);
    } else {
        // Stride > width*4: upload row by row via GL_UNPACK_ROW_LENGTH
        glPixelStorei(0x0CF2 /* GL_UNPACK_ROW_LENGTH */, stride / 4);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, gt.width, gt.height,
                        GL_BGRA_EXT, GL_UNSIGNED_BYTE, data);
        glPixelStorei(0x0CF2 /* GL_UNPACK_ROW_LENGTH */, 0);
    }

    glBindTexture(GL_TEXTURE_2D, 0);
}

void GLTex_UploadRegion(GLTextureUpload& gt, const void* data, int stride,
                        int dstX, int dstY, int regionW, int regionH) {
    if (!gt.active || !data || regionW <= 0 || regionH <= 0) return;

    glBindTexture(GL_TEXTURE_2D, gt.tex);

    if (stride == regionW * 4) {
        glTexSubImage2D(GL_TEXTURE_2D, 0, dstX, dstY, regionW, regionH,
                        GL_BGRA_EXT, GL_UNSIGNED_BYTE, data);
    } else {
        glPixelStorei(0x0CF2 /* GL_UNPACK_ROW_LENGTH */, stride / 4);
        glTexSubImage2D(GL_TEXTURE_2D, 0, dstX, dstY, regionW, regionH,
                        GL_BGRA_EXT, GL_UNSIGNED_BYTE, data);
        glPixelStorei(0x0CF2 /* GL_UNPACK_ROW_LENGTH */, 0);
    }

    glBindTexture(GL_TEXTURE_2D, 0);
}

bool GLTex_Resize(GLTextureUpload& gt, int width, int height) {
    if (!gt.active) return GLTex_Init(gt, width, height);

    glDeleteTextures(1, &gt.tex);
    gt.tex = 0;
    gt.active = false;
    return GLTex_Init(gt, width, height);
}

GLuint GLTex_GetTexture(const GLTextureUpload& gt) {
    return gt.tex;
}

void GLTex_Shutdown(GLTextureUpload& gt) {
    if (gt.tex) { glDeleteTextures(1, &gt.tex); gt.tex = 0; }
    gt.active = false;
}