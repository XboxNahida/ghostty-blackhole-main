# Blackhole GNOME 渲染性能优化候选计划

## 1. 状态与授权边界

本文档原为待用户审查的候选计划。用户已于 2026-07-15 先后授权实施路线 A
与路线 B；路线 A 已通过重登、完整视觉和定量性能门禁并正式保留。
路线 B 的视觉门禁通过，但配对 CPU、GPU 和功耗均明显回退，已判定失败并从
仓库与系统安装目录回退为路线 A；当前 GNOME Shell 仍缓存路线 B，等待注销重登
完成回退加载确认。用户已决定最终止于路线 A：路线 B 不保留，路线 C 不实施，
不再编写或部署任何 B/C 生产代码。

路线 A 实施与验收期间遵循了以下边界：

- 除路线 A 外，不修改其他 GNOME Shell 扩展渲染路径。
- 只部署路线 A，不在同一次注销重登中混入路线 B、C。
- 不调整 shader 分辨率、帧率、光线积分步数或黑洞视觉参数。
- 不创建 Git 提交，不推送 GitHub。

## 2. 已知基线

优化后的校正测量原始数据位于：

```text
/tmp/blackhole-resource-profile-corrected-20260715
```

测量条件：

- 临时停用 Gnome Live Wallpaper，排除 `mpv` 视频解码和持续 damage。
- UI 空闲阈值临时从 30 秒提高到 3600 秒，避免“停止态”内自动启动效果。
- 每个状态预热 5 秒、采样 30 秒，重复 3 轮，使用中位数。
- CPU 100% 等于占满一个逻辑 CPU。

当前有效基线：

| 指标 | 停止效果 | 稳定运行 | 运行增量 |
|---|---:|---:|---:|
| GNOME Shell CPU | 13.329% | 39.919% | 配对中位数 27.733 个百分点 |
| GNOME Shell RSS | 447.2 MiB | 447.3 MiB | 约 0.1 MiB |
| GPU SM | 7.45% | 37.93% | 约 30.24 个百分点 |
| VRAM | 236.8 MiB | 258.0 MiB | 约 21.2 MiB |
| GPU 功耗 | 16.0 W | 22.9 W | 约 7.11 W |

本计划使用上述校正基线，不再与立项时 0.13% / 1.93% 的不同桌面噪声环境直接比较。

## 3. 上轮黑屏的原因判定

### 3.1 证据链

1. 阶段 1 将大部分 uniform 从 `vfunc_paint_target()` 移到 shader 装载、配置重载或 `prepareStart()`。
2. 阶段 2 又将两个 16 ms 定时器合并成扩展级共享驱动。
3. 真机显示桌面和普通窗口变成全屏黑幕，Dock 和 top bar 仍可见；无 shader、JS、Mutter 或 GPU 错误。
4. 只回退阶段 2 后黑屏仍存在。
5. 继续回退阶段 1、恢复在 `vfunc_paint_target()` 内设置全部 uniform 后，用户真屏确认正常黑洞恢复。

### 3.2 结论

黑屏主因是阶段 1 破坏了 `Clutter.ShaderEffect` 的 uniform 绘制生命周期。Clutter 的约定是：ShaderEffect 子类应在 `paint_target` 内设置当次绘制使用的 uniform，再调用父类实现。

阶段 2 共享驱动和阶段 1 当时同时存在。因为只回退阶段 2 不能恢复画面，阶段 2 并未被单独证明会导致黑屏。

## 4. 优化约束

所有候选改动必须同时满足：

- 不改黑洞大小、颜色、亮度、旋转、游走路径、出生位置和预设过渡。
- 不降低分辨率、帧率、`N_STEPS` 或 shader 数值精度。
- 不恢复会产生静态桌面截图的 `Clutter.Clone` overlay。
- 不扭曲 Dock、top bar、锁屏或 GNOME Shell UI。
- 不依赖 GNOME Shell 50 之外的兼容性。
- 任一阶段有黑屏、静帧、边界缝、跳位、转速变化或新日志错误时，只回退当前阶段。

## 5. 候选路线 A：删除未使用的每帧 uniform 设置

### 5.1 动机

