# README 首页重构实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将仓库首页改造成面向普通用户的 `v1.2.1` 下载、使用、兼容性和排障入口，并基于代码与官方平台要求修正设备门槛。

**Architecture:** 只重写根目录 `README.md`，不修改程序行为。首页按“认识软件、下载、使用、兼容、排障、开发”顺序组织；技术细节链接到 `Doc/TECHNICAL.md`，历史技术文档不作为当前兼容性事实来源。

**Tech Stack:** GitHub Markdown、Qt 6.8+、Windows 10/11、OpenGL 3.3、D3D11 WGC/DXGI。

## Global Constraints

- 文档使用中文和 UTF-8 编码。
- 当前发布版本固定为 `v1.2.1`。
- 最低系统写为 64 位 Windows 10 1809（build 17763），推荐 Windows 11 22H2+。
- 显卡及驱动必须同时支持 OpenGL 3.3 和 D3D11 硬件设备。
- 不承诺覆盖所有播放器、游戏、显卡、双显卡组合、远程桌面或虚拟机。
- 不提交 ZIP、`QR_payment.jpg` 或 `WeChat_QR.png`。

---

### Task 1: 重写 GitHub 首页

**Files:**
- Modify: `README.md`

**Interfaces:**
- Consumes: `demo.gif`、`Doc/TECHNICAL.md`、`LICENSE`、GitHub Releases URL、`v1.2.1` 已发布功能。
- Produces: GitHub 仓库首页的下载、使用、兼容性、排障和构建入口。

- [ ] **Step 1: 建立用户优先的页面骨架**

将 README 一级标题设为 `Black Hole / 黑洞桌面特效`，保留 `demo.gif`，然后按以下二级标题排序：

```markdown
## 下载与安装
## 主要功能
## 快速开始
## 设备要求与兼容性
## 视频、游戏与名单
## 常见问题
## 更新机制
## 开发者构建
## 项目结构
## 灵感来源
## License
```

- [ ] **Step 2: 写入经审核的设备要求**

设备表必须使用以下事实，不保留旧显卡系列兼容表：

```markdown
| 项目 | 最低要求 / 说明 |
|------|-----------------|
| 操作系统 | 64 位 Windows 10 1809（build 17763）或更高版本 |
| 推荐系统 | Windows 11 22H2（build 22621）或更高版本 |
| 处理器架构 | x86-64；不提供 32 位或 Windows on ARM 原生版本 |
| 图形能力 | OpenGL 3.3 + 可创建硬件 D3D11 设备 |
| 显卡驱动 | 使用显卡厂商提供的较新正式驱动 |
```

同时解释 OpenGL 用于预览和渲染、D3D11 用于桌面捕获；Windows 11 22H2+ 自动优先无黄边 WGC，其他受支持系统自动回退 DXGI。

- [ ] **Step 3: 写入检测边界和排障说明**

明确说明哔哩哔哩等播放器、三种游戏窗口形态、无声视频和低负载游戏的漏检边界，以及运行程序选择、可执行文件选择和手动输入三个名单入口。常见问题必须覆盖完整解压、`DXGI_Init primary failed`、杀毒误报和日志反馈；不得要求用户关闭杀毒软件。

- [ ] **Step 4: 校验 README 内容与链接**

运行：

```powershell
rg -n "v1\.2\.1|Windows 10 1809|OpenGL 3\.3|D3D11|DXGI_Init primary failed|blackhole_debug\.txt|Doc/TECHNICAL\.md" README.md
rg -n "新功能还在开发|旧 UI 仍可使用|作者最近比较忙|完全支持" README.md
Test-Path demo.gif
Test-Path Doc\TECHNICAL.md
Test-Path LICENSE
git diff --check
```

预期：第一条覆盖所有关键事实；第二条无匹配；三个路径均为 `True`；`git diff --check` 退出码为 0。

- [ ] **Step 5: 提交 README**

```powershell
git add -- README.md docs/superpowers/specs/2026-07-14-readme-refresh-design.md docs/superpowers/plans/2026-07-14-readme-refresh.md
git diff --cached --name-only
git commit -m "更新v1.2.1项目首页与使用说明"
```

预期暂存内容只有 README、设计文档修订和实施计划；ZIP 与收款码不在提交中。

### Task 2: 推送并核对远程提交

**Files:**
- Modify: none

**Interfaces:**
- Consumes: 已验证的本地 `main` 提交。
- Produces: 与本地相同 SHA 的 `origin/main`。

- [ ] **Step 1: 推送 main**

```powershell
git push origin main
```

预期：推送成功，无拒绝或冲突。

- [ ] **Step 2: 核对本地和远程 SHA**

```powershell
$local = git rev-parse HEAD
$remote = git ls-remote origin refs/heads/main | ForEach-Object { ($_ -split "`t")[0] }
if ($local -ne $remote) { throw "origin/main SHA mismatch" }
```

预期：命令退出码为 0，本地与远程 SHA 完全一致。
