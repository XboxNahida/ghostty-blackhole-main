# 鼠标跟随惯性 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 新增一个“鼠标惯性”滑块，让跟随鼠标在 0 时贴住鼠标，在大于 0 时具备追赶滞后和小范围游走。

**Architecture:** 沿用现有高级配置链路。Qt Core 保存 `mouseInertia` 到 `blackhole_advanced.txt`，渲染器读取同一字段后在 C++ 主循环中生成带惯性和游走的 `uHomeX/uHomeY`。

**Tech Stack:** Qt/QML、C++、PowerShell 静态检查、CMake Release 构建。

## Global Constraints

- 中文文档。
- 保留现有 txt 配置格式。
- 不做运行时热更新，修改后重启渲染器生效。
- 本次不推送云端。

---

### Task 1: 静态检查

**Files:**
- Create: `tools/check_mouse_inertia.ps1`

**Interfaces:**
- Produces: `MOUSE_INERTIA_LINK_OK`

- [ ] **Step 1: 写红灯检查**

检查以下文件是否包含 `mouseInertia` 链路:
- `Blakhole_UI/pages/AdvancedConfig.qml`
- `Blakhole_UI/core/blackholecore.h`
- `Blakhole_UI/core/blackholecore.cpp`
- `src/gui_config.h`
- `src/gui_config.cpp`
- `src/main.cpp`

- [ ] **Step 2: 运行红灯**

Run: `.\tools\check_mouse_inertia.ps1`

Expected: FAIL，提示缺少 `mouseInertia`。

### Task 2: 配置与 UI

**Files:**
- Modify: `Blakhole_UI/core/blackholecore.h`
- Modify: `Blakhole_UI/core/blackholecore.cpp`
- Modify: `Blakhole_UI/pages/AdvancedConfig.qml`
- Modify: `src/gui_config.h`
- Modify: `src/gui_config.cpp`

**Interfaces:**
- Produces: `BlackHoleCore::mouseInertia()`
- Produces: `BlackHoleCore::setMouseInertia(float)`
- Produces: `BlackholeConfig::mouseInertia`
- Produces config key: `mouseInertia=0.30`

- [ ] **Step 1: 增加 Qt Core 属性**

在 `BlackHoleCore` 增加 `Q_PROPERTY(float mouseInertia READ mouseInertia WRITE setMouseInertia NOTIFY mouseInertiaChanged)`、成员变量、getter、setter、信号。

- [ ] **Step 2: 增加保存读取**

在 `saveAdvancedConfig()` 写入 `mouseInertia`，在 `loadAdvancedConfig()` 读取并限制到 `0.0f..1.0f`。

- [ ] **Step 3: 增加 QML 滑块**

在高级页“跟随鼠标移动”附近加入 `Components.ESlider`，仅在 `followMouse` 开启时显示，值同步到 `bhCore.mouseInertia`。

- [ ] **Step 4: 增加渲染器配置字段**

在 `BlackholeConfig` 增加 `mouseInertia`，在 `SaveAdvancedConfig()` / `LoadAdvancedConfig()` 读写。

### Task 3: 渲染行为

**Files:**
- Modify: `src/main.cpp`

**Interfaces:**
- Consumes: `cfg.mouseInertia`

- [ ] **Step 1: 替换固定平滑系数**

将现有 `0.25f` 平滑改为基于 `mouseInertia` 的动态跟随系数。

- [ ] **Step 2: 增加惯性游走**

当 `mouseInertia > 0` 时，在平滑后的坐标上叠加小范围时间驱动游走；当 `mouseInertia <= 0` 时直接贴住鼠标且不游走。

- [ ] **Step 3: 裁剪最终坐标**

将最终 `frameHomeX/frameHomeY` 限制在 `0.0f..1.0f`。

### Task 4: 验证、打包、提交

**Files:**
- Modify: `debug_state.md`

**Interfaces:**
- Produces: Release package in `release/`

- [ ] **Step 1: 静态检查转绿**

Run: `.\tools\check_mouse_inertia.ps1`

Expected: `MOUSE_INERTIA_LINK_OK`

- [ ] **Step 2: 构建**

Run:
- `cmake --build build --config Release`
- `cmake --build Blakhole_UI/build/Desktop_Qt_6_11_1_MinGW_64_bit-Release --config Release`

Expected: exit code 0。

- [ ] **Step 3: 打包**

Run: `.\package_release.ps1`

Expected: `Release package ready`。

- [ ] **Step 4: 本地提交**

Commit message: `增加鼠标跟随惯性调节`
