# Ubuntu 迁移计划（审查稿）

- 状态：v2 可执行规格，待审查，尚未进入实现阶段
- 目标分支：`port/ubuntu-wayland`
- 唯一目标/验收平台：当前本机 Ubuntu 26.04、GNOME、Wayland、x86_64、NVIDIA RTX 3060
- 上游基线：`origin/main@1c570a8`
- 执行方式：DeepSeek 逐任务包实现，Codex 逐包验收与最终收尾

## 1. 目标与边界

### 1.1 第一版目标

- 在 Ubuntu GNOME Wayland 会话中原生编译、安装和启动。
- 保留现有 Qt 6/QML 配置界面、预设管理和黑洞 GLSL 效果。
- 支持全屏黑洞渲染、多显示器选择、键鼠活动后退出。
- 支持按系统空闲时间自动启动和恢复活动后停止。
- 支持用户级自启动、托盘控制和配置持久化。
- 生成可重复构建的安装产物，优先提供 `.deb`，同时保留 CMake 安装方式。

### 1.2 分阶段能力

桌面实时捕获是 Wayland 下风险最高的部分，按以下顺序交付：

1. **无桌面捕获模式**：先以星空/程序生成背景运行黑洞，完成原生窗口、渲染和空闲触发闭环。
2. **Portal 捕获模式**：接入 XDG Desktop Portal + PipeWire，允许用户授权捕获屏幕作为黑洞背景。
3. **桌面融合体验**：处理多屏、反馈回路、授权持久性以及 GNOME 下的窗口层级限制。

### 1.3 第一版不承诺

- 不通过 Wine 运行 Windows 可执行文件。
- 不移植 D3D11/HLSL 实验渲染路径。
- 不绕过 Wayland/GNOME 的屏幕捕获授权或窗口安全策略。
- 不保证实现 Windows `TOPMOST + 鼠标穿透 + 不抢焦点` 的完全等价行为；以 GNOME Wayland 实际允许的能力为准。
- 不修改 GNOME 锁屏界面本身；第一版是用户会话内的空闲视觉程序，不是登录/锁屏认证插件。
- 不承诺 Windows 可配置、可编译、可运行或行为回归；Windows 不是本迁移项目的交付平台。

### 1.4 平台验收范围（优先级最高）

- 本计划从现在起只要求项目在上述本机 Ubuntu/Linux 环境构建、测试和运行通过。
- DeepSeek 和 Codex 不得为了 Windows、MinGW、MSVC、WGL、WGC、D3D11 或 Windows CI 兼容增加实现、测试或验收工作。
- 现有 Windows 源码可以保留作为历史参考，但不构成兼容性约束；Linux 实现需要时可以调整共享代码，不必证明 Windows 行为不变。
- 发生平台取舍时直接选择更简单、可靠、适合本机 Ubuntu Wayland 的方案，无需增加跨平台抽象。
- 本节覆盖本文其他章节中遗留的“保留 Windows 支持”“Windows 行为不变”“双平台构建”等旧要求。

## 2. 已确认的可行性

- 当前显卡提供 OpenGL 4.6 / GLSL 4.60，项目 shader 只要求 GLSL 3.30。
- GCC 15、CMake 4.2、Ninja、GLFW 3.4、Qt 6.10 和 PipeWire 1.6 开发环境已就绪。
- `blackhole.glsl`、预览 shader、预设格式以及 Qt/QML 主要界面可复用。
- 当前根构建脚本声明 `RC` 语言并无条件链接 D3D11、DXGI 和 Win32 系统库，不能直接在 Linux 配置。
- 渲染器、捕获、窗口、空闲检测和系统集成之间已有模块边界，可复用其中适合 Linux 的部分。

## 3. 总体设计

### 3.1 平台分层

建立明确的平台接口，避免在业务代码中散布大量条件编译：

| 能力 | Windows 实现 | Ubuntu 实现 |
|---|---|---|
| OpenGL 窗口/输入 | Win32 + WGL | Qt 6 Window/FBO，必要时 GLFW |
| 桌面捕获 | WGC / DXGI | XDG Desktop Portal + PipeWire |
| 显示器枚举 | Win32 Monitor API | Qt `QScreen` |
| 空闲时间 | Win32 API | Mutter IdleMonitor D-Bus；准备后备实现 |
| 前台/全屏应用检测 | Win32 窗口 API | 首版降级；后续按 GNOME 可用接口实现 |
| 媒体播放状态 | Windows Audio Session | MPRIS D-Bus |
| 自启动 | 注册表 | XDG autostart `.desktop` |
| 托盘 | Qt System Tray | Qt + AppIndicator/StatusNotifier 可用性验证 |
| 配置目录 | 可执行文件旁 | `QStandardPaths::AppConfigLocation` |

