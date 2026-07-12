# 系统媒体会话检测阶段 2 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**目标：** 使用 Windows 系统媒体会话识别实际播放/暂停状态，重点补齐哔哩哔哩 Windows 客户端检测，同时保持浏览器标题约束和现有音频回退。

**架构：** 新增不依赖 Qt 的 `media_session` 模块，使用从本机 `Windows.Media.winmd` 核验的最小 WinRT ABI。模块缓存 SessionManager，输出播放状态与来源应用；Qt 与原生监控在现有进程分类之后消费该结果。

**技术栈：** C++17、WinRT ABI、`runtimeobject`、`ole32`、MinGW、Qt 6、PowerShell。

## 全局约束

- 不下载或引入第三方 WinRT 包。
- 仅声明本阶段使用的 Manager、Session、PlaybackInfo 与异步操作 ABI。
- 系统媒体接口不可用或超时时必须回退到现有音频检测，不能阻塞或崩溃。
- `Playing` 才阻止；`Paused`、`Stopped`、无会话均不因媒体会话阻止。
- 浏览器仍需前景标题命中视频关键词，不能因后台浏览器标签播放而扩大误判。
- 每个生产修改前先建立失败测试，节点完成后更新主工作区 `debug_state.md`。

---

### 任务 1：建立媒体状态与来源匹配测试

**文件：**
- 创建：`tests/media_session_tests.cpp`
- 修改：`tools/run_idle_phase1_tests.ps1`

**接口：**
- 消费：`MediaSession_QueryCurrent()` 与 `MediaSession_SourceMatchesProcess()`。
- 产出：`./tools/run_idle_phase1_tests.ps1 -Only MediaSession`。

- [ ] **步骤 1：写来源匹配测试**

```cpp
assert(MediaSession_SourceMatchesProcess("bilibiliuwp_abc", "bilibili.exe"));
assert(MediaSession_SourceMatchesProcess("chrome.exe", "chrome.exe"));
assert(!MediaSession_SourceMatchesProcess("spotify.exe", "bilibili.exe"));
assert(!MediaSession_SourceMatchesProcess("", "bilibili.exe"));
```

- [ ] **步骤 2：写实时探测容错测试**

调用 `MediaSession_QueryCurrent(500)`；无媒体会话时允许返回 `None`，系统接口不可用时允许返回 `Unavailable`，但调用必须在 1 秒内返回且不崩溃。输出状态、来源与 HRESULT 供诊断。

- [ ] **步骤 3：运行并确认红灯**

运行：`./tools/run_idle_phase1_tests.ps1 -Only MediaSession`

预期：编译失败，报告缺少 `src/media_session.h/.cpp`。

---

### 任务 2：实现最小 Windows 媒体会话 ABI

**文件：**
- 创建：`src/media_session.h`
- 创建：`src/media_session.cpp`
- 修改：`CMakeLists.txt`
- 修改：`Blakhole_UI/CMakeLists.txt`

**接口：**
- 产出：`MediaSessionSnapshot MediaSession_QueryCurrent(unsigned timeoutMs = 500)`。
- 产出：`bool MediaSession_SourceMatchesProcess(const std::string &, const std::string &)`。

```cpp
enum class MediaPlaybackState {
    Unavailable,
    None,
    Closed,
    Opened,
    Changing,
    Stopped,
    Playing,
    Paused
};

struct MediaSessionSnapshot {
    MediaPlaybackState state = MediaPlaybackState::Unavailable;
    std::string sourceAppId;
    HRESULT error = S_OK;
};
```

- [ ] **步骤 1：声明经元数据核验的最小接口**

使用以下官方接口 GUID：

- ManagerStatics：`2050C4EE-11A0-57DE-AED7-C97C70338245`
- Manager：`CACE8EAC-E86E-504A-AB31-5FF8FF1BCE49`
- Session：`7148C835-9B14-5AE2-AB85-DC9B1C14E1A8`
- PlaybackInfo：`94B4B6CF-E8BA-51AD-87A7-C10ADE106127`

