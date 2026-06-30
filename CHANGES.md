# 相对原项目的改动说明

本文档记录相对于上游 `ghostty-blackhole-main`（macOS Ghostty 终端黑洞装饰）的改动，用于 fork 后推回给原作者参考。

---

## 一、定位变更

| 维度 | 原项目 | 本 fork |
|------|--------|---------|
| 平台 | macOS Ghostty 终端 | Windows 桌面屏保 |
| 触发 | Claude Code 会话/上下文填充 | 系统空闲检测 / 始终显示 |
| 渲染目标 | 终端 cursor 颜色 → shader 解码 | Win32 桌面顶层窗口 |
| 配置入口 | Swift `tuner/` GUI | ImGui `gui_config.cpp` |
| 信号通道 | OSC 12 转义序列 | Win32 进程间控制 |

> 这是一个**平台级重写**而非增量改进。原项目的 `claude-token.py`、`tuner/` 保留未动（macOS 路径仍可用），Windows 路径全部为新增代码。

---

## 二、新增文件（Windows 屏保实现）

### 构建与资源
- `build_blackhole.ps1` — MSYS2/ucrt64 + windres + g++ 一键构建脚本（自动设置 PATH、嵌入图标、`-O2` 优化）
- `resource.rc` — Windows 资源文件（声明 `MAINICON` 与 `100` 号图标）
- `blackhole.ico` — 多尺寸图标资源（32×32 等）
- `CMakeLists.txt` — CMake 构建配置（含 MSYS2 UCRT64 DLL 自动拷贝）

### 源代码（`src/`）
- `main.cpp` — 入口、配置面板调度、托盘监控、`isWatchingVideo` 检测、`--render` 子进程模式、`--monitor` 调试模式
- `win32_gl.cpp/h` — Win32 原生窗口 + WGL 3.3 兼容上下文（替代 GLFW，获得完整桌面特效控制权）
- `capture_wgc.cpp/h` — Windows Graphics Capture 桌面捕获（默认路径）
- `capture_dxgi.cpp/h` — DXGI Duplication 备用捕获（保留未启用）
- `gl_texture.cpp/h` — OpenGL 纹理上传（WGC Staging → `glTexSubImage2D`）
- `gui_config.cpp/h` — ImGui 配置面板（16 个预设滑块、复制/粘贴、播放模式）
- `renderer_interface.h` / `texture_source.h` — 渲染器/纹理源抽象接口（D3D11/OpenGL 双路径预留）
- `d3d11_renderer.cpp/h` + `win32_window.cpp/h` — D3D11 路径完整实现（保留未启用，详见 README）
- `imgui/` — Dear ImGui 库源码

### 着色器（`shaders/`）
- `vert.glsl` — OpenGL 顶点着色器
- `frag_desktop_header.glsl` — 桌面版片段头（声明 `uHomeX/uHomeY/uRandPhase/uPresetOffset` 等 uniform）
- `frag_header.glsl` / `frag_simple.glsl` — 备用片段头
- `blackhole.hlsl` / `fullscreen_vs.hlsl` — D3D11 路径 HLSL 翻译

---

## 三、对原 `blackhole.glsl` 的兼容性改造

为了在 Windows 屏保路径下运行时改参数，原 shader 中的 `const float` 常量被运行时替换为可被 uniform 覆盖的形式（详见 `main.cpp::buildFragmentShader`）：

| 原 shader 写法 | 运行时替换为 |
|----------------|--------------|
| `const float HOLE_RADIUS = X;` | `float HOLE_RADIUS = uHoleRadius > 0.0 ? uHoleRadius : X;` |
| `const float DISK_GAIN = X;` | `float DISK_GAIN = uDiskGain > 0.0 ? uDiskGain : X;` |
| `DISK_TEMP` / `EXPOSURE` / `DRIFT_SPEED` / `STAR_GAIN` / `DISK_INCL` | 同上模式 |
| `const float TOKEN_HOME_X = 0.96;` | `float TOKEN_HOME_X = uHomeX;` |
| `const float TOKEN_HOME_Y = 0.04;` | `float TOKEN_HOME_Y = uHomeY;` |
| `#define SIZE_MODE MODE_TOKENS` | `#define SIZE_MODE MODE_DEMO` |
| `mod(iTime, DEMO_SEC) / DEMO_GROW_SEC` | `iTime / DEMO_GROW_SEC`（去掉时长回卷，让黑洞持续生长） |
| `lissa(t * TOKEN_CALM)` | `lissa(t * TOKEN_CALM + uRandPhase)`（随机化轨迹相位） |
| `DiskLook demoLook()` 整个函数 | 替换为读 uniform 数组 `uPresetTemp[i]...` 的版本，支持 3 种播放模式 |

新增 uniform（在 `frag_desktop_header.glsl`）：
- `uHomeX`, `uHomeY` — 黑洞初始位置随机化
- `uRandPhase` — 轨迹相位随机化
- `uPresetOffset` — 预设起始偏移随机化
- `uPresetTemp/Incl/Roll/Inner/Outer/Opac/Dopp/Beam/Gain/Contr/Wind/Spd/Expo/Star[16]` — 16 个预设参数数组
- `uPresetCount`, `uSlotSec`, `uPlayMode`, `uBornProgress`

