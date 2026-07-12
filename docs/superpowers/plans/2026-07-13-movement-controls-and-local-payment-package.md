# 黑洞运动控制与本地收款资源打包实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将路径随机化改为五档出现位置，将原动画速度改为仅影响自由轨迹的 `0.1x～3.0x` 黑洞移动速度，并让正式本地 ZIP 在不跟踪图片的前提下显示横向并排收款码。

**Architecture:** 新增一个无 Qt 依赖的共享运动设置模块，统一枚举、范围限制和固定出生坐标；Qt Core 负责配置迁移和 QML 属性，Renderer 只消费规范化后的值并分离真实时间与自由轨迹时间。收款图片由打包脚本从未跟踪本地文件注入 Release，Qt Core 仅在运行目录存在图片时向关于页暴露文件 URL。

**Tech Stack:** C++17、Win32/OpenGL、Qt 6.11/QML、CMake/CTest、PowerShell 发布脚本。

## 全局约束

- 所有源码和文档使用 UTF-8，中文文件优先用 `Get-Content -Encoding UTF8` 检查。
- `movementSpeed` 范围固定为 `0.1` 至 `3.0`，默认 `1.0`，步进 `0.1`。
- `spawnPosition` 固定映射：`0=随机`、`1=左上`、`2=右上`、`3=左下`、`4=右下`。
- 鼠标跟随、鼠标惯性和过冲计算不得读取 `movementSpeed`。
- 旧 `randomPath=1/0` 分别迁移为随机/右上；旧 `animationSpeed` 和 `spd` 继续表示吸积盘动画速度。
- 高级设置页可滚动但不得实例化 `ScrollBar` 或 `ScrollView`。
- Git 不跟踪两张收款码；正式 Release 和 ZIP 必须包含两张图片并横向并排显示。
- 修改预计超过 3 个文件，必须按任务阶段验证并在每阶段更新 `debug_state.md`。

---

### Task 1: 建立共享运动设置模型

**Files:**
- Create: `src/movement_settings.h`
- Create: `src/movement_settings.cpp`
- Create: `tests/movement_settings_tests.cpp`
- Modify: `CMakeLists.txt`
- Modify: `Blakhole_UI/CMakeLists.txt`

**Interfaces:**
- Produces: `enum class SpawnPosition : int`。
- Produces: `int NormalizeSpawnPosition(int value)`。
- Produces: `float ClampMovementSpeed(float value)`。
- Produces: `MovementSpawn ResolveMovementSpawn(int position, float randomX, float randomY, float randomPhase, float randomPresetOffset)`。

- [ ] **Step 1: 编写失败的运动设置单元测试**

测试必须覆盖五档枚举、非法值回退随机、速度下限/上限/默认值和四角坐标：

```cpp
#include "movement_settings.h"
#include <cmath>
#include <cstdlib>

static void RequireNear(float actual, float expected) {
    if (std::fabs(actual - expected) > 0.0001f) std::abort();
}

int main() {
    if (NormalizeSpawnPosition(-1) != 0) return 1;
    if (NormalizeSpawnPosition(5) != 0) return 2;
    RequireNear(ClampMovementSpeed(0.0f), 0.1f);
    RequireNear(ClampMovementSpeed(1.0f), 1.0f);
    RequireNear(ClampMovementSpeed(4.0f), 3.0f);

    MovementSpawn leftTop = ResolveMovementSpawn(1, 0.4f, 0.6f, 1.2f, 9.0f);
    RequireNear(leftTop.x, 0.18f); RequireNear(leftTop.y, 0.18f);
    MovementSpawn rightBottom = ResolveMovementSpawn(4, 0.4f, 0.6f, 1.2f, 9.0f);
    RequireNear(rightBottom.x, 0.82f); RequireNear(rightBottom.y, 0.82f);
    MovementSpawn random = ResolveMovementSpawn(0, 0.4f, 0.6f, 1.2f, 9.0f);
    RequireNear(random.x, 0.4f); RequireNear(random.phaseOffset, 1.2f);
    return 0;
}
```

- [ ] **Step 2: 配置并运行测试，确认按预期失败**

Run:

```powershell
cmake -S Blakhole_UI -B Blakhole_UI\build\Desktop_Qt_6_11_1_MinGW_64_bit-Release -G "MinGW Makefiles" -DQt6_DIR=C:\Qt\6.11.1\mingw_64\lib\cmake\Qt6 -DBUILD_TESTING=ON
cmake --build Blakhole_UI\build\Desktop_Qt_6_11_1_MinGW_64_bit-Release --target movement_settings_tests
```

