# 杀毒误报缓解实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 为 v1.2.0 补齐可信身份、收敛渲染器敏感行为，并生成可追溯的发布包。

**Architecture:** 使用共享 CMake 版本源生成两个 Windows 版本资源；Qt UI 独占自启动职责；渲染器使用命名互斥量、Job Object 和 `WM_HOTKEY` 替代按名称终止与按键轮询；打包阶段只剥离发布副本的调试节区并生成哈希。

**Tech Stack:** CMake 3.20、MinGW UCRT64、Win32 API、PowerShell、Qt 6.11。

## Global Constraints

- 源码和文档使用 UTF-8，新文件无 BOM。
- 不删除屏幕捕获、媒体检测、游戏检测、多显示器或 Qt 自启动功能。
- 不上传任何 EXE 到外部扫描服务。
- 下一正式版本为 `1.2.0`，标签为 `v1.2.0`。
- 每个任务先建立失败测试，完成后更新根工作区 `debug_state.md` 并提交。

---

### Task 1: 统一版本与 EXE 身份

**Files:**
- Create: `cmake/AppVersion.cmake`
- Create: `cmake/app_version.h.in`
- Create: `cmake/renderer_version.rc.in`
- Create: `Blakhole_UI/app_version.rc.in`
- Create: `tools/check_release_identity.ps1`
- Modify: `CMakeLists.txt`
- Modify: `Blakhole_UI/CMakeLists.txt`
- Modify: `Blakhole_UI/main.cpp`

**Interfaces:**
- Produces: `APP_VERSION_STRING`, `APP_VERSION_MAJOR/MINOR/PATCH` and Windows VERSIONINFO for both EXEs.

- [ ] **Step 1: Write the failing identity check**

The script must require `BLACKHOLE_VERSION "1.2.0"`, generated resource templates, `QCoreApplication::setApplicationVersion(APP_VERSION_STRING)`, and after build require non-empty `FileDescription`, `ProductName`, `CompanyName`, `FileVersion`, and `ProductVersion` for both EXEs.

- [ ] **Step 2: Run the identity check and verify red**

Run: `powershell -ExecutionPolicy Bypass -File tools/check_release_identity.ps1`

Expected: `RELEASE_IDENTITY_CHECK_FAILED` because no shared version source exists.

- [ ] **Step 3: Add the shared version source and generated resources**

`cmake/AppVersion.cmake` defines and parses exactly:

```cmake
set(BLACKHOLE_VERSION "1.2.0")
string(REPLACE "." ";" BLACKHOLE_VERSION_PARTS "${BLACKHOLE_VERSION}")
list(GET BLACKHOLE_VERSION_PARTS 0 BLACKHOLE_VERSION_MAJOR)
list(GET BLACKHOLE_VERSION_PARTS 1 BLACKHOLE_VERSION_MINOR)
list(GET BLACKHOLE_VERSION_PARTS 2 BLACKHOLE_VERSION_PATCH)
```

Both targets configure their VERSIONINFO resource with `XboxNahida` as CompanyName and keep `asInvoker`. Qt startup calls:

```cpp
QCoreApplication::setApplicationName(QStringLiteral("Blakhole UI"));
QCoreApplication::setOrganizationName(QStringLiteral("XboxNahida"));
QCoreApplication::setApplicationVersion(QStringLiteral(APP_VERSION_STRING));
```

- [ ] **Step 4: Clean-build both targets and verify green**

Run the project renderer and Qt UI Release build commands, then rerun `tools/check_release_identity.ps1`.

Expected: both builds succeed and output `RELEASE_IDENTITY_OK version=1.2.0`.

- [ ] **Step 5: Commit**

Commit message: `补齐v1.2.0版本身份信息`

### Task 2: 收敛渲染器自启动和进程管理