### 3.2 进程结构

保留双进程思路：

- `appBlakholeUI`：配置、托盘、空闲监控和渲染器生命周期管理。
- `blackhole-renderer`：只负责全屏窗口、背景输入和 shader 渲染。

Linux 端使用稳定的可执行文件名，不再搜索 `.exe`；通过命令行参数传入配置路径、显示器和捕获模式。

### 3.3 Wayland 约束

- Portal 捕获首次使用通常会显示系统选屏授权框，不能静默绕过。
- 普通 Wayland 客户端不能任意读取全局键鼠或控制绝对窗口层级。
- 捕获自身窗口可能产生反馈回路；优先让 Portal 选择显示器，并验证 GNOME 是否能排除渲染窗口。
- 如果 GNOME Wayland 无法满足桌面覆盖层语义，保留以下降级路径：
  - 普通全屏屏保窗口；
  - 无桌面捕获的程序背景；
  - 可选 Xorg 会话增强模式，但不作为默认方案。

## 4. 实施阶段

### 阶段 0：仓库与基线

- 恢复/确认有效 Git 上游和历史。
- 从默认分支创建 `port/ubuntu-wayland`。
- 添加 Linux CI 或至少提供无界面 CMake 配置与单元测试任务。

退出条件：分支存在；工作树干净；Linux 目标方案通过审查。

### 阶段 1：跨平台构建骨架

- 根 `project()` 在非 Windows 平台不启用 `RC`。
- 用 `if(WIN32)` 隔离 Win32/D3D/WGC/DXGI 源文件和链接库。
- 建立 `blackhole-renderer` Linux 目标。
- 统一 shader、QML 和图标的安装路径。
- 引入 CMake 选项：`BLACKHOLE_BUILD_UI`、`BLACKHOLE_ENABLE_PORTAL_CAPTURE`、`BLACKHOLE_BUILD_TESTS`。

退出条件：Ubuntu 上 CMake 配置和编译通过；现有纯逻辑测试通过。

### 阶段 2：Linux 原生渲染最小闭环

- 创建 OpenGL 3.3 Core 上下文和全屏窗口。
- 复用现有顶点/片段 shader、uniform 和预设数据。
- 使用程序生成背景完成渲染，不依赖桌面捕获。
- 实现 Esc、键盘、鼠标活动退出和多显示器选择。
- 处理 DPI、不同刷新率、窗口缩放和 GPU 资源释放。

退出条件：RTX 3060 上连续运行 30 分钟无崩溃、明显泄漏或 shader 错误；可在每个显示器运行。

### 阶段 3：Qt/QML UI Linux 化

- 修正 `.exe` 路径搜索、`QProcess` 启停和 Windows 资源引用。
- 把配置写入用户配置目录，并提供旧配置导入。
- 保留实时 FBO 预览、14 个参数和预设列表。
- 替换注册表自启动；验证 GNOME 托盘表现。
- 将 Windows 专属选项在 Linux UI 中隐藏或替换为对应能力。

退出条件：UI 能配置、启动、停止渲染器，重启后配置保持一致。

### 阶段 4：空闲触发与媒体抑制

- 通过 GNOME/Mutter IdleMonitor D-Bus 获取空闲状态。
- 监听恢复活动并立即关闭渲染器。
- 使用 MPRIS 判断活动媒体播放器，替代 Windows 音频会话的主要用途。
- 对接口缺失或 GNOME 版本变化提供清晰诊断和保守后备行为。

退出条件：设定短超时后可重复触发；键鼠恢复延迟不超过 1 秒；播放媒体时按配置抑制触发。

### 阶段 5：Wayland 桌面捕获

- 实现 ScreenCast Portal 会话、源选择和 PipeWire 流接收。
- 将 DMA-BUF 或视频帧转换为 OpenGL 可采样纹理；先实现可靠路径，再优化零拷贝。
- 处理分辨率变化、显示器热插拔、Portal 会话结束和权限拒绝。
- 避免捕获反馈；无法可靠避免时自动回退至程序背景并提示原因。
- 明确授权行为，不保存或伪造用户未授予的权限。

