# UI 设计稿落地实现计划

> **面向 AI 代理的工作者：** 必需子技能：使用 superpowers:executing-plans 逐任务实现此计划。步骤使用复选框（`- [ ]`）语法来跟踪进度。

**目标：** 按 `docs/versions/v2.0/design/ui-workbench-redesign.md` 将主界面改为投研工作台风格。

**架构：** 保留 Qt Widgets + HTML renderer 架构，不重写业务逻辑。`MainWindow` 负责左侧导航、顶部状态和页面容器；各结果页视觉继续在 renderer 中完成，避免把大段 HTML 塞回主窗口。

**技术栈：** C++17、Qt Widgets、QTextBrowser HTML、现有 `AppTheme` 和 renderer smoke 测试。

---

## 文件职责

- 修改 `src/ui/AppTheme.h/.cpp`：补充工作台侧栏、顶部栏和 HTML 卡片的主题色与样式。
- 修改 `src/ui/MainWindow.h/.cpp`：把主界面从顶部 Tab 改为左侧导航 + 顶部状态条 + 内容区，保留可关闭详情 Tab。
- 修改 `src/ui/renderers/DashboardRenderer.cpp`：让总览页按设计稿显示市场状态、关键事件、机会风险和下一观察点。
- 修改 `src/ui/renderers/EventRadarRenderer.cpp`、`SectorTableRenderer.cpp`、`StrategyRenderer.cpp`、`SectorDetailRenderer.cpp`：统一表格、卡片和风险展示风格。
- 修改 `tests/ui/*.cpp`：先用 smoke 测试锁定左侧导航、工作台 CSS、总览核心区块和详情事件区块。
- 同步 `docs/codex/PROJECT_CONTEXT.md` 与 `docs/product/InvestInsight-product-overview.md`。

## 任务 1：主框架与主题

- [x] 编写失败测试：`tests/ui/AppThemeSmoke.cpp` 检查 `sideNav`、`workspace-shell`、`metric-grid` 等样式存在；主窗口 smoke 检查左侧导航文案。
- [x] 运行 `powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\verify_ui_smoke.ps1`，预期失败在新增样式/左侧导航断言。
- [x] 修改 `AppTheme` 和 `MainWindow`，实现左侧导航、顶部状态条、内容区和配置页视觉同步。
- [x] 运行 UI smoke，预期通过。

## 任务 2：总览和事件工作台

- [x] 编写失败测试：`DashboardRendererSmoke` 检查市场状态、关键事件雷达、板块机会与风险、下一观察点；`EventRadarRendererSmoke` 检查结构化事件路径卡片。
- [x] 运行 UI smoke，预期失败在新增区块断言。
- [x] 修改 `DashboardRenderer` 和 `EventRadarRenderer`，使用设计稿的紧凑卡片、风险同级展示和事件路径布局。
- [x] 运行 UI smoke，预期通过。

## 任务 3：板块机会、策略和详情

- [x] 编写失败测试：`SectorTableRendererSmoke` 检查事件催化列和风险提示；`StrategyRendererSmoke` 检查跟踪状态卡片；`SectorDetailRendererSmoke` 检查首屏指标网格和事件驱动区。
- [x] 运行 UI smoke，预期失败在新增视觉结构断言。
- [x] 修改对应 renderer，保留已有量化数据，不删除图表、回测、资金流、新闻和数据质量。
- [x] 运行 UI smoke，预期通过。2026-06-21 已使用沙箱外 MSBuild 跑通 `tools/verify_ui_smoke.ps1`。

## 任务 4：文档和最终验证

- [x] 同步项目上下文和产品说明的 UI 状态。
- [x] 运行 `cmake --build build --config Release -- /m`。
- [x] 运行 `powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\verify_ui_smoke.ps1`。
- [ ] 检查 `git diff --check`。
