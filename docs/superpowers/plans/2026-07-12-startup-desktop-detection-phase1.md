# 开机自启动与桌面检测阶段 1 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**目标：** 修复 Qt 开机自启动无法即时关闭、遗留注册表值未清理、Windows 桌面被误判为无边框全屏应用三个已确认问题。

**架构：** 新增两个不依赖 Qt 的 Win32 小模块：`autostart_registry` 负责注册表写入、删除与回读验证，`foreground_window` 负责桌面/系统窗口分类和按所在显示器判断无边框全屏。Qt 与原生监控只调用共享模块，不再复制本阶段规则。

**技术栈：** C++17、Win32 Registry/User32 API、Qt 6/QML、CMake、PowerShell。

## 全局约束

- 所有新增源码、测试和文档使用 UTF-8 编码。
- 先写失败测试并确认失败，再写最小实现。
- 本阶段不接入媒体会话和游戏平台扫描，不扩大到未确认逻辑。
- 每个任务完成后更新主工作区 `debug_state.md` 并执行对应验证。

---

### 任务 1：建立 Win32 回归测试入口

**文件：**
- 创建：`tests/autostart_registry_tests.cpp`
- 创建：`tests/foreground_window_tests.cpp`
- 创建：`tools/run_idle_phase1_tests.ps1`

**接口：**
- 消费：任务 2、3 提供的 `AutoStart_Set`、`AutoStart_Query`、`Foreground_ClassifyClass`、`Foreground_CoversMonitor`。
- 产出：统一命令 `./tools/run_idle_phase1_tests.ps1`，成功输出 `IDLE_PHASE1_TESTS_OK`。

- [ ] **步骤 1：先写注册表行为测试**

使用隔离键 `HKCU\Software\BlackholeScreensaverTests\Run`，验证启用后命令路径带引号，关闭后同时删除 `BlakholeUI` 与 `BlackholeScreensaver`，最后清理测试键。

```cpp
AutoStartResult enabled = AutoStart_Set(true, L"C:\\Program Files\\Blackhole\\app.exe", testKey);
assert(enabled.success);
assert(AutoStart_Query(command, testKey));
assert(command == L"\"C:\\Program Files\\Blackhole\\app.exe\"");
assert(AutoStart_Set(false, L"", testKey).success);
assert(!AutoStart_Query(command, testKey));
```

- [ ] **步骤 2：先写窗口分类行为测试**

```cpp
assert(Foreground_ClassifyClass(L"Progman", 0) == ForegroundKind::Desktop);
assert(Foreground_ClassifyClass(L"WorkerW", 0) == ForegroundKind::Desktop);
assert(Foreground_ClassifyClass(L"Shell_TrayWnd", 0) == ForegroundKind::SystemShell);
assert(Foreground_ClassifyClass(L"GameWindow", 0) == ForegroundKind::Application);
RECT window{0, 0, 1920, 1080};
RECT monitor{0, 0, 1920, 1080};
assert(Foreground_CoversMonitor(window, monitor, 2));
```

- [ ] **步骤 3：运行测试并确认红灯**

运行：`./tools/run_idle_phase1_tests.ps1`

预期：编译失败，明确报告缺少 `src/autostart_registry.h` 或 `src/foreground_window.h`。

---

### 任务 2：实现开机自启动注册表模块并接入 Qt

**文件：**
- 创建：`src/autostart_registry.h`
- 创建：`src/autostart_registry.cpp`
- 修改：`Blakhole_UI/CMakeLists.txt`
- 修改：`Blakhole_UI/core/blackholecore.h`
- 修改：`Blakhole_UI/core/blackholecore.cpp`
- 修改：`Blakhole_UI/Main.qml`

**接口：**
- 产出：`AutoStartResult AutoStart_Set(bool, const std::wstring &, const wchar_t *)`。
- 产出：`bool AutoStart_Query(std::wstring &, const wchar_t *)`。
- 产出：Qt 属性 `QString autoStartStatus`。

- [ ] **步骤 1：实现注册表模块**

```cpp
struct AutoStartResult {
    bool success = false;
    DWORD errorCode = ERROR_SUCCESS;
};

AutoStartResult AutoStart_Set(bool enabled,
                              const std::wstring &exePath,
                              const wchar_t *subKey = kAutoStartRunKey);
bool AutoStart_Query(std::wstring &command,
                     const wchar_t *subKey = kAutoStartRunKey);
```

启用使用 `RegCreateKeyExW`、`RegSetValueExW` 和回读验证；禁用删除 `BlakholeUI` 与 `BlackholeScreensaver`，`ERROR_FILE_NOT_FOUND` 视为成功。

- [ ] **步骤 2：运行注册表测试并确认转绿**

运行：`./tools/run_idle_phase1_tests.ps1 -Only AutoStart`

预期：输出 `AUTOSTART_REGISTRY_TESTS_OK`，测试注册表键已清理。

- [ ] **步骤 3：接入 Qt 属性切换**

`setAutoStart()` 先同步注册表，失败时保持原值并更新状态；`saveConfig()` 复用模块；`loadConfig()` 完成文件解析后以实际注册表状态校准界面。