当前每个 effect 每帧调用 19 次 `set_uniform_value()`。其中以下 11 个名称只在 `effect.js` 中被设置，但不被当前 compositor `blackhole.glsl` 引用：

```text
uHoleRadius
uDiskGain
uDiskTemp
uExposure
uSpeed
uStarGain
uDiskIncl
uBornProgress
uDistortion
uSlotSec
uPresetOffset
```

删除这些无效设置不等于将必要 uniform 移出绘制周期。下列 8 个实际使用的 uniform 仍必须在每次 `vfunc_paint_target()` 中设置：

```text
iTime
uResolutionX
uResolutionY
iChannel0
uTransition
uHomeX
uHomeY
uRandPhase
```

### 5.2 实施

1. 添加静态回归测试，解析 compositor shader 及 `effect.js`，约束每帧只设置实际引用的 uniform。
2. 从 `vfunc_paint_target()` 删除上述 11 个无效 setter。
3. 从 `compositor_header.glsl` 删除仅服务历史共享 shader 路径、且 compositor 不使用的 uniform 声明；若删声明增加风险，可只删 JS setter。
4. 不改动必要 uniform 的绘制时机。

### 5.3 理论收益

- 两个 effect 的 setter 合计从 38 次/帧降至 16 次/帧，减少约 58%。
- 只降低 JS → GObject 调用和 uniform 查找/提交开销，不改变 shader 主体 GPU 计算。
- 工程预估：黑洞导致的 Shell CPU 增量降低约 3%–10%；GPU SM 和功耗改善应接近采样噪声。

### 5.4 门禁

- `node --check` 通过。
- 新 uniform 静态回归测试通过。
- 部署后必须注销重登。
- D-Bus 最长 2 秒 `Start` 后自动 `Stop`，由用户真屏确认正常旋转黑洞。
- 出生、游走、16 预设、5.3 秒过渡、650 ms 进出场和窗口动态全部不变。
- 按第 8 节重测。若配对 Shell CPU 增量没有至少 3% 的可重复改善，回退路线 A。

## 6. 候选路线 B：使用单一 Clutter 帧时钟驱动两个 effect

### 6.1 动机

当前两个 effect 分别使用 16 ms `GLib.timeout_add()`，每秒合计约 125 次 JS 回调，并可能与 Mutter 帧时钙错相。

Clutter Timeline 是与 frame clock 绑定的动画驱动机制，`new-frame` 在新帧绘制前触发，比固定 16 ms GLib timer 更符合 Mutter 绘制生命周期。

### 6.2 实施前提

- 只在路线 A 通过并保留后开始。
- 不再尝试“共享 GLib 16 ms timer”。
- 不移动、缓存或减少必要 uniform 在 `vfunc_paint_target()` 内的提交。

### 6.3 实施

1. 扩展级创建一个绑定到实际 actor/frame clock 的 `Clutter.Timeline`。
2. 单个 `new-frame` 回调用同一单调时间更新两个 effect 的转场状态，然后对两个 actor 各调用一次 `queue_redraw()`。
3. `Start` 时启动 Timeline；Stop 退场完成后立即停止并释放帧源。
4. 扩展 `disable()` 必须无条件断开 `new-frame` signal、停止 Timeline 并立即禁用两个 effect。
5. 添加静态回归测试，约束生产路径不再存在 effect 级 `GLib.timeout_add()`。

### 6.4 理论收益

- 帧驱动 JS 回调从约 125 次/秒降为屏幕实际刷新率；当前 60 Hz 环境约为 60 次/秒，减少约 52%。
- 两个 actor 仍需各自绘制一次，因此不预期 GPU shader 成本大幅改善。
- 工程预估：在路线 A 基础上，Shell CPU 增量再降低约 2%–8%。

### 6.5 门禁

- 只允许一个生产帧驱动源。
- Stop 700 ms 后无 Timeline、GLib timer 或周期重绘残留。
- 注销重登后先执行 2 秒真屏门禁，任何黑屏立即回退路线 B，保留已通过的路线 A。
- 转场时长、旋转速度、预设时序和游走路径不变。
- 配对 Shell CPU 增量须在路线 A 结果上再有至少 2% 的可重复改善，否则回退路线 B。

## 7. 候选路线 C：shader 可见影响区域早退

