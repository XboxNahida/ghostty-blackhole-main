# Ubuntu 迁移计划（审查稿）

状态：待审查，尚未进入实现阶段  
目标分支：`port/ubuntu-wayland`  
目标平台：Ubuntu 26.04、GNOME、Wayland、x86_64、NVIDIA RTX 3060

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

## 2. 已确认的可行性

- 当前显卡提供 OpenGL 4.6 / GLSL 4.60，项目 shader 只要求 GLSL 3.30。
- GCC 15、CMake 4.2、Ninja、GLFW 3.4、Qt 6.10 和 PipeWire 1.6 开发环境已就绪。
- `blackhole.glsl`、预览 shader、预设格式以及 Qt/QML 主要界面可复用。
- 当前根构建脚本声明 `RC` 语言并无条件链接 D3D11、DXGI 和 Win32 系统库，不能直接在 Linux 配置。
- 渲染器、捕获、窗口、空闲检测和系统集成之间已有模块边界，可在保留 Windows 实现的同时增加 Linux 后端。

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
- 记录 Windows 构建基线，确保移植不破坏原平台。
- 添加 Linux CI 或至少提供无界面 CMake 配置与单元测试任务。

退出条件：分支存在；工作树干净；Windows 与 Linux 的目标拆分方案通过审查。

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
- 更新 README，分别说明 Windows 与 Ubuntu 的构建、权限和已知限制。

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
| Windows 回归 | 原版本受损 | 平台目标隔离，并保留 Windows CI/构建检查 |

开始实现前需要审查确认的产品决策：

1. 第一版是否接受“普通全屏屏保窗口”，而不是 GNOME 锁屏背景插件。
2. 第一版是否以程序生成背景为必达项，将桌面实时捕获放在后续阶段。
3. 是否保留 Windows 支持并采用同一仓库的双平台构建。
4. 发布格式是否以 `.deb` 为主，暂不引入 Flatpak（Portal 适配稳定后再评估）。

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
- 跨平台 CMake、安装规则与打包文件
- Linux 集成测试和用户文档

计划审查通过前，不进行上述实现性改动。
