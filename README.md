# Black Hole — Windows 桌面黑洞屏保

![demo](demo.gif)

基于 Eric Bruneton 黑洞着色器的 Windows 桌面黑洞可视化程序。捕获桌面画面，实时渲染史瓦西黑洞的引力透镜、吸积盘、光子环等相对论效应。

> **技术细节**（空闲检测原理、双进程设计、WGC vs DXGI、D3D11 实验、已知问题等）请参见 **[TECHNICAL.md](TECHNICAL.md)**。

---

# 目前新功能还在开发中，新 UI 已完成，旧 UI 仍可使用。
# 发布版本见右侧Release里。
# 作者最近比较忙，更新会比较慢。

## 当前状态

- 新版 Qt UI 可用，旧版 ImGui UI 仍保留。
- 捕获方式支持自动 / WGC / DXGI。自动模式会在 Win11 22H2+ 优先使用可抑制黄边框的 WGC，在旧系统回退 DXGI。
- 多显示器支持主屏、副屏、跨屏和一屏一黑洞。
- 固定大小、16 预设、多预设列表持久化已端到端接入。
- D3D11 渲染器仍作为实验路径保留，默认渲染路径仍是 OpenGL。

## 运行要求

### 操作系统

| 要求 | 说明 |
|------|------|
| **最低** | Windows 10 1803+ (build 17134)，64 位 |
| **推荐** | Windows 11 22H2+ (build 22621) |

> Win10 1803 是 WGC (`Windows.Graphics.Capture`) 的最低版本。Win11 22H2 额外支持 `IsBorderRequired(false)` 进一步抑制黄边框。

### 显卡兼容性

当前默认渲染路径：**WGC/DXGI 桌面捕获 → CPU 回读 → OpenGL 3.3 渲染**。WGC/DXGI 内部依赖 D3D11，但默认路径不做 D3D11 渲染。

#### 桌面独显

| 厂商 | 系列 | 兼容性 | 备注 |
|------|------|--------|------|
| **NVIDIA** | GeForce GTX 400 系列及以上 (2010+) | ✅ 完全支持 | Fermi 架构起支持 OpenGL 3.3 |
| **AMD** | Radeon HD 7000 系列及以上 (2012+, GCN) | ✅ 完全支持 | GCN 第一代起支持 |
| **Intel** | Arc 独显系列 | ✅ 完全支持 | |

#### 集显（核显）

| 厂商 | 系列 | 兼容性 | 备注 |
|------|------|--------|------|
| **Intel** | HD Graphics 4000 及以上 (2012+, Ivy Bridge) | ✅ 可用 | OpenGL 驱动偶有小问题，ImGui 已内置 Intel 兼容处理 |
| **Intel** | UHD / Iris Xe (2017+) | ✅ 良好 | |
| **AMD** | Radeon Vega 核显 (Ryzen 2000+) | ✅ 良好 | |
| **AMD** | RDNA2/3 核显 (Radeon 680M/780M) | ✅ 良好 | |

#### 笔记本混合显卡（双显卡）

| 类型 | 兼容性 | 说明 |
|------|--------|------|
| **NVIDIA Optimus**（Intel 核显 + NVIDIA 独显） | ✅ 可用 | CPU 回读路径天然隔离 GPU 差异，不依赖 GPU 间纹理共享 |
| **AMD Switchable Graphics**（核显 + AMD 独显） | ✅ 可用 | 同上 |
| **纯集显笔记本** | ✅ 良好 | 单一 GPU，无需协调 |

> 混合显卡下 D3D11 设备默认创建在核显上（WGC 内部需要），OpenGL 渲染在系统默认 GPU。数据经 CPU 中转，不依赖跨 GPU 纹理共享。

### 运行时依赖

| 程序 | 依赖 |
|------|------|
| `blackhole.exe` | 无额外依赖，静态链接 |
| `appBlakholeUI.exe` | Qt 6.8+ 运行时 DLL（`release/` 已附带） |

### 编译要求

| 工具 | 版本 | 用途 |
|------|------|------|
| **MSYS2** | 最新 | UCRT64 编译环境 |
| **GCC** | 12+ (`mingw-w64-ucrt-x86_64-gcc`) | C++17 编译 |
| **CMake** | 3.20+ | 构建系统 |
| **GLFW3** | 3.3+ (`pacman -S mingw-w64-ucrt-x86_64-glfw`) | 窗口/输入（ImGui 用） |
| **Qt 6** | 6.8+ (`qt6-base`, `qt6-declarative`) | 仅 Blakhole_UI 需要 |

---
## 快速开始

1. 双击 `release\appBlakholeUI.exe`（`release\blackhole.exe` 为旧版 UI / 渲染器入口）
2. 配置参数 → 点击 **"启动"**
3. 黑洞在**空闲时自动显示**，动鼠标/键盘即消失
4. 右下角托盘图标 → 右键可退出

---

### Ubuntu GNOME Wayland 使用须知

- 从 UI 启动的黑洞默认使用生成背景，不会弹出屏幕共享授权。将全屏黑洞本身作为 Portal 桌面捕获源会形成递归反馈，因此实时桌面捕获不用作默认屏保背景。

- “始终显示”会立即启动黑洞；“空闲检测”只在达到设定的空闲时间后启动。
- 桌面背景首次使用时可能弹出 GNOME 选屏授权；请选择要捕获的显示器。拒绝或超时时会退回生成背景。
- 授权流程结束且黑洞稳定显示后，按键、鼠标按键或移动超过 10 像素都会结束黑洞，这是屏保式退出行为。
- 托盘不可用时，主窗口仍应保留启动、停止和退出入口。
- 渲染器启动诊断日志位于 `~/.config/XboxNahida/Blakhole UI/blackhole_debug.txt`。

