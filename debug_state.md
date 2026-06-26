# Debug State — Blackhole v4: WGC + PBO 架构

## 当前状态

### 编译 ✅
- 零警告零错误
- 源文件: main.cpp + capture_wgc.cpp + capture_dxgi.cpp + gl_texture.cpp
- 依赖: glfw + opengl32 + d3d11 + dxgi + runtimeobject + dwmapi + comctl32

### 默认模式: WGC（稳定可用）
```
blackhole.exe always          → WGC (默认)
blackhole.exe always wgc      → WGC 显式
blackhole.exe always dxgi     → DXGI (备选)
```

### GPU 管线
```
Capture(GetFrame) → CopyResource → Map → glTexSubImage2D → blackhole.glsl → 渲染
```
无 fence、无 Flush、无 PBO、无 SPIR-V。纯顺序同步管线。

### 已修复的 Bug
| 问题 | 修复 |
|------|------|
| 黑洞事件视界透明 | 删除 WS_EX_LAYERED + LWA_COLORKEY |
| 切换应用有黄框 | WM_NCACTIVATE→FALSE + DWMWA_NCRENDERING_POLICY |
| 光标不显示 | 删除 WS_EX_LAYERED、恢复 WGC 光标捕获 |
| 闪烁/黑条(WGC) | 删除 fence/flush/PBO，回到简单同步管线 |
| DXGI 帧不刷新 | 循环前预取一帧建立 ReleaseFrame 配对 |

### 窗口设置
- WS_EX_TRANSPARENT — 点击穿透
- WS_EX_TOOLWINDOW — 隐藏任务栏
- WS_EX_NOACTIVATE — 禁止激活
- WDA_EXCLUDEFROMCAPTURE — 防止反馈循环
- GLFW_FOCUS_ON_SHOW=FALSE — 不抢焦点
- DwmExtendFrameIntoClientArea(-1) — 去掉边框
- DWMWA_NCRENDERING_POLICY=DISABLED — 禁用非客户区
- WM_NCACTIVATE→FALSE — 拦截激活消息

## 文件结构
| 文件 | 说明 |
|------|------|
| src/main.cpp | 主程序：窗口 + 渲染循环 + 双 capture 切换 |
| src/capture_wgc.h/cpp | WGC 捕获模块 |
| src/capture_dxgi.h/cpp | DXGI 捕获模块(备选) |
| src/gl_texture.h/cpp | 同步 glTexSubImage2D 上传 |
| blackhole.glsl | 黑洞物理 shader（不修改） |
| shaders/vert.glsl | 顶点 shader |
| shaders/frag_desktop_header.glsl | shader 头 |