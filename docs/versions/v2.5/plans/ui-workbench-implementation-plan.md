# v2.5 工作台界面落地实施计划

最后更新：2026-06-26

## 目标

根据 `docs/versions/v2.5/DESIGN.md` 与 `docs/versions/v2.5/design/` 中的页面稿，将当前 Qt Widgets 界面调整为更接近投研工作台的 `Insight Workbench` 风格。重点不是减少信息，而是重新排序信息层级，让用户先看到决策、风险、下一观察点，再查看证据。

## 实施阶段

1. 主题与基础结构
   - 将 `AppTheme` token 调整到 v2.5 色彩和密度。
   - 补充 `workspace-status-band`、`insight-card`、`scan-table`、`sector-detail-long` 等稳定样式与页面标记。
   - 更新 UI smoke，确保页面输出能被自动化验证。

2. 总览与事件雷达
   - 总览页增加工作台状态带，收敛首屏信息，保留市场仪表、关键事件雷达、Top 板块机会和 AI/规则摘要。
   - 事件雷达页强化事件状态总览、结构化时间线、关键事件队列、传导路径和风险失效条件。

3. 板块机会与策略跟踪
   - 板块机会页使用更高密度的机会表格，优先展示今日、5 日、事件、MACD、RSI、KDJ、资金、点位计划和下一观察。
   - 策略跟踪页沿用信号生命周期定位，统一表格和状态卡样式。

4. 板块详情长页
   - 首屏采用左侧趋势图、右侧购买建议的结构。
   - 首屏下方依次保留相关事件分析、预测矩阵、证据层、新闻证据和数据质量。
   - 长截图需要覆盖首屏、事件/预测区、证据层和新闻底部，避免只按单屏实现导致信息缺失。

5. AI 助手与配置页
   - AI 助手右侧首屏加入可问问题与上下文摘要，减少空白欢迎页。
   - 配置页保留工作台内嵌形式，AI 接入、持仓、后台刷新、数据源健康同屏展示。
   - 扩展自动截图入口，覆盖 AI 助手和配置页。

## 验证方式

- 每个页面实现后运行 UI smoke 或对应 renderer smoke，确认结构和关键字段存在。
- 页面全部完成后运行：
  - `cmake --build build --config Release -- /m`
  - `powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\verify_ui_smoke.ps1`
  - `build\Release\InvestInsight.exe --capture-ui-screenshots docs\versions\v2.5\screenshots\ui-implementation`
- 截图核对范围：
  - 总览：对齐 `overview-dashboard.svg`
  - 事件雷达：对齐 `event-radar.svg`
  - 板块机会：对齐 `sector-opportunities.svg`
  - 板块详情：对齐 `sector-detail.svg` 的长页结构
  - AI 助手与配置：对齐 `assistant-config.svg`

