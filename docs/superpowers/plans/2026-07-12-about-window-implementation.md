# 关于窗口与头像更换实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 实现独立关于窗口、持久化头像、项目说明和常驻双收款码。

**Architecture:** 头像文件复制和持久化由 Core 负责；`AboutWindow.qml` 负责唯一独立窗口及响应式内容；`Main.qml` 连接主页头像与设置入口；二维码嵌入 Qt 资源。

**Tech Stack:** Qt 6 Quick/QML、Qt Widgets 文件对话框、Qt Core、CMake、PowerShell。

## Global Constraints

- 默认头像和失败回退始终可用。
- 自定义头像复制到 `QStandardPaths::AppDataLocation`，不长期引用用户源文件。
- 二维码完整显示且不裁切有效区域。
- 新文件 UTF-8 无 BOM；二进制图片保持原数据。

---

### Task 1: 头像存储后端

**Files:**
- Create: `Blakhole_UI/core/avatar_storage.h`
- Create: `Blakhole_UI/core/avatar_storage.cpp`
- Create: `tests/avatar_storage_tests.cpp`
- Modify: `Blakhole_UI/CMakeLists.txt`
- Modify: `Blakhole_UI/core/blackholecore.h`
- Modify: `Blakhole_UI/core/blackholecore.cpp`

**Interfaces:**
- Produces: `QString customAvatarUrl() const`, `QString avatarStatus() const`, `Q_INVOKABLE void chooseCustomAvatar()`, signals `customAvatarUrlChanged()` and `avatarStatusChanged()`.

- [ ] **Step 1: Write failing storage tests**

Cover copying a valid PNG to an isolated destination, rejecting missing/empty files without replacing the old file, extension normalization, and returning a `file:///` URL for a readable saved avatar.

- [ ] **Step 2: Run tests and verify red**

Expected: compile failure because `avatar_storage.h/.cpp` do not exist.

- [ ] **Step 3: Implement minimal storage and Core bindings**

Use `QImageReader::canRead`, `QSaveFile` or safe copy-to-temporary-and-rename semantics, and a fixed destination basename `custom_avatar.<ext>`. Persist `customAvatarPath=` in `blackhole_system.txt`; invalid saved paths clear to the default resource URL.

- [ ] **Step 4: Run tests and Qt build**

Expected: avatar tests pass and Qt Release builds without new warnings.

- [ ] **Step 5: Commit**

Commit message: `增加自定义头像持久化`

### Task 2: 关于窗口与双入口

**Files:**
- Create: `Blakhole_UI/components/AboutWindow.qml`
- Create: `tools/check_about_window.ps1`
- Modify: `Blakhole_UI/components/EAvatar.qml`
- Modify: `Blakhole_UI/Main.qml`
- Modify: `Blakhole_UI/src.qrc`
- Add binary resources: `Blakhole_UI/fonts/pic/QR_payment.jpg`, `Blakhole_UI/fonts/pic/WeChat_QR.png`

**Interfaces:**
- Consumes: `blackHoleCore.customAvatarUrl`, `chooseCustomAvatar()`, `QCoreApplication::applicationVersion` exposed through Core or context.
- Produces: one reusable `AboutWindow` instance and clickable avatar/settings entry.

- [ ] **Step 1: Write the failing QML contract**

Require two calls to `aboutWindow.openAndActivate()`, one `AboutWindow` instance, GitHub URL, both QR resource URLs, `chooseCustomAvatar()`, and responsive row/column switching.

- [ ] **Step 2: Run contract and verify red**

Expected: `ABOUT_WINDOW_CHECK_FAILED` because the component and resources are absent.

- [ ] **Step 3: Add binary resources and clickable avatar API**

Copy the two user-provided images from the root workspace into the worktree resource directory without recompression. Extend `EAvatar` with `clickable`, `clicked()`, hover feedback and tooltip while preserving the default non-clickable behavior.

- [ ] **Step 4: Implement the independent About window**

Use a single `Window` with `visible: false`, `720x760`, minimum responsive dimensions, `ScrollView`, centered avatar, colored semantic sections, exact approved Chinese text, clickable GitHub URL, restrained coffee heading, and QR images that switch from row to column below the defined content width.

- [ ] **Step 5: Wire both entry points**

Homepage avatar opens About. The settings “关于 Blakhole UI” row becomes clickable, closes the drawer, and opens the same instance. Closing hides the instance; repeated opens call `show()`, `raise()`, and `requestActivate()`.

- [ ] **Step 6: Build and verify**

Run the QML contract and Qt Release build.

Expected: `ABOUT_WINDOW_OK`; no QML compilation errors.

- [ ] **Step 7: Manual UI verification**

Verify dark/light themes, default/minimum window sizes, scroll access, QR integrity, GitHub browser open, avatar selection/cancel/restart persistence, and no duplicate windows.

- [ ] **Step 8: Commit**

Commit message: `增加关于窗口与头像入口`