```cpp
void BlackHoleCore::setAutoStart(bool value)
{
    if (m_autoStart == value) return;
    const auto result = AutoStart_Set(value,
        QDir::toNativeSeparators(QCoreApplication::applicationFilePath()).toStdWString());
    if (!result.success) {
        setAutoStartStatus(tr("开机自启动更新失败，错误码 %1").arg(result.errorCode));
        return;
    }
    m_autoStart = value;
    setAutoStartStatus(value ? tr("开机自启动已开启") : tr("开机自启动已关闭"));
    emit autoStartChanged();
}
```

- [ ] **步骤 4：显示同步状态并构建 Qt UI**

QML 在设置项下显示 `autoStartStatus`；失败使用错误色，成功使用弱文本色。

运行：`cmake --build Blakhole_UI/build/Desktop_Qt_6_11_1_MinGW_64_bit-Release --config Release -- -j1`

预期：构建成功，无新增警告或错误。

- [ ] **步骤 5：提交任务 2**

```powershell
git add src/autostart_registry.* tests/autostart_registry_tests.cpp tools/run_idle_phase1_tests.ps1 Blakhole_UI/CMakeLists.txt Blakhole_UI/core/blackholecore.* Blakhole_UI/Main.qml
git commit -m "修复Qt开机自启动即时同步"
```

---

### 任务 3：共享桌面与全屏窗口判定

**文件：**
- 创建：`src/foreground_window.h`
- 创建：`src/foreground_window.cpp`
- 修改：`CMakeLists.txt`
- 修改：`Blakhole_UI/CMakeLists.txt`
- 修改：`src/main.cpp`
- 修改：`Blakhole_UI/core/blackholecore.cpp`

**接口：**
- 产出：`ForegroundKind Foreground_ClassifyClass(const wchar_t *, LONG_PTR)`。
- 产出：`ForegroundKind Foreground_Classify(HWND)`。
- 产出：`bool Foreground_CoversMonitor(const RECT &, const RECT &, int)`。
- 产出：`bool Foreground_IsBorderlessFullscreen(HWND)`。

- [ ] **步骤 1：实现分类与边界函数**

`Progman`、`WorkerW` 返回 `Desktop`；任务栏、开始菜单和任务切换器返回 `SystemShell`；黑洞类名或完整透明顶层扩展样式返回 `BlackholeOverlay`；其余返回 `Application`。边界比较允许 2 像素误差。

- [ ] **步骤 2：运行窗口测试并确认转绿**

运行：`./tools/run_idle_phase1_tests.ps1 -Only Foreground`

预期：输出 `FOREGROUND_WINDOW_TESTS_OK`。

- [ ] **步骤 3：替换两套重复判断**

Qt 和原生路径先调用 `Foreground_Classify(fg)`；桌面、系统外壳和黑洞自身直接返回“不阻止”。无边框全屏统一调用 `Foreground_IsBorderlessFullscreen(fg)`，内部通过 `MonitorFromWindow` 与 `GetMonitorInfoW` 使用窗口所在显示器。

- [ ] **步骤 4：运行阶段测试和既有检查**

```powershell
./tools/run_idle_phase1_tests.ps1
./tools/check_lighting_effect.ps1
./tools/check_qml_core_bindings.ps1
./tools/check_recording_capture.ps1
./tools/check_hotkey_settings.ps1
```

预期：输出 `IDLE_PHASE1_TESTS_OK` 及四项既有 `*_OK`。

- [ ] **步骤 5：构建两个 Release 目标**

```powershell
cmake --build build --config Release
cmake --build Blakhole_UI/build/Desktop_Qt_6_11_1_MinGW_64_bit-Release --config Release -- -j1
```

预期：退出码均为 0，无新增编译警告或错误。

- [ ] **步骤 6：提交任务 3**

```powershell
git add src/foreground_window.* tests/foreground_window_tests.cpp CMakeLists.txt Blakhole_UI/CMakeLists.txt src/main.cpp Blakhole_UI/core/blackholecore.cpp
git commit -m "修复桌面误判并统一全屏检测"
```

---

### 任务 4：运行时与发布前验证

**文件：**
- 修改：主工作区 `debug_state.md`

- [ ] **步骤 1：验证注册表即时切换**

切换 Qt 开关后立即运行：

```powershell
reg query "HKCU\Software\Microsoft\Windows\CurrentVersion\Run" /v BlakholeUI
```

关闭时预期值不存在；开启时预期值为带引号的当前 Qt 可执行文件路径。完成后恢复用户原始选择。

- [ ] **步骤 2：验证桌面空闲触发**

临时将空闲阈值设为 1 秒并启动空闲检测，回到 Windows 桌面；预期黑洞能够启动。完成后恢复原阈值。

- [ ] **步骤 3：最终检查并记录**

运行 `git diff --check`、UTF-8 无 BOM 检查、阶段测试、四项既有检查和两个 Release 构建；在 `debug_state.md` 记录新增警告、错误、人工验证结果和剩余风险。