异步对象由 `RequestAsync` 直接返回，不依赖参数化接口 IID；通过 `IAsyncInfo` 查询状态，完成后调用 `GetResults`。

- [ ] **步骤 2：实现一次初始化与超时回退**

首次调用通过 `RoGetActivationFactory` 获取 ManagerStatics，等待 `RequestAsync` 最多 `timeoutMs`；成功后缓存 Manager。失败或超时记录 HRESULT，后续返回 `Unavailable`，不在每秒检测路径重复长等待。

- [ ] **步骤 3：读取当前状态**

调用 Manager `GetCurrentSession`；空指针返回 `None`。读取 `SourceAppUserModelId` 与 `GetPlaybackInfo()->get_PlaybackStatus()`，将 Windows 枚举值 0～5映射为 Closed、Opened、Changing、Stopped、Playing、Paused。

- [ ] **步骤 4：实现保守来源匹配**

来源和进程名统一 ASCII 小写、去 `.exe`。长度至少 3 的进程 stem 与来源相等或互相包含才匹配；空值不匹配。

- [ ] **步骤 5：运行媒体测试并确认转绿**

运行：`./tools/run_idle_phase1_tests.ps1 -Only MediaSession`

预期：输出 `MEDIA_SESSION_TESTS_OK`，实时探测在 1 秒内完成。

---

### 任务 3：接入 Qt 与原生监控

**文件：**
- 修改：`src/main.cpp`
- 修改：`Blakhole_UI/core/blackholecore.cpp`

**接口：**
- 消费：任务 2 的 `MediaSessionSnapshot`。
- 保持：现有进程名单、浏览器标题关键词和音频峰值回退。

- [ ] **步骤 1：原生路径接入**

完成进程分类及浏览器标题检查后查询媒体会话。仅当状态为 `Playing` 且来源匹配前台进程时立即返回 `true`；其他状态继续执行现有音频回退。

```cpp
const MediaSessionSnapshot media = MediaSession_QueryCurrent();
if (media.state == MediaPlaybackState::Playing &&
    MediaSession_SourceMatchesProcess(media.sourceAppId, pname)) {
    return true;
}
```

- [ ] **步骤 2：Qt 路径接入**

使用同一条件设置 `watchingVideo = true` 并跳转到统一收尾；不复制 WinRT ABI 或另建 Qt 媒体实现。

- [ ] **步骤 3：运行阶段测试和既有检查**

```powershell
./tools/run_idle_phase1_tests.ps1
./tools/check_lighting_effect.ps1
./tools/check_qml_core_bindings.ps1
./tools/check_recording_capture.ps1
./tools/check_hotkey_settings.ps1
```

预期：媒体、阶段 1 与四项既有检查全部输出 `*_OK`。

- [ ] **步骤 4：构建两个 Release 目标**

```powershell
cmake --build build --config Release
cmake --build Blakhole_UI/build/Desktop_Qt_6_11_1_MinGW_64_bit-Release --config Release -- -j1
```

预期：退出码为 0，无新增编译警告或错误。

- [ ] **步骤 5：提交媒体阶段**

```powershell
git add src/media_session.* tests/media_session_tests.cpp tools/run_idle_phase1_tests.ps1 CMakeLists.txt Blakhole_UI/CMakeLists.txt src/main.cpp Blakhole_UI/core/blackholecore.cpp
git commit -m "接入Windows系统媒体播放状态检测"
```

---

### 任务 4：运行验收与已知限制

**文件：**
- 修改：主工作区 `debug_state.md`

- [ ] **步骤 1：浏览器回归**

哔哩哔哩网页播放时阻止，暂停时允许；普通无视频标题浏览器窗口不因其他媒体会话阻止。

- [ ] **步骤 2：客户端重点验收**

哔哩哔哩 Windows 客户端前台播放时阻止，暂停时允许。若客户端不发布系统媒体会话，记录探测输出并进入后续“进程族音频回退”节点，不猜测修复。

- [ ] **步骤 3：记录结果**

记录可自动完成的实时探测结果、需要用户完成的客户端复测、构建警告与剩余兼容风险。
