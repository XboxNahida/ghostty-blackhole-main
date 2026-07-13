# Linux 迁移基线记录

- 记录日期：2026-07-13
- 目标平台：Ubuntu 26.04 LTS (Resolute Raccoon) amd64, GNOME Wayland, NVIDIA RTX 3060
- 分支：`port/ubuntu-wayland`
- 基线提交：`1c570a8` — "合并渲染器启动诊断与错误弹窗"

## 1. 系统环境

| 项目 | 版本 |
|---|---|
| 操作系统 | Ubuntu 26.04 LTS, Linux 7.0.0-27-generic x86_64 |
| GNOME Shell | 50.1 |
| 会话类型 | Wayland |
| 内核 | 7.0.0-27-generic #27-Ubuntu SMP PREEMPT_DYNAMIC |

## 2. 编译工具链

| 工具 | 版本 |
|---|---|
| GCC | 15.2.0 (Ubuntu 15.2.0-16ubuntu1) |
| CMake | 4.2.3 |
| Ninja | 1.13.2 |
| QMake | 3.1 (Qt 6.10.2) |
| GLFW | 3.4-4 (libglfw3-dev) |
| Qt6 开发包 | 6.10.2 (base-dev, declarative-dev, svg-dev) |

## 3. 图形与驱动

| 项目 | 信息 |
|---|---|
| GPU | NVIDIA GeForce RTX 3060 (PCIe 0000:01:00.0) |
| NVIDIA 驱动 | 595.71.05 (Open Kernel Module) |
| 驱动 GPU UUID | GPU-058a95fe-1ab8-377e-82ed-cffca03847d8 |
| Video BIOS | 94.06.2f.00.ec |
| GLX (XWayland) | 无法创建上下文：`BadValue (integer parameter out of range for operation)` |
| EGL | `eglinfo` segfault（需排查，可能缺少 libegl-dev） |
| OpenGL 预期 | 驱动声明支持 OpenGL 4.6，项目 shader 要求 3.30 |

## 4. 迁移依赖准备

| 依赖 | 状态 |
|---|---|
| libpipewire-0.3-dev | 已安装（0.3.x） |
| libspa-0.2-dev | 已安装 |
| xdg-desktop-portal | 已安装 |
| xdg-desktop-portal-gnome | 已安装 |
| libglfw3-dev | 已安装（3.4-4） |
| qt6-base-dev | 已安装（6.10.2） |
| qt6-declarative-dev | 已安装（6.10.2） |
| libegl-dev | **未安装**（`eglinfo` segfault） |

## 5. CMake 构建现状

### 5.1 根 CMakeLists.txt 失败原因

```text
CMake Error at CMakeLists.txt:3 (project):
  No CMAKE_RC_COMPILER could be found.
```

根 `project()` 声明了 `LANGUAGES CXX RC`，在 Linux 上没有 RC（Windows Resource Compiler）可用。

### 5.2 无条件引入的 Windows 专属链接库

**根目标 `blackhole`**：
```
d3d11 dxgi d3dcompiler gdi32 runtimeobject dwmapi
comctl32 ole32 advapi32 wtsapi32
```

**UI 目标 `appBlakholeUI`**（Blakhole_UI/CMakeLists.txt）：
```
advapi32 dwmapi ole32 runtimeobject version
```

### 5.3 无条件编译的 Windows 专属源文件

