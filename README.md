# Black Hole — Windows 桌面黑洞屏保

![demo](demo.gif)

基于 Eric Bruneton 黑洞着色器的 Windows 桌面黑洞可视化程序。捕获桌面画面，实时渲染史瓦西黑洞的引力透镜、吸积盘、光子环等相对论效应。

---

> **Windows 移植 & 功能增强**：本项目特别致谢 [墨溟ink / MoMing-ink] 改进双进程架构、Win32+WGL 原生窗口、WGC 桌面捕获、16 个预设、Crossfade 过渡、三层空闲检测、DWM 防护、双光标修复等问题。

## 快速开始

1. 双击 `release\blackhole.exe`
2. 配置参数 → 点击 **"启动"**
3. 黑洞在**空闲时自动显示**，动鼠标/键盘即消失
4. 右下角托盘图标 → 右键可退出

## macOS

macOS 版新增独立的 `blackhole-macos` app target，使用 ScreenCaptureKit 截取桌面、AppKit/GLFW/OpenGL 渲染，并提供一个简单的原生控制窗口。

```bash
brew install cmake glfw
cmake -S . -B _build/macos -DCMAKE_BUILD_TYPE=Release
cmake --build _build/macos --config Release
open _build/macos/blackhole-macos.app
```

如果桌面背景不是当前屏幕，请在 `System Settings -> Privacy & Security -> Screen Recording` 给应用或启动它的 Terminal 授权。预览模式也可用：

```bash
open _build/macos/blackhole-macos.app --args --render
```

## 两种模式

| 模式 | 行为 |
|------|------|
| **始终显示** | 黑洞常驻桌面 |
| **空闲检测** | 空闲 N 秒后显示，活跃时自动隐藏 |

空闲时间在配置页面设置（默认 300 秒 / 5 分钟）。
## 空闲检测原理

程序使用**三层检测机制**判断用户是否在观看视频：

### 检测流程

```
每 1 秒执行一次
├── Method 1: D3D 独占全屏检测
│   └── SHQueryUserNotificationState → 检测游戏/全屏独占应用
├── Method 2: 窗口覆盖整个屏幕
│   └── 前景窗口尺寸 ≥ 屏幕分辨率 → 全屏播放
└── Method 3: 音频会话检测
    ├── 获取前景窗口进程名
    ├── 匹配已知视频应用列表（中/英文进程名）
    ├── 对 UWP 应用（电影和电视等）检测窗口标题
    └── 枚举 Windows 音频会话，比对进程名 + 音频峰值
```

### 匹配策略

- **进程名匹配**：所有视频应用统一使用进程名比对（而非 PID），兼容浏览器、Electron 等多进程架构
- **UWP 特殊处理**：`ApplicationFrameHost.exe` 是 UWP 应用的外壳进程，程序会读取窗口标题识别媒体播放器，并检测**所有**音频会话
- **编码兼容**：进程名使用 UTF-8 统一编码，支持中英文混用场景

### 支持的应用

| 类别 | 应用 |
|------|------|
| **浏览器** | Chrome, Edge, Firefox, Opera, Brave |
| **游戏启动器** | Steam, Epic Games, Ubisoft Connect, EA App, Battle.net, Riot, GOG, Xbox, Game Bar |
| **国内视频平台** | 哔哩哔哩, 爱奇艺, 优酷, 芒果TV, 抖音, 快手, 腾讯视频 |
| **本地播放器** | VLC, MPV, PotPlayer, MPC, Windows Media Player, NVIDIA Overlay |
| **UWP 播放器** | 电影和电视, 媒体播放器（窗口标题关键词检测） |

### 桌面壁纸音频

壁纸引擎（如 Wallpaper Engine）播放的音频**不会阻止黑洞触发**——程序通过窗口/进程名过滤，仅前景视频应用才计入活跃状态。


## 配置参数

### 黑洞预设
- 14 个可调参数（色温、倾角、旋转、半径、不透明度、多普勒、光束指数、亮度增益、条纹对比度、缠绕紧度、旋转速度、曝光度、星空亮度）
- 全部使用**滑块调节**
- 支持**复制/粘贴**预设、**上移/下移**排序
- 三种**播放模式**：顺序 / 循环 / 随机

### 播放模式
- **顺序播放**：从第 1 个预设播到最后一个
- **循环播放**：播完回到第一个，无限循环
- **随机播放**：每个时段随机抽取预设

## 架构：双进程分离

