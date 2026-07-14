# Black Hole — 跨平台桌面黑洞屏保

![demo](demo.gif)

基于 Eric Bruneton 黑洞着色器的**跨平台**桌面黑洞可视化程序。现已同时支持 **Windows**（原生 Win32 桌面捕获 + OpenGL 渲染）和 **Linux**（GNOME Shell 合成器扩展），通过实时渲染展现史瓦西黑洞的引力透镜、吸积盘、光子环等相对论效应。

> **技术细节**（空闲检测原理、双进程设计、WGC vs DXGI、D3D11 实验、已知问题等）请参见 **[Doc/TECHNICAL.md](Doc/TECHNICAL.md)**。

---

## 当前状态

- **双平台可用**：Windows 原生屏保 + Ubuntu GNOME Wayland 合成器效果均已落地。
- **新版 Qt UI** 已替换旧版 ImGui 成为主配置面板；旧版仍在但不再主推。
- **Linux 后端**使用 GNOME Shell 扩展 + D-Bus 接口，无需 Portal 桌面捕获。
- **16 预设**、多预设列表、三种播放模式（顺序/循环/随机）端到端可用。
- 捕获方式支持自动 / WGC / DXGI（Windows），自动模式优先 WGC。
- 多显示器支持主屏、副屏、跨屏和一屏一黑洞（Windows）。

> Ubuntu 移植版尚未发布 GitHub Release，请从 `port/ubuntu-wayland` 分支构建。该分支基于上游 1.2.0；上游 1.2.1 的更新尚未合并。

## 运行要求

### 操作系统

| 要求 | 说明 |
|------|------|
| **最低** | Windows 10 1803+ (build 17134)，64 位 |
| **推荐** | Windows 11 22H2+ (build 22621) |
| **Linux 已验证环境** | Ubuntu 26.04 / GNOME 50 / Wayland |

> Win10 1803 是 WGC (`Windows.Graphics.Capture`) 的最低版本。Win11 22H2 额外支持 `IsBorderRequired(false)` 进一步抑制黄边框。
> Linux 后端使用 GNOME Shell 扩展，需 Wayland 会话；其他发行版和 GNOME 版本尚未验证。

### 显卡兼容性

Windows 默认渲染路径：**WGC/DXGI 桌面捕获 → CPU 回读 → OpenGL 3.3 渲染**。WGC/DXGI 内部依赖 D3D11，但默认路径不做 D3D11 渲染。Linux 主链路则直接在 GNOME Shell 合成器中应用 shader。

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
| `appBlakholeUI` (Linux) | Qt 6、GNOME Shell 50、D-Bus 与 OpenGL 运行时；用 `.deb` 安装时由 APT 解决 |

### 编译要求

#### Windows (MSYS2 UCRT64)

| 工具 | 版本 | 用途 |
|------|------|------|
| **MSYS2** | 最新 | UCRT64 编译环境 |
| **GCC** | 12+ (`mingw-w64-ucrt-x86_64-gcc`) | C++17 编译 |
| **CMake** | 3.20+ | 构建系统 |
| **GLFW3** | 3.3+ (`pacman -S mingw-w64-ucrt-x86_64-glfw`) | 窗口/输入（ImGui 用） |
| **Qt 6** | 6.8+ (`qt6-base`, `qt6-declarative`) | 仅 Blakhole_UI 需要 |

#### Linux（本次验证环境）

| 工具 | 版本 | 用途 |
|------|------|------|
| **GCC** | 15+ | C++17 编译 |
| **CMake** | 4.2+ | 构建系统 |
| **Ninja** | 1.13+ | 构建工具 |
| **Qt 6** | 6.10+ (`qt6-base-dev`, `qt6-declarative-dev`) | Blakhole_UI 需要 |
| **GLFW3** | 3.4+ (`libglfw3-dev`) | 渲染器窗口（Linux OpenGL 路径） |
| **PipeWire** | (`libpipewire-0.3-dev`) | 旧全屏渲染器的 Portal 捕获依赖 |

---
## 快速开始

### Windows

1. 双击 `release\appBlakholeUI.exe`（`release\blackhole.exe` 为旧版 UI / 渲染器入口）
2. 配置参数 → 点击 **"启动"**
3. 黑洞在**空闲时自动显示**，动鼠标/键盘即消失
4. 右下角托盘图标 → 右键可退出

### Linux (Ubuntu GNOME Wayland)

1. 按下文“Linux 从源码安装”完成构建和安装，首次安装 GNOME 扩展后注销并重新登录一次。
2. 启用扩展，然后启动 `appBlakholeUI`：

   ```bash
   gnome-extensions enable blackhole@xboxnahida.github.com
   appBlakholeUI
   ```