Expected: FAIL，提示缺少 `movement_settings.h` 或测试目标。

- [ ] **Step 3: 实现最小共享模型**

`movement_settings.h` 定义：

```cpp
#pragma once

enum class SpawnPosition : int {
    Random = 0,
    LeftTop = 1,
    RightTop = 2,
    LeftBottom = 3,
    RightBottom = 4
};

struct MovementSpawn {
    float x;
    float y;
    float phaseOffset;
    float presetOffset;
};

int NormalizeSpawnPosition(int value);
float ClampMovementSpeed(float value);
MovementSpawn ResolveMovementSpawn(int position, float randomX, float randomY,
                                  float randomPhase, float randomPresetOffset);
```

`movement_settings.cpp` 对固定角落返回设计坐标和稳定的 `phaseOffset=0`、`presetOffset=0`；随机模式返回传入随机值。不得引入 Qt 或 Win32 依赖。

- [ ] **Step 4: 将模块加入 Renderer 和测试目标**

根 `CMakeLists.txt` 将 `src/movement_settings.cpp` 加入 `blackhole`。Qt CMake 新增 `movement_settings_tests`，链接 `Qt6::Core` 不是必需条件，仅包含根 `src` 目录并编译共享 `.cpp`。

- [ ] **Step 5: 运行测试和 Renderer 编译**

Run:

```powershell
cmake --build Blakhole_UI\build\Desktop_Qt_6_11_1_MinGW_64_bit-Release --target movement_settings_tests
ctest --test-dir Blakhole_UI\build\Desktop_Qt_6_11_1_MinGW_64_bit-Release -R movement_settings_tests --output-on-failure
cmake --build build --config Release
```

Expected: `movement_settings_tests` 通过，Renderer 编译无新增错误。

- [ ] **Step 6: 更新调试记录并提交**

```powershell
git add src/movement_settings.h src/movement_settings.cpp tests/movement_settings_tests.cpp CMakeLists.txt Blakhole_UI/CMakeLists.txt
git commit -m "增加黑洞运动设置共享模型"
```

---

### Task 2: Qt 配置迁移与无滚动条高级设置页

**Files:**
- Modify: `Blakhole_UI/core/blackholecore.h`
- Modify: `Blakhole_UI/core/blackholecore.cpp`
- Modify: `Blakhole_UI/pages/AdvancedConfig.qml`
- Modify: `tools/check_mouse_inertia.ps1`
- Create: `tools/check_movement_controls.ps1`

**Interfaces:**
- Consumes: `NormalizeSpawnPosition()`、`ClampMovementSpeed()`。
- Produces QML properties: `int spawnPosition`、`float movementSpeed`。
- Preserves compatibility input: `randomPath`、`animationSpeed`、`spd`。

- [ ] **Step 1: 编写失败的 UI/配置契约**

`check_movement_controls.ps1` 必须断言：

```powershell
Require-Pattern 'Blakhole_UI\core\blackholecore.h' 'Q_PROPERTY\(int\s+spawnPosition' 'spawn position property'
Require-Pattern 'Blakhole_UI\core\blackholecore.h' 'Q_PROPERTY\(float\s+movementSpeed' 'movement speed property'
Require-Pattern 'Blakhole_UI\pages\AdvancedConfig.qml' 'text:\s*"黑洞出现位置"' 'spawn position label'
Require-Pattern 'Blakhole_UI\pages\AdvancedConfig.qml' 'label:\s*"黑洞移动速度"' 'movement speed slider'
Require-Pattern 'Blakhole_UI\pages\AdvancedConfig.qml' '\bFlickable\s*\{' 'scrollable advanced page'
```

并禁止 QML 出现 `路径随机化`、`label: "动画速度"`、`ScrollBar`、`ScrollView`。

- [ ] **Step 2: 运行契约并确认失败**

