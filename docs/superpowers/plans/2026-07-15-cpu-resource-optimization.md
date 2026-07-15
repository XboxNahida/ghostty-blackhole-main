# Blackhole 默认配置资源基线与 CPU 优化计划

## 1. 目标与边界

本计划只优化当前唯一产品路径：

```text
appBlakholeUI -> session D-Bus -> GNOME Shell 50 extension
               -> Mutter/Clutter compositor shader effect
```

不恢复、不优化历史的 GLFW/Portal/Windows renderer。不改变黑洞外观、动画速度、出生位置、转场时长、预设切换、空闲触发、媒体抑制、全局快捷键和托盘行为。

## 2. 测量环境与默认配置

- 日期：2026-07-15
- Ubuntu GNOME 50.1 / Wayland
- CPU：12 个逻辑 CPU
- GPU：NVIDIA GeForce RTX 3060，驱动 595.71.05
- 扩展：`blackhole@xboxnahida.github.com` Version 4，ACTIVE
- 安装的 `extension.js` / `effect.js` 与当前工作树逐字节一致
- 采样方法：`pidstat` 1 秒间隔，稳定后采样 10–15 秒；`nvidia-smi dmon` 1 秒间隔
- CPU 数值按 Linux 进程工具口径表示，100% 等于占满一个逻辑 CPU

实际使用的默认配置：

- 空闲检测模式，阈值 30 秒
- 16 个默认预设，5.3 秒顺序切换
- 移动速度 1.0，动画速度 1，黑洞大小 1.0
- 不跟随鼠标，不启用额外光效，畸变强度 1.0
- UI 最小化启动，全局停止快捷键 `Ctrl+Alt+B`

## 3. 实测资源基线

| 状态 | CPU | RSS 内存 | 线程 | GPU/VRAM/功耗 | 稳态 I/O |
|---|---:|---:|---:|---|---|
| 扩展已加载、效果停止 | GNOME Shell 0.13% | GNOME Shell 380,364 KiB | 20 | 0% GPU，202 MiB VRAM，约 12.4–13.1 W | 无可测磁盘/网络 I/O |
| 默认效果运行 | GNOME Shell 1.93% | GNOME Shell 380,447 KiB | 20 | 采样精度下仍为 0% GPU，202 MiB VRAM，约 12–13 W | 无可测磁盘/网络 I/O |
| UI 最小化后稳态 | 0.30% | 208,324 KiB | 11 | UI 预览已初始化，隐藏后无持续可见负载 | MPRIS D-Bus 每 2 秒轮询 |

结论：

- 效果运行对 GNOME Shell 的实测 CPU 增量约为 **1.80 个百分点**。
- 加上最小化 UI，默认运行链路约为 **2.23% 的一个逻辑 CPU**，折合全机 12 逻辑 CPU 容量约 **0.19%**。
- GNOME Shell 的 380 MiB 是整个桌面合成器的 RSS，不能全部归因于本项目；效果启停间没有可测的 RSS 增量。
- UI 的约 203 MiB RSS 可归因于本项目，主要是 Qt/QML 场景与已创建的 OpenGL 预览；它是内存优化点，不是当前最大的稳态 CPU 热点。
- `nvidia-smi` 的整数采样无法分辨很低的合成器图形负载；“0%”不等于 shader 没有在 GPU 执行。

## 4. 代码扫描结论

### 4.1 首要 CPU 候选：每帧重复的 JS/GObject 调用

`effect.js` 为背景组和窗口组各创建一个 effect。每个 effect 都有自己的 16 ms `GLib.timeout_add()`，因此每秒约有 125 次 JS 回调和两套 `queue_redraw()`。

每个 effect 每帧又执行 21 次 `set_uniform_value()`。其中大部分是整次运行都不变的常量，却持续通过 JS -> GObject -> C 边界重复设置。

### 4.2 不必要的 Linux 秒级轮询

Linux 已使用 GNOME IdleMonitor 的 `WatchFired` 事件，但 `applyAndStart()` 仍启动历史的 1 秒 `m_idleTimer`。`checkIdle()` 在 Linux 上每秒只是反复确保 IdleMonitor/MPRIS 已启动并刷新相同状态文本。

### 4.3 MPRIS 固定 2 秒轮询

`MprisMonitor` 每 2 秒调用一次 `org.freedesktop.DBus.ListNames`，然后对每个播放器读取 `PlaybackStatus`。无播放器时也会持续唤醒 UI 进程。

### 4.4 最小化启动仍急切创建整个 QML 界面

`Main.qml` 直接实例化所有主页，`BlackholeConfig.qml` 又直接实例化小预览和大预览。日志确认最小化启动也会创建两个 `BlackholePreviewArea`。隐藏窗口后 Qt 通常停止实际绘制，所以稳态 CPU 不高，但初始化峰值和内存明显偏大。

### 4.5 每次 Start 重复装配 shader

`Start()` 会对两个 effect 分别调用 `reloadConfig()`，重复读文件、执行字符串替换并设置 shader source。这主要影响启动瞬时 CPU，不是稳态负载。

## 5. 分阶段优化计划

### 阶段 0：建立可重复的性能门禁

1. 添加本机专用的只读采样脚本，统一记录停止、转场、稳定运行和 UI 最小化四种状态。
2. 每个状态预热 5 秒，采样 30 秒，至少重复 3 轮，使用中位数比较。
3. 记录 GNOME Shell/UI CPU、RSS、线程、上下文切换、GPU/VRAM/功耗，并在每轮结束后恢复原始运行状态。
4. 增加人眼验收录屏：与当前版本并排比较出生、游走、预设过渡、窗口动态和 650 ms 停止转场。

验收：脚本不改配置，异常中断也会恢复效果原始状态。

### 阶段 1：缓存静态 uniform（首选）

1. 将不随帧变化的 uniform 移到 shader 装载、配置重载或 `prepareStart()` 时设置。
2. 每帧只保留 `iTime`、`uTransition` 以及真正变化的值。
3. 分辨率只在 actor 尺寸变化时更新；出生点和随机相位只在每次 Start 更新。
4. 保留两个 effect，不改变背景组/窗口组的分层。

预期：每帧 JS -> GObject 调用从约 42 次降至 4–6 次，是最低风险、最可能降低 GNOME Shell CPU 的改动。

验收：所有预设和高级参数的实际效果与基线一致；GNOME Shell 稳态 CPU 有可重复的正向改善即可保留，5% 左右的改善也视为达标，不再使用 15% 硬门槛。

### 阶段 2：两个 effect 共用帧驱动器

1. 把 effect 内部的两个 16 ms `GLib.timeout_add()` 收敛为扩展级的一个帧源。
2. 优先评估与 Clutter/Mutter 帧时钟对齐的 timeline/tick 机制，避免 16 ms 固定定时器与显示刷新错相。
3. 一次 tick 同时更新两个 effect 的转场并对两个 actor 排队重绘。
4. Stop 完成后必须完全移除帧源；扩展 disable 也必须无残留回调。

验收：运行时只有一个驱动源，停止 700 ms 后无周期性唤醒；转场时长和动画节奏不变，不增加丢帧。

### 阶段 3：去掉 Linux 路径的遗留 1 秒轮询

1. Linux 空闲模式进入时直接启动 `GnomeIdleMonitor` 和 `MprisMonitor`，不启动 `m_idleTimer`。
2. Windows 历史分支保持原样，或通过平台条件将 1 秒 timer 仅限 Windows。
3. 将空闲状态文本更新改为事件驱动，保持原有的状态内容。

