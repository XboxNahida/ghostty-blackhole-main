# Debug State — Blackhole v6: 自定义预设修复

## 当前状态
### 编译 ✓ - 零警告零错误
- 源文件: main.cpp + capture_wgc.cpp + capture_dxgi.cpp + gl_texture.cpp + gui_config.cpp + imgui/*
- 依赖: glfw + opengl32 + d3d11 + dxgi + runtimeobject + dwmapi + comctl32

### 启动流程
```
双击 blackhole.exe
  → ImGui 配置面板 (900x620)
    → 选择模式: 始终显示 / 空闲检测
    → 设置空闲超时 (仅 Idle 模式)
    → 可选: 勾选"自定义轮播"使用自定义预设
    → 点击 START
  → 全屏黑洞 overlay 启动 (MODE_DEMO 42秒演示循环)
  → ESC 退出
```

## 文件结构
| 文件 | 说明 |
|------|------|
| src/main.cpp | 主程序：配置面板 + 渲染循环 |
| src/gui_config.h/cpp | ImGui 配置面板 |
| src/capture_wgc.h/cpp | WGC 捕获 |
| src/capture_dxgi.h/cpp | DXGI 捕获(备用) |
| src/gl_texture.h/cpp | GL 纹理上传 |
| src/imgui/* | ImGui 库文件 |

---

## 问题修复记录 (2026-06-26)

### 问题
提交 20e4377 "新增自定义黑洞轮播配置" 后黑洞完全不渲染。

### 根因 + 修复

**根因1 (黑洞不渲染):**
`buildFragmentShader()` 将 SIZE_MODE 替换从 MODE_DEMO 改为空操作(MODE_TOKENS→MODE_TOKENS)。
TOKEN 模式需要 Ghostty 光标颜色信号，独立 exe 没有这些信号，tokenLevel() 始终返回 -1，黑洞被隐藏。
→ **修复**: 恢复为 `body.replace(pos, 29, "#define SIZE_MODE MODE_DEMO")`

**问题2 (自定义预设注入失败):**
搜索 `demoLookOrig()` 但 shader 中函数名为 `demoLook()`；注入代码调用了不存在的 `demoPreset()`。
→ **修复**: 改为搜索 `demoLook()`，注入完整 `demoPreset()` 函数定义 + 修改后的 `demoLook()`

**问题3 (预设数据未上传):**
shader 声明了 14 组预设 uniform 数组，但 C++ 端从未上传。
→ **修复**: 添加 `gl_Uniform1fv` 支持，在主循环中上传 cfg.presets[] 到 GPU

### 修改文件
- `src/main.cpp` (唯一修改文件)
  - 添加 `DECL_GL_FUNC(Uniform1fv, ...)` 声明
  - 添加 `LOAD_GL_FUNC(Uniform1fv)` 加载
  - 修复 `buildFragmentShader()`: demoLookOrig→demoLook + demoPreset + MODE_DEMO
  - 添加 15 个预设 uniform location 查询
  - 添加 14 组预设数组 uniform 上传

### 编译结果
✓ 零错误零警告

---

## 当前修改状态

| 步骤 | 状态 |
|------|------|
| 问题分析 | ✅ 完成 |
| 添加 gl_Uniform1fv 声明/加载 | ✅ 完成 |
| 修复 buildFragmentShader | ✅ 完成 |
| 添加预设 uniform location | ✅ 完成 |
| 添加预设 uniform 上传 | ✅ 完成 |
| 编译验证 | ✅ 完成 (零错误零警告) |