**Files:**
- Create: `tools/check_renderer_security.ps1`
- Modify: `src/gui_config.cpp`
- Modify: `src/main.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces: named control mutex; owned-child Job Object cleanup; renderer without Registry Run writes or process-name termination.

- [ ] **Step 1: Write the failing static security contract**

Require that renderer sources contain no `RegSetValueExA`, `RegDeleteValueA`, `OpenProcess(PROCESS_TERMINATE`, startup snapshot used to kill `blackhole.exe`, or direct `TerminateProcess`. Require a named mutex only for non-render modes and Job Object cleanup for owned children.

- [ ] **Step 2: Run the contract and verify red**

Run: `powershell -ExecutionPolicy Bypass -File tools/check_renderer_security.ps1`

Expected: failure listing registry write and process termination patterns.

- [ ] **Step 3: Remove renderer autostart side effects**

Remove the legacy `SetAutoStart` implementation and ImGui checkbox while retaining `cfg.autoStart` parsing/writing for file compatibility. Confirm with `rg` that Qt `AutoStart_Set` remains the only Run-key writer.

- [ ] **Step 4: Replace broad process cleanup**

Create a named mutex for `--config`/`--monitor`/direct control modes only. Do not apply it to `--render`. Delete startup process-name enumeration. Put monitor-created children into a `JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE` job, request `WM_CLOSE`, wait, and close the owned job instead of calling `TerminateProcess`.

- [ ] **Step 5: Build and verify process behavior**

Run the renderer Release build and `tools/check_renderer_security.ps1`.

Expected: build passes; contract reports no registry-write or broad-termination patterns; multi-render arguments remain supported.

- [ ] **Step 6: Run existing idle and renderer regressions**

Run `tools/run_idle_phase1_tests.ps1` and existing static checks.

Expected: all existing checks remain green.

- [ ] **Step 7: Commit**

Commit message: `收敛渲染器自启动与进程管理`

### Task 3: 用注册热键替代按键轮询

**Files:**
- Modify: `tools/check_renderer_security.ps1`
- Modify: `src/main.cpp`

**Interfaces:**
- Produces: `Ctrl+Alt+R` handled by `RegisterHotKey`/`WM_HOTKEY` on the primary renderer only.

- [ ] **Step 1: Extend the contract and verify red**

Forbid `GetAsyncKeyState` and require `RegisterHotKey`, `WM_HOTKEY`, and `UnregisterHotKey` in the renderer.

Expected red reason: `GetAsyncKeyState polling is still present`.

- [ ] **Step 2: Implement message-based hotkey handling**

Register `Ctrl+Alt+R` only when `screenIdx < 0`; drain `WM_HOTKEY` with `PeekMessageW`; toggle recording capture once per message; unregister during all normal exit paths. Registration failure logs diagnostics and does not stop rendering.

- [ ] **Step 3: Build and verify**

Run renderer Release build and security contract.

Expected: no `GetAsyncKeyState` import in final `blackhole.exe`; contract passes.

- [ ] **Step 4: Commit**

Commit message: `改用系统热键降低按键轮询特征`

### Task 4: 发布包卫生与安全说明

**Files:**
- Create: `SECURITY.md`
- Create: `docs/发布新版本指南.md`
- Create: `tools/check_release_security.ps1`
- Modify: `package_release.ps1`

**Interfaces:**
- Produces: stripped Release copies, `release_checksums.sha256`, `RELEASE_INFO.txt`, and Chinese publishing guidance.

- [x] **Step 1: Write the failing release-security check**

Check that packaged EXEs have VERSIONINFO, ASLR and NX, no `.debug_*` sections, no UPX section names, and that `blackhole.exe` has no network/injection/registry-write/`GetAsyncKeyState` imports.

- [x] **Step 2: Run against the current package and verify red**

Expected: failure because current Release EXEs contain DWARF debug sections and lack version information.

- [x] **Step 3: Harden packaging without stripping build artifacts**

After copying EXEs, locate MinGW `strip.exe` and run `--strip-debug` only on Release copies. Generate SHA-256 lines with lowercase filenames and write version `1.2.0`, tag `v1.2.0`, commit hash, build timestamp and unsigned status to `RELEASE_INFO.txt`.

- [x] **Step 4: Write Chinese security and publishing docs**

Explain local-only capture, sensitive permissions, hash verification, Draft/Tag/Pre-release/formal Release differences, attachment naming `BlakholeUI-v1.2.0-windows-x64.zip`, rollback, and false-positive submission evidence.

- [x] **Step 5: Package and verify green**

Run `package_release.ps1`, then `tools/check_release_security.ps1`.

Expected: `RELEASE_SECURITY_OK version=1.2.0`; build artifacts remain unstripped.

- [x] **Step 6: Commit**

Commit message: `增加可追溯安全发布流程`