验收：30 秒空闲触发、恢复活动停止、锁屏停止、服务消失/恢复均通过现有测试与真机验证；UI 待机时不再每秒唤醒。

### 阶段 4：将 MPRIS 改为事件驱动

1. 监听 D-Bus `NameOwnerChanged`，维护 `org.mpris.MediaPlayer2.*` 服务集合。
2. 监听每个播放器的 `PropertiesChanged`，只在状态改变时读取/更新 `PlaybackStatus`。
3. 启动时保留一次初始全量同步，服务重连时重新订阅。
4. 移除固定 2 秒轮询；如 GNOME D-Bus 实测存在丢事件，只保留低频健康校验作为降级路径。

验收：播放、暂停、播放器启动/退出和 D-Bus 重连后的抑制行为不变；无播放器时 UI 无 2 秒周期唤醒。

### 阶段 5：懒加载 UI 和预览

1. `--minimized` 启动时不创建预览 FBO；用户第一次打开配置页时再通过 `Loader` 创建。
2. 大预览只在打开弹窗时实例化，关闭后释放 FBO/GL 资源。
3. 窗口隐藏时明确将小预览 `running=false`，再显示时恢复。
4. 如有必要，将非当前页也改为懒加载，但不改变页面状态保留行为。

验收：托盘后台的 RSS 目标下降至 120 MiB 以下，稳态 CPU 不高于基线；首次打开界面与预览不得卡死或显示黑屏。

### 阶段 6：缓存配置后的 shader source

1. 对会影响 shader 的配置计算指纹；指纹不变时 Start 不重新读文件/拼接源码。
2. 一次构建配置后源码，供两个 effect 共用，避免重复 regex 处理。
3. `ReloadConfig` 必须强制重算指纹并在两个 effect 上同步生效。

验收：连续 Start/Stop 的启动 CPU 峰值和延迟下降；修改预设、大小、移动参数后下次 Start/Reload 不得使用旧配置。

## 6. 不建议的“优化”

- 不降低 shader 分辨率、帧率、采样质量或预设精度；这会改变视觉功能。
- 不恢复 Clone overlay；项目历史已证明它对真实窗口 damage 不稳定，会出现静态截图。
- 不把背景组和窗口组盲目合并成单个 effect；可能影响 GNOME Shell UI、窗口层级和截屏行为。
- 不为了测试方便切回历史 `blackhole-renderer`；它不是生产路径。

## 7. 总体验收标准

全部阶段完成后：

1. 默认效果运行时 GNOME Shell CPU 中位数相对 1.93% 基线至少下降 25%，目标不高于 1.45%。
2. 停止效果 700 ms 后，扩展不留周期重绘，GNOME Shell CPU 回到与 0.13% 基线同一波动区间。
3. UI 最小化待机 CPU 目标不高于 0.10%，RSS 目标低于 120 MiB。
4. 不增加 GPU 功耗、VRAM、丢帧、桌面延迟或日志错误。
5. 现有自动测试全部通过，并完成黑洞视觉、配置重载、空闲/MPRIS、快捷键、锁屏和托盘的真机回归。
6. 每个阶段独立测量和提交；未达到该阶段的 CPU/内存目标就回退，不累积无效复杂度。

## 8. 执行记录与跨会话续跑

### 执行纪律

- 每一轮代码落地、部署、测量、回退或验收结束前，都必须更新本节；不能只把进度留在 Codex 对话中。
- 每轮至少记录：改动文件、自动验证、真机结果、性能数据、是否达到阶段门禁、下一轮唯一入口。
- GNOME Shell JS 改动必须以注销并重新登录后的实际加载版本为准；禁用/启用扩展不能代替重新登录。
- 阶段门禁未通过时，不开始下一阶段。需要回退时只回退本轮优化，保留用户原有工作区改动。

### 第 1 轮：阶段 0 门禁骨架与阶段 1 静态 uniform 缓存

日期：2026-07-15

状态：**代码已落地并部署到系统扩展，静态验证和安装哈希核对通过；尚未完成注销后的性能和视觉门禁。**

本轮改动：

- 新增 `tools/profile_gnome_resources.sh`：采样停止、重复进出场、稳定运行、UI 最小化四种状态；默认预热 5 秒、采样 30 秒、重复 3 轮；记录 `pidstat` 与 `nvidia-smi dmon` 原始数据；正常退出、中断或失败时恢复原始效果状态。
- 门禁冒烟时发现 `pgrep gnome-shell` 会误选 `gnome-shell-calendar-server`，已改成 `pgrep -n -x gnome-shell`。修正后的 1 秒冒烟样本确认 PID 4275、命令名 `gnome-shell`；冒烟数据只验证流程，不作为性能结论。
- 修改 `gnome-extension/blackhole@xboxnahida.github.com/effect.js`：shader 重载时设置 12 个静态 uniform，`prepareStart()` 时设置出生点和随机相位，actor 尺寸变化时才更新分辨率；稳态每个 effect 每帧仅设置 `iTime` 和 `uTransition`，两个 effect 合计由约 42 次降为 4 次 JS/GObject uniform 调用。
- 新增 `tests/gnome_effect_uniform_cache_test.py`，静态约束逐帧 uniform 集合以及静态/Start 生命周期归属。

本轮已完成验证：

```text
python3 tests/gnome_effect_uniform_cache_test.py
  GNOME effect uniform cache regression checks passed

bash -n tools/profile_gnome_resources.sh
git diff --check
  通过

仓库和系统安装的 effect.js SHA-256：
  49b4bc74322f2dd6349782e07b4c7035fd5c41890b3c3310adbee53ed1e111d2
```

本轮尚未通过的门禁：

- 尚未注销重新登录，因此当前 GNOME Shell 仍运行旧模块。
- 尚未执行正式 3 轮采样、计算中位数，也未确认 GNOME Shell 稳态 CPU 相对 1.93% 基线下降至少 15%。
- 尚未完成人眼对比：出生、游走、16 预设过渡、窗口动态、650 ms 进入/停止转场以及配置重载。
- 冒烟时 UI 未运行，`environment.txt` 为 `ui_pid=not-running`，因此 UI 最小化状态没有有效 UI 数据。

### 注销前必须完成

1. 已完成：当前工作树的 `effect.js` 已安装到系统扩展目录，两份文件 SHA-256 一致。
2. 现在直接注销并重新登录；不要用禁用/启用扩展代替注销。

### 下次 Codex 会话的唯一续跑入口

新会话首先阅读本文件，然后按以下顺序继续，不要直接开始阶段 2：

1. 用 `gnome-extensions info blackhole@xboxnahida.github.com` 确认系统路径、Version 4、`ACTIVE`。
2. 比较仓库和实际加载路径中 `effect.js` 的 SHA-256，确认本轮代码已加载。
3. 运行静态测试和相关现有自动测试；检查本次登录以来的 GNOME Shell 日志中是否有 blackhole/shader/JS 错误。
4. 启动安装版 UI 并最小化，确认采样脚本能记录真实 UI PID。
5. 执行正式门禁：`tools/profile_gnome_resources.sh <结果目录>`。保留三轮原始数据并计算各状态 CPU/RSS/上下文切换的中位数。
6. 完成阶段 1 人眼回归。若稳态 GNOME Shell CPU 没有可重复的正向改善，或视觉/行为有回归，只回退本轮 `effect.js` 优化并更新本节；约 5% 改善即可保留并进入阶段 2。
7. 无论通过、回退还是遇到阻塞，结束前继续更新本节，写清数据和下一步。