### 7.1 动机

当前 compositor shader 对两个全屏 actor 各执行一次。远离黑洞、最终畸变低于可见阈值的像素，仍会执行远场计算、星场计算和多次桌面纹理采样。

这是唯一可能同时显著降低 GPU SM、功耗和 GNOME Shell 驱动提交成本的候选路线。

### 7.2 实施前提

- 只在路线 A、B 都完成门禁后开始。
- 先建立可重复的截图/像素差分验收，再改 shader。
- 不修改 `N_STEPS=48`，不降低分辨率、帧率或精度。

### 7.3 分步实施

#### C1：只优化远场纹理采样

1. 当色差偏移低于定量阈值时，将 RGB 三通道分别采样收敛为一次纹理采样。
2. 保留黑洞附近可见色差区域的原三通道路径。
3. 先单独测量 C1，不同时加入空间早退。

#### C2：低于可见阈值的像素直接返回桌面颜色

1. 使用已有的距离窗 `window` 和屏蔽因子计算保守早退条件。
2. 阈值从极小值开始，通过截图差分确认黑洞可见边界没有环形缝或明暗跳变。
3. 不在近场光线追踪区域中减少积分步数。

### 7.4 理论收益

- 路线 C 单独预计降低 GPU/渲染增量约 10%–30%。
- 预计当前 GPU SM 增量可从约 30.24 个百分点降至约 21–27 个百分点。
- 预计附加功耗可从约 7.11 W 降至约 5–6.5 W。
- 该范围是工程预估，必须以实测和真屏结果为准。

### 7.5 门禁

- 截图差分必须把过渡中的正常时间变化与空间回归分开，不得用不同 `iTime` 的帧直接相减。
- 最少覆盖四角出生、屏幕中心、屏幕边缘、16 预设与普通窗口动态。
- 用户真屏不得看到畸变边界、环形缝、色差突变或星场突然消失。
- GPU SM 或功耗增量必须至少有 10% 的可重复改善；否则回退路线 C。

## 8. 每阶段的统一测量方法

### 8.1 采样前

1. 确认 GNOME Shell PID、扩展实际加载路径、Version 和 `ACTIVE`。
2. 核对仓库与 `/usr/share/gnome-shell/extensions/blackhole@xboxnahida.github.com/` 的 JS/shader SHA-256。
3. 确认已注销重登，不使用扩展禁用/启用代替重载。
4. 临时停用 Gnome Live Wallpaper，确认没有 wallpaper `mpv` 进程。
5. 将 UI idle 阈值临时提高到 3600 秒，重启安装版 UI，确认日志中无自动 `IdleEligible` / `RendererRunning`。
6. 确认采样脚本预检通过。

### 8.2 正式采样

```bash
tools/profile_gnome_resources.sh <本阶段的唯一结果目录>
```

要求：

- 每状态预热 5 秒、采样 30 秒、3 轮。
- 分别保留停止、频繁进出场、稳定运行和运行延长样本。
- 以“每轮运行减同轮停止”的配对差值为主，取 3 轮中位数。
- 同时记录 CPU、RSS、上下文切换、GPU SM、VRAM 和功耗。
- 不挑选单轮最优值，不与不同桌面噪声环境的旧绝对值强行比较。

### 8.3 恢复

无论采样通过、失败或中断，都必须：

1. 恢复 UI idle 阈值 30 秒。
2. 恢复 Gnome Live Wallpaper 并确认 `mpv --stop-screensaver=no`。
3. 恢复采样前的黑洞启停状态。
4. 确认安装版 UI 仍以 `--minimized` 后台运行。

## 9. 分阶段决策与回退策略

```text
路线 A：删无用 uniform setter
  ├─ 视觉通过 + CPU 改善 >= 3% → 保留，进入 B
  └─ 否则 → 只回退 A，停止本轮

路线 B：Clutter Timeline 单帧驱动
  ├─ 视觉通过 + 额外 CPU 改善 >= 2% → 保留，进入 C
  └─ 否则 → 只回退 B，保留 A

路线 C1/C2：shader 远场采样与空间早退
  ├─ 视觉通过 + GPU/功耗改善 >= 10% → 保留
  └─ 否则 → 只回退 C，保留 A/B
```

