# GitHub Release 更新提醒实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 每次启动异步检测最新正式 Release，只显示设置红点，并在用户手动检查后显示结果弹窗。

**Architecture:** `update_release` 提供纯版本与 JSON 解析；`UpdateChecker` 管理 Qt Network 请求和持久状态；QML 只显示红点、设置入口和手动结果弹窗；浏览器负责下载。

**Tech Stack:** Qt 6 Core/Network/QML、GitHub Releases API、CMake、PowerShell。

## Global Constraints

- 自动检查永不主动弹窗。
- 只读取正式 Release；Draft 和 Pre-release 由 `/releases/latest` 排除。
- `blackhole.exe` 不链接网络库；网络只进入 Qt UI。
- 不自动下载、解压或覆盖程序。

---

### Task 1: 版本比较与 Release 解析

**Files:**
- Create: `Blakhole_UI/core/update_release.h`
- Create: `Blakhole_UI/core/update_release.cpp`
- Create: `tests/update_release_tests.cpp`
- Modify: `Blakhole_UI/CMakeLists.txt`

**Interfaces:**
- Produces: `ParsedVersion ParseReleaseVersion(const QString&)`, `int CompareReleaseVersions(...)`, and `UpdateReleaseInfo ParseGitHubRelease(const QByteArray&, QString *error)`.

- [ ] **Step 1: Write failing pure tests**

Cover `v1.10.0 > v1.9.9`, equality, missing patch normalization, invalid labels, valid JSON fields, invalid JSON, empty/unsafe `html_url`, and oversized notes truncation.

- [ ] **Step 2: Run and verify red**

Expected: compile failure because update release files do not exist.

- [ ] **Step 3: Implement strict parsing**

Accept only `v?MAJOR.MINOR.PATCH` numeric versions, HTTPS GitHub release URLs, bounded text fields and JSON objects. Never compare version strings lexicographically.

- [ ] **Step 4: Run tests and commit**

Expected: all update release tests pass.

Commit message: `增加Release版本解析模块`

### Task 2: 异步更新状态机

**Files:**
- Create: `Blakhole_UI/core/update_checker.h`
- Create: `Blakhole_UI/core/update_checker.cpp`
- Create: `tests/update_checker_state_tests.cpp`
- Modify: `Blakhole_UI/CMakeLists.txt`
- Modify: `Blakhole_UI/main.cpp`

**Interfaces:**
- Produces QML context `updateChecker` with properties `checking`, `updateAvailable`, `statusText`, `latestVersion`, `latestName`, `latestNotes`, and methods `checkAutomatically()`, `checkManually()`, `openDownloadPage()`, `ignoreCurrentRelease()`; signal `manualResultReady()`.

- [ ] **Step 1: Write failing state tests**

Cover automatic available/red dot, ignored version/no red dot, visited version/red dot retained, manual check clearing suppression, higher version resetting state, and current version clearing stale state.

- [ ] **Step 2: Verify red and implement state transitions**

Persist `ignoredVersion` and `visitedVersion` with `QSettings` under organization `XboxNahida`, application `Blakhole UI`. Manual checks clear suppression before applying a successful newer result.

- [ ] **Step 3: Add bounded asynchronous network requests**

Use `QNetworkAccessManager`, endpoint `https://api.github.com/repos/XboxNahida/ghostty-blackhole-main/releases/latest`, explicit User-Agent, 8-second timeout, redirect policy restricted to HTTPS and maximum 1 MiB response.

- [ ] **Step 4: Register context and verify tests/build**

Set the application version before constructing `UpdateChecker`; expose one instance as `updateChecker`; Qt target links `Qt6::Network` only.

Expected: state tests pass; renderer import table remains network-free; Qt Release builds.

- [ ] **Step 5: Commit**

Commit message: `增加异步Release更新检查`

### Task 3: 设置红点与手动结果弹窗

**Files:**
- Create: `Blakhole_UI/components/UpdateDialog.qml`
- Create: `tools/check_update_ui.ps1`
- Modify: `Blakhole_UI/Main.qml`

**Interfaces:**
- Consumes: `updateChecker` properties, commands and `manualResultReady()`.
- Produces: settings red dot, “检查更新” row and manual-only result dialog.

- [ ] **Step 1: Write failing QML contract**

Require red dot binding to `updateChecker.updateAvailable`, automatic check call without dialog open, manual check button, `manualResultReady` opening the dialog, download and ignore buttons.

- [ ] **Step 2: Verify red and implement UI**

Add a fixed-size red dot to the existing settings button. Add a settings row showing current version and status. `Component.onCompleted` calls `checkAutomatically()` only. The dialog opens exclusively from `manualResultReady()`.

- [ ] **Step 3: Verify semantics**

Run static contract and Qt Release build. Manually verify offline automatic startup is silent, manual offline shows failure, ignored release clears red dot, and download opens the browser.

- [ ] **Step 4: Commit**

Commit message: `增加设置更新红点与手动弹窗`

### Task 4: 发布说明联动与最终回归

**Files:**
- Modify: `docs/发布新版本指南.md`
- Modify: `SECURITY.md`

**Interfaces:**
- Consumes: v1.2.0 package hashes and GitHub release naming.
- Produces: exact user publishing checklist.

- [ ] **Step 1: Document the exact v1.2.0 workflow**

Include build commands, test commands, package command, hash verification, `v1.2.0` tag creation, formal Release selection, ZIP upload, public re-download verification, rollback, and update-check behavior.

- [ ] **Step 2: Run all regressions and clean builds**

Run all existing and newly added tests, `git diff --check`, UTF-8 validation, renderer clean Release build and Qt clean Release build.

- [ ] **Step 3: Package and produce release candidate ZIP**

Run `package_release.ps1`; synchronize the verified package to the root workspace `release` without deleting unrelated user files; create `BlakholeUI-v1.2.0-windows-x64.zip` from the verified package.

- [ ] **Step 4: Final runtime checks**

Launch the packaged UI, open About and Update, start/stop idle mode, verify both EXE identities, resources, imports, hashes and no running test processes.

- [ ] **Step 5: Commit**

Commit message: `完成v1.2.0发布准备`