### 第 2 轮：注销后门禁采样与阶段 2 单帧驱动器

日期：2026-07-15

状态：**阶段 1 已在新 GNOME Shell 会话加载；正式采样已完成但桌面背景负载噪声过大；阶段 2 代码已落地、通过静态验证并部署到系统扩展，尚待注销重登后真机验收。**

本轮核对与采样：

- 实际加载路径为 `/usr/share/gnome-shell/extensions/blackhole@xboxnahida.github.com`，Version 4、`ACTIVE`；GNOME Shell PID 48715 启动于 09:34:08，确认是注销重登后的新会话。
- 安装版 UI PID 48880 以 `--minimized` 运行；仓库与安装路径的阶段 1 `effect.js` SHA-256 均为 `49b4bc74322f2dd6349782e07b4c7035fd5c41890b3c3310adbee53ed1e111d2`。
- 三轮正式原始数据保存在 `/tmp/blackhole-resource-profile-stage1-20260715`；脚本记录了真实 UI PID 并在结束后恢复了原始停止状态。
- 本轮 GNOME Shell 即使停止效果也在 13.9%–31.6% 之间大幅波动，运行状态为 20.1%–38.5%，与 0.13%/1.93% 旧基线不在同一噪声条件，因此这组数据不用于判定阶段 1 百分比收益。UI 采样有效：CPU 约 0.13%–0.37%，RSS 约 232.4 MiB。
- 用户已明确调整门禁：只要存在可重复的正向优化即可保留，约 5% 改善也足够，不再卡 15% 硬门槛。

阶段 2 代码改动：

- 从每个 `BlackholePrototypeEffect` 移除独立 `GLib.timeout_add()` 和 `_redrawId`。
- 在扩展级增加唯一 `_frameSourceId`，每次 tick 使用同一个单调时间更新两个 effect 并排队重绘。
- Stop 转场完成后自动移除唯一帧源；disable 时显式移除帧源并立即禁用两个 effect。
- 新增 `tests/gnome_shared_frame_driver_test.py`，约束 effect 内不再存在 timer、扩展只保留一个帧源和一个 700 ms direct-scanout 恢复源，以及 disable 清理路径。
- 系统扩展部署后哈希已核对：`effect.js` 为 `76c96d86e0fa3a385d634183ef0d3b9b260738c0e7a59871eed1e776681d115d`，`extension.js` 为 `3969594ab0ba6ed3c3ea7e20bfe861f16492a63b577f4034d79fe9d4efa58d22`，仓库与 `/usr/share/gnome-shell/extensions/...` 一致。

本轮已完成验证：

```text
python3 tests/gnome_effect_uniform_cache_test.py
  GNOME effect uniform cache regression checks passed
python3 tests/gnome_shared_frame_driver_test.py
  GNOME shared frame driver regression checks passed
bash -n tools/profile_gnome_resources.sh
git diff --check
  通过
```

下一轮唯一入口：

1. 注销并重新登录，不以扩展禁用/启用代替。
2. 检查新会话日志，完成出生、游走、预设过渡、窗口动态、650 ms 进出场、配置重载和 Stop 700 ms 后无周期回调的真机回归。
3. 在桌面空闲且无录屏/快速变化窗口时重测；将停止态作为同轮差分基线，而不直接与旧会话绝对值比较。

### 第 3 轮：Gnome Live Wallpaper 空闲触发兼容性

日期：2026-07-15

状态：**冲突原因已定位，本机动态壁纸扩展已修改，尚待注销重登后真机验证。**

诊断结论：

- Gnome Live Wallpaper 1.2.1 通过一个铺满桌面的普通 `mpv` 窗口实现动态壁纸；黑洞 D-Bus 直接启动 12 秒时，用户确认黑洞、动态壁纸和普通窗口都能正常渲染，因此不是 compositor shader 层级冲突。
- 当前会话没有 `org.mpris.MediaPlayer2.*` 服务，排除了 MPRIS 媒体播放抑制。
- 用户不动键鼠时连续读取 Mutter `GetIdletime`，计时在约 9–10 秒后周期性回到接近 0 ms，所以永远无法达到 30 秒黑洞触发阈值。
- 本机 `mpv --list-options` 确认 `--stop-screensaver` 默认值为 `yes`；动态壁纸扩展未关闭该行为，导致 mpv 周期性抑制/重置桌面空闲计时。

本机兼容修复：

- 已在 `~/.local/share/gnome-shell/extensions/gnome-wallpaper-engine@gjs.com/modules/wallpaper.js` 的 mpv 参数中增加 `--stop-screensaver=no`；动态壁纸继续播放，但不再阻止 GNOME 进入空闲。
- 原文件备份为同目录的 `wallpaper.js.bak-20260715-idle`。
- `node --check` 通过；临时补丁文件与实际扩展文件 SHA-256 均为 `4c5087b80b731f738445a311b2c75c63d07e8b2fc58e6b16ebcbb3debd1726e4`。

重登后验收：

1. `pgrep -a -x mpv` 必须显示 `--stop-screensaver=no`。
2. 保持动态壁纸运行，连续读取 Mutter `GetIdletime` 至少 35 秒，期间不得周期性归零。
3. 空闲 30 秒后黑洞必须正常出现，键鼠活动后正常停止；同时确认动态壁纸仍流畅播放。

### 第 4 轮：重登验收、阶段 2 黑屏回归与阶段 3 代码落地

日期：2026-07-15

状态：**阶段 2 真机门禁失败，共享帧驱动已回退并部署；阶段 3 代码已编译、测试和安装，但在回退版重登真机验收前不判定阶段通过。**

重登后现场证据：

- GNOME Shell 新会话 PID 61182，扩展 Version 4、`ACTIVE`，重登时加载的阶段 2 哈希与部署记录一致。
- 动态壁纸 `mpv` PID 62119 命令行已含 `--stop-screensaver=no`。Mutter `GetIdletime` 从 33 ms 连续增长到 35,109 ms，期间未归零；30 秒时 D-Bus `GetState` 由 false 变为 true，证明动态壁纸兼容修复和 IdleMonitor 事件生效。
- 用户真屏确认：30 秒触发后整屏变黑，活动使效果停止后画面恢复。随后通过 D-Bus `Start`、等待 2 秒、`Stop` 再次稳定复现：用户批准命令后黑屏两秒，自动 Stop 后恢复。
- 对应日志为 10:03:09 `IdleEligible -> RendererRunning` / `effect started`，10:03:18 活动后 `effect stopped`；无 shader、JS、Mutter 或 GPU 错误。判定为阶段 2 新加载渲染路径的视觉回归，不是显示器掉电或空闲事件失败。

阶段 2 回退：

- 删除扩展级 `_frameSourceId` 和 `advanceFrame()` 共享驱动，恢复每个 effect 的 `_redrawId` / `_ensureRedrawTimer()`。
- 保留阶段 1 的静态 uniform 缓存和已有 fullscreen direct-scanout 启停逻辑。如果回退版重登后仍黑屏，下一步才回退阶段 1，不同时猜测多个变量。
- 删除已不再符合生产路径的 `tests/gnome_shared_frame_driver_test.py`。
- 回退版仓库/系统哈希已一致：`effect.js` = `551e2cd68a25bdb183c6cce741001d6a9d7a981b9502b858d98f09d21646b351`，`extension.js` = `ac8ac84c7c88d57ee7d3c1f925b7fbc40350563fded0b70e4573013d8641f33b`。当前 GNOME Shell 仍是部署前已加载的旧模块，必须再次注销。

阶段 3 代码：

