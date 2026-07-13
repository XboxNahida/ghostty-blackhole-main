# Renderer 启动诊断与错误弹窗实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在 Renderer 文件缺失、进程创建失败、Ready 前退出、初始化日志失败或 8 秒未就绪时，恢复 Qt 主窗口并显示可复制、可打开日志目录的诊断弹窗。

**Architecture:** 新增纯 QtCore 的 `RendererStartupDiagnostics` 状态模块，负责文件预检、启动序号、日志增量解析、超时和防重复；`BlackHoleCore` 只负责 QProcess/QTimer 事件接线并向 QML 发出结构化故障信号。QML 使用独立 `RendererErrorDialog` 展示错误，同时恢复隐藏或托盘状态的主窗口。

**Tech Stack:** C++17、Qt 6 Core/Gui/Quick、QProcess、QTimer、QElapsedTimer、QFile、QDesktopServices、QML、CMake/CTest、PowerShell 静态契约。

## Global Constraints

- 所有源码与文档使用 UTF-8；修改中文时使用 `apply_patch`。
- 不改变 Renderer 图形算法、空闲判定、媒体检测、游戏检测和更新系统。
- 启动诊断超时固定为 8000 ms，日志轮询周期为 200 ms。
- 日志详情最多保留最后 8192 字节。
- 成功标记固定为 `[OK] Ready, entering main loop`；失败标记使用现有 `[FAIL]`。
- 用户主动停止和 Renderer 已 Ready 后的正常停止不得弹出启动错误。
- 同一启动序号最多报告一次；自动空闲重试在故障锁存期间不得重复弹窗。
- 本功能涉及超过 3 个文件，按四个阶段实施，每阶段结束后验证并提交。
- `debug_state.md` 与 `.superpowers` 进度文件仅本地更新，不加入 Git。

## 文件结构

- 新建 `Blakhole_UI/core/renderer_startup_diagnostics.h`：诊断枚举、结果结构、状态机接口。
- 新建 `Blakhole_UI/core/renderer_startup_diagnostics.cpp`：文件检查、日志增量解析、状态转换和错误详情生成。
- 新建 `tests/renderer_startup_diagnostics_tests.cpp`：不启动真实 GPU 的状态模块测试。
- 修改 `Blakhole_UI/CMakeLists.txt`：主程序源文件、测试目标和 CTest 注册。
- 修改 `Blakhole_UI/core/blackholecore.h/.cpp`：QProcess/QTimer 接线、自动重试锁存、结构化信号、打开日志目录。
- 新建 `Blakhole_UI/components/RendererErrorDialog.qml`：错误摘要、可选择详情和三个操作按钮。
- 修改 `Blakhole_UI/Main.qml`：接收故障、恢复窗口、同步托盘并打开弹窗。
- 新建 `tools/check_renderer_diagnostics.ps1`：后端、QML 和发布契约。

---

### Task 1: 可测试的 Renderer 启动诊断状态模块

**Files:**
- Create: `Blakhole_UI/core/renderer_startup_diagnostics.h`
- Create: `Blakhole_UI/core/renderer_startup_diagnostics.cpp`
- Create: `tests/renderer_startup_diagnostics_tests.cpp`
- Modify: `Blakhole_UI/CMakeLists.txt`

**Interfaces:**
- Consumes: Qt6::Core 的 `QString`、`QStringList`、`QFileInfo`、`QDateTime`。
- Produces:
  - `enum class RendererStartupState { Stopped, Starting, Ready, Stopping, Failed };`
  - `enum class RendererFailureKind { None, MissingFile, FailedToStart, ExitedBeforeReady, InitializationFailed, ReadyTimeout };`
  - `struct RendererDiagnostic { bool valid; quint64 attemptId; RendererFailureKind kind; QString title; QString summary; QString details; QString logPath; };`
  - `class RendererStartupDiagnostics`，公开 `beginAttempt()`、`validateRequiredFiles()`、`consumeLogSnapshot()`、`processFailedToStart()`、`processFinished()`、`timeout()`、`beginStopping()`、`state()`、`attemptId()`。

- [ ] **Step 1: 写失败测试，覆盖全部状态转换**

在 `tests/renderer_startup_diagnostics_tests.cpp` 创建 `QCoreApplication` 和临时目录，使用真实临时文件验证以下行为：