**src/** 下所有文件均无条件加入 `blackhole` 目标：

| 文件 | Windows 依赖 |
|---|---|
| `src/main.cpp` | Win32/WGL/D3D11, 1854 行主入口 |
| `src/win32_gl.cpp` / `src/win32_window.cpp` | Win32 窗口与 OpenGL |
| `src/capture_wgc.cpp` / `src/capture_dxgi.cpp` | WGC/DXGI 桌面捕获 |
| `src/d3d11_renderer.cpp` | D3D11 渲染路径 |
| `src/foreground_window.cpp` | Win32 前台窗口 API |
| `src/game_detection.cpp` | Win32 游戏检测 |
| `src/media_session.cpp` | Windows Audio Session |
| `resource.rc` / `renderer_version.rc` | Windows RC 资源 |

### 5.4 验证命令与输出

```bash
$ cmake -S . -B build-test -G Ninja 2>&1 | tail -5
CMake Error at CMakeLists.txt:3 (project):
  No CMAKE_RC_COMPILER could be found.
```

配置未能到达任何 `add_executable`；无目标可编译。

## 6. 测试现状

### 6.1 测试文件清单

所有测试位于 `tests/` 目录，均为 Qt/C++ 单元测试：

| 测试文件 | 被测模块 | Qt 依赖 | Windows 依赖 |
|---|---|---|---|
| `application_catalog_tests.cpp` | 应用目录 | Core, Gui, Widgets | `version` 库 |
| `autostart_registry_tests.cpp` | 自启动注册表 | Core | Win32 注册表 API |
| `avatar_storage_tests.cpp` | 头像存储 | Core, Gui | — |
| `foreground_window_tests.cpp` | 前台窗口 | Core | Win32 API |
| `game_detection_tests.cpp` | 游戏检测 | Core | Win32 API |
| `media_session_tests.cpp` | 媒体会话 | Core | Win32 Audio Session |
| `movement_settings_tests.cpp` | 移动参数 | 无 | — |
| `renderer_startup_diagnostics_tests.cpp` | 启动诊断 | Core | — |
| `update_checker_state_tests.cpp` | 更新状态机 | Core, Network, Gui | — |
| `update_release_tests.cpp` | 更新发布 | Core | — |

### 6.2 Linux 编译性

在 Linux 上，UI 目标因 CMake 配置失败无法编译，测试作为 UI 目标的子目标同样不可达。部分纯逻辑测试（如 `movement_settings_tests.cpp`、`renderer_startup_diagnostics_tests.cpp`）理论上可独立编译，但当前 CMake 结构未提供此路径。

### 6.3 Windows 基线（预期保留）

Windows 上 `cmake -S . -B build-win -G Ninja` 可成功配置并编译，所有测试通过。此基线不应被 Linux 移植破坏。

## 7. 项目结构概览

```
.
├── CMakeLists.txt                  # 根构建：仅 Windows 目标 blackhole
├── blackhole.glsl                  # 主着色器（可复用）
├── resource.rc                     # Windows 图标资源
├── cmake/
│   ├── AppVersion.cmake            # 版本定义 (1.2.0)
│   ├── app_version.h.in            # 版本头模板
│   └── renderer_version.rc.in      # 渲染器版本 RC 模板
├── shaders/                        # GLSL 着色器文件（可复用）
│   ├── vert.glsl, frag_header.glsl, frag_preview_header.glsl
│   ├── frag_desktop_header.glsl, frag_simple.glsl
│   ├── blackhole_preview.glsl
│   └── blackhole.hlsl, fullscreen_vs.hlsl（HLSL 不可复用）
├── src/                            # Windows 渲染器源码
│   ├── main.cpp (1854行)
│   ├── win32_gl.cpp/.h, win32_window.cpp/.h
│   ├── capture_wgc.cpp/.h, capture_dxgi.cpp/.h
│   ├── d3d11_renderer.cpp/.h
│   ├── gl_texture.cpp/.h, bloom_renderer.cpp/.h
│   ├── gui_config.cpp/.h, monitors.cpp/.h
│   ├── foreground_window.cpp/.h, game_detection.cpp/.h
│   ├── media_session.cpp/.h, movement_settings.cpp/.h
│   ├── renderer_interface.h, texture_source.h
│   └── imgui/                      # ImGui 依赖
├── Blakhole_UI/                    # Qt/QML UI
│   ├── CMakeLists.txt, main.cpp
│   ├── Main.qml, pages/, components/
│   ├── core/                       # UI 业务逻辑
│   └── app.rc, app_icon.ico, app_version.rc.in
├── tests/                          # 单元测试（UI 子项目）
└── Doc/
    ├── UBUNTU_MIGRATION_PLAN.md    # 迁移计划
    └── LINUX_BASELINE.md           # 本文件
```

## 8. 关键发现

1. **GLX 不可用**：纯 Wayland 会话下 GLX 无法创建上下文。Linux 渲染器应使用 EGL + OpenGL 3.3 Core。
2. **eglinfo segfault**：`eglinfo` 在 Wayland + NVIDIA 下 segfault，可能需要 `libegl-dev` 或 NVIDIA EGL 库路径配置。
3. **PipeWire/Portal 开发库已安装**：可直接用于 DS-08 桌面捕获实现。
4. **Qt6 DBus 已安装**：可用于 DS-06 (Mutter IdleMonitor) 和 DS-07 (MPRIS)。
5. **测试框架仅 Windows 可运行**：需要 CMake 拆分后才能用于 Linux 验证。
6. **UI 包含 Windows 专属链接**：`advapi32 dwmapi ole32 runtimeobject version` 需在 Linux 构建中隔离。