- Linux `applyAndStart()` 直接启动 `GnomeIdleMonitor` 和 `MprisMonitor`，不再启动 1 秒 `m_idleTimer`；Windows 路径保持原样。
- `GnomeIdleMonitor` 增加只读 `isStarted()`，Linux `isSystemActive()` 以真实监控启停状态为准。
- 空闲状态文本由 IdleMonitor `stateChanged` 和 MPRIS `playingChanged` 刷新；修改空闲阈值时立即重挂 GNOME watch。
- 新增 `tests/linux_idle_event_driven_test.py`，扩展 `tests/gnome_idle_monitor_tests.cpp` 覆盖 started/stopped 状态。
- 新 UI 已安装到 `/usr/bin/appBlakholeUI`，仓库/安装 SHA-256 均为 `9cd4b32178cf525d2c842c77babd619d964b9de80475deb065487712bc575f8d`。
- 安装版 UI 以 PID 74305 真实启动后，日志确认 `GnomeIdleMonitor: state -> Active` 和 `event-driven idle detection started`。由于当前 GNOME Shell 仍加载会黑屏的旧阶段 2 模块，验证后已主动停止 UI，避免重登前再次空闲触发；当前 `blackhole-ui-codex.service` 为 `inactive`。

本轮自动验证：

```text
cmake --build build --target appBlakholeUI -j4
  通过

ctest --test-dir /tmp/blackhole-cpu-tests-20260715 --output-on-failure
  gnome_idle_monitor_tests / mpris_monitor_tests / ds09_linux_tests: 3/3 通过

python3 tests/gnome_effect_uniform_cache_test.py
python3 tests/linux_idle_event_driven_test.py
node --check effect.js / extension.js
git diff --check
  通过
```

下一轮唯一入口：

1. 注销并重新登录，不使用扩展禁用/启用替代。
2. 核对新 GNOME Shell PID、扩展 `ACTIVE` 和上述回退版两个哈希。
3. 使用已安装的 `gnome-screenshot` 执行最长 2 秒的自动 Start/截图/Stop，同时以用户真屏为最终标准。若回退版仍黑屏，立即回退阶段 1 uniform 缓存。
4. 视觉恢复后再验证阶段 3：无 1 秒周期唤醒，30 秒空闲触发、活动停止、锁屏和服务恢复正常。
5. 阶段 2 暂不重做；后续若重启，必须先使用 Clutter/Mutter 帧时钟方案并单独验收，不再使用本轮 16 ms `GLib.timeout_add()` 共享实现。

### 第 5 轮：阶段 2 回退版重登验收与阶段 1 回退

日期：2026-07-15

状态：**阶段 2 已排除为唯一原因；阶段 1 静态 uniform 缓存也已回退并部署，尚待再次注销重登后真机验收。**

本轮重登现场：

- 新 GNOME Shell PID 79846，扩展路径 `/usr/share/gnome-shell/extensions/blackhole@xboxnahida.github.com`，Version 4、`ACTIVE`；仓库与安装路径中的阶段 2 回退版 JS 哈希一致。
- 动态壁纸 `mpv` PID 80792，命令行仍含 `--stop-screensaver=no`；安装版 UI PID 80012，以 `--minimized` 运行。
- 按计划通过 D-Bus 最长启动 2 秒并自动 Stop。`gnome-screenshot` 在当前 Wayland 会话无法使用 GNOME Shell 截图接口，回退 X11 后也未取得图像，因此没有把自动截图误作视觉证据。
- 用户真屏确认阶段 2 回退后问题仍存在：黑洞启动时桌面和所有普通窗口区域成为全屏黑幕，单击不能恢复，只能退出黑洞；系统 Dock 和 top bar 仍可见。由此排除共享帧驱动是唯一原因。

阶段 1 回退：

- 删除 shader reload 时的静态 uniform 写入、Start 时的出生点/随机相位写入，以及 actor 尺寸缓存；恢复原先在 `vfunc_paint_target()` 每帧设置全部 uniform 的行为。
- 删除只服务于该优化、现已不符合生产路径的 `tests/gnome_effect_uniform_cache_test.py`。
- 保留 `stopImmediately()`，因为当前 `extension.js` 的 disable 清理路径需要它；没有回退阶段 3 的 Linux 事件驱动空闲检测，也没有触碰用户其他工作区改动。
- 回退版 `effect.js` 已经通过 `pkexec` 部署，仓库与系统安装文件 SHA-256 均为 `67b4a4bd83552a88ad92b975c8693cc91b0aef8c06ae89ad1a6024c8be019320`。当前 GNOME Shell 仍缓存登录时加载的旧模块，必须再次注销。

本轮自动验证：

```text
node --check gnome-extension/blackhole@xboxnahida.github.com/effect.js
node --check gnome-extension/blackhole@xboxnahida.github.com/extension.js
git diff --check
  通过

cmake --build build --target appBlakholeUI -j4
  通过

cmake --build /tmp/blackhole-cpu-tests-20260715 \
  --target gnome_idle_monitor_tests mpris_monitor_tests ds09_linux_tests -j4
ctest --test-dir /tmp/blackhole-cpu-tests-20260715 --output-on-failure \
  -R '^(gnome_idle_monitor_tests|mpris_monitor_tests|ds09_linux_tests)$'
  3/3 通过

python3 tests/linux_idle_event_driven_test.py
  通过
```

下一轮唯一入口：

1. 注销并重新登录，不使用扩展禁用/启用替代。
2. 核对新 GNOME Shell PID、扩展 `ACTIVE`，以及仓库/安装版 `effect.js` 哈希 `67b4a4bd...9320`。
3. 先通过 D-Bus 最长启动 2 秒后自动 Stop，以用户真屏为最终标准。若视觉恢复，说明黑屏由阶段 1 uniform 生命周期改动触发；阶段 1 判定失败并永久保留回退。
4. 若仍黑屏，不再继续 CPU 阶段；下一步对比 Git 基线的 effect 和 extension，优先隔离 fullscreen direct-scanout 控制及此前未纳入阶段 1/2 的渲染路径改动。
5. 视觉恢复后再单独验收阶段 3 的 30 秒空闲触发、活动停止、锁屏、服务恢复和无 1 秒周期唤醒；阶段 3 门禁通过前不进入阶段 4。

### 第 6 轮：阶段 1 回退版重登后短时启停验收

日期：2026-07-15

状态：**新会话已加载阶段 1 回退版，2 秒自动启停的控制链和日志正常；等待用户真屏确认是正常旋转黑洞还是仍为黑幕。**

本轮现场证据：

- 新 GNOME Shell PID 为 89797，扩展路径为 `/usr/share/gnome-shell/extensions/blackhole@xboxnahida.github.com`，Version 4、`ACTIVE`。
- 仓库与安装路径中的 `effect.js` SHA-256 均为 `67b4a4bd83552a88ad92b975c8693cc91b0aef8c06ae89ad1a6024c8be019320`；`extension.js` 均为 `ac8ac84c7c88d57ee7d3c1f925b7fbc40350563fded0b70e4573013d8641f33b`。
- 安装版 UI PID 89969 以 `--minimized` 运行；动态壁纸 mpv PID 90732 仍含 `--stop-screensaver=no`。
- 10:23:45 通过 D-Bus `Start`，10:23:47 自动 `Stop`，最终 `GetState` 为 `false`；10:23:48 已恢复 fullscreen direct scanout。
- 本次启停没有新的 blackhole/shader/Mutter/GPU 错误。会话早期的托盘图标解码和其他扩展 `Util.Logger.warning` 错误与本次启停时间不重合。

