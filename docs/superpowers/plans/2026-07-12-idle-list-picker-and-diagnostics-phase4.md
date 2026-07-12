# 名单选择器与检测诊断阶段 4 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**目标：** 为三类空闲检测名单同时提供运行程序选择、EXE 文件选择和手动输入，并在名单页显示低频、无窗口标题的前台检测摘要。

**架构：** 新增独立 Qt `application_catalog` 模块，在用户打开选择器时枚举可访问的前台应用元数据并生成小图标 Data URL。`BlackHoleCore` 提供 QML 可调用入口与检测摘要属性；QML 抽取可复用名单面板和运行程序选择弹窗，三类名单共用同一套添加逻辑。

**技术栈：** C++17、Win32 Toolhelp/Process API、Qt 6 Core/Gui/Widgets、QML、CMake/CTest、PowerShell。

## 全局约束

- 三种添加方式必须同时保留：运行程序选择、选择 EXE、手动输入。
- 运行程序只在用户打开选择器时枚举，不进入每秒空闲检测路径。
- 默认排除无可执行路径、当前 Qt UI、黑洞渲染器和 Windows 系统目录中的进程。
- 展示应用名称、进程名、完整路径和图标；图标读取失败时允许使用空图标。
- 列表保存仍使用现有进程名字符串，路径只用于帮助用户辨认，不改变 v2 配置格式。
- 诊断只显示前台进程名、窗口模式和判定原因，不记录窗口标题。
- 所有新增源码使用 UTF-8 无 BOM；每个节点独立运行测试和 Qt Release 构建。

---

### 任务 1：建立应用目录模块红灯并实现

**文件：**
- 创建：`Blakhole_UI/core/application_catalog.h`
- 创建：`Blakhole_UI/core/application_catalog.cpp`
- 创建：`tests/application_catalog_tests.cpp`
- 修改：`Blakhole_UI/CMakeLists.txt`

**接口：**
- `struct ApplicationCatalogEntry { QString displayName; QString processName; QString executablePath; QString iconDataUrl; };`
- `ApplicationCatalogEntry ApplicationCatalog_FromExecutable(const QString &path, bool includeIcon = true)`。
- `QVector<ApplicationCatalogEntry> ApplicationCatalog_EnumerateRunning(DWORD excludedPid = 0)`。

- [ ] **步骤 1：写失败测试**

测试空路径返回空条目；当前测试 EXE 返回非空 `displayName`、精确 `processName` 和规范化绝对路径；传入 `excludedPid = 0` 时运行进程枚举包含当前测试进程，且不返回空路径。

- [ ] **步骤 2：配置测试目标并确认红灯**

在 Qt CMake 中增加 `application_catalog_tests` 与 CTest，运行：

```powershell
cmake --build Blakhole_UI/build/Desktop_Qt_6_11_1_MinGW_64_bit-Release --target application_catalog_tests -- -j1
```

预期：因缺少 `application_catalog.h/.cpp` 失败。

- [ ] **步骤 3：实现元数据读取**

`FromExecutable` 使用 `QFileInfo::canonicalFilePath()`，失败时退回绝对路径；进程名使用文件名，展示名优先读取 Win32 `FileDescription`，读取失败使用 `completeBaseName()`；图标通过 `QFileIconProvider` 转为 24x24 PNG Data URL。

- [ ] **步骤 4：实现保守运行进程枚举**

使用 `CreateToolhelp32Snapshot`、`OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION)` 和 `QueryFullProcessImageNameW`；按规范化路径去重并排序。所有快照和进程句柄在每条返回路径关闭；过滤调用方传入的 `excludedPid`、`blackhole.exe` 和 `%WINDIR%` 下的程序。

- [ ] **步骤 5：测试转绿并提交**

运行 CTest 和 Qt Release 构建，确认无新增警告后提交：

```powershell
git commit -m "增加运行应用目录模块"
```

---

### 任务 2：向 QML 暴露选择入口

**文件：**
- 创建：`tools/check_idle_list_pickers.ps1`
- 修改：`Blakhole_UI/core/blackholecore.h`
- 修改：`Blakhole_UI/core/blackholecore.cpp`

**接口：**
- `Q_INVOKABLE QVariantList runningApplications() const`。
- `Q_INVOKABLE QVariantMap chooseExecutable()`。
- 两个入口均返回 `displayName`、`processName`、`executablePath`、`iconDataUrl` 四个字段；取消文件选择返回空 map。

