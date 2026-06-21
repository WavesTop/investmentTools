# 事件传导引擎 Phase 1-5 执行计划

最后更新：2026-06-21

## 目标

在 Phase 0 已完成 UI renderer 拆分和事件雷达入口后，继续实现规格文档中的 Phase 1-5：让系统能从新闻中抽取结构化宏观事件，解释事件到板块的影响路径，把事件催化分接入现有评分，并在 UI 与诊断命令中展示可验证结果。

本轮实现遵循小切片提交：每个 commit 原则上控制在 500 行以内，优先 200-300 行；每个切片验证通过后只提交到本地仓库，不推送远端。

## 切片边界

### 1.1 事件领域模型与抽取测试

目标：先用测试固定“美联储降息预期”这类文本应输出的结构化事件。

主要文件：

- `src/domain/MacroEvent.h`
- `src/core/EventRuleBook.h/.cpp`
- `src/core/EventExtractionEngine.h/.cpp`
- `tests/core/EventImpactSmoke.cpp`
- `CMakeLists.txt`

验收：

- 纯函数 smoke 测试覆盖 `Expected`、`Scheduled`、`Confirmed`、`Revised`。
- 证据来源、标题和发布时间能保留。
- 运行 `cmake --build build --config Release -- /m`。
- 运行 `.\build\Release\InvestInsightEventSmoke.exe`。

### 2.1 影响路径规则库

目标：事件能映射到板块影响，并说明直接/间接路径。

主要文件：

- `src/core/ImpactGraphEngine.h/.cpp`
- `src/core/SectorImpactAnalyzer.h/.cpp`
- `tests/core/EventImpactSmoke.cpp`
- `CMakeLists.txt`

验收：

- 美联储降息预期映射到黄金、有色、半导体、创新药、证券。
- 每条影响包含方向、强度、置信度、关系类型和解释。
- 运行 `cmake --build build --config Release -- /m`。
- 运行 `.\build\Release\InvestInsightEventSmoke.exe`。

### 3.1 接入分析流水线

目标：把事件影响注入 `AnalysisResult` 和 `SectorSnapshot`，并让 `forecastScore` 只轻微吸收事件催化。

主要文件：

- `src/domain/AnalysisResult.h`
- `src/core/InsightOrchestrator.cpp`
- `docs/codex/PROJECT_CONTEXT.md`
- `docs/product/InvestInsight-product-overview.md`

验收：

- `SectorSnapshot` 拥有 `eventImpacts`、`eventCatalystScore`、`eventSummary`。
- 未确认事件只作为观察催化，单次对 `forecastScore` 的影响被限制在小范围内。
- 运行 `cmake --build build --config Release -- /m`。
- 运行 `.\build\Release\InvestInsightEventSmoke.exe`。

### 4.1 事件 UI 展示

目标：事件雷达和板块详情页能展示“事件 -> 路径 -> 板块”的结构化解释。

主要文件：

- `src/ui/renderers/EventRadarRenderer.cpp`
- `src/ui/renderers/SectorDetailRenderer.cpp`
- `tests/ui/EventRadarRendererSmoke.cpp`
- `tests/ui/SectorDetailRendererSmoke.cpp`
- `docs/codex/PROJECT_CONTEXT.md`
- `docs/product/InvestInsight-product-overview.md`

验收：

- 事件雷达优先展示结构化事件影响，旧的未来事件文案作为兜底。
- 板块详情页新增事件驱动区，不删除收益、评分、技术指标、资金流、回测、新闻和数据质量。
- 运行 `powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\verify_ui_smoke.ps1`。
- 运行 `.\build\Release\InvestInsightEventSmoke.exe`。

### 5.1 事件追踪和去重

目标：同一事件不会反复刷屏，并记录首次发现、最近出现和状态变化。

主要文件：

- `src/core/EventRepository.h/.cpp`
- `src/core/InsightOrchestrator.cpp`
- `tests/core/EventImpactSmoke.cpp`
- `docs/codex/PROJECT_CONTEXT.md`
- `docs/product/InvestInsight-product-overview.md`

验收：

- 仓库可以按事件 key 合并重复事件。
- 能记录 `firstSeenAt`、`lastSeenAt`、`seenCount` 和状态历史。
- 运行 `cmake --build build --config Release -- /m`。
- 运行 `.\build\Release\InvestInsightEventSmoke.exe`。

### 5.2 诊断命令

目标：新增 CLI 诊断入口，便于在不打开 GUI 的情况下验证事件引擎。

主要文件：

- `src/main.cpp`
- `docs/codex/PROJECT_CONTEXT.md`
- `docs/product/InvestInsight-product-overview.md`

验收：

- `.\build\Release\InvestInsight.exe --debug-event-impact "美联储降息预期升温，市场关注下次 FOMC 会议"` 输出 `type=MonetaryPolicy`、`state=Expected`、`region=US`、`checkpoint=FOMC`，并列出黄金、半导体等影响板块。
- 运行 `cmake --build build --config Release -- /m`。
- 运行上述诊断命令。

## 实现约束

- 不修改板块“今日涨幅”口径；本轮不需要运行 `--dump-sector-changes`，除非实际触碰行情数据逻辑。
- 新增规则先保持保守：事件催化分用于解释和观察，不直接强推买入。
- 规则引擎必须可测试、可回放；AI 后续只能作为补充，不作为唯一来源。
- 新增 UI 信息只能进入 renderer，不在 `MainWindow.cpp` 追加大段 HTML。
- 每个阶段完成后同步更新 `docs/codex/PROJECT_CONTEXT.md` 和 `docs/product/InvestInsight-product-overview.md` 中的流程、文件地图和验证命令。

## 进度记录

- [ ] 1.1 事件领域模型与抽取测试
- [ ] 2.1 影响路径规则库
- [ ] 3.1 接入分析流水线
- [ ] 4.1 事件 UI 展示
- [ ] 5.1 事件追踪和去重
- [ ] 5.2 诊断命令