3. 配置参数 → 点击 **"启动黑洞"**
4. 程序通过 D-Bus 向 GNOME Shell 扩展发送指令，在合成器层实时渲染。
5. 也可在终端使用 D-Bus 直接控制：

   ```bash
   gdbus call --session --dest io.github.xboxnahida.Blackhole \
     --object-path /io/github/xboxnahida/Blackhole \
     --method io.github.xboxnahida.Blackhole.Start
   gdbus call --session --dest io.github.xboxnahida.Blackhole \
     --object-path /io/github/xboxnahida/Blackhole \
     --method io.github.xboxnahida.Blackhole.Stop
   ```

---

### Ubuntu GNOME Wayland 使用须知

- Linux 主链路由 Qt UI 通过 D-Bus 控制 GNOME Shell 合成器扩展，不需要 Portal 选屏授权，也不会启动旧全屏渲染器。
- “始终显示”会立即启动黑洞；“空闲检测”在达到设定时间后启动，用户恢复活动后自动停止。
- 托盘不可用时，主窗口仍应保留启动、停止和退出入口。
- 当前 Linux 实机验收平台为 Ubuntu 26.04、GNOME 50、Wayland 和 NVIDIA RTX 3060。

---

## 两种模式

| 模式 | 行为 |
|------|------|
| **始终显示** | 黑洞常驻桌面 |
| **空闲检测** | 空闲 N 秒后显示，活跃时自动隐藏 |

空闲时间在配置页面设置（默认 300 秒）。支持三层检测：D3D 全屏检测、窗口尺寸检测、音视频进程名单匹配。

> Linux 上使用 GNOME idle monitor (D-Bus) + MPRIS 媒体播放检测替代 Windows 三层检测。

---

## 配置参数

- **主要可调参数**：色温、倾角、旋转、半径、不透明度、多普勒、光束指数、亮度增益、条纹对比度、缠绕紧度、旋转速度、曝光度、星空亮度
- **16 个预设**，支持复制/粘贴、上移/下移排序
- **三种播放模式**：顺序 / 循环 / 随机
- **Windows 捕获方式**：自动 / WGC / DXGI
- **Windows 显示器模式**：主屏 / 副屏 / 跨屏 / 一屏一黑洞
- **固定大小**：可让黑洞保持固定比例，不再随时间增长

---

## 项目结构

### 双程序架构

| 程序 | 技术栈 | 用途 |
|------|--------|------|
| `blackhole.exe` | C++17 / OpenGL+WGL / ImGui | Windows 原始实现：桌面黑洞渲染 + ImGui 配置面板 |
| `Blakhole_UI/appBlakholeUI` | Qt6 QML / OpenGL FBO | 新版跨平台 UI：可视化配置 + 实时预览 + 进程管理 |
| `blackhole@xboxnahida.github.com` | GNOME Shell 扩展 (JS) | Linux 后端：合成器效果 + D-Bus 控制接口 |

### 源代码模块

#### Windows 活跃模块

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

#### Linux 活跃模块

| 文件 | 功能 |
|------|------|
| `Blakhole_UI/core/blackholecore.cpp` | Qt UI 配置、D-Bus 效果启停与生命周期管理 |
| `src/gnome_idle_monitor.cpp/h` | GNOME IdleMonitor 空闲与恢复活动检测 |
| `src/mpris_monitor.cpp/h` | MPRIS 媒体播放检测 |
| `src/autostart_xdg.cpp/h` | XDG 用户级自启动 |
| `gnome-extension/blackhole@xboxnahida.github.com/` | GNOME 50 合成器效果、D-Bus 接口与 shader |
| `src/main_linux.cpp` | 旧全屏渲染器入口，当前保留作为兼容/诊断路径 |
| `src/portal_capture.cpp/h` | 旧全屏路径的 XDG Desktop Portal + PipeWire 捕获 |

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
| `src/` | blackhole.exe 源代码（含 Windows + Linux 双平台入口） |
| `shaders/` | GLSL/HLSL 着色器 |
| `Blakhole_UI/` | Qt6/QML 跨平台配置面板（独立 CMake 项目） |
| `gnome-extension/` | GNOME Shell 合成器扩展 |
| `Doc/` | 技术文档 |
| `build/` | blackhole.exe 构建输出 |
| `release/` | 发布目录（exe + DLL + shaders） |
| `.vscode/` | VS Code 配置 |
| `packaging/` | Linux 桌面图标和应用元数据 |