禁止在一次重登中同时部署两条尚未验收的路线。每条路线都必须能独立测量、独立回退。

## 10. 总体预期

在 A、B、C 全部通过的理想情况下：

- 黑洞导致的 GNOME Shell CPU 增量预计总体下降约 15%–35%。
- 以当前 27.733 个百分点的配对增量估算，可能降至约 18–24 个百分点。
- GPU SM 增量可能从约 30.24 个百分点降至约 20–26 个百分点。
- 附加功耗可能从约 7.11 W 降至约 4.5–6 W。

上述是工程预估，不是承诺值。在不降低分辨率、帧率、`N_STEPS` 或视觉质量的前提下，不把 GNOME Shell 运行态降到 1.45% 作为现实承诺。

## 11. 不建议的路线

- 不再将必要 uniform 移出 `vfunc_paint_target()`。
- 不以减少 `N_STEPS`、降低分辨率、降低帧率或关闭色差作为默认优化。
- 不将背景组和窗口组盲目挂到会包含 GNOME Shell UI 的公共父 actor 上。
- 不恢复 Clone overlay 或历史 Portal/fullscreen renderer。
- 不同时合并 A、B、C 后只做一次重登验收。

## 12. 审查后的可选决定

用户审查本文档后可以选择：

1. **仅实施路线 A**：最低风险，先验证删除无效 setter 的实际收益。
2. **按 A → B → C 完整执行**：每个阶段单独重登、测量、保留或回退。
3. **不再优化渲染**：以当前阶段 3–6 成果收尾，经最终验证后提交并推送用户 GitHub fork。

路线 A 已获授权并通过；路线 B 已获授权、已部署并等待重登门禁；路线 C
仍停留在候选状态。

## 13. 实施记录

### 13.1 路线 A：删除未使用的每帧 uniform 设置

- 授权时间：2026-07-15。
- 最终状态：**已落地成功并正式保留。** 路线 B、C 尚未授权、未修改、未部署。
- 前置环境：用户已关闭 Gnome Live Wallpaper，将 UI 空闲阈值从 30 秒临时提高到
  3600 秒，并停用黑洞。
- 部署时状态：代码与测试完成后先部署，随后通过注销重登加载路线 A，再执行真屏
  门禁、完整视觉回归和资源复测。
- 本轮范围：只实施路线 A；不实施路线 B、C。
- 计划改动：新增并注册到 CTest 的静态 uniform 回归测试；从
  `vfunc_paint_target()` 删除 11 个未使用 setter；从 compositor header 删除对应的
  未使用声明；保留 8 个实际使用 uniform 的每帧提交时机。
- 实际代码改动：
  - `effect.js` 的每 effect 每帧 setter 从 19 个降为 8 个；两个 effect 合计从
    38 个降为 16 个。
  - 保留 `iTime`、`uResolutionX`、`uResolutionY`、`iChannel0`、`uTransition`、
    `uHomeX`、`uHomeY`、`uRandPhase`，且仍全部在 `vfunc_paint_target()` 内提交。
  - 删除 `uHoleRadius`、`uDiskGain`、`uDiskTemp`、`uExposure`、`uSpeed`、
    `uStarGain`、`uDiskIncl`、`uBornProgress`、`uDistortion`、`uSlotSec`、
    `uPresetOffset` 的 setter 和 compositor header 声明。
  - 新增 `tests/gnome_uniform_usage_test.py`，并在 `BUILD_TESTING` 下注册为
    `gnome_uniform_usage_test`。
- 自动化验证（2026-07-15）：
  - `node --check gnome-extension/blackhole@xboxnahida.github.com/effect.js`：通过。
  - `python3 tests/gnome_uniform_usage_test.py`：通过。
  - `python3 tests/gnome_shader_cache_test.py`：通过。
  - `cmake --build build --target blackhole-gnome-extension shader_source_tests -j2`：
    通过。
  - `ctest --test-dir build --output-on-failure -R
    '^(gnome_uniform_usage_test|shader_source_tests)$'`：2/2 通过。
  - 路线 A 相关文件 `git diff --check`：通过。