退出条件：授权后桌面画面持续更新；切换窗口、显示器和分辨率不冻结；拒绝授权时正常降级。

### 阶段 6：安装、打包与回归

- 增加 `cmake --install` 规则、`.desktop`、图标和用户自启动文件。
- 制作 Ubuntu 26.04 amd64 `.deb`。
- 检查运行时依赖，避免携带不必要的开发库。
- 完成 NVIDIA Wayland、多屏、睡眠唤醒、锁屏/解锁和重复启停测试。
- 更新 README，说明 Ubuntu 的构建、权限和已知限制。

退出条件：在干净的 Ubuntu 26.04 环境安装包后可启动、卸载干净，核心验收项全部通过。

## 5. 测试计划

### 5.1 自动化

- 保留并扩展预设、移动参数、应用列表和更新状态单元测试。
- 为平台接口提供 mock，测试空闲状态机和渲染器进程生命周期。
- 测试配置迁移、损坏配置恢复和路径中含中文/空格的情况。
- CMake 至少覆盖 `portal on/off`、`UI on/off` 组合。

### 5.2 手工验收

- GNOME Wayland 单屏及多屏。
- 100%/125%/200% 缩放和不同刷新率。
- Portal 允许、拒绝、取消、会话中断四种路径。
- 键鼠恢复、媒体播放、睡眠唤醒、锁屏解锁。
- 连续运行 2 小时的显存、内存、CPU/GPU 占用和画面稳定性。
- 自启动开启/关闭、安装升级和完整卸载。

## 6. 主要风险与决策点

| 风险 | 影响 | 处理方式 |
|---|---|---|
| Wayland 不允许 Windows 式桌面覆盖层 | 体验差异 | 第一版定义为普通全屏屏保窗口，验证后再增强 |
| Portal 每次授权或选屏 | 无法完全自动触发 | 验证 restore token；始终尊重系统授权模型 |
| 捕获自身导致反馈 | 画面递归 | 排除/延迟显示/程序背景降级三层策略 |
| Mutter IdleMonitor 接口变化 | 空闲触发失效 | D-Bus 能力探测、诊断信息和后备策略 |
| Qt 托盘在 GNOME 中表现不一致 | 控制入口缺失 | 验证 AppIndicator；保留主窗口控制入口 |

开始实现前需要审查确认的产品决策：

1. 第一版是否接受“普通全屏屏保窗口”，而不是 GNOME 锁屏背景插件。
2. 第一版是否以程序生成背景为必达项，将桌面实时捕获放在后续阶段。
3. 发布格式是否以 `.deb` 为主，暂不引入 Flatpak（Portal 适配稳定后再评估）。

## 7. 预计改动范围

预计复用：

- `blackhole.glsl` 与 `shaders/*.glsl`
- `Blakhole_UI` 的主要 QML 页面和组件
- 预设数据模型、移动参数和部分纯逻辑测试
- 双进程总体结构

预计新增或重写：

- Linux 平台抽象与渲染器入口
- Portal/PipeWire 捕获后端
- Linux 空闲、媒体、多屏和自启动实现
- Linux CMake、安装规则与打包文件
- Linux 集成测试和用户文档

计划审查通过前，不进行上述实现性改动。

## 8. DeepSeek 执行协议

本节是实施约束，不是建议。DeepSeek 实现时必须遵守。

### 8.1 工作方式

1. 只在 `port/ubuntu-wayland` 分支工作，每次开始先执行：

   ```bash
   git branch --show-current
   git status --short
   git log -1 --oneline
   ```

2. 一次只处理一个 `DS-xx` 任务包。不允许“顺手”进入下一包。
3. 每包最多一个主题提交；如必须分开，只能按“重构”和“功能”分两个提交。
4. 每包完成后停止，等待 Codex 验收；验收修复不得与下一包混在一起。
5. 不推送远端、不开 PR、不发 Release，除非用户另行明确授权。

### 8.2 禁止事项

