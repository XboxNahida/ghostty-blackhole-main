# Black Hole / 黑洞桌面特效

[![Release](https://img.shields.io/badge/release-v1.2.1-2ea44f)](https://github.com/XboxNahida/ghostty-blackhole-main/releases/tag/v1.2.1)
[![Platform](https://img.shields.io/badge/platform-Windows%20x64-0078d4)](#设备要求与兼容性)
[![License](https://img.shields.io/badge/license-MIT-blue)](LICENSE)

![Black Hole 演示](demo.gif)

Black Hole 是一款 Windows 桌面黑洞可视化工具。它捕获当前桌面并实时渲染引力透镜、吸积盘和光子环效果，可常驻显示，也可在系统空闲时自动出现。

**当前版本：v1.2.1**

[下载 v1.2.1](https://github.com/XboxNahida/ghostty-blackhole-main/releases/tag/v1.2.1) · [查看全部 Releases](https://github.com/XboxNahida/ghostty-blackhole-main/releases)

## 下载与安装

1. 从 [Releases](https://github.com/XboxNahida/ghostty-blackhole-main/releases) 下载 `BlakholeUI-v1.2.1-windows-x64.zip`。
2. 将 ZIP **完整解压**到一个可写目录。
3. 双击 `appBlakholeUI.exe` 启动界面。

> 不要只复制单个 EXE。Qt DLL、插件、Shader、图片和 `blackhole.exe` Renderer 都是运行所需文件；文件缺失会导致界面、预览或黑洞渲染无法启动。

## 主要功能

- **黑洞实时渲染**：引力透镜、吸积盘、光子环、星空背景及多组外观参数。
- **两种触发模式**：始终显示，或达到设定空闲时间后自动出现；鼠标和键盘恢复活动后自动隐藏。
- **视频与游戏检测**：综合媒体会话、前台窗口、全屏状态和进程特征，减少观看视频或游戏时误触发。
- **三类名单**：始终允许触发、媒体识别提示、前台强制不触发。
- **便捷添加程序**：支持从正在运行的程序选择、浏览 EXE，也保留手动输入。
- **多显示器**：主屏、副屏、跨屏和一屏一黑洞。
- **移动控制**：鼠标跟随、鼠标惯性、范围限制、四角或随机出现位置、自由移动速度。
- **外观配置**：预设管理、播放顺序、固定大小、吸积盘光影及实时预览。
- **更新提醒**：启动后后台检查新版本，在设置入口显示红点，不阻塞主界面。
- **启动诊断**：Renderer 缺文件、初始化失败、提前退出或超时会显示错误详情，可复制信息并打开日志目录。

## 快速开始

1. 启动 `appBlakholeUI.exe`。
2. 在主界面调整外观、显示器和运行模式。
3. 点击“启动”运行黑洞。
4. 使用空闲模式时，等待设定的空闲时间；移动鼠标或按键会隐藏黑洞。
5. 需要完全退出时，在系统托盘中右键程序图标并选择退出。

若首次使用只想确认渲染是否正常，可先选择“始终显示”。确认黑洞能出现后，再切换到空闲检测并配置视频、游戏和名单规则。

## 设备要求与兼容性

以下是根据当前正式包、Qt 平台支持范围和实际代码路径审核后的要求。

| 项目 | 最低要求 / 说明 |
|------|-----------------|
| 操作系统 | 64 位 Windows 10 1809（build 17763）或更高版本 |
| 推荐系统 | Windows 11 22H2（build 22621）或更高版本 |
| 处理器架构 | x86-64；不提供 32 位或 Windows on ARM 原生版本 |
| 图形能力 | OpenGL 3.3，并且能够创建硬件 D3D11 设备 |
| 显卡驱动 | 建议使用 Intel、NVIDIA 或 AMD 提供的较新正式驱动 |

为什么同时需要两套图形能力：

- Qt 实时预览和黑洞 Renderer 都显式使用 **OpenGL 3.3**。
- 桌面画面通过 **WGC 或 DXGI Desktop Duplication** 捕获，两条路径都需要 D3D11 硬件设备。
- Windows 11 22H2+ 的自动模式优先使用可抑制黄色捕获边框的 WGC。
- Windows 10 和较早的 Windows 11 在自动模式下通常回退到 DXGI；也可以手动选择 WGC，但可能出现黄色捕获边框。

显卡型号或发布时间不能单独证明兼容。双显卡、远程桌面、虚拟机、特殊显卡驱动，以及同时运行的录屏/远控软件都可能影响桌面捕获。项目目前没有覆盖所有硬件组合，因此不提供“某系列显卡完全兼容”的保证。

## 视频、游戏与名单

视频检测会综合 Windows 媒体会话、前台窗口和进程信息，内置常用浏览器、本地播放器及哔哩哔哩、腾讯视频、爱奇艺、优酷等常见客户端的识别提示。

- 媒体处于播放状态时可阻止空闲黑洞触发；暂停后允许触发。
- 开启“播放视频时视为空闲”后，即使检测到播放也允许黑洞触发。
- 无声视频、未向 Windows 上报媒体状态的客户端或特殊播放方式可能漏检。
- 游戏检测覆盖独占全屏、无边框和部分窗口化游戏；低负载、特殊渲染方式或不在前台的游戏可能漏检。

检测不到时，可以在“空闲检测名单”中选择目标程序：

| 名单 | 行为 |
|------|------|
| 始终允许触发 | 目标在前台时忽略媒体和游戏检测，允许黑洞触发 |
| 媒体识别提示 | 仅在目标实际播放且检测到媒体信号时阻止触发 |
| 前台强制不触发 | 目标在前台时无条件阻止；切到后台后失效 |

每类名单都支持“运行程序选择”“浏览可执行文件”和“手动输入”。自动检测采用偏保守策略，少数漏检程序由用户名单补充，避免普通后台进程长期误阻止黑洞。

## 常见问题

### 启动后没有黑洞

1. 确认 ZIP 已完整解压，`appBlakholeUI.exe` 与 `blackhole.exe` 位于同一目录，`shaders`、Qt DLL 和插件目录没有缺失。
2. 先切换到“始终显示”并点击启动，排除尚未达到空闲时间的情况。
3. 如果程序弹出 Renderer 启动错误，点击“复制错误详情”并打开日志目录。
4. 更新显卡驱动后重试；不要从压缩包内直接运行 EXE。

### 日志出现 `DXGI_Init primary failed`

这表示桌面捕获初始化失败。窗口创建和 OpenGL 初始化可能已经成功，问题通常位于 D3D11/DXGI Desktop Duplication 环境。

按顺序尝试：

1. 关闭 OBS、录屏工具、远程控制软件和动态壁纸后重试。
2. 更新显卡驱动。双显卡电脑可在 Windows“图形设置”中让 `appBlakholeUI.exe` 和 `blackhole.exe` 使用同一块 GPU。
3. 在高级设置中手动选择 WGC。Windows 10 上可能出现黄色捕获边框。
4. 退出远程桌面或虚拟机，在本机物理桌面会话中测试。

需要补充 DXGI 详细错误码时，在解压目录打开 PowerShell：

```powershell
.\blackhole.exe --render 2> dxgi_error.txt
```

### Windows 或杀毒软件报毒

当前 EXE **没有商业代码签名**。程序还会进行桌面捕获、全屏置顶、进程检测、开机自启动配置和联网更新检查，这些行为可能触发 SmartScreen 或杀毒软件的启发式检测。

- 只从本仓库的 [Releases](https://github.com/XboxNahida/ghostty-blackhole-main/releases) 下载。
- 优先核对 Release 发布说明或校验文件中的 SHA-256。
- 不建议关闭杀毒软件。确认下载来源和校验值后，可按安全软件提示单独放行；无法放行时请停止运行。
- 反馈误报时请提供安全软件名称、检测名称和文件 SHA-256，便于提交误报样本。

### 如何反馈问题

请在 [GitHub Issues](https://github.com/XboxNahida/ghostty-blackhole-main/issues) 提交以下信息：

- `blackhole_debug.txt`。
- 如果是 DXGI 问题，再附上 `dxgi_error.txt`。
- Windows 版本和 build 号。
- CPU、显卡型号，以及是否为双显卡电脑。
- 是否使用远程桌面或虚拟机。
- 是否同时运行录屏、远控或动态壁纸软件。
- Renderer 错误弹窗中复制的完整详情。

日志可能包含本机目录路径或正在识别的程序名，公开上传前请自行检查隐私信息。

## 更新机制

当前版本为 **v1.2.1**。程序每次启动会在后台检查 GitHub Release，不阻塞启动流程：

- 有新版本时，设置入口显示红点。
- 只有用户点击“检查更新”后才弹出更新窗口。
- 点击下载会在默认浏览器中打开 Release 链接。
- 忽略某个版本后会记住选择；下一个新版本仍会重新提醒。

## 开发者构建

构建要求：CMake 3.20+、支持 C++17 的 MinGW-w64 UCRT64、Qt 6.8+、GLFW3 和 OpenGL 开发库。当前正式包使用 Qt 6.11.1 x64 构建。

```powershell
# Renderer
cmake -S . -B build -G "MinGW Makefiles"
cmake --build build --config Release --clean-first

# Qt UI：按本机 Qt 安装路径调整 CMAKE_PREFIX_PATH
cmake -S Blakhole_UI `
  -B Blakhole_UI/build/Desktop_Qt_6_11_1_MinGW_64_bit-Release `
  -G "MinGW Makefiles" `
  -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_PREFIX_PATH=C:/Qt/6.11.1/mingw_64
cmake --build Blakhole_UI/build/Desktop_Qt_6_11_1_MinGW_64_bit-Release `
  --config Release --clean-first

# 从已完成的构建生成 release 目录
.\package_release.ps1 -NoBuild
```

更深入的捕获、双进程和渲染实验记录见 [技术文档](Doc/TECHNICAL.md)。该文档包含历史方案，当前发布行为以代码和本 README 为准。

## 项目结构

| 目录 | 用途 |
|------|------|
| `src/` | `blackhole.exe` Renderer、桌面捕获和原生 Windows 逻辑 |
| `Blakhole_UI/` | Qt 6 / QML 配置界面、实时预览、名单和启动诊断 |
| `shaders/` | 桌面与预览使用的 GLSL/HLSL Shader |
| `Doc/` | 架构、测试和历史技术文档 |
| `release/` | 本地发布目录，包含 EXE、运行库、插件和资源 |

## 灵感来源

[Eric Bruneton's black hole shader](https://github.com/ebruneton/black_hole_shader)（BSD-3-Clause）

## License

本项目使用 [MIT License](LICENSE)。