- 部署记录（2026-07-15 12:03 CST）：
  - 只部署 `effect.js` 与 `shaders/compositor_header.glsl`；部署前确认其余扩展文件与
    当前系统安装版一致，没有夹带其他未部署改动。
  - 系统路径：`/usr/share/gnome-shell/extensions/blackhole@xboxnahida.github.com/`。
  - `effect.js` SHA-256：
    `27a343681d95495de5420b13c08154faf4dc509ef583c9ff620ac27f1a69f4ae`。
  - `compositor_header.glsl` SHA-256：
    `c50d3d4644d76ef2d8aae90e0e016e87d2483900302e79f15396d0474251aa77`。
  - 两个系统文件均已校正为 `0644 nobody:nogroup`。
  - 路线 A 部署前版本临时备份于 `/tmp/blackhole-route-a-backup/`，用于本轮独立回退。
- 重登门禁（已完成）：用户注销并重新登录后，核对 GNOME Shell 50.1、扩展加载路径、
  Version 4、`ACTIVE` 与上述 SHA-256；然后执行最长 2 秒 Start 后自动 Stop，并由用户
  真屏确认无黑屏、静帧、边界缝、跳位或转速变化。
- 注销重登结果、2 秒 Start/Stop、完整视觉回归、资源复测与最终保留决定均已记录
  于下文。

#### 重登后短时门禁（2026-07-15 12:29 CST）

- GNOME Shell 已在本次重登后以 PID 4299 运行，Mutter 50.1 / Wayland；扩展实际从
  `/usr/share/gnome-shell/extensions/blackhole@xboxnahida.github.com` 加载，Version 4、
  `ACTIVE`。
- 仓库与系统安装版 `effect.js` SHA-256 均为
  `27a343681d95495de5420b13c08154faf4dc509ef583c9ff620ac27f1a69f4ae`；
  `compositor_header.glsl` 均为
  `c50d3d4644d76ef2d8aae90e0e016e87d2483900302e79f15396d0474251aa77`。
- `gnome_uniform_usage_test.py`、`gnome_shader_cache_test.py`、`node --check` 以及 CTest
  中的 `gnome_uniform_usage_test` / `shader_source_tests` 均通过。
- 12:29:12 通过 session D-Bus 启动效果，12:29:14 自动停止，最终 `GetState=false`；
  12:29:15 fullscreen direct scanout 已恢复。本次时间窗没有新增 blackhole shader、
  Mutter 或 GPU 错误。12:26:49 的 `Util.Logger.warning` 来自 Ubuntu AppIndicators，
  与本次门禁无关。
- 短时门禁控制链与日志通过；用户报告首次启动前有一次黑屏闪烁，但随后三次短启停
  均未再复现，最终完整视觉门禁见下文。

#### 路线 A 正式资源复测（2026-07-15 12:33--12:40 CST）

- 用户报告重登后的第一次短启动在黑洞出现前有一次黑屏闪烁；随后连续执行三次
  2 秒 Start/Stop 均未再复现，用户确认“刚刚没有闪了”。暂记为首次 shader / direct
  scanout 预热观察项，不作为稳定视觉回归。
- 正式原始数据保存在
  `/tmp/blackhole-resource-profile-route-a-20260715`。Gnome Live Wallpaper 已停用，
  黑洞原始状态为 stopped；采样期间 UI 进程未运行，因此没有空闲自动触发，也不将
  本批数据用于评价 UI。GNOME Shell / GPU 的停止、转场和运行样本有效。
- 三轮 GNOME Shell CPU（`pidstat`，100% 为一个逻辑 CPU）：
  - round 1：停止 21.133%，运行 39.633%，配对增量 18.500 个百分点；
  - round 2：停止 14.795%，运行 38.987%，配对增量 24.192 个百分点；
  - round 3：停止 14.961%，运行 39.067%，配对增量 24.105 个百分点。
- 运行减停止的配对增量中位数为 **24.105 个百分点**；相对路线 A 前校正基线
  27.733 个百分点降低约 **13.1%**，超过 3% 保留门槛。
- GPU SM 配对增量三轮约为 21.483、28.035、27.965 个百分点，中位数约
  **27.965**；相对路线 A 前 30.24 约低 7.5%，仅作参考。功耗配对增量中位数约
  6.90 W。