`
blackhole.exe             配置器 + 空闲监控器（托盘图标）
blackhole.exe --render    黑洞渲染器（由监控器自动启停）
`

### 为什么这样设计？

Windows 11 的 DWM 会对全屏透明窗口施加**强调色边框**（黄边框），且 show/hide 窗口会触发 DWM 重建缓存。经过多轮迭代，发现以下方案均存在缺陷：

| 尝试方案 | 问题 |
|----------|------|
| opacity=0 隐藏 | 破坏 swap chain，导致黑屏 |
| ShowWindow 隐藏 | 触发 DWM 黄色 accent 边框 |
| 缩成 1×1 像素 | 仍有 DWM 合成开销 |
| WM hook 拦截 | 无法阻止 DWM 的缓存 accent layer |

**最终方案：进程级隔离**
- 渲染器启动即创建窗口（带 DWM 防护：DWMWA_BORDER_COLOR=0 + DWMNCRP_DISABLED + WS_EX_NOREDIRECTIONBITMAP + DwmFlush）
- 活跃时**直接终止渲染器进程**（优雅 WM_CLOSE 退出）
- 空闲时**重新启动渲染器进程**（全新 GL 上下文 + 全新 DWM 状态）
- 每次启动使用**唯一窗口标题**防止 DWM 缓存匹配


### 窗口层级保护 — 关键技术演进

黑洞作为桌面覆盖层，窗口 Z 序分层经历了多次迭代才稳定。

#### 最终方案：WS_EX_TOPMOST 扩展样式

```cpp
// win32_gl.cpp — 窗口创建（最终稳定版）
DWORD exStyle = WS_EX_NOACTIVATE | WS_EX_TOPMOST | WS_EX_TOOLWINDOW |
                WS_EX_TRANSPARENT | WS_EX_LAYERED;

// 初始化时一次置顶即可，无需运行时轮询
SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
             SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
```

- **WS_EX_TOPMOST** — 创建时即声明为 DWM 合成层最高优先级。与运行时 `HWND_TOPMOST` 不同，扩展样式在 DWM 中给予最稳定的 z-order，不会被其他置顶窗口（Game Bar、任务管理器等）覆盖
- **WS_EX_NOACTIVATE** — 永不抢焦点，不打断用户当前操作
- **WS_EX_TRANSPARENT + WS_EX_LAYERED** — Layered Window 鼠标穿透（配合 `WM_NCHITTEST → HTTRANSPARENT`）
- **WS_EX_TOOLWINDOW** — 不在任务栏和 Alt+Tab 中出现

#### 尝试过但废弃的方案

| 方案 | 问题 | 原因 |
|------|------|------|
| 周期性 `SetWindowPos(HWND_TOPMOST)` | 屎山代码，浪费 CPU | `WS_EX_LAYERED` 窗口在 DWM 下 Z 序不稳定，定时刷新治标不治本 |
| 移除 `WS_EX_LAYERED` | 鼠标消失 | 非层叠窗口的鼠标穿透行为与 `WS_EX_LAYERED` 不完全等价 |
| Shell Hook 事件纠偏 | 不触发 | `HSHELL_WINDOWACTIVATED` 等事件在 `WS_EX_NOACTIVATE` 窗口注册时不触发激活事件 |
| 系统 UI 白名单 | 复杂度高，效果不稳定 | 需要维护大量类名，DWM 内部合成行为难以完全预测 |

**关键经验**：WS_EX_TOPMOST 的扩展样式（而非运行时 HWND_TOPMOST）是在 DWM 层最稳定的一锤定音方案——改一行代码解决问题，零运行时开销。
### DWM 防护层
- DWMWA_BORDER_COLOR = 0 — accent 边框透明
- DWMNCRP_DISABLED — 禁用非客户区渲染
- WS_EX_NOREDIRECTIONBITMAP — 阻止 DWM 创建重定向表面
- DwmEnableBlurBehindWindow(FALSE) — 禁用模糊背景
- DwmFlush() — 清除合成器缓存
- WM_NCCALCSIZE return 0 — 移除整个非客户区

## 文件结构

