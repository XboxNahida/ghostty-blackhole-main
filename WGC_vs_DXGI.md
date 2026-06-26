# WGC vs DXGI Desktop Duplication 对比

## 一句话总结

**WGC 帧可能不完整，但持续更新。DXGI 帧完整，但生命周期敏感。**

## 详细对比

| 维度 | WGC | DXGI Duplication |
|------|-----|------------------|
| 帧来源 | DWM 合成快照 | GPU backbuffer 拷贝 |
| 帧完整性 | 大画面变动时可能拿到半帧 | 每帧完整 |
| 帧稳定性 | 持续输出，不卡死 | 配对错误则全部失效 |
| 光标 | 默认捕获（可关） | 不捕获 |
| 多 GPU | 支持 | 需同 GPU |
| DRM 内容 | 可捕获 | 可能黑屏 |
| 全屏独占程序 | 可捕获 | 可能失败 |
| 延迟 | 有 DWM 合成延迟 | 低延迟 |
| API 复杂度 | WinRT（高） | COM（低） |
| 恢复机制 | 自动 | 需手动重建 |

## 本项目实测

| 问题 | WGC | DXGI |
|------|-----|------|
| 编译通过 | ✅ | ✅ |
| 启动捕获 | ✅ | ✅ |
| 画面持续刷新 | ✅ | ❌ (INVALID_CALL) |
| 黑洞效果正常 | ✅ | ❌ (背景冻结) |
| 切应用无闪烁 | ⚠ 偶发轻微 | (未测) |
| 高帧率稳定 | ✅ 165Hz | (未测) |

## DXGI 失败根因

DXGI Desktop Duplication 的 ReleaseFrame / AcquireNextFrame 配对极其严格：
- 必须先取帧，才能释放
- 循环前不取第一帧 → 第一个 ReleaseFrame 无对应帧 → INVALID_CALL → 后续全部失效

v3 原代码之所以能用，是因为循环前预取了一帧。

## 结论

当前项目使用 **WGC 作为默认方案**。DXGI 代码保留在 `capture_dxgi.cpp` 中，可通过 `blackhole.exe always dxgi` 切换。

WGC 的闪烁问题在移除 fence/flush/PBO 后已大幅改善，残余的偶发闪烁是大画面变动时 DWM 合成延迟所致，不影响日常使用。