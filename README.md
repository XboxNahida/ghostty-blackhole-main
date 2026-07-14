# Black Hole — Ubuntu GNOME 50 本机桌面黑洞

![demo](demo.gif)

基于 Eric Bruneton 黑洞着色器的桌面黑洞可视化程序。本仓库当前只服务一台确定的 Ubuntu GNOME 工作站，通过 GNOME Shell 合成器实时绘制引力透镜、吸积盘和光子环效果。

> [!IMPORTANT]
> 本项目不是跨平台发行版，也不承诺 Windows 可用。仓库中的 Win32、WGC、DXGI、D3D11、ImGui 和 PowerShell 文件是上游及迁移过程留下的历史源码；它们不属于当前构建、运行或验收目标。请不要据此推断本项目仍可在 Windows 上编译或运行。

## 项目定位

唯一支持和验收的环境：

| 项目 | 本机基线 |
|---|---|
| 操作系统 | Ubuntu 26.04 amd64 |
| 桌面 | GNOME Shell 50.1 |
| 会话 | Wayland |
| 显卡 | NVIDIA GeForce RTX 3060 |
| 显示器 | 单显示器 |
| 主分支 | `port/ubuntu-wayland` |

核心约束：

- GNOME Shell 50 扩展 + 本地会话 D-Bus 是唯一正式渲染路径。
- 只适配当前单显示器主机，不实现或测试多显示器。
- 不维护、不验证 Windows；Linux 改动不以保持 Windows 兼容为前提。
- 不继续追随上游功能，不把作者后续版本作为必须合并的更新线。
- 不新增产品功能，不改进黑洞视觉算法；只处理本机核心链路中的真实缺陷。
- Windows 专属能力没有简单可靠的 GNOME/Wayland 替代时直接放弃。
- 其他 Ubuntu/Linux、GNOME、Xorg、GPU 环境均不在承诺范围内。

完整调查与收尾计划见 [UPSTREAM_1.2.1_UPDATE_ANALYSIS.md](UPSTREAM_1.2.1_UPDATE_ANALYSIS.md)。

## 当前核心能力

- GNOME Shell 合成器内的实时黑洞 shader 效果。
- Qt 6/QML 配置界面与实时预览；预览同步洞大小、移动速度、旋转速度和固定/动态大小。
- Linux UI 使用单实例；重复打开只唤起已有窗口。
- 最小化/XDG 自启动会恢复已保存的运行策略：始终显示模式立即启动效果，空闲模式则等待配置的空闲阈值。
- 16 个预设及顺序、循环、随机播放。
- 通过 D-Bus 启动、停止和重新加载扩展效果。
- 由 GNOME Shell 扩展注册 Wayland 原生全局停止快捷键；默认 `Ctrl+Alt+B`，可在 UI 中修改或禁用。
- GNOME IdleMonitor 空闲触发及恢复活动后停止。
- MPRIS 媒体播放抑制。
- XDG 用户级开机自启动和用户配置持久化。
- Linux UI 只展示 GNOME 主路径真正消费的配置，启动时不请求上游更新服务。
- 本地 CMake 构建与 `.deb` 安装。

正式运行链路：

```text
appBlakholeUI
  -> session D-Bus
  -> GNOME Shell 50 extension
  -> Mutter/Clutter compositor shader effect
```

该链路不需要 XDG Desktop Portal 选屏，也不使用 Windows 桌面捕获。

## 明确不做的功能

以下能力不属于待办，后续开发者不应继续“补齐”：

- Windows 构建、Windows 屏保、WGC/DXGI/D3D11 兼容。
- 主屏、副屏、跨屏、一屏一黑洞等多显示器模式。
- Windows 游戏/前台窗口检测、三类应用名单及运行应用选择器。
- `Ctrl+Alt+R` 录制捕获热键及通用键盘事件捕获；仅保留由 GNOME Shell 扩展实现的全局停止热键。
- Windows 式录屏捕获、冻结纹理和窗口捕获排除。
- Portal/PipeWire 全屏 renderer 作为正式产品后端。
- 面向公网的 GitHub Release 自动更新通道。
- 收款码发布资源和 Windows PE/PowerShell 发布检查。
- shader 视觉算法扩展或重新设计。

