// scr_entry.h — Windows 原生屏保入口
// 处理 /s /p /c /a 命令行参数，对应 Windows 屏保协议
#pragma once
#include <windows.h>
#include "gui_config.h"

// /s 全屏屏保模式
int ScrDoFullscreen(const BlackholeConfig& cfg);

// /p <hwnd> 预览模式（显示在"显示属性"的小窗口里）
int ScrDoPreview(HWND hParent);

// /c 配置对话框
int ScrDoConfig();

// /a <hwnd> 密码对话框（Windows 10+ 已废弃，弹出提示即可）
int ScrDoPassword(HWND hParent);