下一步唯一入口：

1. 获取用户对 10:23:45–10:23:47 真屏画面的确认。
2. 若已恢复正常旋转黑洞，则阶段 1 正式判定失败并保留回退，继续阶段 3 真机门禁。
3. 若仍为黑幕，停止 CPU 后续阶段，直接对比 Git 基线并隔离 fullscreen direct-scanout 与更早的渲染路径改动。

### 第 7 轮：阶段 1 回退确认与阶段 3 事件驱动真机门禁

日期：2026-07-15

状态：**阶段 1 与阶段 2 均已判定失败并保留回退；阶段 3 的 30 秒空闲触发、活动停止、无 Linux 1 秒轮询和服务恢复自动测试已通过，只剩真机锁屏/解锁门禁。**

本轮结论与证据：

- 用户确认 10:23:45–10:23:47 阶段 1 回退版显示的是正常黑洞，没有黑屏。因此黑幕由阶段 1 的 uniform 生命周期改动引入；阶段 2 共享帧驱动不是唯一原因，两者均不再保留。
- 10:25:29 起 Mutter `GetIdletime` 从 38 ms、4,931 ms、9,949 ms、14,975 ms、20,004 ms 连续增长到 25,028 ms，期间未被动态壁纸归零。
- 10:25:59，30 秒阈值到达后 UI 记录 `Active -> IdleEligible -> RendererRunning`，GNOME Shell 记录 `effect started`，黑洞正常出现。
- 10:26:18，新的用户活动使 UI 记录 `RendererRunning -> Active` 并以 `GNOME activity detected` 停止；GNOME Shell 同时恢复 fullscreen direct scanout。
- Linux `applyAndStart()` 仅启动 `GnomeIdleMonitor` 和 `MprisMonitor`，`m_idleTimer->start()` 仅存于 `Q_OS_WIN` 分支；会话日志也只在真实状态变化时输出，没有每秒 `checkIdle()` 轨迹。
- `gnome_idle_monitor_tests` / `mpris_monitor_tests` / `ds09_linux_tests` 3/3 通过；`linux_idle_event_driven_test.py` 通过。其中单元测试已覆盖 IdleMonitor 服务消失后降级、服务恢复后重建 watch，以及锁屏/解锁的状态转换。

下一步唯一入口：

1. 在用户明确知情后执行一次真机锁屏，由用户解锁回到桌面。
2. 核对 UI 必须记录 `Locked`，若锁屏前效果正在运行则必须停止；解锁后必须重建 idle/active watch 并回到 `Active`。
3. 真机锁屏通过后将阶段 3 判定通过，再开始阶段 4 MPRIS 事件驱动改造。

### 第 8 轮：阶段 3 通过与阶段 4 MPRIS 事件驱动

日期：2026-07-15

状态：**阶段 3 全部门禁通过；阶段 4 已完成代码、自动测试、安装和真实 D-Bus 回归，固定 2 秒 MPRIS 轮询已移除。**

阶段 3 最终真机门禁：

- 10:29:05 按 `Super+L` 后 UI 记录 `GnomeIdleMonitor: state -> Locked` 和 `GNOME screen lock detected`。
- 锁屏时 GNOME Shell 正常禁用扩展；当时效果本已停止，因此 UI 对已消失 D-Bus 服务的空错误 `Stop failed` 不是渲染回归。
- 10:29:14 解锁后扩展重新 `enabled with 2 compositor effects`，IdleMonitor 回到 `Active`，当前 `GetState=false`，证明 watch 已恢复。

阶段 4 代码改动：

- `MprisMonitor::start()` 仅保留一次初始 `ListNames` 全量同步，删除 `m_pollTimer` 和 2,000 ms 轮询常量。
- 监听 `org.freedesktop.DBus.NameOwnerChanged`，播放器出现时只查询该服务的 `PlaybackStatus`，退出时立即移除缓存。
- 监听每个 MPRIS 对象的 `PropertiesChanged`；`PlaybackStatus` 有新值时直接更新，属性被 invalidated 时才对该播放器补查一次。
- 初始同步时异步解析 well-known name 的 unique owner，用于将 `PropertiesChanged` 的 sender 精确映射回播放器。
- 扩展 `mpris_monitor_tests.cpp`，覆盖无周期枚举、播放器出现/退出、Playing/Paused 属性变化、属性失效后的单目标补查。

阶段 4 验证：

```text
cmake --build build --target appBlakholeUI -j4
  通过

gnome_idle_monitor_tests / mpris_monitor_tests / ds09_linux_tests
  3/3 通过
python3 tests/linux_idle_event_driven_test.py
  通过
git diff --check
  通过

仓库构建与 /usr/bin/appBlakholeUI SHA-256：
  1b0699b4aadaef4179e676e4be59528795280a65175032b138344714782758c5
```

- 安装版 UI PID 97601 以 `--minimized` 启动，无 MPRIS 服务时初始化正常。
- 临时真实 MPRIS 服务 `org.mpris.MediaPlayer2.codex_stage4` 以 `Playing` 出现；10:34:59 UI 立即记录 `MprisMonitor: playing = true` 并执行媒体抑制。
- 10:35:01 服务发出 `PropertiesChanged: PlaybackStatus=Paused`，UI 立即记录 `playing = false`；随后服务退出无残留播放状态。
- 安装版 UI 待机时使用 `dbus-monitor` 监听 7 秒，未出现任何 `ListNames` method call，证明 2 秒轮询已消失。

下一轮唯一入口：

1. 开始阶段 5 UI/预览懒加载；当前 `--minimized` 日志仍显示创建了一个 `BlackholeLargePreview` 和两个 `BlackholePreviewArea`。
2. 以 PID 97601 当前 systemd 口径的 69.5 MiB 常驻内存（81.1 MiB 峰值）作为本轮重启后参考，但阶段 5 仍需用同一采样脚本与原 208–233 MiB RSS 口径对比。

### 第 9 轮：阶段 5 UI 与预览懒加载

日期：2026-07-15

状态：**阶段 5 已通过代码、自动测试、安装版冷启动与用户真机视觉/交互门禁；安装版冷启动最小化常驻内存为 66.1 MiB。**

本轮改动：

- C++ 在加载 QML 前将 `startMinimized` 暴露给 `Main.qml`；`--minimized` 或保存的最小化启动设置使根窗口从第一帧起就不可见。
- `BlackholeConfig` 页在窗口第一次显示时才通过 `Loader` 创建，之后保留页面状态。
- 小预览 FBO 仅在配置页可见时存在；切页或最小化到托盘时销毁。
- 大预览只在用户点击放大后创建，关闭弹窗后立即销毁 FBO 和组件。
- 托盘隐藏窗口时调用 `QQuickWindow::releaseResources()`，并关闭 persistent graphics/scene graph，使 Qt 在长时间后台时回收图形资源。
- 新增 `tests/qml_lazy_preview_test.py` 约束最小化首帧、页面 Loader、小/大预览生命周期和 Qt 图形资源释放设置。

本轮验证：

