# InvestInsight v2.5 文档

最后更新：2026-06-28

v2.5 聚焦新一轮投研工作台视觉系统：用 `DESIGN.md` 固化界面语言，把当前“信息很全但层级偏弱”的页面，重排为更适合投研、事件监控和板块决策的高密度工作台。

当前版本已经进入代码落地阶段，主界面、总览、事件雷达、板块机会、板块详情、AI 助手和配置页已按 v2.5 设计稿完成第一轮实现，并通过 UI smoke 与截图核对。

## 目录

| 目录 | 内容 |
| --- | --- |
| `DESIGN.md` | v2.5 设计系统，记录设计来源、颜色、排版、组件、页面结构和 Qt 落地约束。 |
| `design/` | v2.5 新界面设计稿，覆盖总览、事件雷达、板块机会、板块详情、AI 助手与配置。 |
| `plans/` | v2.5 UI 工作台落地实施计划和阶段验证要求。 |
| `screenshots/ui-implementation/` | 当前代码实现后的自动化截图，用于和设计稿逐页核对。 |

## 核心文档

- `DESIGN.md`：InvestInsight v2.5 的 DESIGN.md 设计系统。
- `design/workbench-redesign.md`：新界面结构说明和页面级改造重点。
- `design/overview-dashboard.svg`：总览页设计稿。
- `design/event-radar.svg`：事件雷达设计稿。
- `design/sector-opportunities.svg`：板块机会设计稿。
- `design/sector-detail.svg`：板块详情长页设计稿。
- `design/assistant-config.svg`：AI 助手和配置页设计稿。
- `plans/ui-workbench-implementation-plan.md`：代码落地实施计划和验证流程。

## 验收截图

`screenshots/ui-implementation/` 中保留本轮实现后的页面截图：

- `01-overview.png`
- `02-event-radar.png`
- `03-sector-opportunities.png`
- `04-strategy-tracking.png`
- `05-sector-detail.png`
- `06-sector-detail-evidence.png`
- `07-ai-assistant.png`
- `08-config.png`