Run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_movement_controls.ps1
```

Expected: FAIL，首先提示缺少 `spawnPosition` 属性。

- [ ] **Step 3: 替换 Qt Core 属性**

在 `blackholecore.h` 删除公开的 `randomPath` 属性、getter/setter、signal 和成员；新增：

```cpp
Q_PROPERTY(int spawnPosition READ spawnPosition WRITE setSpawnPosition NOTIFY spawnPositionChanged)
Q_PROPERTY(float movementSpeed READ movementSpeed WRITE setMovementSpeed NOTIFY movementSpeedChanged)
```

成员默认值：

```cpp
int m_spawnPosition = 0;
float m_movementSpeed = 1.0f;
```

setter 必须调用共享规范化函数后再比较和发信号。

- [ ] **Step 4: 实现新配置保存和旧配置迁移**

保存：

```cpp
out << "spawnPosition=" << m_spawnPosition << "\n";
out << "movementSpeed=" << QString::number(m_movementSpeed, 'f', 1) << "\n";
```

加载时记录是否见到新字段；只有新字段缺失时才读取旧 `randomPath`：

```cpp
bool hasSpawnPosition = false;
int legacyRandomPath = -1;
// spawnPosition -> NormalizeSpawnPosition
// movementSpeed -> ClampMovementSpeed
// randomPath -> 暂存，不立即覆盖新字段
if (!hasSpawnPosition && legacyRandomPath >= 0)
    m_spawnPosition = legacyRandomPath ? 0 : 2;
```

保留 `animationSpeed`/`spd` 读取和保存逻辑，以免改变吸积盘速度；该属性可继续留在 Core 内部，但不再出现在高级设置 QML。

- [ ] **Step 5: 重构高级设置页为无滚动条 Flickable**

根 `Item` 内使用：

```qml
Flickable {
    anchors.fill: parent
    contentWidth: width
    contentHeight: settingsColumn.implicitHeight
    clip: true
    boundsBehavior: Flickable.StopAtBounds

    ColumnLayout {
        id: settingsColumn
        width: parent.width
    }
}
```

不得声明 `ScrollBar` 或 `ScrollView`。将运动区域替换为以下五项下拉框和 `ESlider`：

```qml
RowLayout {
    Layout.fillWidth: true
    Text {
        text: "黑洞出现位置"
        color: theme.textColor
        Layout.fillWidth: true
    }
    Components.EDropDown {
        preferredWidth: 150
        model: ["随机", "左上", "右上", "左下", "右下"]
        currentIndex: advPage.spawnPosition
        onActivated: function(index) {
            advPage.spawnPosition = index
            if (bhCore) bhCore.spawnPosition = index
        }
    }
}
```

移动速度滑块：

```qml
Components.ESlider {
    label: "黑洞移动速度"
    from: 0.1; to: 3.0; stepSize: 0.1; decimals: 1
    value: advPage.movementSpeed
}
```

- [ ] **Step 6: 运行契约、QML 编译和既有绑定检查**

Run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_movement_controls.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_qml_core_bindings.ps1
cmake --build Blakhole_UI\build\Desktop_Qt_6_11_1_MinGW_64_bit-Release --config Release
```

Expected: 两个契约通过，QML AOT 编译成功。

- [ ] **Step 7: 更新调试记录并提交**

```powershell
git add Blakhole_UI/core/blackholecore.h Blakhole_UI/core/blackholecore.cpp Blakhole_UI/pages/AdvancedConfig.qml tools/check_mouse_inertia.ps1 tools/check_movement_controls.ps1
git commit -m "替换黑洞出现位置与移动速度设置"
```

---

### Task 3: Renderer 应用出现位置和独立移动时间轴

**Files:**
- Modify: `src/gui_config.h`
- Modify: `src/gui_config.cpp`
- Modify: `src/main.cpp`
- Modify: `tools/check_movement_controls.ps1`

**Interfaces:**
- Consumes: `ResolveMovementSpawn()`、`ClampMovementSpeed()`。
- Config fields: `int spawnPosition=0`、`float movementSpeed=1.0f`。
- Shader contract: `iTime` 保持真实时间；新增 `uMovementTime` 仅替换轨迹函数所使用的 `t`。

- [ ] **Step 1: 扩展失败契约**

契约必须检查 Renderer 字段、配置键、共享出生位置解析和独立时间 uniform，并禁止鼠标跟随代码块引用 `movementSpeed`。

```powershell
Require-Pattern 'src\gui_config.h' 'int\s+spawnPosition\s*=\s*0' 'renderer spawn position field'
Require-Pattern 'src\gui_config.h' 'float\s+movementSpeed\s*=\s*1\.0f' 'renderer movement speed field'
Require-Pattern 'src\main.cpp' 'ResolveMovementSpawn' 'spawn resolver use'
Require-Pattern 'src\main.cpp' 'uMovementTime' 'movement time uniform'
```