相关旧源码可以留作历史参考，但不需要兼容性维护或回归测试。Linux UI 已移除无效的 Windows 捕获、多屏、应用名单、定时显示和上游更新入口。

## 快速开始

首次安装 GNOME 扩展后，需要注销并重新登录一次。重新登录后执行：

```bash
gnome-extensions enable blackhole@xboxnahida.github.com
gnome-extensions info blackhole@xboxnahida.github.com
appBlakholeUI
```

`gnome-extensions info` 应显示 `State: ACTIVE`。在 UI 中配置参数后点击“启动黑洞”。

“关闭渲染快捷键”默认是 `Ctrl+Alt+B`。它由 GNOME Shell 注册，在原生 Wayland
应用、XWayland 应用以及 UI 隐藏时均可停止效果；项目不使用 JNativeHook/XRecord。

也可直接通过本地 D-Bus 控制：

```bash
gdbus call --session \
  --dest io.github.xboxnahida.Blackhole \
  --object-path /io/github/xboxnahida/Blackhole \
  --method io.github.xboxnahida.Blackhole.Start

gdbus call --session \
  --dest io.github.xboxnahida.Blackhole \
  --object-path /io/github/xboxnahida/Blackhole \
  --method io.github.xboxnahida.Blackhole.Stop
```

### 运行模式

| 模式 | 行为 |
|---|---|
| 始终显示 | 立即启用合成器黑洞效果 |
| 空闲检测 | 达到设定空闲时间后启用，恢复活动后停止 |

Linux 端通过 GNOME IdleMonitor 获取空闲状态，通过 MPRIS 判断媒体是否播放；不使用 Windows 的全屏窗口、音频峰值或进程名单检测。

## 从源码构建

### 1. 安装依赖

正式本机包只构建 Qt UI 和 GNOME 扩展，不需要历史 renderer 的 GLFW、PipeWire 或
Wayland 开发依赖。

```bash
sudo apt update
sudo apt install build-essential cmake ninja-build pkg-config dpkg-dev \
  qt6-base-dev qt6-declarative-dev libglib2.0-bin
```

### 2. 配置并构建

```bash
cmake -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=/usr \
  -DBLACKHOLE_BUILD_UI=ON \
  -DBLACKHOLE_BUILD_RENDERER=OFF \
  -DBLACKHOLE_ENABLE_PORTAL_CAPTURE=OFF \
  -DBUILD_TESTING=OFF

cmake --build build
```

`blackhole-renderer` 仍可为历史测试单独构建，但不会进入本机 `.deb`。GNOME 扩展和
预览资源的构建、安装已经与该历史目标解耦。

### 3. 测试

如需运行包含历史 renderer/Portal 模块的完整测试，额外安装
`libglfw3-dev libpipewire-0.3-dev libwayland-dev wayland-protocols`，并使用
`-DBLACKHOLE_BUILD_RENDERER=ON -DBUILD_TESTING=ON` 配置构建目录。测试目标默认不进入
主构建，需要显式构建：

```bash
cmake --build build --parallel --target \
  shader_source_tests portal_capture_tests \
  renderer_startup_diagnostics_tests movement_settings_tests \
  application_catalog_tests avatar_storage_tests update_release_tests \
  update_checker_state_tests renderer_search_tests \
  gnome_idle_monitor_tests mpris_monitor_tests ds09_linux_tests

QT_QPA_PLATFORM=offscreen \
  ctest --test-dir build --output-on-failure
```

部分测试覆盖历史模块。现行门禁只关注本机 GNOME 50 主链路，不要求 Windows、多屏或 Portal 产品能力。

### 4. 生成并安装 `.deb`

```bash
cpack --config build/CPackConfig.cmake -G DEB
sudo apt install ./blackhole-1.2.1-ubuntu-local-linux-amd64.deb
```

应用和 About 显示版本为 `1.2.1-ubuntu-local`；Debian 包内部版本为
`1.2.1+ubuntu.local`，以便与上游发布身份和旧的本机 `1.2.0` 包明确区分。