- 不得把 Linux 端继续塞进现有 1854 行 `src/main.cpp` 并堆叠大量 `#ifdef`。
- 不要求维护或验证 WGC、DXGI、D3D11、Win32 原实现；不要为这些路径增加兼容工作。
- 不得为了“编译通过”注释掉 shader uniform、预设、Bloom 或错误检查。
- 不得用 `system()`、shell 字符串或自行拼接命令启动子进程；Qt UI 必须使用 `QProcess` 的 program/arguments 分离形式。
- 不得直接连接 Mutter 私有 ScreenCast API 取代 Portal；捕获必须走 XDG Desktop Portal。
- 不得绕过屏幕捕获授权、伪造 restore token 或默认捕获未经用户选择的屏幕。
- 不得引入 SDL、Electron、GStreamer 或新的 GUI 框架。如认为必须新增依赖，先停止并说明理由。
- 不得修改 shader 视觉算法，除非修复经证明的 Linux GLSL 兼容问题。
- 不得在仓库内提交 `build/`、`.deb`、日志、Portal token、屏幕录像或用户配置。
- 不得做全仓格式化或无关重命名。

### 8.3 每包交付报告格式

DeepSeek 停止前必须报告：

```text
任务包：DS-xx
提交：<commit hash> <subject>
改动文件：<逐项列出>
实际执行的命令：<逐条列出>
测试结果：<通过/失败及关键输出>
未解决问题：<没有则写“无”>
手工验证步骤：<Codex 可直接复现>
```

## 9. 目标代码结构和接口

### 9.1 预期目录

文件名可在不改变职责的前提下小幅调整，但不得把所有 Linux 逻辑合并成一个文件。

```text
src/
├── render/
│   ├── shader_source.h/.cpp       # 文件读取、fragment shader 组装
│   ├── gl_program.h/.cpp          # shader 编译、链接、uniform 定位
│   ├── blackhole_renderer.h/.cpp  # VAO/VBO、uniform 更新、Bloom、绘制
│   └── frame.h                     # CPU 帧的格式、尺寸和 stride
├── platform/
│   ├── desktop_capture.h          # 平台无关捕获接口
│   ├── idle_source.h              # 空闲/活动与锁屏事件接口
│   └── media_activity.h           # 媒体活动判断接口
├── linux/
│   ├── main_linux.cpp
│   ├── glfw_window_linux.h/.cpp
│   ├── portal_capture.h/.cpp
│   ├── pipewire_stream.h/.cpp
│   ├── mutter_idle_source.h/.cpp
│   └── mpris_media_activity.h/.cpp
└── windows/                     # 可选：后续再移动现有 Windows 文件
Blakhole_UI/core/platform/
├── renderer_locator.h/.cpp
├── autostart_backend.h
├── autostart_windows.cpp
└── autostart_linux.cpp
packaging/linux/
├── io.github.xboxnahida.Blakhole.desktop
├── io.github.xboxnahida.Blakhole.metainfo.xml
└── icons/
```

### 9.2 捕获帧契约

Linux 端的第一个可靠版本统一向渲染器交付 CPU 帧，不先做 DMA-BUF 零拷贝。建议接口：

```cpp
enum class PixelFormat { Bgra8888, Rgba8888, Bgrx8888 };

struct DesktopFrame {
    const std::byte *data = nullptr;
    int width = 0;
    int height = 0;
    int strideBytes = 0;
    PixelFormat format = PixelFormat::Bgra8888;
    std::uint64_t sequence = 0;
};

class DesktopCapture {
public:
    virtual ~DesktopCapture() = default;
    virtual bool start() = 0;
    virtual bool poll(DesktopFrame &frame) = 0; // 无新帧不是错误
    virtual void stop() noexcept = 0;
    virtual std::string lastError() const = 0;
};
```

约束：

- `DesktopFrame::data` 只在当次 `poll()` 返回后、下一次 PipeWire dequeue/requeue 前有效。
- 不假定 `strideBytes == width * 4`。
- 必须支持帧尺寸运行时改变，改变时调用 `GLTex_Resize()`。
- 首版只接受 4 字节 RGB 格式；遇到不支持格式要返回可见错误，不能黑屏静默失败。
- PipeWire 回调不得执行 OpenGL 调用；用有界双缓冲或最新帧槽在采集线程与渲染线程之间传递。

### 9.3 渲染核心契约

`BlackholeRenderer` 只依赖已激活的 OpenGL 3.3 Core 上下文，不知道 Win32、GLFW、Portal 或 PipeWire。

```cpp
struct RenderInput {
    int framebufferWidth = 0;
    int framebufferHeight = 0;
    double elapsedSeconds = 0.0;
    float pointerX = 0.5f;
    float pointerY = 0.5f;
    const BlackholeConfig *config = nullptr;
    const DesktopFrame *desktopFrame = nullptr; // nullptr = 程序生成背景
};

class BlackholeRenderer {
public:
    bool initialize(const std::filesystem::path &resourceRoot, FILE *log);
    bool resize(int width, int height);
    bool render(const RenderInput &input);
    void shutdown() noexcept;
};
```

