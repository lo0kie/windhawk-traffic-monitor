# 上架 Windhawk 官方 Catalog 清单

目标仓库：[ramensoftware/windhawk-mods](https://github.com/ramensoftware/windhawk-mods)
本 mod id：`windhawk-traffic-monitor`

> 提交前逐项勾选。参考官方 Wiki：
> [Creating a new mod](https://github.com/ramensoftware/windhawk-mods/wiki/Creating-a-new-mod) ·
> [Submission checklist](https://github.com/ramensoftware/windhawk-mods/wiki/Submission-checklist)

## 三个仓库的关系（别 fork 错）

| 仓库 | 角色 | 你的操作 |
|---|---|---|
| `zhongyang219/TrafficMonitor` | 被参考的原项目（GPL-3.0） | **不碰**，只在 README / 许可里声明引用 |
| `lo0kie/windhawk-traffic-monitor` | 你自己 mod 源码仓库（当前项目） | 把源码推到这里 |
| `ramensoftware/windhawk-mods` | **官方 mod 目录仓库** | **fork 这个**，加文件后开 PR |

**流程**：mod 源码留在你自己的仓库 → fork `ramensoftware/windhawk-mods` → 在你的 fork 里加 `mods/windhawk-traffic-monitor.wh.cpp`（内容 = `dist/windhawk-traffic-monitor.wh.cpp`）→ 向官方仓库开 PR。

> ⚠️ **不要 fork TrafficMonitor**。它只是被参考的项目，上架走的是官方目录仓库 `windhawk-mods`。

---

## 一、代码合规（已完成 ✅）

- [x] 工具 mod 化：`@include windhawk.exe`，独立进程运行，不注入 explorer
- [x] 工具 mod 入口：`WhTool_ModInit` / `WhTool_ModUninit` / `WhTool_ModSettingsChanged` + 官方 launcher 片段
- [x] 删除 `@architecture`（工具 mod 不设）
- [x] `@license GPL-3.0`（因移植了 GPL-3.0 的 TrafficMonitor）
- [x] `@github` / `@author` = `lo0kie`
- [x] `@version` = `1.0.1`（与 git tag `v1.0.1` 一致，CI 会校验）
- [x] GPL-3.0 全文：`LICENSE` 文件 + 内嵌进 `dist/windhawk-traffic-monitor.wh.cpp` 注释
- [x] 去掉了压缩/混淆（上架要求源码可读、可审查）
- [x] 去掉了 `#ifdef TM_PREVIEW` 调试代码（`build.py pack` 的 `drop_preview`，产物 0 残留）
- [x] 无 hook、无持久化更改、无网络访问、无多余 enabled/logging 设置
- [x] 鼠标事件：无点击动作时窗口穿透（`WS_EX_TRANSPARENT`），不干扰任务栏右键菜单
- [x] 多语言：元数据/设置英文默认 + `:zh-CN` 中文，tooltip 运行时中英切换

## 二、提交前必须做的测试（人工，待办 ⚠️）

- [ ] **进程启动时加载**：启用后重启 explorer.exe / 重启系统，确认开机自启生效（官方明确要求，仅运行中启用才生效 = 不合格）
- [ ] **稳定版 Windhawk 1.7.3** 测试（不能用 2.0 alpha）
- [ ] 任务栏自动隐藏 / 显示时窗口正确跟随
- [ ] 多显示器、125% / 150% DPI 缩放
- [ ] explorer.exe 重启后窗口恢复（工具 mod 独立进程，理论上不受影响，但需验证定位）
- [ ] 禁用 → 启用 mod 干净卸载、无残留、无崩溃
- [ ] 运行中修改每个设置，立即生效

## 三、元数据与内容（待办 ⚠️）

- [ ] 搜索 `https://windhawk.net/mods` 确认无同名 / 同功能 mod（撞名看 "Overlap" 规则）
- [ ] `@name`「Windhawk Traffic Monitor」确认范围匹配、描述是用户能看懂的功能
- [ ] 准备**截图 / GIF**，托管到 `i.imgur.com` 或 `raw.githubusercontent.com`（视觉类 mod 必需）
- [ ] 确认 `@description` / README 里无开发历史、无实现细节、无完整许可证正文（全文应在代码注释里）

## 四、提交流程（GitHub 操作，待办 ⚠️）

- [ ] 把本地 `src/` 源码推到 `lo0kie/windhawk-traffic-monitor`
- [ ] fork `ramensoftware/windhawk-mods`
- [ ] 开 PR，**只加一个文件** `mods/windhawk-traffic-monitor.wh.cpp`（内容 = `dist/windhawk-traffic-monitor.wh.cpp` 可读版）
- [ ] 填 PR 模板 "Mod authorship"，**如实声明使用了 AI 辅助**
- [ ] 评论 `/ai-review` → 修复发现的问题（每条未处理发现需加简短解释）
- [ ] 评论 `/ready-for-reviewer` → 等人工审查

## 五、注意点

- **mod id 一旦提交不可改**，提交前确认 `windhawk-traffic-monitor` 这个 id 合适
- **每个 mod 单一作者**：`@github` 必须 = 开 PR 的账号，后续更新也只能由该账号提交
- **上架用可读版**，不是压缩版；代码要过 `.clang-format`（不能超长行 / 多语句一行）
- **许可合规**：GPL-3.0 衍生作品，已附全文 + 版权声明 + 修改说明，再分发合规

## 六、法律合规自查（再分发 TrafficMonitor）

- [x] 以 GPL-3.0 发布（与原项目一致）
- [x] 附 GPL-3.0 全文（LICENSE 文件 + 代码注释内嵌）
- [x] 保留原作者版权声明（zhongyang219）
- [x] 注明修改内容（README「与 TrafficMonitor 的差异」）
- [x] 无附加限制、源码随附（Windhawk mod 本身即源码分发）