```cpp
RendererStartupDiagnostics diagnostics;
const quint64 attempt = diagnostics.beginAttempt(logPath, 12, oldModified);
Require(attempt == 1, "first attempt id");
Require(diagnostics.state() == RendererStartupState::Starting, "attempt enters Starting");

RendererDiagnostic missing = diagnostics.validateRequiredFiles(
    exePath, rootPath,
    {QStringLiteral("blackhole.glsl"),
     QStringLiteral("shaders/vert.glsl"),
     QStringLiteral("shaders/frag_desktop_header.glsl")});
Require(missing.valid && missing.kind == RendererFailureKind::MissingFile,
        "missing shader produces MissingFile");

diagnostics.beginAttempt(logPath, 0, {});
RendererDiagnostic ready = diagnostics.consumeLogSnapshot(
    QByteArrayLiteral("[OK] Ready, entering main loop\n"), 0, true);
Require(!ready.valid && diagnostics.state() == RendererStartupState::Ready,
        "current attempt Ready marker succeeds without diagnostic");

diagnostics.beginAttempt(logPath, 0, {});
RendererDiagnostic failed = diagnostics.consumeLogSnapshot(
    QByteArrayLiteral("[FAIL] Program link failed\n"), 0, true);
Require(failed.valid && failed.kind == RendererFailureKind::InitializationFailed,
        "FAIL marker produces initialization diagnostic");
Require(failed.details.contains(QStringLiteral("Program link failed")),
        "diagnostic contains renderer failure line");
```

继续覆盖：旧日志 Ready 不生效、日志截断后从头解析、Ready 前退出及退出码、FailedToStart 系统字符串、8000 ms 超时、Stopping 下退出无错误、同一 attempt 不重复报告、详情裁剪到 8192 字节以内。

- [ ] **Step 2: 配置测试目标并验证 RED**

在 `Blakhole_UI/CMakeLists.txt` 的 `BUILD_TESTING` 中增加：

```cmake
add_executable(renderer_startup_diagnostics_tests EXCLUDE_FROM_ALL
    ../tests/renderer_startup_diagnostics_tests.cpp
    core/renderer_startup_diagnostics.cpp
)
target_include_directories(renderer_startup_diagnostics_tests PRIVATE core)
target_link_libraries(renderer_startup_diagnostics_tests PRIVATE Qt6::Core)
add_test(NAME renderer_startup_diagnostics_tests COMMAND renderer_startup_diagnostics_tests)
```

Run:

```powershell
cmake --build Blakhole_UI\build\Desktop_Qt_6_11_1_MinGW_64_bit-Release --target renderer_startup_diagnostics_tests
```

Expected: FAIL，缺少 `renderer_startup_diagnostics.h/.cpp` 或未定义上述接口。

- [ ] **Step 3: 实现最小状态模块**

头文件必须声明精确接口：

```cpp
class RendererStartupDiagnostics {
public:
    quint64 beginAttempt(const QString &logPath,
                         qint64 previousLogSize,
                         const QDateTime &previousLogModified);
    RendererDiagnostic validateRequiredFiles(
        const QString &exePath,
        const QString &workingDirectory,
        const QStringList &relativeRequiredFiles);
    RendererDiagnostic consumeLogSnapshot(const QByteArray &content,
                                          qint64 fileSize,
                                          bool fileReplacedOrTruncated);
    RendererDiagnostic processFailedToStart(const QString &processError);
    RendererDiagnostic processFinished(int exitCode, bool crashed);
    RendererDiagnostic timeout();
    void beginStopping();
    RendererStartupState state() const;
    quint64 attemptId() const;
private:
    RendererDiagnostic fail(RendererFailureKind kind,
                            const QString &summary,
                            const QString &technicalDetails);
};
```

实现规则：

```cpp
static constexpr qsizetype kMaxDiagnosticLogBytes = 8192;
static constexpr auto kReadyMarker = "[OK] Ready, entering main loop";
static constexpr auto kFailureMarker = "[FAIL]";
```

`fail()` 仅在 `Starting` 且当前 attempt 未报告时返回 `valid=true`，随后进入 `Failed` 并锁定该 attempt。`consumeLogSnapshot()` 根据 baseline 大小和“文件被替换/截断”标志只解析本次新增内容；优先处理最后一条 `[FAIL]`，否则处理 Ready。

- [ ] **Step 4: 运行聚焦测试并验证 GREEN**

Run:

```powershell
cmake --build Blakhole_UI\build\Desktop_Qt_6_11_1_MinGW_64_bit-Release --target renderer_startup_diagnostics_tests
Blakhole_UI\build\Desktop_Qt_6_11_1_MinGW_64_bit-Release\renderer_startup_diagnostics_tests.exe
```