渲染器必须保留现有：

- `buildFragmentShader()` 的 shader 组装和替换行为。
- 14 组预设 uniform 数组和高级 override uniform。
- 随机出生点、鼠标跟随、移动速度、生长和 Bloom。
- shader 编译/链接日志，任何失败都必须以非 0 状态退出。

### 9.4 命令行契约

Linux 渲染器不复制 Windows 的 `--monitor` 和旧 ImGui 配置入口。Qt UI 是唯一控制端。

```text
blackhole-renderer --render
                   --config <absolute-path>
                   --resources <absolute-path>
                   [--screen <qt-screen-name-or-index>]
                   [--background generated|portal]
                   [--windowed]
                   [--diagnostic]
```

- 参数错误：退出码 `2`。
- 初始化失败：退出码 `1`。
- 用户键鼠活动正常退出：退出码 `0`。
- `--diagnostic` 必须输出平台、会话类型、GL vendor/renderer/version、shader 路径和捕获后端状态。

### 9.5 资源与用户数据契约

- 安装资源：`${CMAKE_INSTALL_DATADIR}/blackhole/`。
- 用户配置：`QStandardPaths::AppConfigLocation`，实际预期为 `~/.config/XboxNahida/Blakhole UI/`。
- 用户数据/头像：`QStandardPaths::AppDataLocation`。
- 日志：`QStandardPaths::AppLocalDataLocation/logs/`，日志不得写到安装目录。
- 开发树运行时可回退搜索源码资源；安装版不得依赖当前工作目录。
- 首次启动如发现可执行文件旁的旧 `blackhole_presets.txt` / `blackhole_advanced.txt`，只导入一次，不删除原文件。

## 10. 可分配任务包

### DS-00：基线记录（不改功能）

目标：让后续回归有可比较证据。

允许改动：

- 新增 `Doc/LINUX_BASELINE.md`。
- 必要时增加不影响构建的检查脚本。

必须记录：

- OS、GNOME、Wayland、Qt、GCC、CMake、NVIDIA 和 OpenGL 版本。
- `cmake -S .` 当前因 RC 语言失败的关键输出。
- 当前纯逻辑测试及其在 Linux 上的可编译性。
- Windows 专属源文件/链接库列表。

验收：只有文档或检查脚本变化，不应有产品代码变化。

### DS-01：CMake 平台拆分

目标：Linux 可以完成 CMake configure，Windows 源文件不进入 Linux 目标。

必须改动：

- 根 `project()` 仅在 `WIN32` 时启用 `RC`。
- 使用显式源文件列表，不使用 `file(GLOB src/*.cpp)`。
- Windows 目标保留名称 `blackhole`；Linux 新目标名称 `blackhole-renderer`。
- 根工程使用 `add_subdirectory(Blakhole_UI)`，不再要求用户单独配置 UI 目录。
- 增加并给出默认值：
  - `BLACKHOLE_BUILD_UI=ON`
  - `BLACKHOLE_BUILD_RENDERER=ON`
  - `BLACKHOLE_ENABLE_PORTAL_CAPTURE=OFF`（到 DS-07 再默认开）
  - `BUILD_TESTING=ON`
- Linux UI 链接 `Qt6::Quick Qt6::Widgets Qt6::OpenGL Qt6::Network Qt6::DBus`。
- Linux 渲染目标链接 `glfw` 和 `OpenGL::GL`。

不做：本包不要制造假 `main_linux.cpp` 以外的大量占位实现，不要引入 PipeWire。

验收命令：

```bash
cmake -S . -B build-linux -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DBLACKHOLE_ENABLE_PORTAL_CAPTURE=OFF \
  -DBUILD_TESTING=ON
cmake --build build-linux --target help
```

验收点：配置输出不能查找 RC、D3D11、DXGI、`advapi32`、`dwmapi`、`runtimeobject` 或 `version`。

### DS-02：抽取可测试的渲染核心

目标：抽出可供 Linux 渲染器复用和测试的 shader 与绘制逻辑；只验收 Linux 行为。

实施顺序：

