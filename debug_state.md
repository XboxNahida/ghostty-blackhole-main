# Debug State — Blackhole v9: 最终稳定版（统一渲染管线）

## 当前状态
### 编译 ✓ - 零警告零错误 (2026-06-27)

## 核心修复：统一渲染管线

### 问题根源
之前的 idle/active 使用**两套不同的渲染路径**，导致 OpenGL 状态在切换时错位：

| 状态 | 旧路径 | 问题 |
|------|--------|------|
| active | glClear + swap + continue | 跳过 capture/shader |
| idle | capture + shader + swap | 完整路径 |

### 最终架构：单一路径，只改参数

`
          ┌─ active: opacity=0, Sleep(100) ─┐
          │                                  │
glfwPollEvents → capture → shader → swap ───┤
          │                                  │
          └─ idle: opacity=1, 全速 ──────────┘
`

- **渲染管线**：主动/空闲完全一致（capture → shader → draw → swap）
- **区别仅在于**：opacity（0 vs 1）和 Sleep（100ms vs 0）
- **不 hide、不 continue、不分裂管线**

## 修改文件
- src/main.cpp — 空闲模式重构为统一管线

## 修改状态
| 步骤 | 状态 |
|------|------|
| 黄边框修复 | ✅ 已完成 |
| 黑屏修复（统一管线） | ✅ 已完成 |
| 黑洞参数滑块化 | ✅ 已完成 |
| 编译验证 | ✅ 0警告0错误 |
