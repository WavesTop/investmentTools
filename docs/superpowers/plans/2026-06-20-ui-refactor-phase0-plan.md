# UI 重构 Phase 0 执行计划

最后更新：2026-06-20

## 目标

在不改变 1.0 现有分析能力和界面输出的前提下，把 `src/ui/MainWindow.cpp` 中的 UI 基础设施逐步拆出，为后续事件雷达、板块详情重排和长期逻辑展示预留可维护结构。

## 提交边界

- 本轮 commit 只提交到本地仓库，不 push 远端。
- 每个代码 commit 目标 200 到 300 行，原则上不超过 500 行。
- 文档、设计稿、截图和二进制图片可以单独提交，并在交付说明中标明其来源。
- 每个 commit 前必须运行匹配的验证命令，验证失败不提交。

## 当前设计输入

- 产品说明：`docs/product/InvestInsight-product-overview.md`
- 代码地图：`docs/codex/PROJECT_CONTEXT.md`
- 事件引擎规格：`docs/superpowers/specs/2026-06-20-event-impact-engine-design.md`
- UI 设计说明：`docs/design/InvestInsight-ui-redesign-mockup.md`
- 当前板块详情长截图参考：`docs/design/assets/current-sector-detail-long-user-reference.png`
- 优化后详情长图：`docs/design/assets/investinsight-ui-redesign-sector-detail-long.png`

## 切片 0.1：主题与通用样式

范围：

- 新增 `src/ui/AppTheme.h`
- 新增 `src/ui/AppTheme.cpp`
- 修改 `src/ui/MainWindow.cpp`
- 修改 `CMakeLists.txt`

实现：

- 0.1a：移出 `ThemeColors`、`lightTheme()`、`darkTheme()`、`detectDarkMode()`。
- 0.1b：移出 `buildWidgetStyleSheet()`。
- 0.1c：移出 `buildHtmlCss()`。
- `MainWindow` 只保留当前主题状态和调用，不再承载大段主题/CSS 字符串。

当前进度：0.1a、0.1b 和 0.1c 已完成；0.2a 已新增独立 `ChartRenderer` 和图表 smoke 测试；0.2b 已让 `MainWindow::buildTrendChart` 委托该 renderer，下一片分批删除旧静态绘图 helper。

验证：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\verify_ui_smoke.ps1
```

预期提交信息：

```text
refactor(ui): 拆分主题和基础样式
```

## 切片 0.2：图表绘制拆分

范围：

- 新增 `src/ui/renderers/ChartRenderer.h`
- 新增 `src/ui/renderers/ChartRenderer.cpp`
- 修改 `src/ui/MainWindow.cpp`
- 修改 `CMakeLists.txt`

实现：

- 移出趋势图、K 线、成交量、MACD、资金流等绘图辅助函数。
- 保持板块详情中的图表内容和数量不减少。

验证：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\verify_ui_smoke.ps1
```

预期提交信息：

```text
refactor(ui): 拆分板块图表渲染
```

## 切片 0.3：HTML 页面 renderer 拆分

范围：

- 新增 `src/ui/renderers/DashboardRenderer.h/.cpp`
- 新增 `src/ui/renderers/SectorTableRenderer.h/.cpp`
- 新增 `src/ui/renderers/StrategyRenderer.h/.cpp`
- 修改 `src/ui/MainWindow.cpp`
- 修改 `CMakeLists.txt`

实现：

- 总览页、板块表、策略页从 `MainWindow` 迁移到 renderer。
- 每个 renderer 只接收必要的 `AnalysisResult` 或页面数据。

验证：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\verify_ui_smoke.ps1
```

## 切片 0.4：详情页 renderer 拆分

范围：

- 新增 `src/ui/renderers/SectorDetailRenderer.h/.cpp`
- 新增 `src/ui/renderers/IndexDetailRenderer.h/.cpp`
- 修改 `src/ui/MainWindow.cpp`
- 修改 `CMakeLists.txt`

实现：

- 板块详情页必须保留收益、评分、技术指标、资金流、回测、新闻证据和数据质量。
- 新增事件驱动区块时，不删除原有量化信息。

验证：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\verify_ui_smoke.ps1
```

## 切片 0.5：页面骨架和视觉对齐

范围：

- 在现有主页面基础上增加事件雷达入口或 Tab 骨架。
- 对齐 `docs/design` 中的总览、事件雷达、板块机会、策略跟踪、AI 助手、配置页和板块详情长图。

验证：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\verify_ui_smoke.ps1
```

验收时需要人工或截图检查 1366x768、1920x1080 下文字不重叠，主信息可扫描。