- 未安装构建冷启动 5 秒：66.4 MiB，日志中没有任何预览 `CREATED`。
- 首次打开窗口只创建一个 `BlackholePreviewArea`，未创建大预览；用户确认配置界面与小预览显示正常。
- 点击放大时新建一个 `BlackholeLargePreview` 和其 FBO；用户确认显示正常，关闭时两者均记录 `DESTROYED`。
- 最小化时小预览记录 `DESTROYED`，线程数从 17 降到 10；加入 Qt 图形资源释放后，已打开过窗口的回收后内存为 117.4 MiB，通过低于 120 MiB 的门禁。
- `appBlakholeUI` 构建、`ds09_linux_tests`、`qml_lazy_preview_test.py` 和 `git diff --check` 均通过。
- 安装版与仓库构建 SHA-256 均为 `ed88810c5cb045242d5df3bc9c343ef7afba515b13356468ea10bebaece3d245`。
- 安装版 PID 99225 冷启动 5 秒内存 66.1 MiB，峰值 80 MiB，无预览创建日志，并已正常启动阶段 3/4 的事件驱动空闲与 MPRIS 监控。

下一轮唯一入口：

1. 开始阶段 6：对影响 shader 的配置计算指纹，在配置不变时避免 Start 重复读文件和拼接源码。
2. 一次构建配置后 shader source 并供两个 compositor effect 共用；`ReloadConfig` 必须强制重算并同步两个 effect。

### 第 10 轮：阶段 6 共享 shader source 缓存

日期：2026-07-15

状态：**阶段 6 代码、静态回归和系统扩展部署已完成；当前 GNOME Shell 仍是部署前已加载的旧模块，必须注销重登后才能做最终门禁。**

本轮改动：

- 新增扩展级 `ConfiguredShaderCache`，在一个 GNOME Shell 会话内只读取/拼接一份基础 shader 模板。
- 使用两个 UI 配置文件的 size、mtime 和 mtime-usec 计算轻量指纹。普通 `Start` 指纹未变时直接返回同一缓存对象，不重读文件、不重做 regex/预设拼接。
- 每次真实重建会生成新 `revision`；两个 effect 共享一份 source，并在 revision 未变时连 `set_shader_source()` 也跳过，避免重复 shader 装配。
- `ReloadConfig` 使用 `get(true)` 强制重读配置、重建 source 和 revision，再同步到两个 effect。
- effect 的逐帧 uniform 路径完全未变，不重新引入阶段 1 的黑屏回归。
- 新增 `tests/gnome_shader_cache_test.py`，约束单缓存所有权、指纹命中、基础模板缓存、revision 跳过、强制 Reload 与禁止 effect 独立重建。

部署前验证：

```text
node --check effect.js / extension.js
gnome_shader_cache_test.py
linux_idle_event_driven_test.py
qml_lazy_preview_test.py
  全部通过

appBlakholeUI 构建通过
gnome_idle_monitor_tests / mpris_monitor_tests / ds09_linux_tests
  3/3 通过
git diff --check
  通过
```

- 独立 `gjs` 进程无法加载 GNOME Shell 私有 Clutter typelib，因此不将 Shell 外动态 import 作为有效验收；真实执行必须以重登后 GNOME Shell 日志和真屏为准。
- 仓库与 `/usr/share/gnome-shell/extensions/blackhole@xboxnahida.github.com` 部署哈希已一致：
  - `effect.js` = `60e23f65ad4e74d3379aa549c35668f75e4a8ea326d47b364fa848fcf1e8c7e7`
  - `extension.js` = `6386688fa9e6e0afc8292896537d503855cc604e867d388fc54d6558feb9554d`

下一轮唯一入口：

1. 注销并重新登录，不使用扩展禁用/启用代替。
2. 核对新 GNOME Shell PID、扩展 `ACTIVE` 与上述两个哈希；日志中启用时只应有一条 config 构建日志，不再是两条。
3. 执行 Start/Stop/Start：第二次 Start 不应新增 config 构建日志，且两次都必须是正常旋转黑洞。
4. 执行 `ReloadConfig`：必须新增一条 config 构建日志并保持视觉正常。修改任一配置后再 Start，指纹变化也必须自动重建。
5. 完成阶段 6 启动延迟/日志门禁和最终视觉回归后，更新本节并宣告全计划完成。

### 第 11 轮：阶段 6 重登后缓存与指纹门禁

日期：2026-07-15

状态：**阶段 6 已通过缓存命中、强制重载、指纹变化、日志、自动回归和用户真屏门禁；本计划执行完成。**

重登后现场：

- 新 GNOME Shell PID 为 102775；扩展从 `/usr/share/gnome-shell/extensions/blackhole@xboxnahida.github.com` 加载，Version 4、`ACTIVE`。
- 仓库与安装路径的 `effect.js` SHA-256 均为 `60e23f65ad4e74d3379aa549c35668f75e4a8ea326d47b364fa848fcf1e8c7e7`，`extension.js` 均为 `6386688fa9e6e0afc8292896537d503855cc604e867d388fc54d6558feb9554d`。
- 扩展 10:50:57 启用时只有一条 config 构建日志，随后创建两个 compositor effect，不再对两个 effect 各构建一次。
- 安装版 UI PID 102953 以 `--minimized` 运行；systemd scope 口径为 60.2 MiB（峰值 74.9 MiB）、10 个任务，日志中没有创建任何预览 FBO。

阶段 6 真实 D-Bus 门禁：

- 10:52:50 和 10:52:53 连续执行两次 `Start`，每次运行 2 秒后 `Stop`；两次 `Start` 都没有新的 config 构建日志，证明指纹未变时命中共享缓存。
- 10:52:56 执行 `ReloadConfig`，恰好新增一条 config 构建日志和一条 `configuration reloaded`，证明强制重建并同步两个 effect。
- 仅更新 `blackhole_advanced.txt` 的 mtime，不改文件内容；10:53:53 下一次普通 `Start` 恰好新增一条 config 构建日志，证明指纹变化能自动失效缓存。
- 三次短测都自动 `Stop`，最终 `GetState=false`；测试时间窗内没有 blackhole/shader/Mutter/GPU 错误。会话初期的 `Util.Logger.warning` JS 错误堆栈明确来自 Ubuntu AppIndicators 扩展，与本项目无关。
- 用户确认 10:52:50、10:52:53 和 10:53:53 三次短时启动全部是正常黑洞，没有黑屏；阶段 6 真屏门禁通过。

本轮自动验证：

```text
appBlakholeUI 构建通过
gnome_idle_monitor_tests / mpris_monitor_tests / ds09_linux_tests
  3/3 通过
gnome_shader_cache_test.py
linux_idle_event_driven_test.py
qml_lazy_preview_test.py
git diff --check
  全部通过
```

最终结论：

1. 保留阶段 3–6：Linux 空闲检测和 MPRIS 已改为事件驱动，后台 UI 懒加载内存达标，Start/Reload 共享 shader source 缓存已通过真实 GNOME Shell 验证。
2. 阶段 1–2 因黑幕视觉回归保持回退，没有为追求 CPU 数字牺牲黑洞外观或稳定性。
3. 因阶段 1–2 回退，GNOME Shell 稳态 CPU 相对 1.93% 基线降低 25% 的总目标未被证明；本计划以“有效、无视觉回归的优化已全部落地”收官，不宣称该 CPU 目标已达成。
4. 本计划无剩余实施步骤；后续若重启阶段 1 或 2，必须作为新一轮独立方案和真屏门禁处理。

### 第 12 轮：优化后正式资源复测与定量结论

日期：2026-07-15

状态：**已完成 3 轮校正后正式复测。UI 后台 CPU、定时唤醒和配置组装有明确改善；GNOME Shell 稳态渲染 CPU 目标未达成。**

采样校正：