- 采样后已恢复 `blackhole_presets.txt` 的 30 秒空闲阈值、黑洞 stopped 状态和安装版
  UI 后台运行。路线 A 的代码、短时真屏和 CPU 定量门禁通过后，继续执行了覆盖
  16 预设的约 90 秒完整视觉时序回归。
- 第一轮 90 秒视觉测试被恢复后的 30 秒空闲自动化在 12:42:43 中途 Stop/Start，
  因此不计入完整回归。随后临时停止 UI，在 12:44:55--12:46:25 完成一次只有单次
  Start/Stop 的干净 90 秒序列；日志无 shader、Mutter、GPU 或本项目 JS 错误，最终
  `GetState=false`，fullscreen direct scanout 已恢复。
- 干净回归结束后安装版 UI 已恢复为 PID 15005、`--minimized`，持久配置第二行重新
  确认为 30 秒；用户随后完成该 90 秒真屏确认。
- 用户随后确认 12:44:55--12:46:25 的连续 90 秒真屏显示正确、没有问题；出生、
  游走、16 预设、5.3 秒过渡、窗口动态和停止转场的最终视觉门禁通过。
- 最终判定：路线 A 将两个 compositor effect 的每帧 uniform setter 合计从 38 次
  降至 16 次，GNOME Shell 配对 CPU 增量中位数从 27.733 降至 24.105 个百分点，
  改善约 13.1%；代码、自动测试、重登加载、短时启停、完整视觉和性能门禁全部通过。
  保留路线 A。用户后续已单独授权路线 B，其实施记录见 13.2；路线 B
  仍必须再次独立重登、视觉和性能验收，不得与路线 C 合并部署。

### 13.2 路线 B：Clutter Timeline 单帧驱动

- 授权时间：2026-07-15；用户在路线 A 完整门禁通过并注销重登后明确要求
  开始落地路线 B。
- 当前状态：**路线 B 的短时与 90 秒完整视觉门禁均通过，但正式三轮性能门禁
  明显失败；路线 B 已从仓库和系统安装目录回退为路线 A，等待注销重登后确认
  GNOME Shell 实际加载回退版。**
- 部署前会话基线：
  - 新 GNOME Shell 会话日志显示 PID 22766、Mutter 50.1；12:51:12 成功加载
    两个 compositor effect。
  - 路线 A 安装版与仓库基线哈希一致：`effect.js` =
    `27a343681d95495de5420b13c08154faf4dc509ef583c9ff620ac27f1a69f4ae`，
    `extension.js` =
    `6386688fa9e6e0afc8292896537d503855cc604e867d388fc54d6558feb9554d`。
- 实际代码改动：
  - 从 `BlackholePrototypeEffect` 删除 `_redrawId` 和 effect 级 16 ms
    `GLib.timeout_add()`。
  - 在扩展级只创建一个绑定 `global.window_group` 的无限重复
    `Clutter.Timeline`；只连接一个 `new-frame` 回调。
  - 单次帧回调只读取一次 `GLib.get_monotonic_time()`，使用同一时间推进
    两个 effect 的转场并分别 `queue_redraw()`。
  - `Start` 启动 Timeline；`Stop` 的 650 ms 退场完成后停止 Timeline；
    `disable()` 无条件断开 signal、停止并释放 Timeline，再立即禁用两个
    effect。
  - 路线 A 保留的 8 个必要 uniform 仍全部在 `vfunc_paint_target()` 中每帧提交；
    shader、视觉参数和 `TRANSITION_DURATION_US=650000` 未修改。
  - 新增 `tests/gnome_timeline_frame_driver_test.py`，并在 CTest 中注册。
- 自动验证：
  - `node --check effect.js / extension.js`：通过。
  - `gnome_timeline_frame_driver_test.py` / `gnome_uniform_usage_test.py` /
    `gnome_shader_cache_test.py` / `linux_idle_event_driven_test.py` /
    `qml_lazy_preview_test.py`：全部通过。
  - `blackhole-gnome-extension` 与 `shader_source_tests` 构建通过；CTest 中
    `gnome_timeline_frame_driver_test` / `gnome_uniform_usage_test` /
    `shader_source_tests` 3/3 通过。
  - `git diff --check`：通过。