Expected: exit code 0，输出 `RENDERER_STARTUP_DIAGNOSTICS_TESTS_OK`。

- [ ] **Step 5: 接入主目标并提交**

把两个模块源文件加入 `qt_add_executable(appBlakholeUI ...)`，运行：

```powershell
git diff --check
git add Blakhole_UI/CMakeLists.txt Blakhole_UI/core/renderer_startup_diagnostics.h Blakhole_UI/core/renderer_startup_diagnostics.cpp tests/renderer_startup_diagnostics_tests.cpp
git commit -m "增加渲染器启动诊断状态模块"
```

---

### Task 2: BlackHoleCore 启动握手与故障锁存

**Files:**
- Modify: `Blakhole_UI/core/blackholecore.h`
- Modify: `Blakhole_UI/core/blackholecore.cpp`
- Create: `tools/check_renderer_diagnostics.ps1`

**Interfaces:**
- Consumes: Task 1 的 `RendererStartupDiagnostics` 和 `RendererDiagnostic`。
- Produces:
  - Signal `rendererStartupFailed(const QString &title, const QString &summary, const QString &details, const QString &logPath)`。
  - `Q_INVOKABLE void openRendererLogDirectory() const`。
  - 私有 `startRendererInternal(bool userInitiated)`、`pollRendererStartup()`、`publishRendererDiagnostic(const RendererDiagnostic&)`。

- [ ] **Step 1: 写后端静态失败契约**

创建 `tools/check_renderer_diagnostics.ps1`，要求：

```powershell
Require-Pattern 'Blakhole_UI\core\blackholecore.h' 'rendererStartupFailed' 'structured renderer failure signal'
Require-Pattern 'Blakhole_UI\core\blackholecore.h' 'openRendererLogDirectory' 'log directory action'
Require-Pattern 'Blakhole_UI\core\blackholecore.cpp' '8000' 'startup timeout'
Require-Pattern 'Blakhole_UI\core\blackholecore.cpp' '200' 'log polling interval'
Require-Pattern 'Blakhole_UI\core\blackholecore.cpp' 'startRendererInternal\(bool userInitiated\)' 'automatic retry distinction'
Require-Pattern 'Blakhole_UI\core\blackholecore.cpp' 'rendererStartupFailed' 'diagnostic publication'
```

同时禁止 `finished` 无条件报错：契约必须找到 `RendererStartupState::Stopping` 或状态模块的 `processFinished()` 调用。

- [ ] **Step 2: 运行契约验证 RED**

Run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_renderer_diagnostics.ps1
```

Expected: FAIL，首个错误为缺少结构化 `rendererStartupFailed` 信号。

- [ ] **Step 3: 在 BlackHoleCore 中接入启动尝试**

在头文件新增：

```cpp
Q_INVOKABLE void openRendererLogDirectory() const;
signals:
    void rendererStartupFailed(const QString &title,
                               const QString &summary,
                               const QString &details,
                               const QString &logPath);
private:
    void startRendererInternal(bool userInitiated);
    void pollRendererStartup();
    void publishRendererDiagnostic(const RendererDiagnostic &diagnostic);
    RendererStartupDiagnostics m_rendererDiagnostics;
    QTimer *m_rendererStartupTimer = nullptr;
    QElapsedTimer m_rendererStartupElapsed;
    QString m_rendererLogPath;
    qint64 m_rendererLogBaselineSize = 0;
    QDateTime m_rendererLogBaselineModified;
    bool m_rendererFailureLatched = false;
```

构造函数创建 200 ms 定时器并连接 `pollRendererStartup()`。`startRenderer()` 调用 `startRendererInternal(true)`；`checkIdle()` 改为 `startRendererInternal(false)`。

启动顺序必须是：

```cpp
if (!userInitiated && m_rendererFailureLatched) return;
if (userInitiated) m_rendererFailureLatched = false;
saveConfig();
findRendererExe(&projectRoot);
validateRequiredFiles(...);
record log baseline;
m_rendererDiagnostics.beginAttempt(...);
m_rendererStartupElapsed.restart();
m_rendererProcess->start(exePath, {QStringLiteral("--render")});
m_rendererStartupTimer->start(200);
```

缺失文件立即 `publishRendererDiagnostic()`，不得启动进程。

- [ ] **Step 4: 接入 QProcess 和日志轮询**

事件处理规则：

```cpp
errorOccurred(FailedToStart)
    -> processFailedToStart(errorString())
finished before Ready
    -> processFinished(code, status == QProcess::CrashExit)
stopRenderer()
    -> beginStopping(), stop timer, terminate/kill