---

## 编译

### Windows

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

### Linux

#### 1. 获取 Ubuntu 移植分支

```bash
git clone --branch port/ubuntu-wayland \
  https://github.com/Kiarelemb/ghostty-blackhole-ubuntu.git
cd ghostty-blackhole-ubuntu
```

#### 2. 安装构建依赖（Ubuntu 26.04）

```bash
sudo apt update
sudo apt install build-essential cmake ninja-build pkg-config dpkg-dev \
  qt6-base-dev qt6-declarative-dev libglfw3-dev \
  libpipewire-0.3-dev libwayland-dev wayland-protocols
```

#### 3. 构建

```bash
cmake -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=/usr \
  -DBLACKHOLE_BUILD_UI=ON \
  -DBLACKHOLE_BUILD_RENDERER=ON \
  -DBLACKHOLE_ENABLE_PORTAL_CAPTURE=ON \
  -DBUILD_TESTING=ON
cmake --build build
```

单元测试目标默认不随主程序构建，需要显式构建后再运行 CTest：

```bash
cmake --build build --parallel --target \
  shader_source_tests portal_capture_tests \
  renderer_startup_diagnostics_tests movement_settings_tests \
  application_catalog_tests avatar_storage_tests update_release_tests \
  update_checker_state_tests renderer_search_tests \
  gnome_idle_monitor_tests mpris_monitor_tests ds09_linux_tests
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
```

#### 4. 生成并安装 `.deb`

```bash
cpack --config build/CPackConfig.cmake -G DEB
sudo apt install ./blackhole-1.2.0-linux-amd64.deb
```

APT 会安装包中声明的运行时依赖。首次安装 GNOME Shell 扩展后，请注销并重新登录一次，然后执行：

```bash
gnome-extensions enable blackhole@xboxnahida.github.com
gnome-extensions info blackhole@xboxnahida.github.com
appBlakholeUI
```

`gnome-extensions info` 应显示 `State: ACTIVE`。如果 D-Bus 启动后无效果，请检查：

```bash
journalctl -b /usr/bin/gnome-shell --no-pager | tail -n 200
```

#### 5. 卸载

```bash
sudo apt remove blackhole
```

---

## 技术栈

| 组件 | 技术 | 说明 |
|------|------|------|
| **桌面捕获 (Win)** | WGC / DXGI Desktop Duplication | 自动选择或手动指定捕获方式 |
| **纹理传输 (Win)** | CPU 回读 (`Staging → Map → glTexSubImage2D`) | 跨厂商兼容，隔离 GPU 差异 |
| **渲染 (Win)** | OpenGL 3.3 + WGL | 原生 Win32 窗口，全屏顶层覆盖 |
| **Linux 后端** | GNOME Shell 扩展 (JS) + D-Bus | 合成器层实时效果，无需 Portal |
| **旧全屏路径 (Linux)** | XDG Desktop Portal / 生成背景 | 保留作为兼容与诊断路径，不是当前 UI 默认后端 |
| **空闲检测 (Win)** | D3D全屏 / 窗口尺寸 / 音频会话 | 三层检测 |
| **空闲检测 (Linux)** | GNOME idle monitor + MPRIS | D-Bus 空闲 + 媒体播放检测 |
| **配置面板（旧）** | ImGui | blackhole.exe 内置 |
| **配置面板（新）** | Qt 6 + QML | 跨平台，独立进程，含实时 FBO 预览 |
| **构建** | MinGW-w64 (UCRT64) + CMake (Win) / GCC+CMake+Ninja (Linux) | |

> DXGI Duplication 和 D3D11 渲染器代码保留在仓库中，当前未启用。详见 [Doc/TECHNICAL.md](Doc/TECHNICAL.md)。

---

## 迁移手记

这次 Ubuntu GNOME Wayland 迁移经历了多轮 AI 编码、审查和实机验收。按开发过程中的用量统计与估算，累计消耗约 **5 亿 DeepSeek token**，以及约 **75% 的 Codex Plus 周限额**。期间先后尝试了原生全屏窗口、Portal 桌面捕获和合成器原型等多种方案；在 Wayland 递归捕获与窗口层级限制下，最终转向 **GNOME Shell 扩展 + D-Bus**，才完成可用落地。

这段经历最直接的教训是：构建和单元测试只是起点，桌面合成器效果必须在真实图形会话中反复验收。


## 灵感来源

[Eric Bruneton's black hole shader](https://github.com/ebruneton/black_hole_shader) (BSD-3-Clause)

## License

MIT — 见 [LICENSE](LICENSE)