- 独立部署与回退（2026-07-15 12:59 CST）：
  - 只部署 `effect.js` 与 `extension.js`，未部署路线 C 或其他运行时文件。
  - 仓库与 `/usr/share/gnome-shell/extensions/blackhole@xboxnahida.github.com/`
    哈希已一致：`effect.js` =
    `510c62f86611a953160269fc77293a1e1d91319c66cd82c824cd67897b45bc97`，
    `extension.js` =
    `0e91cb8c42dbba56b002e3850c20f4a093b4dfd2ada094b333703743b98fa852`。
  - 系统文件为 `0644 nobody:nogroup`。路线 A 版本独立备份在
    `/tmp/blackhole-route-b-backup/`。
- 下一轮唯一入口：
  1. 注销并重新登录，不使用扩展禁用/启用代替。
  2. 核对新 GNOME Shell PID、扩展 `ACTIVE` 与上述路线 B 哈希，并先检查
     Timeline 构造、signal 和 JS/shader/Mutter 日志错误。
  3. 通过 D-Bus 最长 `Start` 2 秒后自动 `Stop`，以用户真屏为最终标准；
     任何黑屏、静帧、边界缝或转速变化都立即只回退路线 B。
  4. 短时门禁通过后，验证出生、游走、16 预设、5.3 秒过渡、650 ms 进出场、
     窗口动态和 Stop 700 ms 后无周期重绘。
  5. 以路线 A 配对 Shell CPU 增量中位数 24.105 个百分点为基准，按第 8 节
     独立重测；路线 B 必须再有至少 2% 可重复改善才保留。

#### 路线 B 重登后短时门禁（2026-07-15 13:06 CST）

- 新 GNOME Shell 会话 PID 为 31138，启动于 13:03:13，Mutter 50.1 / Wayland；
  扩展实际从 `/usr/share/gnome-shell/extensions/blackhole@xboxnahida.github.com`
  加载，Version 4、`ACTIVE`。
- 仓库与系统安装版哈希一致：`effect.js` =
  `510c62f86611a953160269fc77293a1e1d91319c66cd82c824cd67897b45bc97`，
  `extension.js` =
  `0e91cb8c42dbba56b002e3850c20f4a093b4dfd2ada094b333703743b98fa852`。
- 13:06:01 通过 session D-Bus 执行 `Start`，13:06:03 自动 `Stop`；最终
  `GetState=false`，13:06:04 fullscreen direct scanout 已恢复。该时间窗没有新增
  Timeline、Clutter、shader、Mutter、GPU 或本项目 JS 错误。
- 重登后复跑 `node --check`、5 个 GNOME/UI 静态回归脚本、扩展与 shader 构建、
  CTest 3/3 以及 `git diff --check`，全部通过。
- 当前唯一待确认项：用户需确认 13:06:01--13:06:03 真屏为正常旋转黑洞，且无
  黑屏、静帧、边界缝、跳位或转速变化。确认通过后才能执行 90 秒完整视觉回归和
  配对 CPU 复测；若有任一视觉回归，立即只回退路线 B，保留路线 A。

#### 路线 B 完整视觉序列（2026-07-15 13:07--13:09 CST）

- 用户确认 13:06:01--13:06:03 短时真屏正常，无黑屏、静帧、边界缝、跳位或
  明显转速变化，路线 B 短时门禁通过。
- 为避免 30 秒空闲触发或 MPRIS 状态打断完整序列，测试期间临时停止安装版 UI；
  13:07:54 单次 `Start`，连续运行 90 秒覆盖出生、游走、16 预设和 5.3 秒过渡，
  13:09:24 单次 `Stop`。主测试时间窗没有额外 UI 自动化介入。
- 主测试最终 `GetState=false`，Stop 同秒恢复 fullscreen direct scanout；日志无
  Timeline、Clutter、shader、Mutter、GPU 或本项目 JS 错误。
- 恢复 UI 时，因测试期间桌面 idle time 已超过 30 秒，UI 曾立即额外触发一次
  `Start`；已主动 `Stop`。这发生在 90 秒主测试结束后，不计入视觉序列。
