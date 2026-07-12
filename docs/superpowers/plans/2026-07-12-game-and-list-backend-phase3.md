# 游戏检测与三类名单后端阶段 3 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**目标：** 自动识别 Windows 已记录游戏和 Steam 安装游戏，并将现有两类名单迁移为“始终允许、媒体提示、前台强制不触发”三类后端模型。

**架构：** 新增共享 `game_detection` 模块，缓存 `HKCU\System\GameConfigStore\Children` 中的精确游戏路径，并识别 `steamapps\common` 安装路径。Qt 新增独立强制名单，现有 `idleBlacklist` 保留为媒体提示名单以兼容旧配置。

**技术栈：** C++17、Win32 Registry/Process API、Qt 6、QML、PowerShell。

## 全局约束

- 自动游戏判断不使用 CPU/GPU 功耗阈值。
- 未命中可靠路径时允许漏检，不用宽泛图形模块规则扩大误判。
- 强制名单仅当前台进程命中时生效，后台常驻不阻止。
- 旧 `[blacklist]` 数据迁移为媒体提示，不得直接迁移成强制阻止。
- 所有生产修改先建立失败测试或失败静态检查。

---

### 任务 1：建立游戏路径与名单契约红灯

**文件：**
- 创建：`tests/game_detection_tests.cpp`
- 创建：`tools/check_idle_lists.ps1`
- 修改：`tools/run_idle_phase1_tests.ps1`

- [ ] **步骤 1：写游戏路径测试**

```cpp
assert(GameDetection_IsKnownGamePath(
    L"D:\\SteamLibrary\\steamapps\\common\\Game\\game.exe", {}));
assert(GameDetection_IsKnownGamePath(
    L"D:\\Games\\Known\\game.exe", {L"d:\\games\\known\\game.exe"}));
assert(!GameDetection_IsKnownGamePath(
    L"C:\\Program Files\\Browser\\browser.exe", {}));
```

实时加载 Windows 游戏目录必须在 1 秒内返回；目录为空在干净系统上允许，但不能报错或泄漏句柄。

- [ ] **步骤 2：写名单契约静态检查**

要求 Qt Core 存在 `idleForceBlocklist` 属性、配置写入 `[mediaHints]` 与 `[forceBlocklist]`、加载兼容旧 `[blacklist]`、前台检测在音频条件前检查强制名单。

- [ ] **步骤 3：运行并确认红灯**

```powershell
./tools/run_idle_phase1_tests.ps1 -Only GameDetection
./tools/check_idle_lists.ps1
```

预期：分别因缺少 `game_detection` 模块和 `idleForceBlocklist` 失败。

---

### 任务 2：实现共享保守游戏检测

**文件：**
- 创建：`src/game_detection.h`
- 创建：`src/game_detection.cpp`
- 修改：`CMakeLists.txt`
- 修改：`Blakhole_UI/CMakeLists.txt`
- 修改：`src/main.cpp`
- 修改：`Blakhole_UI/core/blackholecore.cpp`

**接口：**
- `std::vector<std::wstring> GameDetection_LoadWindowsCatalog()`。
- `bool GameDetection_IsKnownGamePath(const std::wstring &, const std::vector<std::wstring> &)`。
- `bool GameDetection_IsKnownGameProcess(DWORD pid)`。
- `void GameDetection_RefreshCatalog()`。

- [ ] **步骤 1：读取 Windows 精确路径目录**

枚举 `GameConfigStore\Children` 子键，只读取非空 `MatchedExeFullPath`；规范化斜杠和大小写，去重后缓存。所有注册表句柄在当前返回路径关闭。

- [ ] **步骤 2：实现路径判定**

精确命中缓存目录，或规范化路径包含 `\steamapps\common\` 时返回 true；浏览器、编辑器、Steam 客户端自身路径不命中。

- [ ] **步骤 3：实现进程判定**

使用 `OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION)` 与 `QueryFullProcessImageNameW` 获取前台进程路径，读取失败返回 false。

- [ ] **步骤 4：测试转绿并接入两条监控**

Qt 与原生监控在获得前台 PID 后调用 `GameDetection_IsKnownGameProcess(pid)`；命中立即阻止。全屏判断仍保留更高优先级。

- [ ] **步骤 5：构建两个 Release 目标**

运行完整阶段测试、四项既有检查和两个 Release 构建，预期无新增警告或错误。

---

### 任务 3：三类名单迁移与前台强制语义

**文件：**
- 修改：`Blakhole_UI/core/blackholecore.h`
- 修改：`Blakhole_UI/core/blackholecore.cpp`
- 修改：`Blakhole_UI/pages/IdleListConfig.qml`

**接口：**
- 新增 Qt 属性 `QStringList idleForceBlocklist`。
- 现有 `idleWhitelist` 语义改名显示为“始终允许触发”。
- 现有 `idleBlacklist` 后端保留，界面显示为“媒体识别提示”。

- [ ] **步骤 1：新增强制名单属性**

添加 getter、setter、signal 与成员，setter 规范化空白、忽略空项并按不区分大小写去重。

- [ ] **步骤 2：实现 v2 配置保存与 v1 迁移**

保存头为 `# Blackhole Idle Lists v2`，写入 `[whitelist]`、`[mediaHints]`、`[forceBlocklist]`。加载时旧 `[blacklist]` 进入 `m_idleBlacklist`，新三段分别进入对应成员。

- [ ] **步骤 3：接入前台强制检查**

获取 `pname` 后、媒体与音频判断前检查 `m_idleForceBlocklist`；仅当前台进程名命中才设置 `watchingVideo = true`。

- [ ] **步骤 4：修正现有名单页语义**

本阶段先显示三类列表并保留手动输入；“运行程序选择”和“选择 EXE”在阶段 4 实现。媒体提示列表不再写“始终阻止”。

- [ ] **步骤 5：验证并提交**

运行 `check_idle_lists.ps1`、完整测试、四项既有检查、两个 Release 构建、UTF-8 与空白检查。

```powershell
git add src/game_detection.* tests/game_detection_tests.cpp tools/run_idle_phase1_tests.ps1 tools/check_idle_lists.ps1 CMakeLists.txt Blakhole_UI/CMakeLists.txt src/main.cpp Blakhole_UI/core/blackholecore.* Blakhole_UI/pages/IdleListConfig.qml
git commit -m "增加保守游戏检测与前台强制名单"
```

---

### 任务 4：运行验收

- [ ] 使用本机 `GameConfigStore` 中至少一个现有游戏路径验证目录命中。
- [ ] 使用浏览器或编辑器路径验证不会误判。
- [ ] 手动把一个前台普通程序加入强制名单，验证前台阻止、切后台后不阻止。
- [ ] 更新主工作区 `debug_state.md`，记录自动与人工验证结果。
