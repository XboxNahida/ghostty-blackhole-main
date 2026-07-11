# 吸积盘高质量光影 Implementation Plan

> **供智能代理执行：** 必须使用 `executing-plans` 按任务逐项执行。所有步骤使用复选框跟踪，并在每个任务结束后复核差异。

**目标：** 用一个“吸积盘光影效果”开关完整替换吞噬功能，为所有预设增加随盘面姿态变化的金蓝分层照明、暗纹、光子环和 HDR Bloom。

**架构：** 配置层只暴露 `lightingEffect` 布尔值并兼容读取旧 `screenSwallow`。基础黑洞 shader 负责产生贴合盘面坐标的高动态范围光源，独立 `BloomRenderer` 负责 RGBA16F 离屏纹理、亮部提取、横纵高斯模糊和最终合成。关闭开关时直接渲染默认帧缓冲，不经过 Bloom。

**技术栈：** C++17、OpenGL 3.3/WGL、GLSL 330、Qt 6/QML、PowerShell 静态回归检查、CMake。

## 全局约束

- 所有源码和文档使用 UTF-8，中文读取使用 `rg` 或显式 `Get-Content -Encoding UTF8`。
- 不修改 CubeMX、代码生成物或工作区外文件。
- 不新增第三方依赖；默认 OpenGL 路径是本功能唯一实现范围。
- 事件视界保持纯黑，所有光影遮罩连续，不允许出现硬圆边界、整屏过曝或 NaN。
- 金蓝方向使用盘面自身坐标，必须随 `L.roll`、`L.incl` 和预设交叉淡入变化。
- 每完成一个任务立即更新忽略跟踪的 `debug_state.md`，再执行该任务的验证。
- 不删除无法通过 `rg` 确认无调用关系的代码；吞噬字段只保留旧配置读取兼容，不保留渲染行为。

---

## 文件结构

- `tools/check_lighting_effect.ps1`：光影配置、QML、shader 结构和 Bloom 集成的静态回归检查。
- `src/gui_config.h/.cpp`：渲染器侧 `lightingEffect` 配置及旧 `screenSwallow` 兼容读取。
- `Blakhole_UI/core/blackholecore.h/.cpp`：Qt 属性、信号、配置保存和旧字段兼容读取。
- `Blakhole_UI/pages/AdvancedConfig.qml`：单一光影勾选框，不再显示强度滑块。
- `shaders/frag_desktop_header.glsl`：`uLightingEffect` uniform。
- `src/main.cpp`：删除吞噬注入和运动降速，注入分层盘面光源并调用 Bloom 模块。
- `src/bloom_renderer.h/.cpp`：拥有 Bloom 所需的 GL 对象、尺寸变化、渲染和释放逻辑。
- `CMakeLists.txt`：编译 `src/bloom_renderer.cpp`。
- `blackhole_advanced.txt`：默认配置只使用 `lightingEffect=0`。

---

### Task 1：用单一光影开关替换吞噬配置和界面

**文件：**
- 新建：`tools/check_lighting_effect.ps1`
- 修改：`src/gui_config.h`
- 修改：`src/gui_config.cpp`
- 修改：`Blakhole_UI/core/blackholecore.h`
- 修改：`Blakhole_UI/core/blackholecore.cpp`
- 修改：`Blakhole_UI/pages/AdvancedConfig.qml`
- 修改：`blackhole_advanced.txt`

**接口：**
- 输入：新键 `lightingEffect=0|1`；兼容输入 `screenSwallow=0|1`。
- 输出：`BlackholeConfig::lightingEffect`、Qt 属性 `lightingEffect`、信号 `lightingEffectChanged()`。

- [ ] **Step 1：写配置和 UI 红灯检查**

`tools/check_lighting_effect.ps1` 必须读取上述文件并要求：