- 安装版 UI 已恢复为 `blackhole-ui-codex.service`，PID 35781、`--minimized`，
  服务状态 `active/running`；黑洞最终 stopped。
- 当前唯一待确认项：用户确认 13:07:54--13:09:24 的 90 秒真屏视觉是否全程正常。
  若通过，下一步按路线 A 同口径执行 3 轮配对 CPU 复测；若失败，立即只回退路线 B。

#### 路线 B 性能门禁与回退（2026-07-15 13:13--13:22 CST）

- 用户确认 13:07:54--13:09:24 的 90 秒真屏全程正常；路线 B 的短时和完整视觉
  门禁均通过。
- 正式原始数据保存在
  `/tmp/blackhole-resource-profile-route-b-20260715`。采样期间安装版 UI 和动态壁纸
  `mpv` 均未运行，黑洞初始与最终状态均为 stopped；每状态预热 5 秒、采样 30 秒，
  3 轮完整结束，没有自动空闲触发或状态混采。
- 三轮 GNOME Shell CPU（`pidstat`，100% 为一个逻辑 CPU）：
  - round 1：停止 13.429%，运行 49.000%，配对增量 35.571 个百分点；
  - round 2：停止 13.095%，运行 49.084%，配对增量 35.988 个百分点；
  - round 3：停止 13.200%，运行 49.684%，配对增量 36.484 个百分点。
- 路线 B 的 Shell 配对增量中位数为 **35.988 个百分点**；路线 A 基准为
  **24.105 个百分点**，路线 B 没有达到额外改善 2% 的保留门槛，反而增加约
  **49.3%**。
- GPU SM 配对增量三轮为 32.069、31.862、31.517 个百分点，中位数
  **31.862**；相比路线 A 的 27.966 增加约 **13.9%**。功耗配对增量中位数为
  **11.069 W**；相比路线 A 的 6.897 W 增加约 **60.5%**。三轮方向一致，不能
  解释为单轮噪声。
- 结论：Clutter Timeline 单驱动虽然减少 JS callback 数量并保持视觉正确，但在
  当前 Mutter 50.1 / NVIDIA Wayland 实机上增加了合成器渲染负载。路线 B 性能门禁
  失败，不保留该实现。
- 已只回退路线 B：`effect.js` 与 `extension.js` 恢复路线 A，删除
  `tests/gnome_timeline_frame_driver_test.py` 及其 CTest 注册；路线 A 的 8 个必要
  uniform 每帧提交、阶段 3--6 和用户其他工作区改动均保留。
- 回退后自动验证通过：`node --check`、uniform/shader/idle/QML 静态回归、扩展与
  shader 构建、CTest 2/2、`git diff --check`。CMake 仅输出既有 Qt QML plugin
  链接警告，目标构建和测试成功。
- 仓库与系统安装目录已部署并核对为路线 A：`effect.js` =
  `27a343681d95495de5420b13c08154faf4dc509ef583c9ff620ac27f1a69f4ae`，
  `extension.js` =
  `6386688fa9e6e0afc8292896537d503855cc604e867d388fc54d6558feb9554d`，权限均为
  `0644 nobody:nogroup`。
- 测试后安装版 UI 已恢复为 PID 37266、`--minimized`、service active/running；
  idle 阈值保持原始 30 秒，动态壁纸保持采样前的未运行状态，黑洞 stopped。
- 下一轮唯一入口：注销并重新登录，不使用扩展禁用/启用替代；核对新 GNOME Shell
  PID、扩展 `ACTIVE` 与上述路线 A 哈希，再执行 2 秒自动 Start/Stop 并由用户真屏
  确认正常。通过后路线 B 回退完成，路线 A 为最终保留的渲染优化，整个项目收官；
  不再实施路线 B 或 C。

#### 最终范围决定（2026-07-15）

- 用户明确决定项目止于路线 A；路线 B 的失败实现保持回退，路线 C 不再实施。
- B/C 候选与实验记录仅作为审计和性能证据保留在计划文档中，不再转化为生产代码。
- 重启后的唯一工作是核对路线 A 实际加载，并执行一次最长 2 秒的自动 Start/Stop、
  用户真屏确认和日志检查；通过后本项目即告结束，不再开启新的优化路线。
