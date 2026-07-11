# 吞噬采样尺度修复实施计划

> **供自动化开发代理使用：** 必须按测试驱动流程逐项实施，每个任务先验证检查失败，再修改生产代码。

**目标：** 消除吞噬模式覆盖半屏的球形采样边界，同时保留近场旋入和程序化吸积盘。

**架构：** 远场继续使用 `noShellLensP` 单次透镜采样。近场旋入改为以 `rh` 为长度单位，颜色层只由实际吸积盘、遮挡和近场范围触发 UI 替换。

**技术栈：** C++17、GLSL、PowerShell 静态回归检查、CMake、Qt 6/QML。

## 全局约束

- 所有源码和中文文档使用 UTF-8。
- 不修改录屏、配置保存、鼠标跟随和窗口层级逻辑。
- 关闭 `uScreenSwallow` 时保持原有 shader 路径。
- 单次提交只包含 `src/main.cpp`、`tools/check_screen_swallow.ps1` 和本计划文档之后的调试记录。

---

### 任务一：约束近场旋入位移

**文件：**
- 修改：`tools/check_screen_swallow.ps1`
- 修改：`src/main.cpp`

**接口：**
- 输入：现有 `rh`、`ringBirth`、`infallTangentFlow`、`inwardPull`。
- 输出：`infallDisplacementScale`、`tangentialStretch`、`radialCompression`、`orbitalInfall`。

- [ ] **步骤 1：增加失败检查**

在检查脚本中要求 `infallDisplacementScale`，并禁止以下未缩放表达式：

```powershell
if ($mainText -match "orbitalInfall\s*=\s*screenTangentDir\s*\*\s*infallTangentFlow\s*-\s*radialDir\s*\*\s*inwardPull") {
    throw "Screen swallow orbital infall is still measured in full-screen coordinates"
}
```

- [ ] **步骤 2：确认检查按预期失败**

运行：`./tools/check_screen_swallow.ps1`

预期：因缺少 `infallDisplacementScale` 或仍存在未缩放表达式而失败。

- [ ] **步骤 3：实现最小修复**

将旋入位移改为：

```glsl
float infallDisplacementScale = rh * (0.42 + 0.28 * ringBirth);
float tangentialStretch = infallTangentFlow * infallDisplacementScale;
float radialCompression = inwardPull * infallDisplacementScale;
vec2 orbitalInfall = screenTangentDir * tangentialStretch - radialDir * radialCompression;
```

- [ ] **步骤 4：确认检查恢复通过**

运行：`./tools/check_screen_swallow.ps1`

预期：`SCREEN_SWALLOW_OK`。

---

### 任务二：移除广域颜色替换形成的暗球

**文件：**
- 修改：`tools/check_screen_swallow.ps1`
- 修改：`src/main.cpp`

**接口：**
- 输入：`realAccretionMask`、`opacityWake`、`screenR`、`rh`、`swallowPhase`。
- 输出：`nearFieldUiCutoff`、`tidalUiErase`、`uiDebrisSuppression`、`uiSuppression`。

- [ ] **步骤 1：增加失败检查**

禁止反向 `smoothstep`，并禁止 `wideTidalErase` 直接主导 `uiDebrisSuppression`：

```powershell
if ($mainText -match "smoothstep\(rh \* 3\.20, rh \* 0\.74, screenR\)") {
    throw "Screen swallow still uses undefined reversed smoothstep edges"
}
if ($mainText -match "uiDebrisSuppression\s*=.*wideTidalErase") {
    throw "Screen swallow still replaces UI colors across the wide radial field"
}
```

- [ ] **步骤 2：确认检查按预期失败**

运行：`./tools/check_screen_swallow.ps1`

预期：因反向 `smoothstep` 或广域颜色替换失败。

- [ ] **步骤 3：实现最小修复**

近场范围改成定义明确的正向平滑过渡，广域场仅参与轻度去色：

```glsl
float nearFieldRange = 1.0 - smoothstep(rh * 0.74, rh * 3.20, screenR);
float nearFieldUiCutoff = clamp(max(realAccretionMask, opacityWake) + nearFieldRange * swallowPhase, 0.0, 1.0);
float tidalUiErase = clamp(max(nearFieldUiCutoff * (0.80 + 0.18 * swallowPhase), wideTidalErase * realAccretionMask * 0.35), 0.0, 1.0);
float uiDebrisSuppression = clamp(max(nearFieldUiCutoff, realAccretionMask * 0.86), 0.0, 1.0);
```

- [ ] **步骤 4：确认检查恢复通过**

运行：`./tools/check_screen_swallow.ps1`

预期：`SCREEN_SWALLOW_OK`。

---

### 任务三：完整验证、打包和提交

**文件：**
- 验证：`src/main.cpp`
- 验证：`tools/check_screen_swallow.ps1`
- 更新：`debug_state.md`（被 `.gitignore` 忽略，仅作本地记录）

- [ ] **步骤 1：运行静态检查**

运行：`./tools/check_screen_swallow.ps1`、`./tools/check_qml_core_bindings.ps1`、`./tools/check_recording_capture.ps1` 和 `git diff --check`。

预期：三个脚本输出各自的 `OK`，空白检查无错误。

- [ ] **步骤 2：构建两个 Release 目标**

运行：

```powershell
cmake --build build --config Release
cmake --build Blakhole_UI/build/Desktop_Qt_6_11_1_MinGW_64_bit-Release --config Release
```

预期：两个目标均构建成功，无新增错误和警告。

- [ ] **步骤 3：执行 shader 运行短测并打包**

运行现有运行短测方式，然后执行 `./package_release.ps1`。

预期：fragment shader 编译、程序链接成功，Release 文件更新时间刷新。

- [ ] **步骤 4：提交**

```powershell
git add src/main.cpp tools/check_screen_swallow.ps1 docs/superpowers/plans/2026-07-11-swallow-scale-fix.md
git commit -m "约束吞噬采样尺度并消除暗色球面"
```