```powershell
Require-Pattern "src\gui_config.h" "bool\s+lightingEffect\s*=\s*false" "renderer lighting flag"
Require-Pattern "src\gui_config.cpp" 'strcmp\(key, "lightingEffect"\)' "renderer lighting read key"
Require-Pattern "src\gui_config.cpp" 'strcmp\(key, "screenSwallow"\).*lightingEffect' "legacy renderer migration"
Require-Pattern "Blakhole_UI\core\blackholecore.h" "Q_PROPERTY\(bool\s+lightingEffect" "Qt lighting property"
Require-Pattern "Blakhole_UI\core\blackholecore.cpp" 'out << "lightingEffect="' "Qt lighting save key"
Require-Pattern "Blakhole_UI\pages\AdvancedConfig.qml" 'text:\s*"吸积盘光影效果"' "lighting checkbox label"

$qml = Get-Content -Raw -Encoding UTF8 (Join-Path $projectRoot "Blakhole_UI\pages\AdvancedConfig.qml")
if ($qml -match "吞噬|swallowStrength|吞噬强度|光影强度") { throw "Advanced UI still exposes swallow/strength controls" }
```

- [ ] **Step 2：运行检查并确认按预期失败**

运行：`./tools/check_lighting_effect.ps1`

预期：失败并提示缺少 `renderer lighting flag`，证明新配置尚未实现。

- [ ] **Step 3：实现渲染器配置迁移**

将 `screenSwallow` 和 `swallowStrength` 成员替换为：

```cpp
bool lightingEffect = false; // 吸积盘分层光影与 Bloom
```

保存只写：

```cpp
fprintf(f, "lightingEffect=%d\n", cfg.lightingEffect ? 1 : 0);
```

读取顺序使用本地迁移标志，保证新键优先于旧键：

```cpp
bool hasLightingEffect = false;
// lightingEffect: 设置值并置 hasLightingEffect=true
// screenSwallow: 仅在 !hasLightingEffect 时映射到 cfg.lightingEffect
// swallowStrength: 识别后忽略，避免旧文件解析异常
```

- [ ] **Step 4：实现 Qt 属性和单一勾选框**

Qt Core 使用以下闭合接口：

```cpp
Q_PROPERTY(bool lightingEffect READ lightingEffect WRITE setLightingEffect NOTIFY lightingEffectChanged)
bool lightingEffect() const;
void setLightingEffect(bool value);
void lightingEffectChanged();
bool m_lightingEffect = false;
```

QML 只保留：

```qml
property bool lightingEffect: bhCore ? bhCore.lightingEffect : false

Text { text: "吸积盘光影效果" }
Text { text: "分层吸积盘、冷暖双色照明与辉光" }
CheckBox {
    checked: advPage.lightingEffect
    onToggled: {
        advPage.lightingEffect = checked
        if (bhCore) bhCore.lightingEffect = checked
    }
}
```

保存只输出 `lightingEffect`；加载新键优先，旧 `screenSwallow` 仅在新键缺失时迁移；旧 `swallowStrength` 不设置任何属性。

- [ ] **Step 5：更新默认配置并验证绿灯**

将 `blackhole_advanced.txt` 中两个吞噬键替换为：

```text
lightingEffect=0
```

运行：

```powershell
./tools/check_lighting_effect.ps1
./tools/check_qml_core_bindings.ps1
git diff --check
```

预期：分别输出 `LIGHTING_EFFECT_OK`、`QML_CORE_BINDINGS_OK`，空白检查退出码为 0。

- [ ] **Step 6：更新调试记录并提交阶段 1**

```powershell
git add tools/check_lighting_effect.ps1 src/gui_config.h src/gui_config.cpp Blakhole_UI/core/blackholecore.h Blakhole_UI/core/blackholecore.cpp Blakhole_UI/pages/AdvancedConfig.qml blackhole_advanced.txt
git commit -m "用单一光影开关替换吞噬配置"
```

---

### Task 2：删除吞噬渲染并生成随盘面变化的 HDR 光源

**文件：**
- 修改：`tools/check_lighting_effect.ps1`
- 修改：`shaders/frag_desktop_header.glsl`
- 修改：`src/main.cpp`

**接口：**
- 输入：`uLightingEffect`、`emitc`、`trans`、`captured`、`p`、`rh`、`rin`、`rout`、`L.roll`、`L.incl`、`iTime`。
- 输出：HDR `col`，包含盘面渐变、暗纹、盘面局部金蓝光和光子环。

- [ ] **Step 1：扩展红灯检查**

要求下列标记存在：

