# 吞噬模式分层修复实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 消除吞噬模式中的玻璃球采样边界，并阻止屏幕 UI 颜色进入吸积盘核心区域。

**Architecture:** 保留原版黑洞 geodesic 计算作为物理基础，将吞噬增强拆成远场透镜、连续坍缩场、程序化吸积盘颜色三层。远场仍可采样屏幕背景；吸积盘和事件视界附近改由 `emitc/trans/captured` 生成结果，不再直接继承 UI 贴图颜色。

**Tech Stack:** C++ 字符串注入 GLSL、OpenGL fragment shader、PowerShell 静态检查、CMake Release 构建。

## Global Constraints

- 中文记录和中文最终说明。
- 不跨工作区目录修改文件。
- 不修改自动生成文件。
- 本阶段预计修改不超过 3 个跟踪文件。
- 每个节点更新 `debug_state.md`。
- 源码保持 UTF-8，无 BOM。
- 先写失败检查，再改实现。

---

### Task 1: 静态检查先行

**Files:**
- Modify: `tools/check_screen_swallow.ps1`

**Interfaces:**
- Consumes: 当前 `src/main.cpp` shader 注入字符串。
- Produces: 可检测吞噬分层结构的 `SCREEN_SWALLOW_OK` 检查。

- [ ] **Step 1: 增加失败检查**

新增要求：
- `gravityWellField`
- `softDiskMatterField`
- `uiFreeAccretion`
- `nearFieldUiCutoff`
- `smoothInfallField`

新增禁止：
- `vec3 swallowedColor = deUiBg * trans`
- `adaptiveCollapse = clamp(max(projectedCollapse`

- [ ] **Step 2: 运行旧实现确认失败**

Run: `.\tools\check_screen_swallow.ps1`

Expected: FAIL，提示缺少新字段或仍保留旧的 UI 混合/硬坍缩模式。

### Task 2: Shader 分层修复

**Files:**
- Modify: `src/main.cpp`

**Interfaces:**
- Consumes: 原版 shader 中的 `emitc`、`trans`、`captured`、`bg`、`col`、`p`、`rh`、`window`、`shield`。
- Produces: 无硬壳边界的吞噬采样和不继承 UI 颜色的吸积盘核心。

- [ ] **Step 1: 改采样层**

将 `adaptiveCollapse` 改为连续场：
- `gravityWellField`：由半径和 `rh` 推导，连续覆盖近场。
- `softDiskMatterField`：由投影盘距离推导，用有理函数或指数函数，不使用硬 smoothstep 盘带作为最终切换。
- `smoothInfallField`：组合重力井和盘面物质场，作为 `finalP` 混合权重。

- [ ] **Step 2: 改颜色层**

新增：
- `nearFieldUiCutoff`：吸积盘和事件视界附近切断 UI 颜色。
- `uiFreeAccretion`：只由 `diskLight` 与热色构成的吸积盘颜色。

替换旧的 `swallowedColor = deUiBg * trans ... + programmaticAccretion`，避免 UI 颜色继续进入光盘。

### Task 3: 验证与打包

**Files:**
- Modify: `debug_state.md`

**Interfaces:**
- Consumes: Task 1/2 的修改结果。
- Produces: 可测试 Release。

- [ ] **Step 1: 运行静态检查**

Run:
- `.\tools\check_screen_swallow.ps1`
- `.\tools\check_qml_core_bindings.ps1`
- `git diff --check`

- [ ] **Step 2: 构建和运行时 shader 短测**

Run:
- `cmake --build build --config Release`
- `cmake --build Blakhole_UI\build\Desktop_Qt_6_11_1_MinGW_64_bit-Release --config Release`
- 启动 `build\blackhole.exe --render`，检查 shader 编译和链接日志。

- [ ] **Step 3: 打包 Release**

Run: `.\package_release.ps1`

Expected: `release\blackhole.exe` 和 `release\appBlakholeUI.exe` 更新。