- [ ] **步骤 1：写静态契约红灯**

检查 Core 两个 `Q_INVOKABLE`、四字段 map、`QFileDialog::getOpenFileName` 和 QML 三种入口标识，先确认因缺少 Core 接口失败。

- [ ] **步骤 2：实现运行程序转换**

调用 `ApplicationCatalog_EnumerateRunning(GetCurrentProcessId())`，逐条转换为 `QVariantMap`，不缓存图标结果到每秒检测链路。

- [ ] **步骤 3：实现原生 EXE 选择**

使用 `QFileDialog::getOpenFileName(nullptr, tr("选择程序"), QString(), tr("可执行程序 (*.exe)"))`；用户选择后调用 `ApplicationCatalog_FromExecutable()`，无效或取消时返回空 map。

- [ ] **步骤 4：检查推进到 QML 红灯并构建**

预期静态检查只因缺少 QML 入口失败，Qt Release 构建成功。

---

### 任务 3：抽取复用名单面板和运行程序弹窗

**文件：**
- 创建：`Blakhole_UI/components/IdleListPanel.qml`
- 创建：`Blakhole_UI/components/RunningApplicationPicker.qml`
- 修改：`Blakhole_UI/pages/IdleListConfig.qml`

**接口：**
- `IdleListPanel` 属性：`title`、`description`、`accentColor`、`values`；信号：`manualAddRequested(string)`、`runningPickerRequested()`、`executablePickerRequested()`、`removeRequested(string)`。
- `RunningApplicationPicker` 方法：`open(var applications)`；信号：`applicationSelected(var application)`。

- [ ] **步骤 1：创建两个组件并让 Qt 编译**

名单面板提供运行程序图标按钮、EXE 文件图标按钮、手动输入框和删除按钮；弹窗按应用名称、进程名、路径显示条目，图标为空时显示通用程序图标。

- [ ] **步骤 2：逐栏迁移现有页面**

依次把“始终允许触发”“媒体识别提示”“前台强制不触发”迁移到 `IdleListPanel`，每迁移一栏都运行 QML 编译；删除前确认旧控件 ID 仅在当前页面内部使用。

- [ ] **步骤 3：连接统一添加逻辑**

页面记录目标名单类型；运行程序弹窗选择和 EXE 对话框结果都只取 `processName` 写入目标名单。`addEntry(target, processName)` 统一去空白、大小写判重并更新对应 Core 属性；手动输入调用同一函数。

- [ ] **步骤 4：静态契约转绿并运行 Qt**

`check_idle_list_pickers.ps1`、QML AOT 编译和 Qt UI 短时启动均须通过；页面固定 1400x800 下不得出现文字与按钮重叠。

---

### 任务 4：接入低频检测诊断

**文件：**
- 修改：`Blakhole_UI/core/blackholecore.h`
- 修改：`Blakhole_UI/core/blackholecore.cpp`
- 修改：`Blakhole_UI/pages/IdleListConfig.qml`
- 修改：`tools/check_idle_list_pickers.ps1`

**接口：**
- `Q_PROPERTY(QString idleDetectionSummary READ idleDetectionSummary NOTIFY idleDetectionSummaryChanged)`。
- `Q_PROPERTY(bool idleDetectionBlocked READ idleDetectionBlocked NOTIFY idleDetectionBlockedChanged)`。

- [ ] **步骤 1：扩展静态检查并确认红灯**

要求两个只读属性、对应信号、QML 绑定，并禁止摘要使用 `GetWindowText` 内容。

- [ ] **步骤 2：为主要分支设置摘要**

覆盖：空闲检测未启用、Windows 桌面、始终允许名单、前台强制名单、系统全屏、无边框全屏、Windows/Steam 游戏、媒体会话播放、音频回退、普通应用。只有摘要或阻止状态变化时发信号。

- [ ] **步骤 3：显示诊断状态带**

在三栏上方显示状态图标、`idleDetectionSummary` 和“阻止/允许”状态色；不显示或保存窗口标题。

- [ ] **步骤 4：完整验证并提交**

运行应用目录 CTest、完整空闲测试、两个名单检查、四项既有检查、双 Release 构建、UTF-8 与空白检查。人工验证运行程序选择、EXE 取消/选择、三类名单写入和前后台强制语义。

```powershell
git commit -m "优化名单添加与检测诊断"
```
