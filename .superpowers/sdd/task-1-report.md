# Task 1 实施报告：共享运动设置模型

## 完成内容

- 新增 `SpawnPosition` 五档枚举，数值严格为 `0..4`。
- 新增 `MovementSpawn`、`NormalizeSpawnPosition`、`ClampMovementSpeed` 和 `ResolveMovementSpawn`。
- 非法位置回退随机；速度限制为 `0.1f..3.0f`。
- 四角坐标严格为 `0.18f/0.82f`，固定位置的两个偏移均为 `0.0f`；随机模式透传全部随机参数。
- 共享模块未引入 Qt 或 Win32 依赖，并已接入 Renderer 与独立测试目标。

## RED 证据

命令：

```powershell
cmake -S Blakhole_UI -B Blakhole_UI\build\Desktop_Qt_6_11_1_MinGW_64_bit-Release -G "MinGW Makefiles" -DQt6_DIR=C:\Qt\6.11.1\mingw_64\lib\cmake\Qt6 -DBUILD_TESTING=ON
cmake --build Blakhole_UI\build\Desktop_Qt_6_11_1_MinGW_64_bit-Release --target movement_settings_tests
```

关键输出：

```text
CMake Error at CMakeLists.txt:74 (add_executable):
  Cannot find source file:
    ../src/movement_settings.cpp
CMake Generate step failed.
```

失败原因是生产模块尚不存在，符合预期；不是测试语法或工具链错误。

## GREEN 证据

命令：

```powershell
cmake -S Blakhole_UI -B Blakhole_UI\build\Desktop_Qt_6_11_1_MinGW_64_bit-Release -G "MinGW Makefiles" -DQt6_DIR=C:\Qt\6.11.1\mingw_64\lib\cmake\Qt6 -DBUILD_TESTING=ON
cmake --build Blakhole_UI\build\Desktop_Qt_6_11_1_MinGW_64_bit-Release --target movement_settings_tests
ctest --test-dir Blakhole_UI\build\Desktop_Qt_6_11_1_MinGW_64_bit-Release -R movement_settings_tests --output-on-failure
```

关键输出：

```text
[100%] Built target movement_settings_tests
1/1 Test #1: movement_settings_tests .......... Passed
100% tests passed, 0 tests failed out of 1
```

Renderer 初次执行 brief 原命令时，隔离 worktree 的 `build` 尚无 `CMakeCache.txt`。先配置后重跑：

```powershell
cmake -S . -B build -G "MinGW Makefiles"
cmake --build build --config Release
```

关键输出：

```text
[ 21%] Building CXX object CMakeFiles/blackhole.dir/src/movement_settings.cpp.obj
[100%] Linking CXX executable blackhole.exe
[100%] Built target blackhole
```

编译检查结果：无新增错误，无新增警告。Qt 配置阶段仅有可选 `WrapVulkanHeaders` 未找到提示，与本任务无关且不影响生成和测试。

## 修改文件

- `src/movement_settings.h`
- `src/movement_settings.cpp`
- `tests/movement_settings_tests.cpp`
- `CMakeLists.txt`
- `Blakhole_UI/CMakeLists.txt`
- `debug_state.md`
- `.superpowers/sdd/task-1-report.md`

## 自审

- 接口名称、参数和枚举底层类型与 brief 一致。
- 测试覆盖五档合法枚举、非法值回退、速度下限/默认值/上限、四角坐标和随机参数透传。
- 固定位置返回稳定的零偏移；非法位置经规范化后使用随机模式。
- 共享源仅依赖自身头文件，不含 Qt/Win32 依赖。
- 未修改自动生成文件，未删除现有代码，修改范围只涉及 Task 1。
- `git diff --check` 无空白错误；仅出现仓库行尾策略导致的 LF/CRLF 提示。

## 提交

- 标题：`增加黑洞运动设置共享模型`
- SHA：本报告随该提交一并创建，最终 SHA 以 `git rev-parse HEAD` 和交付摘要为准。

## 关注事项

- 无功能性阻塞。隔离 worktree 首次 Renderer 构建必须先配置根 `build` 目录。