---

## 两种模式

| 模式 | 行为 |
|------|------|
| **始终显示** | 黑洞常驻桌面 |
| **空闲检测** | 空闲 N 秒后显示，活跃时自动隐藏 |

空闲时间在配置页面设置（默认 300 秒）。支持三层检测：D3D 全屏检测、窗口尺寸检测、音视频进程名单匹配。

---

## 配置参数

- **14 个可调参数**：色温、倾角、旋转、半径、不透明度、多普勒、光束指数、亮度增益、条纹对比度、缠绕紧度、旋转速度、曝光度、星空亮度
- **16 个预设**，支持复制/粘贴、上移/下移排序
- **三种播放模式**：顺序 / 循环 / 随机
- **捕获方式**：自动 / WGC / DXGI
- **显示器模式**：主屏 / 副屏 / 跨屏 / 一屏一黑洞
- **固定大小**：可让黑洞保持固定比例，不再随时间增长

---

## 项目结构

### 双程序架构

| 程序 | 技术栈 | 用途 |
|------|--------|------|
| `blackhole.exe` | C++17 / OpenGL+WGL / ImGui | 原始实现：桌面黑洞渲染 + ImGui 配置面板 |
| `Blakhole_UI/appBlakholeUI.exe` | Qt6 QML / OpenGL FBO | 新版 UI：可视化配置 + 实时预览 + 进程管理 |

### 源代码模块

#### ✅ 活跃模块（正在使用）

| 文件 | 功能 |
|------|------|
| `src/main.cpp` | 主入口，双进程架构、shader 编译、uniform 注入、空闲检测 |
| `src/win32_gl.cpp/h` | Win32+WGL 原生窗口（替代 GLFW） |
| `src/capture_wgc.cpp/h` | WGC 桌面捕获，支持黄边框抑制和禁用光标捕获 |
| `src/capture_dxgi.cpp/h` | DXGI Duplication 备用/兼容捕获路径 |
| `src/monitors.cpp/h` | 显示器枚举与主屏/副屏选择 |
| `src/gl_texture.cpp/h` | D3D11→OpenGL 纹理上传 |
| `src/gui_config.cpp/h` | ImGui 配置面板（旧版 UI） |
| `src/imgui/` | Dear ImGui 库 |
| `blackhole.glsl` | 核心黑洞着色器（运行时字符串替换） |
| `shaders/vert.glsl` | OpenGL 顶点着色器 |
| `shaders/frag_desktop_header.glsl` | 桌面版 uniform 声明 |
| `shaders/frag_preview_header.glsl` | 预览版 uniform 声明 |
| `shaders/blackhole_preview.glsl` | 预览版着色器（Blakhole_UI FBO 用） |

#### 🔒 预留/实验模块（完整实现，当前未启用）

| 文件 | 说明 |
|------|------|
| `src/d3d11_renderer.cpp/h` | D3D11 渲染器。因 WGC 纹理池化+GPU 异步导致画面冻结，已回退 |
| `src/win32_window.cpp/h` | 纯 Win32 窗口（配合 D3D11 路径） |
| `src/renderer_interface.h` | `IRenderer` 抽象接口（OpenGL/D3D11 双路径预留） |
| `src/texture_source.h` | `ITextureSource` 纹理源抽象（面向未来扩展） |
| `shaders/blackhole.hlsl` | HLSL 翻译（D3D11 路径） |
| `shaders/fullscreen_vs.hlsl` | D3D11 全屏顶点着色器 |

### 目录说明

| 目录 | 内容 |
|------|------|
| `src/` | blackhole.exe 源代码 |
| `shaders/` | GLSL/HLSL 着色器 |
| `Blakhole_UI/` | Qt6/QML 新版配置面板（独立 CMake 项目） |
| `Doc/` | 技术文档 |
| `build/` | blackhole.exe 构建输出 |
| `release/` | 发布目录（exe + DLL + shaders） |
| `.vscode/` | VS Code 配置 |

---

## 编译

```powershell
# 使用 MSYS2 UCRT64
.\build_blackhole.ps1

# 或 CMake
cmake -B build -G "MinGW Makefiles"
cmake --build build
```

D3D11 路径可通过 `CMakeLists.txt` 取消注释以下行启用：
```cmake
target_compile_definitions(blackhole PRIVATE BLACKHOLE_USE_D3D11)
```

---

## 技术栈

| 组件 | 技术 | 说明 |
|------|------|------|
| **桌面捕获** | WGC / DXGI Desktop Duplication | 自动选择或手动指定捕获方式 |
| **纹理传输** | CPU 回读 (`Staging → Map → glTexSubImage2D`) | 跨厂商兼容，隔离 GPU 差异 |
| **渲染** | OpenGL 3.3 + WGL | 原生 Win32 窗口，全屏顶层覆盖 |
| **配置面板（旧）** | ImGui | blackhole.exe 内置 |
| **配置面板（新）** | Qt 6 + QML | Blakhole_UI，独立进程，含实时 FBO 预览 |
| **构建** | MinGW-w64 (UCRT64) + CMake | |

> DXGI Duplication 和 D3D11 渲染器代码保留在仓库中，当前未启用。详见 [TECHNICAL.md](TECHNICAL.md)。


## 灵感来源

[Eric Bruneton's black hole shader](https://github.com/ebruneton/black_hole_shader) (BSD-3-Clause)

## License

MIT — 见 [LICENSE](LICENSE)