- 首次后测发现两个会污染结果的现场因素：动态壁纸 `mpv` 将停止态 GNOME Shell 抬到 42%–45% CPU；UI 的 30 秒空闲触发又在“停止态”样本内自动 `Start`。这两批原始数据保存在 `/tmp/blackhole-resource-profile-final-20260715` 和 `/tmp/blackhole-resource-profile-final-clean-20260715`，但标记为无效，不用于结论。
- 已修改 `tools/profile_gnome_resources.sh`：根据当前 Mutter idle time、UI 空闲阈值和预估总时长做预检，存在自动触发风险时直接拒绝采样，不再静默产生混合状态数据。
- 校正后复测临时将 idle 阈值从 30 秒提高到 3600 秒并停用动态壁纸；连续 3 轮，每状态预热 5 秒、采样 30 秒。UI 日志证明整个采样期间没有 `IdleEligible` / `RendererRunning` 自动触发。
- 有效原始数据在 `/tmp/blackhole-resource-profile-corrected-20260715`。测试后已恢复 30 秒阈值、动态壁纸和最小化 UI。

三轮中位数（`pidstat` 口径，CPU 100% 为一个逻辑 CPU）：

| 状态 | GNOME Shell CPU | UI CPU | Shell RSS | UI RSS | UI 主动上下文切换 |
|---|---:|---:|---:|---:|---:|
| 效果停止 | 13.329% | 0.067% | 447.2 MiB | 221.7 MiB | 2.03/s |
| 频繁进出场 | 39.367% | 0.167% | 447.3 MiB | 221.8 MiB | 4.87/s |
| 效果稳定运行 | 39.919% | 0.033% | 447.3 MiB | 221.8 MiB | 2.03/s |
| 运行态延长样本 | 40.300% | 0.033% | 447.4 MiB | 221.8 MiB | 2.03/s |

GNOME Shell/GPU 解读：

- 校正后每轮“运行减停止” Shell CPU 增量分别为 33.700、27.733、26.590 个百分点，配对中位数为 **27.733 个百分点**。这没有达到立项时“默认运行不高于 1.45%”的 CPU 目标。
- 当前停止态 Shell 本身仍为 13.329% CPU，而立项基线是 0.13%；当前桌面合成环境与立项时不在同一噪声条件，所以不声称“27.733 对比 1.80”是纯代码回归幅度。但可以明确：阶段 1–2 回退后，没有保留稳态 Shell CPU 优化。
- 无动态壁纸的 GPU 中位数：停止态 7.45% SM / 236.8 MiB VRAM / 16.0 W，运行态 37.93% SM / 258.0 MiB / 22.9 W；运行增量约 **30.24 个 SM 百分点、21.2 MiB VRAM、7.11 W**。这也明显高于立项时不可分辨的 GPU 负载，不宣称 GPU 有优化。
- Shell RSS 在当前同轮停止/运行间只差约 0.1 MiB，仍无可测效果增量；Shell 整体 RSS 比立项时高约 67 MiB，属于整个长时间 GNOME Shell 会话，不全归因于本项目。

已证明的优化数值：

1. **UI 后台 CPU：**立项基线 0.30%，校正后运行/最小化中位数 0.033%，下降约 **89%**；停止态中位数 0.067%，也低于 0.10% 目标。
2. **固定轮询：**Linux 遗留 idle 检查从 1 次/秒降为 0，MPRIS `ListNames` 从 1 次/2 秒降为 0；合计删除 **1.5 次/秒**的固定业务轮询。
3. **UI 线程/任务：**11 降至 10，下降 1（约 **9%**）。
4. **UI cgroup 内存：**阶段 5 前同口径参考为 69.5 MiB（峰值 81.1 MiB），优化后长时间后台实测为 60.2 MiB（峰值 74.9 MiB），分别下降 **9.3 MiB / 13.4%** 和 **6.2 MiB / 7.6%**。但 `pidstat` RSS 从立项的 203.4 MiB 升至当前 221.8 MiB；因 RSS 与 cgroup charged memory 口径不同，不声称“RSS 低于 120 MiB”已达成。
5. **shader 配置组装：**扩展启用时的配置/源码构建从 2 份降为 1 份（**-50%**）；配置未变的后续 `Start` 从 2 份降为 0（**-100%**）；`ReloadConfig` 的配置读取/拼接从 2 份降为 1 份（**-50%**）。

定量收官：

- Token 并非没有产生收益：后台 UI CPU、业务轮询、cgroup 内存和 Start 配置组装都有可量化改善。
- 但最大的稳态 GNOME Shell/GPU 渲染成本没有下降；这是本轮未完成的性能目标，不用 UI 改善数字掩盖。

### 第 13 轮：渲染续篇路线 B 单 Clutter Timeline

日期：2026-07-15

状态：**路线 B 的短时和完整视觉门禁通过，但三轮性能门禁明显失败；已只回退
路线 B 并将路线 A 重新部署到系统目录，等待注销重登后的最终加载确认。**

本轮细节与完整回退/验收入口记录在：

```text
docs/superpowers/plans/2026-07-15-blackhole-rendering-optimization.md
  -> 13.2 路线 B：Clutter Timeline 单帧驱动
```

本轮核心结果：

- 两个 effect 的 16 ms `GLib.timeout_add()` 已删除，改为扩展级唯一、绑定
  `global.window_group` 帧时钟的 `Clutter.Timeline`。
- 两个 effect 的转场在同一 `new-frame` 回调中使用同一单调时间推进；
  路线 A 保留的 8 个必要 uniform 的 `vfunc_paint_target()` 生命周期未改。
- 新增并注册 `gnome_timeline_frame_driver_test.py`；相关 CTest 3/3 与现有 GNOME/UI
  静态回归全部通过，`git diff --check` 通过。
- 部署后仓库/系统哈希：`effect.js` =
  `510c62f86611a953160269fc77293a1e1d91319c66cd82c824cd67897b45bc97`，
  `extension.js` =
  `0e91cb8c42dbba56b002e3850c20f4a093b4dfd2ada094b333703743b98fa852`。
- 路线 A 部署版独立备份在 `/tmp/blackhole-route-b-backup/`。

重登后新 GNOME Shell PID 为 31138，扩展 Version 4、`ACTIVE`，仓库与系统安装版
路线 B 哈希一致。13:06:01--13:06:03 的 2 秒自动启停最终 `GetState=false`，日志
无 Timeline、Clutter、shader、Mutter、GPU 或本项目 JS 错误；重登后自动回归也
全部通过。

用户已确认 13:06:01--13:06:03 的短时和 13:07:54--13:09:24 的 90 秒真屏正常。
正式三轮原始数据在 `/tmp/blackhole-resource-profile-route-b-20260715`：Shell 运行减
停止的配对增量为 35.571、35.988、36.484 个百分点，中位数 35.988；相比路线 A
的 24.105 反而增加约 49.3%。GPU SM 配对增量中位数为 31.862，相比路线 A 增加
约 13.9%；功耗增量中位数 11.069 W，相比路线 A 增加约 60.5%。路线 B 性能门禁
失败。

已删除 Timeline 生产实现、对应测试与 CTest 注册，恢复路线 A 的 effect 级 16 ms
驱动；自动验证通过。仓库与系统安装版路线 A 哈希为 `effect.js` =
`27a343681d95495de5420b13c08154faf4dc509ef583c9ff620ac27f1a69f4ae`、
`extension.js` =
`6386688fa9e6e0afc8292896537d503855cc604e867d388fc54d6558feb9554d`。

用户最终决定止于路线 A：路线 B 不保留，路线 C 不实施，不再编写或部署 B/C
生产代码。下一轮唯一入口是重启后核对新会话实际加载路线 A，并执行 2 秒自动启停、
用户真屏和日志确认；通过后整个项目收官，不再开启其他优化路线。

