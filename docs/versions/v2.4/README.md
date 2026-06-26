# InvestInsight v2.4 文档

最后更新：2026-06-26

v2.4 聚焦多周期技术点位与推荐质量：把“今日涨幅好所以推荐”的单点判断，升级为结合 K 线结构、支撑阻力、风险收益比、事件状态和数月维度趋势的推荐计划。

本版本已完成第一轮最小闭环：新增技术点位分析器、推荐生命周期接入和 UI 展示。后续仍需继续补强历史回测、更多形态识别和点位图表叠加。

| 目录 | 内容 |
| --- | --- |
| `design/` | v2.4 板块详情聚焦布局设计稿，用于后续重排决策摘要、趋势图、事件预测和证据层。 |
| `plans/` | v2.4 技术点位、买入观察区、止盈减仓区、多周期推荐模型和验收标准。 |
| `screenshots/technical-entry-exit/` | 第一轮技术点位 UI 验证截图，覆盖总览、事件雷达、板块机会、策略跟踪和板块详情。 |

当前核心计划：

- `plans/technical-entry-exit-and-multi-horizon-recommendation-plan.md`：K 线点位与多周期推荐模型开发方案，包含资料来源记录、分析框架、实施阶段和验证口径。
- `plans/technical-entry-exit-implementation-plan.md`：第一轮技术点位最小闭环实施方案，包含文件边界、TDD 任务和验证命令。
- `plans/sector-detail-focused-layout-implementation-plan.md`：板块详情聚焦布局实施方案，包含页面替换、滚动修复、截图验收和 Qt 替代路线。
- `design/sector-detail-focused-layout.md`：板块详情信息聚焦设计说明。
- `design/sector-detail-focused-layout.svg`：新版板块详情布局矢量图片稿。
- `design/sector-detail-focused-layout.png`：新版板块详情布局预览图。