- [ ] **Step 2: 运行契约并确认失败**

Expected: FAIL，提示缺少 Renderer 字段。

- [ ] **Step 3: 更新 Renderer 配置保存与迁移**

`SaveAdvancedConfig` 写 `spawnPosition` 和 `movementSpeed`，不再写 `randomPath`。`LoadAdvancedConfig` 优先读新字段，缺失时按 `randomPath` 迁移，并对速度调用 `ClampMovementSpeed`。旧 `animationSpeed`/`spd` 仍赋给 `cfg.spd`。

- [ ] **Step 4: 使用共享出生位置解析**

保留随机数生成，但替换条件表达式：

```cpp
MovementSpawn spawn = ResolveMovementSpawn(
    cfg.spawnPosition, randHomeX, randHomeY, randPhase, randPresetOff);
float homeX = spawn.x;
float homeY = spawn.y;
float phaseOffset = spawn.phaseOffset;
float presetOffset = spawn.presetOffset;
```

调试日志记录 `spawnPosition` 和 `movementSpeed`。

- [ ] **Step 5: 分离自由移动时间与真实时间**

Shader header 增加 `uniform float uMovementTime;`，运行时把基础 Shader 中：

```glsl
float t = iTime * DRIFT_SPEED;
```

替换为：

```glsl
float t = uMovementTime * DRIFT_SPEED;
```

每帧上传：

```cpp
gl_Uniform1f(locMovementTime, t * cfg.movementSpeed);
```

鼠标跟随的 C++ 物理时间和游走继续使用真实 `t`，不得改用 `movementSpeed`。

- [ ] **Step 6: 运行单元测试、契约和 Renderer Release 构建**

Run:

```powershell
ctest --test-dir Blakhole_UI\build\Desktop_Qt_6_11_1_MinGW_64_bit-Release -R movement_settings_tests --output-on-failure
powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_movement_controls.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_mouse_inertia.ps1
cmake --build build --config Release --clean-first
```

Expected: 测试和契约通过，Renderer 无新增错误或警告。

- [ ] **Step 7: 更新调试记录并提交**

```powershell
git add src/gui_config.h src/gui_config.cpp src/main.cpp tools/check_movement_controls.ps1
git commit -m "应用黑洞出生位置与自由移动速度"
```

---

### Task 4: 可选本地收款区域与正式打包注入

**Files:**
- Modify: `Blakhole_UI/core/blackholecore.h`
- Modify: `Blakhole_UI/core/blackholecore.cpp`
- Modify: `Blakhole_UI/pages/AboutPage.qml`
- Modify: `package_release.ps1`
- Modify: `tools/check_about_page.ps1`
- Modify: `tools/check_release_security.ps1`

**Interfaces:**
- Produces QML properties: `QString paymentQrPrimaryUrl`、`QString paymentQrSecondaryUrl`、`bool paymentQrAvailable`。
- Local inputs: untracked `Blakhole_UI/fonts/pic/QR_payment.jpg` and `Blakhole_UI/fonts/pic/WeChat_QR.png`。
- Release outputs: `fonts/pic/QR_payment.jpg` and `fonts/pic/WeChat_QR.png`。

- [ ] **Step 1: 编写失败的关于页与发布契约**

公开检查必须继续禁止 `src.qrc` 嵌入图片，同时要求 QML 使用 Core URL 属性、横向 `RowLayout` 和等宽图片。发布安全检查必须要求 Release 中两张文件存在并进入校验清单。

```powershell
Require-Match $page 'paymentQrAvailable' 'payment section must be conditional'
Require-Match $page 'RowLayout\s*\{[\s\S]*paymentQrPrimaryUrl[\s\S]*paymentQrSecondaryUrl' 'payment QR codes must share one row'
```

- [ ] **Step 2: 运行契约并确认失败**