```powershell
Require-Pattern "shaders\frag_desktop_header.glsl" "uniform\s+int\s+uLightingEffect" "lighting uniform"
Require-Pattern "src\main.cpp" "diskLocalP" "preset-local disk coordinates"
Require-Pattern "src\main.cpp" "outerLightMist" "outer light mist"
Require-Pattern "src\main.cpp" "darkFlowBands" "moving dark bands"
Require-Pattern "src\main.cpp" "warmGoldLight" "gold lighting"
Require-Pattern "src\main.cpp" "coldBlueLight" "blue lighting"
Require-Pattern "src\main.cpp" "lightingPhotonRing" "photon ring lighting"
```

并禁止生产代码中残留：`uScreenSwallow`、`uSwallowStrength`、`continuousSwallow`、`gravityWellField`、`uiDebrisSuppression`、`effectiveShaderSpeed` 和 `swallowMotionScale`。

- [ ] **Step 2：运行检查并确认 shader 红灯**

运行：`./tools/check_lighting_effect.ps1`

预期：失败并提示缺少 `lighting uniform`。

- [ ] **Step 3：移除吞噬调用关系**

使用 `rg -n "screenSwallow|swallowStrength|uScreenSwallow|uSwallowStrength|swallow" src shaders` 列出调用点；删除 `buildFragmentShader()` 中远近场吞噬采样和最终吞噬颜色块，删除 uniform 查询/上传及所有吞噬条件下的速度、鼠标跟随和游走缩放。基础 `uDistortion` 透镜路径保持不变。

- [ ] **Step 4：实现盘面坐标和分层光源**

在最终 `fragColor` 前注入以下等价公式，所有除数使用下限保护：

```glsl
vec2 diskLocalP = rot(vec2(p.x, -p.y), L.roll);
float diskInclScale = max(abs(cos(L.incl)), 0.16);
vec2 projectedLightP = vec2(diskLocalP.x, diskLocalP.y / diskInclScale);
float projectedLightRadius = max(length(projectedLightP), 0.0005);
float diskInnerScreen = rin * rh / B_CRIT;
float diskOuterScreen = rout * rh / B_CRIT;
float diskWidth = max(diskOuterScreen - diskInnerScreen, rh * 0.35);
float diskUnit = clamp((projectedLightRadius - diskInnerScreen) / diskWidth, 0.0, 1.0);
float diskEnergy = max(max(diskLight.r, diskLight.g), diskLight.b);
float realDiskMask = smoothstep(0.012, 0.20, diskEnergy);
float outerLightMist = exp(-pow(max(projectedLightRadius - diskOuterScreen, 0.0) / max(diskWidth * 1.8, 0.004), 2.0));
float bandPhase = diskUnit * 42.0 + atan(projectedLightP.y, projectedLightP.x) * 2.3 - iTime * 0.55;
float darkFlowBands = smoothstep(0.48, 0.86, 0.5 + 0.5 * sin(bandPhase + 0.35 * sin(bandPhase * 0.37)));
float diskSide = clamp(0.5 + 0.5 * projectedLightP.x / projectedLightRadius, 0.0, 1.0);
vec3 warmGoldLight = vec3(1.00, 0.48, 0.12) * (1.0 - diskSide);
vec3 coldBlueLight = vec3(0.18, 0.62, 1.00) * diskSide;
float lightingPhotonRing = (1.0 - smoothstep(rh * 0.92, rh * 1.28, length(p))) * (1.0 - (captured ? 1.0 : 0.0));
```

使用 `realDiskMask` 约束中层亮带和暗纹；使用较弱的 `outerLightMist` 叠加外层色雾；将金蓝光以加法 HDR 方式加入 `col`；最后再次用 `captured` 将事件视界混合到纯黑。`uLightingEffect == 0` 时不得改变 `col`。

- [ ] **Step 5：运行静态检查、shader 短测和 Release 构建**

运行：

```powershell
./tools/check_lighting_effect.ps1
./tools/check_qml_core_bindings.ps1
cmake --build build --config Release
```

再启动 `build/blackhole.exe --render` 进行 shader 编译短测，读取 `debug_log.txt`，必须出现 fragment shader compiled 和 program linked，且无新增 shader error。

- [ ] **Step 6：更新调试记录并提交阶段 2**

```powershell
git add tools/check_lighting_effect.ps1 shaders/frag_desktop_header.glsl src/main.cpp
git commit -m "实现随预设姿态变化的吸积盘光源"
```

---

### Task 3：增加高质量 HDR Bloom 渲染链路

