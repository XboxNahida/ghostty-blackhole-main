 # Debug State - Ghostty Blackhole → Windows Standalone
 
 ## 1. 完成状态
 
 ✅ **项目可编译运行**
 
 | 组件 | 状态 |
|------|------|
| `shaders/vert.glsl` | ✅ 全屏四边形顶点着色器 |
| `shaders/frag_header.glsl` | ✅ 程序化星空背景 + 兼容模式 GLSL 头 |
| `src/main.cpp` | ✅ Win32/GLFW/OpenGL 宿主程序（GLSL 字符串替换处理） |
| `build/blackhole.exe` | ✅ 编译通过，165 FPS 运行 |
| VS Code 配置 | ✅ F5: debug 构建 + Ctrl+F5: release 构建 |
| GLFW 依赖 | ✅ `mingw-w64-ucrt-x86_64-glfw` 已安装 |
 
 ## 2. 关键技术决策
 
 - **兼容性 profile**: 使用 `GLFW_OPENGL_COMPAT_PROFILE` + `gl_FragColor`，
   避免 NVIDIA Cg 编译器在核心模式下对复杂 shader 的链接错误
 - **字符串替换**: 用 C++ `std::regex_replace` 替换 `texture(iChannel0, ...)`，
   而非 GLSL `#define` 宏（微软/老驱动 GLSL 预处理器不兼容）
 - **原始 shader 不变**: `blackhole.glsl` 保持原样，仅通过拼接头文件 + 运行时替换适配
 
 ## 3. 文件清单
 
 ### 新增文件
 - `shaders/vert.glsl` — 顶点着色器
 - `shaders/frag_header.glsl` — 片段着色器头（星空背景、uniform 声明）
 - `src/main.cpp` — Win32/GLFW/OpenGL 宿主程序
 - `CMakeLists.txt` — CMake 构建配置（备选）
 - `.vscode/launch.json` — VS Code 调试配置（debug + release）
 - `.vscode/tasks.json` — VS Code 构建任务
 - `.vscode/settings.json` — C++ 项目设置
 - `.vscode/c_cpp_properties.json` — IntelliSense 配置
 
 ### 未改动的文件
 - `blackhole.glsl` — 原始 Ghostty 着色器
 - `claude-token.py` — 不变
 - `tuner/` — 不变
 
 ## 4. 使用方式
 
 ```bash
 # F5 调试构建（带控制台窗口，可看错误输出）
 Ctrl+F5 直接运行（无控制台窗口）
 ESC 退出
 ```
 
 着色器模式：`MODE_DEMO` — 42 秒自动演示循环
 可通过修改 `buildFragmentShader()` 中的 `MODE_DEMO` 改为 `MODE_POMODORO`
 实现无限循环增长模式。