> **关键陷阱**：GLSL `const` 必须是编译期常量，不能用 uniform 赋值。所以 `const float TOKEN_HOME_X = uHomeX;` 会编译失败 —— 必须改成 `float TOKEN_HOME_X = uHomeX;`（去掉 const）。同样 `const float WORK_AREA = 0.0` 替换时漏分号会导致 syntax error。

---

## 四、Bug 修复（Windows 路径独有）

### 1. 系统光标全局隐藏导致退出后异常
- **原方案**：`SetSystemCursor(空光标, OCR_NORMAL)` 全局替换系统箭头
- **问题**：程序异常退出或失焦时光标永久不可见
- **修复**：改用 WGC `IsCursorCaptureEnabled = false`，捕获纹理中不包含系统光标，无需全局隐藏
- **应急兜底**：`Win32GL_Shutdown` 中调用 `SystemParametersInfo(SPI_SETCURSORS, ...)` 重置系统光标

### 2. 渲染窗口被误判为全屏视频导致重启死循环
- **现象**：程序无操作运行一段时间后自动结束，几秒后又重启
- **根因**：`isWatchingVideo()` 中 `if (ww >= sw && wh >= sh) return true` 把黑洞自己的全屏渲染窗口判定为视频/游戏 → `idle=false` → 监控杀掉渲染进程 → 渲染窗口消失 → 重新 spawn → 死循环
- **修复**（`main.cpp:416-455`）：
  1. 排除具有 `WS_EX_NOACTIVATE | WS_EX_TOPMOST | WS_EX_TRANSPARENT` 样式组合的窗口
  2. 排除窗口类名为 `BlackHoleWGL` 的窗口
  3. 排除 `WS_MAXIMIZE` 的最大化普通窗口（只针对真正的无边框全屏游戏）

### 3. 多显示器/高 DPI 下窗口尺寸异常
- **修复**：`SetProcessDPIAware()` + `GetMonitorInfoW` 获取真实分辨率，`SetWindowPos` 显式设置窗口尺寸（不用 `SWP_NOSIZE`）

### 4. 渲染只覆盖屏幕上半部分
- **根因**：fragment shader 的 `WORK_AREA` 底部保护 + shield gradient 截断
- **修复**（`main.cpp::buildFragmentShader`）：替换为 `const float WORK_AREA = 0.0;`（注意分号），移除底部保护

### 5. 编译静默失败导致旧 exe 仍在运行
- **根因**：`C:\msys64\ucrt64\bin` 不在 PATH，g++ 运行时 DLL 加载失败，无任何错误输出，旧 exe 不被覆盖
- **修复**：`build_blackhole.ps1` 第一行 `$env:PATH = "C:\msys64\ucrt64\bin;C:\msys64\usr\bin;$env:PATH"`

### 6. 链接错误 `CoInitializeEx`/`CoCreateInstance` 未定义
- **修复**：构建命令添加 `-lole32`

### 7. GLSL 字符串替换造成的编译错误
- `body.replace(p, ve - p + 1, "const float WORK_AREA = 0.0")` — 替换范围包含分号但新字符串没分号 → 改为 `"const float WORK_AREA = 0.0;"`
- 把 `TOKEN_HOME_X` 全局替换成 `uHomeX` 导致 `const float uHomeX = 0.96;` 与 header 中 `uniform float uHomeX;` 重定义 → 改为只替换初始化值
- `demoLook` 函数体替换时 `body.replace(p, 11, ...)` 只替换前 11 字符导致剩余文本重复 → 改为直接在 newFunc 字符串中内嵌完整表达式

### 8. 旧实例冲突
- 启动时枚举进程（`CreateToolhelp32Snapshot`），杀掉其他 `blackhole.exe` 实例（`--render` 子进程除外，由 monitor 管理）

---

## 五、功能改进

### 1. 16 个黑洞预设（原项目仅 8 个）
新增 8 个预设：Crimson Vortex、Azure Spiral、Ruby Ring、Ghost Halo、Top-down Galaxy、White Dwarf Beam、Solar Forge、Obsidian Eye。配置文件版本升级到 `v4`，旧 `v3` 文件会被忽略并写入新默认值。

### 2. 三种播放模式
- 顺序播放（`uPlayMode == 0`）
- 循环播放（`uPlayMode == 1`）
- 随机播放（`uPlayMode == 2`）— 用 hash 函数 `fract(sin(slot*127.1+311.7)*43758.5453)` 选预设

### 3. 预设切换 crossfade
`DEMO_XFADE = 0.65`（占槽位 65% 的时间做平滑过渡），用 `smoothstep(1.0 - DEMO_XFADE, 1.0, fract(raw))` 加权混合相邻预设。