**文件：**
- 新建：`src/bloom_renderer.h`
- 新建：`src/bloom_renderer.cpp`
- 修改：`CMakeLists.txt`
- 修改：`src/main.cpp`
- 修改：`tools/check_lighting_effect.ps1`

**接口：**
- 输入：窗口宽高、是否启用光影、主 shader 的 HDR 画面。
- 输出：默认帧缓冲中的原画面与 Bloom 合成结果；初始化失败时回退直接渲染并记录错误。

- [ ] **Step 1：增加 Bloom 红灯检查**

```powershell
Require-Pattern "src\bloom_renderer.h" "struct\s+BloomRenderer" "Bloom state"
Require-Pattern "src\bloom_renderer.h" "Bloom_BeginScene" "Bloom begin interface"
Require-Pattern "src\bloom_renderer.h" "Bloom_EndScene" "Bloom composite interface"
Require-Pattern "src\bloom_renderer.cpp" "GL_RGBA16F" "HDR scene texture"
Require-Pattern "src\bloom_renderer.cpp" "blurDirection" "separable Gaussian blur"
Require-Pattern "src\bloom_renderer.cpp" "bloomTexture" "Bloom composite sampler"
Require-Pattern "src\main.cpp" "Bloom_BeginScene" "main loop Bloom begin"
Require-Pattern "src\main.cpp" "Bloom_EndScene" "main loop Bloom end"
```

- [ ] **Step 2：运行检查并确认缺少 Bloom 状态**

运行：`./tools/check_lighting_effect.ps1`

预期：失败并提示 `Bloom state`。

- [ ] **Step 3：声明闭合的 Bloom 资源接口**

`src/bloom_renderer.h`：

```cpp
#pragma once
#include <cstdio>

struct BloomRenderer;

BloomRenderer* Bloom_Create(int width, int height, FILE* debugLog);
bool Bloom_BeginScene(BloomRenderer* bloom, int width, int height, bool enabled);
void Bloom_EndScene(BloomRenderer* bloom, int width, int height, bool enabled);
void Bloom_Destroy(BloomRenderer*& bloom);
```

指针隐藏实现，避免把 GL 扩展类型泄漏到 `main.cpp`。`Bloom_Destroy` 接受引用并置空，防止重复释放。

- [ ] **Step 4：实现 GL 资源、尺寸变化和失败回退**

`BloomRenderer` 内部持有：一个场景 FBO + `GL_RGBA16F` 场景纹理、两个 ping-pong FBO + 两个半分辨率 `GL_RGBA16F` 纹理、一个全屏 VAO、模糊 program、合成 program、当前尺寸和 `ready` 状态。

所有 GL 扩展函数通过 `Win32GL_GetProcAddress()` 加载并逐项判空。`Bloom_Create` 任一步失败时记录 `[Bloom][FAIL]`、释放已创建对象并返回 `nullptr`。`Bloom_BeginScene` 在尺寸变化时原位重建纹理；禁用或不可用时绑定默认帧缓冲并返回 `false`。

纹理过滤使用 `GL_LINEAR`，包裹使用 `GL_CLAMP_TO_EDGE`，FBO 创建后必须用 `glCheckFramebufferStatus(GL_FRAMEBUFFER)` 检查完整性。

- [ ] **Step 5：实现亮部提取、10 轮分离模糊和合成**

模糊 fragment shader 使用固定权重：

```glsl
float weight[5] = float[](0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);
vec3 source = texture(sceneTexture, uv).rgb;
vec3 bright = max(source - vec3(0.78), vec3(0.0));
vec2 texel = 1.0 / vec2(textureSize(sceneTexture, 0));
vec3 result = bright * weight[0];
for (int i = 1; i < 5; ++i) {
    vec2 offset = blurDirection * texel * float(i);
    result += texture(sceneTexture, uv + offset).rgb * weight[i];
    result += texture(sceneTexture, uv - offset).rgb * weight[i];
}
```

首轮从场景纹理提取亮部，后续 9 轮在 ping-pong 纹理间交替横向/纵向模糊。合成 shader：

```glsl
vec3 scene = texture(sceneTexture, uv).rgb;
vec3 bloom = texture(bloomTexture, uv).rgb;
vec3 hdr = scene + bloom * 1.35;
vec3 mapped = vec3(1.0) - exp(-hdr * 1.05);
fragColor = vec4(mapped, 1.0);
```