### 第 14 轮：首次启动 shader 透明预热

日期：2026-07-15

状态：**预热修复已完成代码、全套自动测试和系统扩展部署；当前 GNOME Shell
仍缓存部署前模块，必须注销重登后验证系统首次 Start 是否不再闪黑。**

现象与修复：

- 路线 A 部署后两次重登均确认：系统会话内第一次 Start 在黑洞出现前固定短暂闪黑，
  后续 Start 不再复现。
- 两个生产 `ShaderEffect` 原先在登录时创建后立即禁用，首次真实绘制/编译与
  `Start()` 切出 fullscreen direct scanout 发生在同一时窗。
- 新增 `prewarm()`：扩展启用后让两个真实生产 effect 各以 `uTransition=0` 绘制一帧；
  shader 输出严格透传原桌面，完成 `paint_target` 后才在 idle 回调中自动禁用。
- 真实 Start/Stop 和 disable 都会取消尚未完成的预热回调，不留后台 effect 或周期源。
- 新增并注册 `tests/gnome_shader_prewarm_test.py`，约束零转场透传、生产 effect 所有权、
  先绘制后禁用和 Start/Stop 取消路径。

自动验证：

```text
appBlakholeUI / GNOME 扩展 / 全部测试目标构建通过
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
  15/15 通过
gnome_shader_cache_test.py / gnome_uniform_usage_test.py /
gnome_shader_prewarm_test.py / linux_idle_event_driven_test.py /
qml_lazy_preview_test.py / node --check / bash -n / git diff --check
  全部通过
```

部署后仓库/系统哈希一致：

- `effect.js` = `6f746a3cbdad78570bd6fddd4ef486b7077259bc381e1e63dd45172dbd2a4503`
- `extension.js` = `57790d703b15a0ffa5494d881a81b8a3f3c14bababde173a0dec51bae073e51e`

下一轮唯一入口：

1. 注销并重新登录，不用扩展禁用/启用代替。
2. 核对新 GNOME Shell PID、Version 4、`ACTIVE` 和上述两个哈希。
3. 日志必须先出现两条 `compositor shader effect prewarmed`，且预热期间桌面不闪黑。
4. 只执行一次 2 秒 Start/Stop，以用户真屏确认该会话首次 Start 无闪黑、正常旋转；
   核对最终 `GetState=false`、direct scanout 已恢复且无新错误。
5. 若预热登录期间反而闪黑，或系统首次 Start 仍闪黑，只回退本轮预热修复；保留路线 A。

### 第 15 轮：首次启动预热门禁失败与精确回退

日期：2026-07-15

状态：**透明预热未消除首次 Start 闪黑，已只回退本轮预热实现并重新部署预热前路线 A；等待再次注销重登后的最终加载与真屏确认。**

重登后门禁结果：

- 新 GNOME Shell PID 为 4277；系统路径扩展 Version 4、`ACTIVE`，仓库与系统安装版加载前哈希为 `effect.js` = `6f746a3c...a4503`、`extension.js` = `57790d70...e51e`。
- 13:39:33 日志按预期出现两条 `compositor shader effect prewarmed`，没有本项目 shader、Mutter、GPU 或 JS 错误。
- 13:42:34 执行本会话唯一一次 2 秒 `Start`，13:42:36 自动 `Stop`；最终 `GetState=false`，fullscreen direct scanout 已恢复，控制链和日志正常。
- 用户真屏确认：系统会话首次启动黑洞仍有一次黑屏闪屏。由此判定预热方案视觉门禁失败，不能保留，也不能据此提交或上传。

精确回退与验证：

- 删除 `BlackholePrototypeEffect.prewarm()`、预热状态/idle 回调、Start/Stop 取消逻辑和扩展启用时的预热调用；逐帧 8 个必要 uniform、路线 A、阶段 3--6 均未改动。
- 删除仅约束失败实现的 `tests/gnome_shader_prewarm_test.py` 及其 CTest 注册。
- 回退后恢复到预热前路线 A 的记录哈希：`effect.js` = `27a343681d95495de5420b13c08154faf4dc509ef583c9ff620ac27f1a69f4ae`，`extension.js` = `6386688fa9e6e0afc8292896537d503855cc604e867d388fc54d6558feb9554d`。
- 回退版已部署到 `/usr/share/gnome-shell/extensions/blackhole@xboxnahida.github.com`，仓库与系统文件哈希一致。
- `appBlakholeUI`、GNOME 扩展及全部测试目标构建通过；`QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure` 为 14/14 通过。
- `gnome_shader_cache_test.py`、`gnome_uniform_usage_test.py`、`linux_idle_event_driven_test.py`、`qml_lazy_preview_test.py`、JS/脚本语法和 `git diff --check` 全部通过。

下一轮唯一入口：

1. 再次注销并重新登录，不使用扩展禁用/启用替代。
2. 核对新 GNOME Shell PID、Version 4、`ACTIVE` 和上述预热前路线 A 两个哈希；登录日志不应再出现 `prewarmed`。
3. 只执行一次 2 秒 `Start/Stop`，由用户确认除已知的系统会话首次 Start 闪屏外，黑洞外观、旋转和停止恢复均正常；核对最终 `GetState=false`、direct scanout 恢复且无本项目错误。
4. 透明预热作为失败实验永久保持回退。最终门禁通过后再更新本节、提交当前完整工作区改动并推送到用户 GitHub `origin/port/ubuntu-wayland`。

### 第 16 轮：路线 A 最终重登验收与已知限制收口

日期：2026-07-15

状态：**最终重登门禁已通过；会话首次 Start 的短暂黑闪由用户决定作为当前版本已知小问题保留。**

最终重登现场：

- 新 GNOME Shell PID 为 14711，使用 Mutter 50.1 / Wayland；扩展从 `/usr/share/gnome-shell/extensions/blackhole@xboxnahida.github.com` 加载，Version 4、`ACTIVE`。
- 仓库与系统安装版哈希一致：`effect.js` = `27a343681d95495de5420b13c08154faf4dc509ef583c9ff620ac27f1a69f4ae`，`extension.js` = `6386688fa9e6e0afc8292896537d503855cc604e867d388fc54d6558feb9554d`。
- 新会话日志没有 `compositor shader effect prewarmed`，证明预热失败实验已经精确回退并加载生效。
- 30 秒空闲规则在 13:49:55 自然触发本会话首次 Start；用户确认该次仍出现一次短暂黑闪。该现象在本轮透明预热前已存在，且预热未能消除。
- 13:50:58--13:51:00 完成受控 2 秒 `Start/Stop`；最终 `GetState=false`，13:51:01 fullscreen direct scanout 已恢复，时间窗内无 blackhole、shader、Mutter、GPU 或本项目 JS 错误。

最终自动回归：

```text
cmake --build build -j4
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
  14/14 通过

gnome_shader_cache_test.py / gnome_uniform_usage_test.py /
linux_idle_event_driven_test.py / qml_lazy_preview_test.py /
node --check / bash -n / git diff --check
  全部通过
```

最终决定：

1. 保留路线 A 及阶段 3--6；路线 B/C 不再实施，透明预热保持回退。
2. 每次 GNOME Shell 会话首次 Start 可能短暂全屏黑闪，同会话后续 Start 和 Stop 恢复正常。用户决定将其写入 `README.md` 作为非阻断已知限制，不再为此增加渲染复杂度。
3. 当前生产路径、自动回归、安装版加载和重登真机门禁均已收口；本计划无剩余实施步骤。