### 4. 每次启动随机化
- 初始位置 `uHomeX/uHomeY` 随机
- 轨迹相位 `uRandPhase` 随机
- 预设起始偏移 `uPresetOffset` 随机

### 5. 双进程架构
- 主进程：配置面板 + 托盘监控（`blackhole.exe`）
- 渲染进程：`blackhole.exe --render`，由监控器空闲时 spawn、活跃时 terminate
- **优势**：每次启动获得全新 GL 上下文 + 全新 DWM 状态，规避 DWM 黄边框缓存问题

### 6. Win32 原生窗口替代 GLFW
`WS_EX_NOACTIVATE | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT | WS_EX_LAYERED` 组合实现：
- 永不抢焦点
- DWM 合成层最高 Z 序
- 鼠标穿透（`WM_NCHITTEST → HTTRANSPARENT`）
- 不出现在任务栏/Alt+Tab

### 7. DWM 防护层
- `DWMWA_BORDER_COLOR = 0` — accent 边框透明
- `DWMNCRP_DISABLED` — 禁用非客户区渲染
- `WS_EX_NOREDIRECTIONBITMAP` — 阻止 DWM 创建重定向表面
- `DwmEnableBlurBehindWindow(FALSE)` + `DwmFlush()` — 清除合成器缓存
- `WM_NCCALCSIZE return 0` — 移除整个非客户区
- `WDA_EXCLUDEFROMCAPTURE` — 排除自身被 WGC 捕获（避免反馈循环）

### 8. 三层视频/游戏检测
- `SHQueryUserNotificationState` — D3D 独占全屏
- 前景窗口尺寸 ≥ 屏幕 + 非 `WS_MAXIMIZE` — 无边框全屏游戏
- 进程名匹配（中英文）：浏览器、视频平台（B站/爱奇艺/优酷/腾讯/抖音/快手等）、本地播放器（VLC/MPV/PotPlayer/MPC）、游戏启动器（Steam/Epic/Ubisoft/EA/Battle.net/Riot/GOG/Xbox）
- UWP 特殊处理：`ApplicationFrameHost.exe` 通过窗口标题关键词识别媒体播放器
- 音频会话峰值检测（仅当进程名匹配后才执行）

### 9. 程序图标嵌入
通过 `windres --output-format=coff` 编译 `resource.rc` 为 `resource.o`，链接进 exe。`Win32GL_Init` 中 `wc.hIcon = LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(100))` 让任务栏/Alt+Tab 显示黑洞图标。

### 10. `-O2` 优化
exe 从 4.7MB 缩小到 1.77MB。

### 11. 开机自启
配置面板可选写入注册表 `HKCU\Software\Microsoft\Windows\CurrentVersion\Run\BlackholeScreensaver`。

---

## 六、删除的文件（相对原项目）

- `claude-token.py` — Claude Code 终端集成脚本，Windows 屏保路径不需要
- `watchdog.ps1` — 早期 PowerShell 守护脚本，已被 `main.cpp` 内置监控取代
- `build_shader.ps1` — SPIR-V 编译脚本，D3D11 路径已禁用
- `release/` — 旧版本备份目录
- `build/CMakeFiles/`、`build/CMakeCache.txt` 等 — CMake 缓存
- 根目录调试日志：`blackhole_debug.log`、`blackhole_debug.txt`、`blackhole_monitor.log`、`build_err.txt`、`build_out.txt`、`debug_log.md`、`debug_state.md`

---

## 七、保留未动的原项目文件

- `blackhole.glsl` — 核心 shader（仅运行时字符串替换，源文件未改）
- `tuner/` — macOS Swift 调参 GUI（macOS 路径仍可用）
- `README.md` — 原项目文档（重写为 Windows 屏保版本）
- `LICENSE` — MIT 许可证
- `demo.gif` — 演示动图
- `presets-grid.png` — 预设网格图
- `WGC_vs_DXGI.md` — 捕获方案对比文档

---

## 八、建议原作者关注的设计点

1. **shader 运行时改 const 的模式**：用字符串替换把 `const float X = V;` 改成 `float X = uniform > 0 ? uniform : V;`，让同一份 shader 兼容"原项目固定值"和"Windows 运行时调参"两种用法 —— 可考虑在上游加 `#ifndef DESKTOP_HOST` 宏开关统一管理
2. **`DEMO_XFADE` crossfade**：`smoothstep(1.0 - DEMO_XFADE, 1.0, fract(raw))` 在多个播放模式间复用，逻辑清晰可借鉴
3. **预设数组 uniform**：`uPresetTemp[16]` 等数组 uniform 替代多次 `glUniform1f` 调用，性能更好
4. **`isWatchingVideo` 自身窗口排除**：用窗口样式组合 + 类名双校验，避免屏保自身被误判为全屏视频 —— 任何"全屏覆盖 + 检测全屏视频"的程序都会遇到这个问题
5. **WGC `IsCursorCaptureEnabled = false`**：相比 `SetSystemCursor` 全局隐藏更安全，不会因崩溃留下永久隐藏的光标