禁用时主 shader 直接绘制到默认帧缓冲，`Bloom_EndScene` 不执行任何 pass。

- [ ] **Step 6：接入预热帧和主循环**

shader/VAO 初始化后调用 `Bloom_Create(fbW, fbH, debugLog)`。预热绘制与主循环都按相同顺序调用：

```cpp
const bool bloomActive = Bloom_BeginScene(bloom, fbW, fbH, cfg.lightingEffect);
// 现有黑洞 program、uniform 和 DrawArrays
Bloom_EndScene(bloom, fbW, fbH, bloomActive);
Win32GL_SwapBuffers(wgl);
```

退出时先 `Bloom_Destroy(bloom)`，再删除主 shader program/VAO/VBO 和 WGL 上下文。不得在当前上下文销毁后释放 GL 对象。

- [ ] **Step 7：构建并检查资源生命周期**

运行：

```powershell
./tools/check_lighting_effect.ps1
git diff --check
cmake --build build --config Release
```

分别以光影关闭和开启运行短测；日志不得出现 `[Bloom][FAIL]`、FBO incomplete 或 shader link failed。调整窗口/跨屏尺寸后再次检查无崩溃和黑屏。

- [ ] **Step 8：更新调试记录并提交阶段 3**

```powershell
git add src/bloom_renderer.h src/bloom_renderer.cpp src/main.cpp CMakeLists.txt tools/check_lighting_effect.ps1
git commit -m "增加吸积盘HDR辉光渲染链路"
```

---

### Task 4：多预设视觉复核、完整构建和 Release 打包

**文件：**
- 修改：`debug_state.md`（Git 忽略，仅本地记录）
- 删除：`tools/check_screen_swallow.ps1`
- 修改：`Doc/功能完成度清单.md`

**接口：**
- 输入：至少三个不同倾角/滚转的预设和光影开关状态。
- 输出：通过静态检查、构建、截图复核且已更新的 Release 包。

- [ ] **Step 1：确认旧吞噬检查无调用关系后删除**

运行：

```powershell
rg -n "check_screen_swallow" -g "!*build*" -g "!release/**"
rg -n "screenSwallow|swallowStrength|uScreenSwallow|uSwallowStrength|吞噬屏幕UI特效|吞噬强度" -g "!*build*" -g "!release/**"
```

第一条只允许历史设计/调试文档引用；第二条只允许兼容读取和历史文档。确认后删除 `tools/check_screen_swallow.ps1`，并把功能清单中的吞噬条目改成吸积盘光影。

- [ ] **Step 2：运行所有静态检查和编码检查**

```powershell
./tools/check_lighting_effect.ps1
./tools/check_qml_core_bindings.ps1
./tools/check_recording_capture.ps1
./tools/check_hotkey_settings.ps1
git diff --check
```

对本次触碰的源码和 QML 执行 UTF-8 BOM 检查；结果必须无 BOM、无乱码。

- [ ] **Step 3：执行两个 Release 构建**

```powershell
cmake --build build --config Release
cmake --build Blakhole_UI/build/Desktop_Qt_6_11_1_MinGW_64_bit-Release --config Release
```

记录是否存在新增错误和警告；既有 Qt 部署提示单独注明。

- [ ] **Step 4：进行多预设视觉验证**

开启光影，至少选择：近正面盘、明显倾斜盘、滚转方向不同的预设。每种状态检查：外层淡光、中层亮带和黑纹、盘面局部金蓝光、白金光子环、纯黑事件视界、无硬圆边界。关闭光影后截图对比，确认 Bloom 和新增光源均消失。

若自动截图无法捕获 OpenGL 窗口，明确记录限制，并使用用户可见的实际运行窗口截图，不将空白 GDI 截图作为证据。

- [ ] **Step 5：打包并做最终回归**

```powershell
./package_release.ps1
./tools/check_lighting_effect.ps1
cmake --build build --config Release
```

确认 `release/blackhole.exe`、`release/appBlakholeUI.exe` 和 shader 文件已更新。

- [ ] **Step 6：更新调试记录并提交最终文档/检查清理**

```powershell
git add tools/check_screen_swallow.ps1 Doc/功能完成度清单.md
git commit -m "完成吸积盘光影验证与吞噬清理"
```

最终用 `git status --short` 确认没有遗漏的跟踪文件，并逐项对照设计文档验收标准。
