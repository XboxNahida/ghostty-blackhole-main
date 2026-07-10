# 持续吞噬实现计划 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将黑洞吞噬从出生/退出特效改为运行期间持续的吸积流效果，并让吸积盘附近不再保留可辨认桌面纹理。

**Architecture:** 保留现有 `screenSwallow` 和 `swallowStrength` 配置链路，只修改 OpenGL shader 注入逻辑与静态检查。每帧直接基于当前桌面采样做潮汐撕裂、螺旋吸入、中心黑化和吸积盘不可辨认化，不增加永久 FBO 轨迹缓存。

**Tech Stack:** C++17, OpenGL/GLSL 字符串注入, PowerShell 静态检查, CMake Release 构建。

## Global Constraints

- 中文记录和最终说明。
- 源文件使用 UTF-8，无 BOM。
- 不修改工作区外文件。
- 涉及代码修改后更新 `debug_state.md`。
- 修改范围控制在 `src/main.cpp`、`tools/check_screen_swallow.ps1` 和调试/计划文档内。

---

### Task 1: 静态检查先红灯

**Files:**
- Modify: `tools/check_screen_swallow.ps1`

**Interfaces:**
- Consumes: `src/main.cpp` 中的 shader 注入字符串。
- Produces: `SCREEN_SWALLOW_OK` 或明确失败信息。

- [ ] **Step 1: 写入失败检查**

要求检查脚本拒绝以下回归：
- `swallowPhase` 继续依赖 `1.0 - uBornProgress`。
- 缺少 `continuousSwallow`。
- 缺少 `eventHorizonMask`。
- 缺少 `accretionScramble`。
- 缺少 `recognizableOuterLens`。

- [ ] **Step 2: 运行检查确认失败**

Run: `.\tools\check_screen_swallow.ps1`
Expected: FAIL，提示当前吞噬仍依赖出生动画或缺少持续吞噬标记。

### Task 2: 持续吸积 shader

**Files:**
- Modify: `src/main.cpp`

**Interfaces:**
- Consumes: `uScreenSwallow`、`uSwallowStrength`、现有 `window`、`shield`、`center`、`aspect`、`uv`、`col`。
- Produces: 持续运行期吞噬、中心黑化、吸积盘不可辨认化、外侧保留可识别透镜扭曲。

- [ ] **Step 1: 修改采样坐标注入**

将 `swallowPhase` 改成持续相位：
`float continuousSwallow = (uScreenSwallow > 0) ? clamp(uSwallowStrength, 0.0, 1.0) : 0.0;`

将碎片门控、潮汐拉伸和螺旋吸入绑定到 `continuousSwallow`、`window * shield` 和半径 `r`，不再依赖 `uBornProgress`。

- [ ] **Step 2: 修改最终颜色分层**

按半径分层：
- `eventHorizonMask`：中心强制接近黑色。
- `accretionScramble`：吸积盘附近做碎片色块、暗化和能量扰动，让桌面内容不可辨认。
- `recognizableOuterLens`：外侧弱透镜保留可识别桌面。

### Task 3: 验证与打包

**Files:**
- Modify: `debug_state.md`

**Interfaces:**
- Consumes: 构建脚本和现有检查脚本。
- Produces: Release 目录可测试产物。

- [ ] **Step 1: 运行静态检查**

Run: `.\tools\check_screen_swallow.ps1`
Expected: `SCREEN_SWALLOW_OK`

- [ ] **Step 2: 运行构建**

Run: `cmake --build build --config Release`
Expected: exit code 0

Run: `cmake --build Blakhole_UI/build/Desktop_Qt_6_11_1_MinGW_64_bit-Release --config Release`
Expected: exit code 0

- [ ] **Step 3: 运行 shader 短测**

Run: `build\blackhole.exe --render` 后读取 `blackhole_debug.txt`
Expected: vertex/fragment shader compiled, program linked。

- [ ] **Step 4: 打包**

Run: `.\package_release.ps1`
Expected: `release\appBlakholeUI.exe` 和 `release\blackhole.exe` 更新。