poll every 200 ms
    -> read at most current log file, call consumeLogSnapshot()
    -> elapsed >= 8000 calls timeout()
```

`publishRendererDiagnostic()` 设置 `m_rendererFailureLatched=true`、停止轮询、发出新结构化信号，并保留旧 `rendererError(summary)` 以兼容已有接口。

`openRendererLogDirectory()` 使用：

```cpp
const QFileInfo logInfo(m_rendererLogPath);
const QString directory = logInfo.absolutePath();
QDesktopServices::openUrl(QUrl::fromLocalFile(directory));
```

- [ ] **Step 5: 验证后端契约、单测和 Qt 构建**

Run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_renderer_diagnostics.ps1
cmake --build Blakhole_UI\build\Desktop_Qt_6_11_1_MinGW_64_bit-Release --target renderer_startup_diagnostics_tests appBlakholeUI
Blakhole_UI\build\Desktop_Qt_6_11_1_MinGW_64_bit-Release\renderer_startup_diagnostics_tests.exe
```

Expected: 契约输出 `RENDERER_DIAGNOSTICS_BACKEND_OK`；测试通过；Qt UI 链接成功。

- [ ] **Step 6: 提交后端阶段**

```powershell
git diff --check
git add Blakhole_UI/core/blackholecore.h Blakhole_UI/core/blackholecore.cpp tools/check_renderer_diagnostics.ps1
git commit -m "接入渲染器启动握手与故障诊断"
```

---

### Task 3: Renderer 错误弹窗与后台窗口恢复

**Files:**
- Create: `Blakhole_UI/components/RendererErrorDialog.qml`
- Modify: `Blakhole_UI/Main.qml`
- Modify: `tools/check_renderer_diagnostics.ps1`

**Interfaces:**
- Consumes: `BlackHoleCore::rendererStartupFailed(...)` 和 `openRendererLogDirectory()`。
- Produces: `RendererErrorDialog.showFailure(title, summary, details, logPath)`。

- [ ] **Step 1: 扩展 QML 失败契约并验证 RED**

契约要求：

```powershell
Require-Pattern 'Blakhole_UI\components\RendererErrorDialog.qml' 'function showFailure' 'dialog failure API'
Require-Pattern 'Blakhole_UI\components\RendererErrorDialog.qml' 'copy\(\)' 'copy details action'
Require-Pattern 'Blakhole_UI\components\RendererErrorDialog.qml' 'openRendererLogDirectory' 'open log directory action'
Require-Pattern 'Blakhole_UI\Main.qml' 'onRendererStartupFailed' 'renderer failure connection'
Require-Pattern 'Blakhole_UI\Main.qml' 'root\.showNormal\(\)' 'restore minimized window'
Require-Pattern 'Blakhole_UI\Main.qml' 'root\.requestActivate\(\)' 'activate restored window'
Require-Pattern 'Blakhole_UI\Main.qml' 'systemTray\.visible\s*=\s*false' 'tray state reset'
```

Run 后预期因缺少 `RendererErrorDialog.qml` 失败。

- [ ] **Step 2: 新建专用错误弹窗**

`RendererErrorDialog.qml` 使用现有 `EBlurCard` 和 `EButton`，固定接口：

```qml
property var theme
property var core
property string failureTitle: ""
property string failureSummary: ""
property string failureDetails: ""
property string failureLogPath: ""

function showFailure(title, summary, details, logPath) {
    failureTitle = title
    failureSummary = summary
    failureDetails = details
    failureLogPath = logPath
    visible = true
}
```

详情使用只读 `TextArea`，设置 `selectByMouse: true`、`wrapMode: TextEdit.WrapAnywhere`。三个按钮行为：

```qml
onClicked: {
    detailsText.selectAll()
    detailsText.copy()
    detailsText.deselect()
}

onClicked: if (core) core.openRendererLogDirectory()
onClicked: dialogRoot.close()
```

弹窗宽度 `Math.min(680, parent.width - 64)`，高度 `Math.min(620, parent.height - 64)`，详情区域可滚动，避免技术日志撑出窗口。

- [ ] **Step 3: Main.qml 接收错误并恢复窗口**

在根窗口实例化弹窗，并在现有 `Connections { target: blackHoleCore }` 中加入：

```qml
function onRendererStartupFailed(title, summary, details, logPath) {
    root.visible = true
    root.showNormal()
    systemTray.visible = false
    root.raise()
    root.requestActivate()
    rendererErrorDialog.showFailure(title, summary, details, logPath)
}
```

