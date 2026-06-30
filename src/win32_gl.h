// win32_gl.h  Win32 原生窗口 + WGL OpenGL 上下文
// 替代 GLFW，直接控制桌面特效窗口属性（NOACTIVATE / TOOLWINDOW / TRANSPARENT）
#pragma once
#include <windows.h>
#include <GL/gl.h>

struct Win32GL {
    HWND   hwnd   = nullptr;
    HDC    hdc    = nullptr;
    HGLRC  hglrc  = nullptr;
    int    width  = 0;
    int    height = 0;
    int    capOffX = 0;   // 捕获裁剪偏移（工作区左上角相对于全屏）
    int    capOffY = 0;
    int    capFullW = 0;  // 全屏分辨率
    int    capFullH = 0;
    bool   active = false;
};

// 初始化：创建全屏无边框窗口 + WGL OpenGL 3.3 兼容上下文
// width/height = 0 表示使用主显示器分辨率
bool Win32GL_Init(Win32GL& wgl, const char* title, int width, int height);

// 交换帧缓冲
void Win32GL_SwapBuffers(Win32GL& wgl);

// 获取帧缓冲区大小
void Win32GL_GetFramebufferSize(Win32GL& wgl, int* w, int* h);

// 加载 OpenGL 函数指针
void* Win32GL_GetProcAddress(const char* name);

// 高精度计时器（秒，替代 glfwGetTime）
double Win32GL_GetTime();

// 设置窗口标题
void Win32GL_SetWindowTitle(Win32GL& wgl, const char* title);

// 设置 VSync 间隔 (0=关, 1=开)
bool Win32GL_SetSwapInterval(int interval);

// 销毁窗口和 OpenGL 上下文
void Win32GL_Shutdown(Win32GL& wgl);

// 窗口消息处理（返回 false 表示窗口应关闭）
bool Win32GL_PollEvents(Win32GL& wgl);

// 仅处理消息队列，不做关闭检测（用于退出动画期间）
void Win32GL_DrainMessages(Win32GL& wgl);

// 显示窗口（在所有初始化完成后调用，避免启动黑屏）
void Win32GL_Show(Win32GL& wgl);

// 启用分层模式（在首次渲染完成后调用）
void Win32GL_EnableLayered(Win32GL& wgl);

// 立即隐藏窗口（用于退出动画结束时）
void Win32GL_Hide(Win32GL& wgl);

// 隐藏/恢复系统光标（全局）
void Win32GL_HideSystemCursor();
void Win32GL_RestoreSystemCursor();
