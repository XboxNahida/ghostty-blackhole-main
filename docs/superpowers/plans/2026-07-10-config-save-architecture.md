# 配置保存架构实施计划

> 执行要求：按阶段实施。每个阶段先做失败检查，再改代码，再构建验证。保留现有 `txt` 配置格式，不做一次性大迁移。

## 目标

让 Qt UI 和渲染器读写同一份配置文件，并让黑洞大小模式的 UI 语义清晰。

## 架构原则

- 保留现有 `blackhole_presets.txt` 和 `blackhole_advanced.txt` 两个文本文件。
- Qt UI 统一使用 `BlackHoleCore::configDir()` 计算配置目录。
- 渲染器继续从当前工作目录读取同名配置文件。
- UI 层负责把大小模式映射成兼容字段：
  - 默认出生动画：`fixedSize=false`, `growEnabled=false`
  - 自定义渐变：`fixedSize=false`, `growEnabled=true`
  - 固定大小：`fixedSize=true`, `growEnabled=false`

## 全局约束

- 新写文档使用中文。
- 不跨工作区修改文件。
- 源码和文档保持 UTF-8。
- 每个小节点更新 `debug_state.md`。
- 能编译时必须执行主程序和 Qt UI 构建检查。
- 不修改自动生成文件。

## 任务 1：统一高级配置路径

修改文件：
- `Blakhole_UI/core/blackholecore.cpp`
- `debug_state.md`

实现内容：
- `saveAdvancedConfig()` 使用 `configDir() + "/blackhole_advanced.txt"`。
- `loadAdvancedConfig()` 使用 `configDir() + "/blackhole_advanced.txt"`。
- 不改变 `blackhole_advanced.txt` 的字段格式。

验证命令：

```powershell
$core = Get-Content -Raw Blakhole_UI/core/blackholecore.cpp
$saveUsesConfigDir = $core -match 'void BlackHoleCore::saveAdvancedConfig\(\)[\s\S]*configDir\(\) \+ "/blackhole_advanced.txt"'
$loadUsesConfigDir = $core -match 'void BlackHoleCore::loadAdvancedConfig\(\)[\s\S]*configDir\(\) \+ "/blackhole_advanced.txt"'
if (-not ($saveUsesConfigDir -and $loadUsesConfigDir)) {
  Write-Host 'RED: advanced config path is not unified'
  exit 1
}
Write-Host 'ADVANCED_CONFIG_PATH_OK'
```

构建命令：

```powershell
cmake --build Blakhole_UI/build/Desktop_Qt_6_11_1_MinGW_64_bit-Release --config Release
```

## 任务 2：整理大小模式 UI 语义

修改文件：
- `Blakhole_UI/pages/AdvancedConfig.qml`
- `debug_state.md`

实现内容：
- 将“逐渐增长”改成“自定义渐变”，避免用户理解成关闭后就完全不增长。
- 增加说明：两者都不选时使用默认出生动画。
- 保持现有互斥逻辑：
  - 开启自定义渐变时关闭固定大小。
  - 开启固定大小时关闭自定义渐变。
- 不改 shader 的默认出生动画语义。

验证命令：

```powershell
$qml = Get-Content -Raw -Encoding UTF8 Blakhole_UI/pages/AdvancedConfig.qml
$hasModeText = $qml -match '默认出生动画|自定义渐变|固定大小'
$hasMutualExclusion = $qml -match 'bhCore\.growEnabled = false' -and $qml -match 'bhCore\.fixedSize = false'
if (-not ($hasModeText -and $hasMutualExclusion)) {
  Write-Host 'RED: size mode UI is not explicit enough'
  exit 1
}
Write-Host 'SIZE_MODE_UI_OK'
```

构建命令：

```powershell
cmake --build Blakhole_UI/build/Desktop_Qt_6_11_1_MinGW_64_bit-Release --config Release
```

## 任务 3：总体验证与 release 同步

操作内容：
- 构建主程序。
- 构建 Qt UI。
- 执行 `git diff --check`。
- 同步 `release/blackhole.exe` 和 `release/appBlakholeUI.exe`。
- 说明：`release/` 当前被 Git 忽略，同步运行包不会进入提交。

验证命令：

```powershell
cmake --build build --config Release
cmake --build Blakhole_UI/build/Desktop_Qt_6_11_1_MinGW_64_bit-Release --config Release
git diff --check
```

同步命令：

```powershell
Copy-Item -LiteralPath build\blackhole.exe -Destination release\blackhole.exe -Force
Copy-Item -LiteralPath Blakhole_UI\build\Desktop_Qt_6_11_1_MinGW_64_bit-Release\appBlakholeUI.exe -Destination release\appBlakholeUI.exe -Force
```

## 任务 4：轻量配置路径辅助函数

修改文件：
- `Blakhole_UI/core/blackholecore.h`
- `Blakhole_UI/core/blackholecore.cpp`
- `debug_state.md`

实现内容：
- 新增 `BlackHoleCore::configPath(const QString &fileName) const`，集中拼接 `configDir()` 与配置文件名。
- 将以下侧车配置统一改为 `configPath(...)`：
  - `blackhole_advanced.txt`
  - `blackhole_idlelist.txt`
  - `blackhole_schedule.txt`
  - `blackhole_system.txt`
  - `blackhole_lists.txt`
- 保留 `configFilePath()` 与 `findRendererExe()` 中的 `applicationDirPath()`，它们仍负责发现应用目录和渲染器路径。
- 不改变任何配置文件格式和保存时机。

验证命令：

```powershell
$core = Get-Content -Raw -Encoding UTF8 Blakhole_UI/core/blackholecore.cpp
$header = Get-Content -Raw -Encoding UTF8 Blakhole_UI/core/blackholecore.h
$hasHelper = $header -match 'QString\s+configPath\(const QString\s*&\s*fileName\)\s+const;'
$advancedUsesHelper = $core -match 'configPath\("blackhole_advanced\.txt"\)'
$idleUsesHelper = $core -match 'configPath\("blackhole_idlelist\.txt"\)'
$scheduleUsesHelper = $core -match 'configPath\("blackhole_schedule\.txt"\)'
$systemUsesHelper = $core -match 'configPath\("blackhole_system\.txt"\)'
$listsUsesHelper = $core -match 'configPath\("blackhole_lists\.txt"\)'
if (-not ($hasHelper -and $advancedUsesHelper -and $idleUsesHelper -and $scheduleUsesHelper -and $systemUsesHelper -and $listsUsesHelper)) {
  Write-Host 'RED: sidecar config paths are not centralized'
  exit 1
}
Write-Host 'CONFIG_PATH_HELPER_OK'
```

## 风险分析

- 这次只统一路径和 UI 语义，不重写配置格式，迁移风险低。
- 鼠标跟随、高级效果和固定大小仍然需要重启渲染器后读取新配置；仅重启 UI 不会让正在运行的渲染器热更新。
- `growEnabled=false` 保留为默认出生动画，不直接变成满级固定大小，避免破坏之前修复过的逐渐变大逻辑。
- 路径 helper 会让更多 UI 侧车配置跟随渲染器工作目录；如果用户旧配置散落在 UI exe 同级目录，可能需要手动迁移一次旧文件。