安装后注销并重新登录，再启用扩展：

```bash
gnome-extensions enable blackhole@xboxnahida.github.com
appBlakholeUI
```

### 5. 卸载

```bash
sudo apt remove blackhole
```

卸载会删除 `/usr` 下由包安装的 UI、扩展、shader、桌面入口、图标和 AppStream
元数据；保留 `~/.config/XboxNahida/Blakhole UI/` 下的用户配置。XDG 自动启动应先在
UI 中关闭，避免留下指向已卸载程序的用户级 `.desktop` 文件。

## 故障检查

```bash
gnome-extensions info blackhole@xboxnahida.github.com
journalctl -b /usr/bin/gnome-shell --no-pager | tail -n 200
```

当前本机核心链路没有已知阻塞问题。真实 MPRIS 播放器、锁屏/解锁和睡眠/唤醒测试
已按用户决定跳过，不再作为本机安装验收门禁。

## 项目结构

### 当前产品路径

| 路径 | 用途 |
|---|---|
| `Blakhole_UI/` | Qt 6/QML 配置 UI、预览和生命周期控制 |
| `Blakhole_UI/core/blackholecore.cpp` | 配置、GNOME D-Bus 启停和空闲策略 |
| `src/gnome_idle_monitor.*` | GNOME IdleMonitor 空闲与活动检测 |
| `src/mpris_monitor.*` | MPRIS 媒体播放检测 |
| `src/autostart_xdg.*` | XDG 用户级自启动 |
| `gnome-extension/blackhole@xboxnahida.github.com/` | GNOME Shell 50 合成器效果和 D-Bus 接口 |
| `blackhole.glsl`、`shaders/` | 核心及预览 shader |
| `packaging/linux/` | Linux 桌面入口、图标和元数据 |

### 历史与非产品路径

| 路径 | 说明 |
|---|---|
| `src/main.cpp`、`src/win32_*` | Windows 主程序与 Win32/WGL 代码，不维护 |
| `src/capture_wgc.*`、`src/capture_dxgi.*` | Windows 捕获代码，不维护 |
| `src/d3d11_renderer.*`、`shaders/*.hlsl` | Windows 实验渲染路径，不维护 |
| `src/main_linux.cpp`、`src/portal_capture.*` | 迁移阶段旧全屏/Portal 路径，不再作为产品后端 |
| `build_blackhole.ps1`、`package_release.ps1` | Windows 构建发布脚本，不属于本机流程 |
| `update-1.2.1/` | 作者更新包调查输入，不纳入构建或 Git |

## 开发与验收原则

接受改动前依次判断：

1. 是否直接修复本机 GNOME 50 的渲染、配置、空闲启停或安装问题？
2. 是否保持现有 shader 算法和视觉方向，不引入新功能？
3. 是否避免为了 Windows、多屏、Portal 或其他机器增加复杂度？
4. 是否让 UI 只展示确实会在 GNOME 扩展主路径生效的设置？

如果答案不是明确的“是”，默认不做。

## 文档

- [上游 v1.2.1 更新洞察与本机化计划](UPSTREAM_1.2.1_UPDATE_ANALYSIS.md)
- [GNOME Shell 扩展说明](gnome-extension/README.md)
- [Ubuntu 迁移历史计划](Doc/UBUNTU_MIGRATION_PLAN.md)——仅作历史记录
- [Linux 环境基线](Doc/LINUX_BASELINE.md)
- [上游技术文档](Doc/TECHNICAL.md)——包含大量 Windows 历史内容

## 迁移手记

项目曾尝试原生全屏窗口、Portal 桌面捕获和合成器原型。由于 Wayland 捕获反馈与窗口层级限制，最终采用 GNOME Shell 扩展 + D-Bus，直接在合成器中应用黑洞效果。

当前方向不是继续做通用 Linux/Windows 产品，而是把已落地的 GNOME 50 方案收敛成当前工作站上稳定、准确、可维护的本地程序。

## 灵感来源

[Eric Bruneton's black hole shader](https://github.com/ebruneton/black_hole_shader)（BSD-3-Clause）

## License

MIT — 见 [LICENSE](LICENSE)