Run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_about_page.ps1
```

Expected: FAIL，提示缺少条件收款区域。

- [ ] **Step 3: 暴露可选本地图片 URL**

Core 使用 `QCoreApplication::applicationDirPath()` 查找 Release 文件，只有两张都存在时 `paymentQrAvailable=true`。URL 通过 `QUrl::fromLocalFile(...).toString()` 返回；不得把图片加入 `src.qrc`。

- [ ] **Step 4: 实现横向并排关于页布局**

使用一个可见性绑定到 `paymentQrAvailable` 的未嵌套页面区块：

```qml
RowLayout {
    Layout.fillWidth: true
    spacing: 18

    Image {
        Layout.fillWidth: true
        Layout.preferredWidth: 1
        source: bhCore.paymentQrPrimaryUrl
        fillMode: Image.PreserveAspectFit
    }
    Image {
        Layout.fillWidth: true
        Layout.preferredWidth: 1
        source: bhCore.paymentQrSecondaryUrl
        fillMode: Image.PreserveAspectFit
    }
}
```

两张图片共享相同 `Layout.maximumWidth` 和稳定 `aspectRatio`/高度约束，禁止使用 `ColumnLayout` 包裹两张码，窄窗口仍保持同一行。

- [ ] **Step 5: 修改正式打包脚本注入本地资源**

`package_release.ps1` 在复制发布资源阶段检查两个本地输入，缺少时 `throw` 明确路径；存在时复制到 Release `fonts/pic`。该步骤必须发生在生成 `release_checksums.sha256` 之前。

- [ ] **Step 6: 运行公开源码与本地 Release 双重验证**

Run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_about_page.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File package_release.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_release_security.ps1 -ReleaseDir .\release
powershell -NoProfile -ExecutionPolicy Bypass -File tools\verify_release_checksums.ps1 -ReleaseDir .\release
git ls-files | rg "QR_payment|WeChat_QR"
```

Expected: 源码检查通过，Git 查询无输出，Release 安全检查通过且包含两张图片。

- [ ] **Step 7: 更新调试记录并提交**

只提交代码和脚本，不添加图片：

```powershell
git add Blakhole_UI/core/blackholecore.h Blakhole_UI/core/blackholecore.cpp Blakhole_UI/pages/AboutPage.qml package_release.ps1 tools/check_about_page.ps1 tools/check_release_security.ps1
git commit -m "支持本地收款资源注入与并排显示"
```

---

### Task 5: 集中验证、ZIP 打包与推送

**Files:**
- Modify locally only: `debug_state.md`
- Generate locally only: `BlakholeUI-v1.2.0-windows-x64.zip`

**Interfaces:**
- Consumes: clean committed `main`、两张未跟踪本地收款码。
- Produces: 可供用户手动上传的 ZIP；推送后的 `origin/main`。

- [ ] **Step 1: 运行完整静态契约**

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_movement_controls.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_mouse_inertia.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_about_page.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_qml_core_bindings.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_renderer_security.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File tools\run_idle_phase1_tests.ps1
```

Expected: 全部输出对应 `*_OK`。

- [ ] **Step 2: 构建并运行全部 Qt 测试**

```powershell
cmake --build Blakhole_UI\build\Desktop_Qt_6_11_1_MinGW_64_bit-Release --target application_catalog_tests avatar_storage_tests update_release_tests update_checker_state_tests movement_settings_tests
ctest --test-dir Blakhole_UI\build\Desktop_Qt_6_11_1_MinGW_64_bit-Release --output-on-failure
```

Expected: 5/5 tests passed。

- [ ] **Step 3: 从干净提交重新生成 Release**

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File package_release.ps1
```

Expected: Renderer、Qt UI 干净构建完成，发布安全和校验清单通过；只允许既有 `QImage::mirrored`、可选 Vulkan/D3D12 提示。

- [ ] **Step 4: 生成并检查 ZIP**

```powershell
Compress-Archive -Path .\release\* -DestinationPath .\BlakholeUI-v1.2.0-windows-x64.zip -CompressionLevel Optimal -Force
```

用 `System.IO.Compression.ZipFile` 断言 ZIP 同时包含两个 EXE、两张收款码、`RELEASE_INFO.txt` 和校验清单，并输出 SHA-256。

- [ ] **Step 5: 确认公开提交不包含本地资源**

```powershell
git status --short
git ls-files | rg "QR_payment|WeChat_QR"
git diff --check
```

Expected: 两张图片和 ZIP 仅为未跟踪文件；`git ls-files` 无收款码输出。

- [ ] **Step 6: 推送并核对远程 SHA**

```powershell
git push origin main
git rev-parse main
git ls-remote --heads origin main
```

Expected: 本地和远程 SHA 完全一致，不使用 `--force`，不移动 `v1.2.0` 标签。