1. 先抽 `readFile`、`buildFragmentShader`、shader compile/link。
2. 再抽 VAO/VBO、uniform location 缓存和每帧 uniform 更新。
3. 最后把 Bloom 调用放入 `BlackholeRenderer`。
4. Linux 渲染入口调用共享渲染核心；Windows 入口不属于验收范围。

必须增加测试：

- shader 资源缺失时失败。
- shader body 与 header 组装后包含唯一 `#version 330`。
- 现有关键字符串替换成功；替换锚点不存在时必须失败并给出错误。
- 预设数量被限制在 `[1, 64]`。

验收：编译器警告不得因抽取明显增加；shader 视觉代码 diff 应为零。

### DS-03：Linux 程序背景渲染 MVP

目标：在不捕获桌面的情况下，Ubuntu Wayland 原生显示黑洞。

必须实现：

- GLFW 请求 OpenGL 3.3 Core，记录实际 GL vendor/renderer/version。
- `--windowed` 使用 1280x720 可缩放窗口，用于开发和自动化验证。
- 默认使用指定显示器全屏窗口。
- `--background generated` 绑定 1x1 黑色纹理或明确的程序背景，不伪装已捕获桌面。
- Esc、任意按键、鼠标按键立即退出。鼠标移动要有 8–12 px 阈值，防止窗初始化事件误退出。
- 处理 framebuffer size 而不是只使用逻辑窗口尺寸。
- 任何初始化失败都清理已创建的 GL/GLFW 资源。

手工验收：

```bash
./build-linux/blackhole-renderer --render \
  --windowed --background generated \
  --config "$PWD/blackhole_presets.txt" \
  --resources "$PWD" --diagnostic
```

验收点：黑洞可见、动画连续、窗口缩放正常、输入退出码为 0、shader 错误退出码非 0。

### DS-04：Qt/QML UI 的 Linux 可编译性

目标：UI 在 Linux 编译并打开，实时预览可用。

必须处理的 Windows 点：

- `.rc`、`.ico` 资源编译仅限 Windows；Linux 使用 PNG/SVG 图标或 Qt resource。
- `application_catalog`、全局热键、自启动和 DWM 外观分支。
- `version` 链接库仅限 Windows。
- `Blakhole_UI/main.cpp` 中托盘关联和 `launchMinimized()` 不能整块只在 `Q_OS_WIN` 下；只把 DWM 调用留在 Windows 分支。
- 保留 `QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL)`，因现有 `QQuickFramebufferObject` 预览依赖 OpenGL RHI。

验收：

```bash
cmake --build build-linux --target appBlakholeUI
QT_FATAL_WARNINGS=0 ./build-linux/Blakhole_UI/appBlakholeUI
ctest --test-dir build-linux --output-on-failure
```

不得通过删除 QML 页面或禁用预览来达成验收。

### DS-05：配置、渲染器定位与进程生命周期

目标：用 UI 启动/停止 Linux 渲染器，不再依赖 `.exe` 或当前工作目录。

渲染器搜索顺序：

1. 测试注入的显式路径。
2. UI 可执行文件同目录的 `blackhole-renderer`。
3. 开发树根目录已知 build 位置。
4. `QStandardPaths::findExecutable("blackhole-renderer")`。

`QProcess` 必须：

- program 使用绝对路径。
- arguments 使用 `QStringList`。
- 分开 stdout/stderr 并保存有界日志，不允许无限内存累积。
- 先 `terminate()`，3 秒后仍未退出才 `kill()`。
- UI 退出时不留孤儿渲染器。

必须增加测试：搜索顺序、含空格/中文路径、启动失败诊断、异常退出状态机。

### DS-06：GNOME 空闲、活动和锁屏

目标：实现“空闲启动，恢复活动停止，锁屏期间不运行”。

已在目标机器验证的 D-Bus 契约：

```text
bus:       session
service:   org.gnome.Mutter.IdleMonitor
path:      /org/gnome/Mutter/IdleMonitor/Core
interface: org.gnome.Mutter.IdleMonitor
methods:   AddIdleWatch(t milliseconds) -> u watch_id
           AddUserActiveWatch() -> u watch_id
           GetIdletime() -> t milliseconds
           RemoveWatch(u watch_id)
signal:    WatchFired(u watch_id)

service:   org.gnome.ScreenSaver
path:      /org/gnome/ScreenSaver
interface: org.gnome.ScreenSaver
method:    GetActive() -> b
signal:    ActiveChanged(b active)
```

状态机：

