# Black Hole — Windows 桌面黑洞屏保

![demo](demo.gif)

基于 Eric Bruneton 黑洞着色器的 Windows 桌面黑洞可视化程序。捕获桌面画面，实时渲染史瓦西黑洞的引力透镜、吸积盘、光子环等相对论效应。

> **技术细节**（空闲检测原理、双进程设计、WGC vs DXGI、D3D11 实验、已知问题等）请参见 **[TECHNICAL.md](TECHNICAL.md)**。

---

## 运行要求

| 类别 | 最低要求 | 推荐 |
|------|----------|------|
| **操作系统** | Windows 10 1803+ (build 17134) | Windows 11 22H2+ (build 22621) |
| **架构** | x64 | x64 |
| **显卡** | OpenGL 3.3 + D3D11 Feature Level 11_0 | 独立显卡 |
| **显卡驱动** | 2018 年后的 WDDM 2.4+ 驱动 | 最新驱动 |
| **内存** | 4 GB | 8 GB |
| **显示器** | 任意分辨率 | ≥ 1920×1080 |

> **已知限制**：Win11 黄边框已通过 `IsBorderRequired(false)` 抑制，但无法 100% 消除。双鼠标问题有待修复。详见 [TECHNICAL.md](TECHNICAL.md)。

### Blakhole_UI 额外要求

| 类别 | 要求 |
|------|------|
| **Qt 运行时** | Qt 6.8+（`release/` 目录已附带 DLL） |
| **OpenGL** | 3.3 Core Profile（FBO 预览需要） |

### 编译要求（从源码构建）

| 工具 | 版本 | 说明 |
|------|------|------|
| **MSYS2** | 最新 | UCRT64 环境 |
| **GCC** | 12+ | `mingw-w64-ucrt-x86_64-gcc` |
| **CMake** | 3.20+ | |
| **GLFW3** | 3.3+ | `pacman -S mingw-w64-ucrt-x86_64-glfw` |
| **Qt 6** | 6.8+ | 仅 Blakhole_UI 需要 (`qt6-base`, `qt6-declarative`) |

---

## 快速开始

1. 双击 `release\blackhole.exe`
2. 配置参数 → 点击 **"启动"**
3. 黑洞在**空闲时自动显示**，动鼠标/键盘即消失
4. 右下角托盘图标 → 右键可退出

也可使用新版 Qt6 配置面板：`Blakhole_UI\build\appBlakholeUI.exe`

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
| `src/capture_wgc.cpp/h` | WGC 桌面捕获（**默认捕获方案**） |
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
| `src/capture_dxgi.cpp/h` | DXGI Duplication 备用捕获。存在 `INVALID_CALL` 问题，保留供未来修复 |
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

- **OpenGL 3.3** — 渲染
- **Win32 + WGL** — 原生窗口
- **Windows Graphics Capture (WGC)** — 桌面捕获（默认）
- **DXGI Duplication** — 备用捕获（保留）
- **ImGui** — 旧版配置界面
- **Qt 6 + QML** — 新版配置面板
- **MinGW-w64 + CMake** — 构建系统

---

## 灵感来源

[Eric Bruneton's black hole shader](https://github.com/ebruneton/black_hole_shader) (BSD-3-Clause)

## License

MIT — 见 [LICENSE](LICENSE)