```
ghostty-blackhole-main/
├── blackhole.exe              # 主程序（构建输出）
├── blackhole.glsl             # 黑洞着色器（GLSL）
├── blackhole_presets.txt      # 预设配置文件
├── CMakeLists.txt             # CMake 构建（默认 OpenGL 路径）
├── debug_log.md               # D3D11 迁移调试记录
├── LICENSE                    # MIT 许可证
├── README.md                  # 本文件
├── resource.rc                # Windows 资源文件（图标）
├── WGC_vs_DXGI.md             # 捕获方案对比文档
├── build_shader.ps1           # Shader 编译脚本
├── watchdog.ps1               # 进程守护脚本
│
├── src/                       # 源代码
│   ├── main.cpp               # 入口（OpenGL / D3D11 双路径，#ifdef 切换）
│   ├── capture_wgc.cpp/h      # WGC 桌面捕获（默认）
│   ├── capture_dxgi.cpp/h     # DXGI Duplication 备用捕获
│   ├── gl_texture.cpp/h       # OpenGL 纹理管理
│   ├── win32_gl.cpp/h         # Win32 + WGL 窗口（OpenGL 路径）
│   ├── win32_window.cpp/h     # 纯 Win32 窗口（D3D11 路径，未启用）
│   ├── d3d11_renderer.cpp/h   # D3D11 渲染器（未启用）
│   ├── renderer_interface.h   # 渲染器抽象接口
│   ├── texture_source.h       # 纹理源抽象接口
│   ├── gui_config.cpp/h       # ImGui 配置面板
│   └── imgui/                 # ImGui 库
│       ├── imgui.cpp/h
│       ├── imgui_draw.cpp
│       ├── imgui_widgets.cpp
│       ├── imgui_tables.cpp
│       ├── imgui_impl_glfw.cpp/h
│       └── imgui_impl_opengl3.cpp/h
│
├── shaders/                   # 着色器
│   ├── vert.glsl              # OpenGL 顶点着色器
│   ├── frag_header.glsl       # OpenGL 公共头
│   ├── frag_desktop_header.glsl
│   ├── frag_simple.glsl       # OpenGL 简单片段着色器
│   ├── blackhole.hlsl         # HLSL 黑洞着色器（D3D11，未启用）
│   └── fullscreen_vs.hlsl     # HLSL 全屏顶点着色器（D3D11，未启用）
│
└── release/                   # 发布目录
    ├── blackhole.exe          # 运行时可执行文件
    ├── blackhole.glsl         # 着色器
    ├── glfw3.dll              # GLFW 运行时
    ├── libgcc_s_seh-1.dll     # MinGW 运行时
    └── libstdc++-6.dll        # MinGW C++ 运行时
```

### 切换渲染后端

默认使用 OpenGL。D3D11 渲染路径已完整实现（d3d11_renderer.*, blackhole.hlsl）但因 WGC 帧池与 GPU 异步管线的竞争条件暂不启用。详见 [debug_log.md](debug_log.md)。

要启用 D3D11 路径：

```cmake
# CMakeLists.txt 第 34 行，取消注释：
target_compile_definitions(blackhole PRIVATE BLACKHOLE_USE_D3D11)
```
## 桌面捕获：WGC vs DXGI

**WGC 帧可能不完整，但持续更新。DXGI 帧完整，但生命周期敏感。**

| 维度 | WGC | DXGI Duplication |
|------|-----|------------------|
| 帧来源 | DWM 合成快照 | GPU backbuffer 拷贝 |
| 帧完整性 | 大画面变动时可能拿到半帧 | 每帧完整 |
| 帧稳定性 | 持续输出，不卡死 | 配对错误则全部失效 |
| 光标 | 默认捕获（可关） | 不捕获 |
| 多 GPU | 支持 | 需同 GPU |
| DRM 内容 | 可捕获 | 可能黑屏 |
| 全屏独占程序 | 可捕获 | 可能失败 |
| 延迟 | 有 DWM 合成延迟 | 低延迟 |
| API 复杂度 | WinRT（高） | COM（低） |
| 恢复机制 | 自动 | 需手动重建 |


### WGC 黄边框抑制

Win11 会对屏幕捕获绘制黄色强调色边框。通过 `IGraphicsCaptureSession3::put_IsBorderRequired(false)` 通知 DWM 不要绘制边框（Win11 22H2+）。

```cpp
// capture_wgc.cpp — StartCapture() 后
IGraphicsCaptureSession3* sess3 = nullptr;
sess->QueryInterface(IID_IGCS3, (void**)&sess3);
if (sess3) {
    sess3->put_IsBorderRequired(false);
    sess3->Release();
}
```

> ⚠️ 非官方关闭开关，Windows 在某些场景下仍可能显示边框。

### 本项目实测