```text
Active --IdleWatch--> IdleEligible --policy allows--> RendererRunning
RendererRunning --UserActiveWatch--> Active + stop renderer
Any --ScreenSaver active--> Locked + stop renderer
Locked --ScreenSaver inactive--> Active + re-arm watches
Any --D-Bus service lost--> Degraded + stop renderer + visible diagnostic
```

约束：

- 每次修改空闲时长都要 `RemoveWatch` 旧 ID 后重新注册。
- 不用 100 ms 轮询；事件是主，`GetIdletime()` 只用于初始同步/恢复。
- 锁屏优先级高于“始终显示”。
- D-Bus 失败时 fail closed：不自动启动渲染器。

必须使用 fake D-Bus/backend 对状态机做单元测试，手工测试不能代替。

### DS-07：MPRIS 媒体抑制

目标：用 MPRIS 替代 Windows Audio Session 的主要用户体验。

实现规则：

- 枚举 session bus 上 `org.mpris.MediaPlayer2.*` 服务。
- 读取 `/org/mpris/MediaPlayer2` 上 `org.mpris.MediaPlayer2.Player.PlaybackStatus`。
- `Playing` 表示活动媒体；`Paused`/`Stopped` 不抑制屏保。
- 单个播放器超时、失效或不规范不得阻塞整个空闲状态机。
- `videoAsIdle=true` 时忽略 MPRIS Playing；为 `false` 时 Playing 阻止启动并停止已运行渲染器。
- 不声称可以无权限判断 Wayland 前台窗口。白名单/黑名单功能在 Linux 上应明确标注“有限支持”或暂时禁用。

测试：用 fake MPRIS service 覆盖 Playing/Paused/Stopped、服务消失、超时和多播放器。

### DS-08：XDG Portal + PipeWire 桌面捕获

目标：用户授权后获得桌面帧，为黑洞提供引力透镜背景。

已在目标机器验证：

```text
Portal ScreenCast version: 5
AvailableSourceTypes: 7
AvailableCursorModes: 7
methods:
  CreateSession(a{sv}) -> request object
  SelectSources(session, a{sv}) -> request object
  Start(session, parent_window, a{sv}) -> request object
  OpenPipeWireRemote(session, a{sv}) -> fd
```

严格调用顺序：

1. 为请求生成不可预测、仅含安全字符的 `handle_token` 和 `session_handle_token`。
2. 调用 `CreateSession`，监听返回 Request 对象的 `org.freedesktop.portal.Request::Response`。
3. 从 Response 结果取 session handle，不猜测对象路径。
4. `SelectSources`：首版 `types=MONITOR`、`multiple=false`，cursor 策略与 UI 设置一致。
5. 如 Portal 版本支持，传 `persist_mode=2`；仅在 Portal 成功返回时保存 `restore_token`。
6. `Start`；处理 Response code `0=success`、`1=cancelled`、`2=other error`。
7. 从 Start 结果读取 `streams`，取 PipeWire node id 和 size 等属性。
8. `OpenPipeWireRemote`获得 fd；明确 fd 所有权，只关闭一次。
9. 用 `pw_context_connect_fd()` 或当前 PipeWire 等价 API 连接，建立 stream 并协商 4 字节 RGB 格式。
10. 关闭时顺序：停 stream → 释放 PipeWire 对象 → 关 fd → `Session.Close`。

线程与缓冲：

- Portal D-Bus 在 Qt 主线程异步运行，不使用无限期 `waitForFinished()`。
- PipeWire 在专用 thread loop 运行。
- process 回调只复制至有界最新帧缓冲并尽快 requeue。
- 渲染线程只取最新完整帧；允许丢帧，不允许撤裂、越界或等待 PipeWire 阻塞渲染。

失败策略：

- 用户取消/拒绝：本次不再弹窗，回退 generated 背景并向 UI 报告。
- Portal 或 PipeWire 中途断开：停捕获、回退 generated，不让渲染器崩溃。
- 格式/尺寸变化：原子替换帧存储并调整 GL 纹理。
- 不可自动无限重试授权对话。

必须测试：Portal 成功/取消/错误、token 有/无、fd 关闭一次、stride 大于 width*4、尺寸变化、连续无新帧、中途断开。

手工验收：在 GNOME Wayland 下实际选择屏幕，切换窗口 10 次，连续运行 30 分钟，验证没有画面冻结、递归反馈和内存持续增长。

### DS-09：多屏、托盘和自启动

目标：完成 GNOME 用户体验整合。

多屏：

