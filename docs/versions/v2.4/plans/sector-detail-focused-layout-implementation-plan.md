# 板块详情聚焦布局实施方案

最后更新：2026-06-26

## 目标

把当前“信息堆叠式”的板块详情页替换为“先决策、再证据”的投研页面。首屏必须让用户快速看懂：当前是否适合行动、买入观察区在哪里、止损和止盈点位是什么、该判断由哪些事件和趋势支持。长页下半部分继续保留新闻、资金流、技术指标、回测和数据质量，避免为了简洁丢失关键证据。

## 本次范围

1. 更新 `SectorDetailRenderer`，把详情页重排为以下结构：
   - 决策摘要：动作、今日涨幅、综合评分、置信度、数据质量。
   - 购买建议：观察买入区、止损失效位、止盈减仓区、风险收益比和执行建议。
   - 趋势图与点位：保留当前趋势图，放到首屏核心区域。
   - 相关事件分析：展示事件标题、方向、路径、周期、可信度和失效条件。
   - 预测矩阵：短期、中期、长期观点，以及 AI/规则分歧提示。
   - 证据层：新闻证据、资金流、相关板块线索、技术指标、阶段收益/回测、数据质量。
2. 修复详情页承载控件：
   - 动态打开的板块详情页改用 `ClickableBrowser`，统一链接打开、页内滚动和焦点策略。
   - 指数详情页同步使用同一控件，避免相同滚动问题在指数页复现。
3. 更新截图验证入口：
   - `--capture-ui-screenshots` 保留原有 5 张截图。
   - 新增板块详情滚动后截图，用于验证长页证据层可达，避免再次出现“只能看到首屏、无法确认滚动”的问题。
4. 同步项目文档：
   - 更新 v2.4 README、产品说明和 Codex 项目上下文。
   - 记录新的截图路径和验证命令。

## 技术选择

本次继续使用 Qt Widgets + `QTextBrowser` 渲染详情页，但布局实现尽量采用 Qt 富文本支持更稳定的 table、inline style 和基础 class，而不是依赖复杂 CSS grid/flex。这样可以在不引入新模块的前提下尽快替换页面，并保持现有打包体积和跨平台构建方式。

如果后续需要更复杂的交互，可考虑三种替代方式：

| 方案 | 优点 | 代价 |
| --- | --- | --- |
| Qt Widgets + `QTextBrowser` | 当前依赖最少、跨平台稳定、打包脚本无需大改 | CSS 能力有限，复杂响应式布局和折叠交互较弱 |
| Qt WebEngine + 本地 HTML | 现代 CSS/JS 能力完整，适合复杂卡片、折叠、锚点和图表交互 | 增加 QtWebEngine 依赖，打包体积、部署和 macOS 签名复杂度上升 |
| 原生 Qt Widget 组件化 | 滚动、焦点、响应式尺寸最可靠，可细粒度控制交互 | C++ UI 代码量较大，迭代设计稿速度较慢 |

本阶段推荐先完成 `QTextBrowser` 版本；如果后续继续做可折叠证据层、可点击图表点位或图表联动，再评估 Qt WebEngine。

## 测试先行

先更新 `tests/ui/SectorDetailRendererSmoke.cpp`，让测试断言新版页面关键标识和核心信息：

- `sector-detail-focused` 页面标识。
- “决策摘要”“购买建议”“趋势图与点位”“相关事件分析”“预测矩阵”“证据层”。
- 买入区、止损、止盈、风险收益比。
- 事件路径、失效条件、新闻链接和趋势图 base64。
- 技术指标、资金流、阶段收益/回测、数据质量仍然存在。

测试预期先失败，再改 renderer 通过。

## 实施步骤

1. 文档切片：提交本实施方案。
2. 测试切片：更新板块详情 smoke，确认旧页面不满足新版结构。
3. 页面切片：重排 `SectorDetailRenderer`，用 table/卡片式 HTML 实现聚焦布局。
4. 滚动切片：更新 `ClickableBrowser` 焦点/滚动策略，板块详情和指数详情改用它，并给截图入口新增滚动后截图。
5. 文档同步：更新项目上下文、产品说明和 v2.4 README。
6. 验证与提交：运行 Release 构建、UI smoke、自动化截图，并查看详情页首屏和证据层截图。

## 验收命令

```powershell
cmake --build build --config Release -- /m
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\verify_ui_smoke.ps1
.\build\Release\InvestInsight.exe --capture-ui-screenshots docs\versions\v2.4\screenshots\sector-detail-focused-layout-implementation
```

截图验收重点：

- `05-sector-detail.png`：首屏能看到决策摘要、趋势图与购买建议。
- `06-sector-detail-evidence.png`：滚动后能看到证据层，说明长页内容可达。

## 风险与约束

- `QTextBrowser` 对 CSS 支持有限，视觉实现应以可读、稳定、不卡滚动为优先。
- 本次不改变评分模型，只改变详情页的信息组织方式。
- 不删除已有量化信息；如果首屏放不下，必须迁移到证据层，而不是直接移除。
- 不引入 Qt WebEngine，除非后续明确进入交互式投研报告形态。
