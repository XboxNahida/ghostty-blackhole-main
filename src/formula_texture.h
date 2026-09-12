#pragma once
#include <windows.h>
#include <GL/gl.h>

// 在当前 OpenGL 上下文创建自有公式图集，调用方负责 glDeleteTextures。
GLuint CreateFormulaTexture();