- UI 通过 `QGuiApplication::screens()` 获取屏幕，保存稳定名称并对索引做后备。
- Wayland 下不假定可以将一个窗口跨越任意全局坐标。
- “一屏一黑洞”使用每屏一个渲染器进程；UI 统一跟踪和停止所有子进程。
- Portal `multiple=false` 的首版只捕获一块用户选择的屏幕；多屏捕获不与窗布局同包实现。

托盘：

- 检测 `QSystemTrayIcon::isSystemTrayAvailable()`。
- 不可用托盘作为唯一退出通道；主窗必须保留停止和退出。
- GNOME 不可用时显示非致命诊断，不得阻止启动。

自启动：

- 写入 `~/.config/autostart/io.github.xboxnahida.Blakhole.desktop`。
- `Exec=` 使用安装后 UI 绝对路径和 `--minimized`，正确转义。
- 写入使用 `QSaveFile` 原子替换；禁用时只删除本应用自己创建的文件。

### DS-10：安装、`.deb` 和文档

目标：形成可重复安装的 Ubuntu 26.04 amd64 产物。

必须安装：

- UI 和 `blackhole-renderer` 到 `${CMAKE_INSTALL_BINDIR}`。
- shader 和 QML 需要的运行资源到 `${CMAKE_INSTALL_DATADIR}/blackhole`。
- `.desktop`、metainfo 和 hicolor 图标。
- 不安装测试、Windows DLL、`.pdb`、`.rc` 或源码树路径。

打包可使用 CPack DEB，但必须显式声明运行时依赖，不把 Qt/PipeWire 开发包写入 Depends。

验收命令：

```bash
cmake --build build-linux
ctest --test-dir build-linux --output-on-failure
DESTDIR="$PWD/stage" cmake --install build-linux
find stage -type f -o -type l | sort
cpack --config build-linux/CPackConfig.cmake -G DEB
dpkg-deb --info build-linux/*.deb
dpkg-deb --contents build-linux/*.deb
```

Codex 最终在实机上安装、运行、卸载并检查残留。

## 11. Codex 逐包验收清单

每个 DeepSeek 提交至少检查：

1. `git diff <previous-accepted>...HEAD --check`。
2. 变更是否严格在当前任务包范围。
3. 本机 Linux CMake configure/build 是否无隐式工作目录依赖。
4. `ctest --output-on-failure`。
5. 新增错误路径是否有错误信息、清理和可测试结果。
6. 线程、D-Bus request、PipeWire buffer、fd、GL 资源的所有权是否明确。
7. 手工运行是否与 DeepSeek 报告一致。

验收结论只有三种：

- `ACCEPTED`：该包锁定，进入下一包。
- `CHANGES_REQUESTED`：只修当前包。
- `REWORK_REQUIRED`：架构偏离，回到当前包起点重做，不用补丁堆叠。

## 12. 最终 Definition of Done

只有同时满足以下条件才算迁移完成：

- Ubuntu 26.04 GNOME Wayland 上从干净构建目录 configure/build/test 全部通过。
- Qt UI 、实时预览、14 参数、预设、高级设置能使用并持久化。
- generated 背景不依赖 Portal 即可稳定运行。
- Portal 授权成功后桌面引力透镜可见；取消或中断时可控降级。
- 空闲触发可重复，任意键鼠活动 1 秒内停止，锁屏期间不运行。
- MPRIS 媒体策略与 `videoAsIdle` 配置一致。
- 单屏和多屏模式没有孤儿进程。
- 连续运行 2 小时没有持续内存/显存增长、画面冻结或无限授权对话。
- `.deb` 可安装、应用菜单可启动、自启动可切换、卸载不删除用户配置且不留系统文件。

## 13. 可直接交给 DeepSeek 的首次指令

```text
你正在 port/ubuntu-wayland 分支上工作。

完整阅读 Doc/UBUNTU_MIGRATION_PLAN.md，特别是第 8–13 节。
本次只完成 DS-00，不得开始 DS-01 或任何产品代码改动。

要求：
1. 先检查分支、工作树和基线提交。
2. 按 DS-00 记录目标机器与构建基线。
3. 不修改 CMakeLists.txt、src/、Blakhole_UI/ 产品代码。
4. 执行可重现的检查，不伪造未实际运行的结果。
5. 提交信息使用：docs: record Linux migration baseline
6. 按第 8.3 节格式报告并停止，等待 Codex 验收。
```