不要在 `rendererError` 旧信号上再次打开弹窗，避免同一故障显示两次。

- [ ] **Step 4: 运行 QML 契约和完整 Qt 构建**

Run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_renderer_diagnostics.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_qml_core_bindings.ps1
cmake --build Blakhole_UI\build\Desktop_Qt_6_11_1_MinGW_64_bit-Release --target appBlakholeUI
```

Expected: `RENDERER_DIAGNOSTICS_UI_OK`、`QML_CORE_BINDINGS_OK`，QML AOT 和链接成功。

- [ ] **Step 5: 提交 UI 阶段**

```powershell
git diff --check
git add Blakhole_UI/components/RendererErrorDialog.qml Blakhole_UI/Main.qml tools/check_renderer_diagnostics.ps1
git commit -m "增加渲染器启动错误弹窗"
```

---

### Task 4: 故障演练、集中回归与 Release 打包

**Files:**
- Modify: `debug_state.md`（本地，不提交）
- Generated: `release/`
- Generated: `BlakholeUI-v1.2.0-windows-x64.zip`

**Interfaces:**
- Consumes: Tasks 1-3 的完整启动诊断链路。
- Produces: 可供用户复测的 Release ZIP 与已验证提交。

- [ ] **Step 1: 运行全部测试目标**

Run:

```powershell
cmake --build Blakhole_UI\build\Desktop_Qt_6_11_1_MinGW_64_bit-Release --target application_catalog_tests avatar_storage_tests update_release_tests update_checker_state_tests movement_settings_tests renderer_startup_diagnostics_tests
ctest --test-dir Blakhole_UI\build\Desktop_Qt_6_11_1_MinGW_64_bit-Release --output-on-failure
```

Expected: 6/6 tests passed，0 failed。

- [ ] **Step 2: 运行静态与安全回归**

Run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_renderer_diagnostics.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_mouse_inertia.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_movement_controls.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_qml_core_bindings.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_renderer_security.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File tools\run_idle_phase1_tests.ps1
```

Expected: 所有脚本 exit code 0。

- [ ] **Step 3: 执行两种真实故障演练**

在构建目录临时重命名 `blackhole.glsl`，启动 UI 并点击应用，确认主窗口出现 `MissingFile` 弹窗；恢复文件后，把测试用 Renderer 替换为一个立即以退出码 7 结束的测试进程，确认出现 `ExitedBeforeReady` 且详情包含 `7`。演练只操作构建/临时目录，不修改或删除源码文件。

同时正常启动 Renderer，确认日志出现：

```text
[OK] Mouse-follow shader center injected
[OK] Ready, entering main loop
```

并确认不弹错误。

- [ ] **Step 4: 正式干净打包**

确保 Git 跟踪文件干净后运行：

```powershell
$env:Path='C:\Qt\Tools\mingw1310_64\bin;C:\Qt\6.11.1\mingw_64\bin;' + $env:Path
powershell -NoProfile -ExecutionPolicy Bypass -File .\package_release.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_release_security.ps1 -ReleaseDir .\release
powershell -NoProfile -ExecutionPolicy Bypass -File tools\verify_release_checksums.ps1 -ReleaseDir .\release
```

Expected: `Release package ready`、`RELEASE_SECURITY_OK version=1.2.0`、`RELEASE_CHECKSUMS_OK`。

- [ ] **Step 5: 生成 ZIP 并核对本地资源边界**

```powershell
Compress-Archive -Path .\release\* -DestinationPath .\BlakholeUI-v1.2.0-windows-x64.zip -CompressionLevel Optimal -Force
Get-FileHash .\BlakholeUI-v1.2.0-windows-x64.zip -Algorithm SHA256
git ls-files | rg "QR_payment|WeChat_QR"
```

ZIP 必须包含 `appBlakholeUI.exe`、`blackhole.exe`、两张本地收款码、`RELEASE_INFO.txt`、`release_checksums.sha256`；`git ls-files` 对两张收款码无输出。

- [ ] **Step 6: 最终审查、记录和推送**

检查：

```powershell
git diff --check
git status --short
git log -4 --oneline
```

`debug_state.md` 记录测试数、故障演练、构建警告、ZIP SHA-256 和提交 SHA，但不加入 Git。确认只有 ZIP 和两张收款码未跟踪后：

```powershell
git push origin main
$local=(git rev-parse main).Trim()
$remote=(git ls-remote --heads origin main).Trim().Split()[0]
if ($local -ne $remote) { throw "origin/main SHA mismatch" }
```

Expected: 本地与远程 `main` SHA 完全一致。