| 测试项 | WGC | DXGI |
|--------|-----|------|
| 编译通过 | ✅ | ✅ |
| 启动捕获 | ✅ | ✅ |
| 画面持续刷新 | ✅ | ❌ INVALID_CALL |
| 黑洞效果正常 | ✅ | ❌ 背景冻结 |
| 切应用无闪烁 | ⚠️ 偶发轻微 | 未测 |
| 高帧率 165Hz | ✅ | 未测 |

DXGI 失败原因是 `ReleaseFrame / AcquireNextFrame` 配对极其严格——循环前未预取第一帧导致首个 `ReleaseFrame` 无对应帧，返回 `INVALID_CALL`，后续全部失效。当前项目默认使用 **WGC**，DXGI 代码保留在 `capture_dxgi.cpp` 中。

---

## D3D11 渲染路径实验

尝试过将渲染栈从 OpenGL+WGL 迁移到原生 D3D11（WGC 输出本身就是 `ID3D11Texture2D`，无需 CPU 往返）。

### 已完整实现的模块

- `src/d3d11_renderer.cpp/h` — D3D11 渲染器（SwapChain、Shader 编译、ConstantBuffer、全屏四边形）
- `src/win32_window.cpp/h` — 纯 Win32 窗口（无 WGL/D3D11 绑定，职责分离）
- `shaders/blackhole.hlsl` — GLSL→HLSL 精确翻译（Schwarzschild 光线积分、吸积盘、星空）
- `shaders/fullscreen_vs.hlsl` — 全屏顶点着色器
- `src/renderer_interface.h` — `IRenderer` 抽象接口（OpenGL/D3D11 双实现路径）
- `src/texture_source.h` — 纹理源抽象接口（Desktop/Video/Image/Camera 可扩展）

### 遇到的核心问题

```
WGC 帧池 (3 texture 轮换)
        ↓
CopyResource (GPU 异步)
        ↓
Pixel Shader 采样
        ↓
Present
```

**根因**：WGC 内部对 3 个 D3D11 纹理做池化复用。GPU 异步管线下 `CopyResource` 未完成时 WGC 可能复用同一纹理，导致 shader 采样到旧帧（画面冻结/残影叠加）。

### 尝试过的修复

| 尝试 | 结果 |
|------|------|
| 帧指针去重跳过 | 主动丢弃新帧，WGC 队列冻结 |
| GPU Query fence 同步 | 伪同步，CPU busy-wait 破坏 WGC 采集节奏 |
| 帧缓冲队列 (2~3 buffer 延迟) | 理论上正确，但 WGC/DWM 纹理生命周期不保证帧连续性 |
| SRV 每帧重建 + 延迟 Release | GPU 仍在读旧内存时被 CPU 释放，资源竞争 |

### 结论

D3D11+WGC 组合在当前 Windows/WGC 版本下存在架构级阻抗：WGC 返回的是 DWM 合成快照的弱绑定 GPU 资源，不适合作为连续视频流纹理源。OpenGL+WGL 路径的 `Staging → Map → glTexSubImage2D` 虽然多一次 CPU 往返，但天然隔离了 GPU 异步竞争问题。

D3D11 代码完整保留，可通过 `CMakeLists.txt` 第 34 行取消注释重新启用。详见 [`debug_log.md`](debug_log.md)。

---

## 已知问题

| 问题 | 状态 | 说明 |
|------|------|------|
| 双鼠标 | 待修复 | 正常光标 + 黑洞扭曲后光标同时可见，根因未完全定位；不预设 D3D11 自动解决 |
| Win11 黄边框 | 已抑制 | `IsBorderRequired(false)` 降低概率，非 100% 消除 |
| 游戏时空闲检测 | 已修复 | 1s 检测 + 无边框全屏窗口提前判断 + 游戏启动器匹配 |
| 被其他窗口遮挡 | 已修复 | `WS_EX_TOPMOST` 扩展样式一锤定音 |

---

## 技术栈

- **OpenGL 3.3** — 渲染
- **Win32 + WGL** — 原生窗口 + OpenGL 上下文（替代 GLFW 以获得完整的桌面特效控制）
- **ImGui** — 配置界面
- **Windows Graphics Capture (WGC)** — 桌面捕获
- **DXGI Duplication** — 备用捕获方案
- **MinGW-w64** — 编译工具链
- **CMake** — 构建系统

## 灵感来源

[Eric Bruneton's black hole shader](https://github.com/ebruneton/black_hole_shader) (BSD-3-Clause)

## License

MIT — 见 [LICENSE](LICENSE)
