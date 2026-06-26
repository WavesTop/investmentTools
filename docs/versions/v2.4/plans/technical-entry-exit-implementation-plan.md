# 技术点位与多周期推荐实施方案

最后更新：2026-06-26

> 面向 Codex 和后续开发者：本方案是 `technical-entry-exit-and-multi-horizon-recommendation-plan.md` 的落地拆解。实现时必须遵守 TDD，先补 smoke/单元测试，再写生产代码；每个提交前运行对应验证命令，只提交到本地仓库。

## 目标

本次 v2.4 第一轮实施先完成一个可工作的最小闭环：

1. 给每个板块生成技术点位计划，包括趋势状态、观察买入区、止损失效位、止盈/减仓区、风险收益比和持有周期。
2. 推荐生命周期使用点位计划校正入场时机，避免单日大涨直接增配。
3. 板块机会页和板块详情页展示点位计划，让用户知道“现在适不适合看、跌到哪里观察、涨到哪里减仓”。

## 文件边界

| 文件 | 操作 | 职责 |
| --- | --- | --- |
| `src/domain/AnalysisResult.h` | 修改 | 新增 `PriceLevelPlan` 结构，并挂到 `SectorSnapshot`。 |
| `src/core/PriceLevelAnalyzer.h` | 新增 | 声明 K 线点位分析器和趋势/动作枚举。 |
| `src/core/PriceLevelAnalyzer.cpp` | 新增 | 基于 K 线、技术指标和事件/评分生成点位计划。 |
| `tests/core/PriceLevelAnalyzerSmoke.cpp` | 新增 | 覆盖上涨回调、过热不追、下跌破位和数据不足。 |
| `CMakeLists.txt` | 修改 | 将分析器接入主程序、UI smoke、推荐 smoke 和新增 core smoke。 |
| `src/core/InsightOrchestrator.cpp` | 修改 | 在技术指标和策略生成后写入 `priceLevelPlan`，并将关键提示加入正负因素。 |
| `src/core/RecommendationTracker.cpp` | 修改 | 使用风险收益比和点位动作压缩或增强入场时机。 |
| `src/ui/renderers/SectorTableRenderer.cpp` | 修改 | 表格增加“点位计划/周期”摘要，减少只看今日涨幅。 |
| `src/ui/renderers/SectorDetailRenderer.cpp` | 修改 | 详情页新增“技术点位计划”区块。 |
| `tests/ui/SectorTableRendererSmoke.cpp` | 修改 | 验证表格展示观察区和风险收益比。 |
| `tests/ui/SectorDetailRendererSmoke.cpp` | 修改 | 验证详情页展示买入区、止损位、止盈区和周期。 |
| `docs/codex/PROJECT_CONTEXT.md` | 修改 | 更新 v2.4 文件地图、验证命令和推荐逻辑说明。 |
| `docs/product/InvestInsight-product-overview.md` | 修改 | 更新产品能力和推荐逻辑说明。 |
| `docs/versions/v2.4/README.md` | 修改 | 标注本轮实施内容。 |

## 数据模型

新增 `PriceLevelPlan`：

- `trendStateLabel`：上升趋势、下降趋势、震荡区间、突破确认、过热拉升、数据不足。
- `actionLabel`：观察、回调分批、突破确认、过热不追、风险预警、逻辑失效。
- `entryZoneLow` / `entryZoneHigh`：观察买入区间。
- `stopLossLevel`：跌破后失效的技术位。
- `takeProfitLow` / `takeProfitHigh`：止盈或分批减仓区间。
- `riskRewardRatio`：潜在收益 / 潜在亏损。
- `holdingHorizonLabel`：短期、中期、长周期观察。
- `summary`、`entryReason`、`exitReason`、`invalidationReason`：用于 UI 直接展示。

## 分析规则

### 上升趋势

触发条件：

- 收盘价在 MA20 上方，MA20 不低于 MA60，或 `forecastScore` 明显为正。
- MACD 或均线结构不明显转空。

输出：

- 观察区：MA20、近期支撑和 BOLL 中轨附近，允许 ATR 缓冲。
- 止损位：观察区下沿减 1.5 倍 ATR。
- 止盈区：近期高点、BOLL 上轨或 2.5 倍 ATR 上沿。
- 动作：价格已经接近观察区时为“回调分批”，否则为“观察”。

### 过热拉升

触发条件：

- RSI 超买、KDJ 高位、价格高于 BOLL 上轨、5 日涨幅过大或今日涨幅过大。

输出：

- 动作：过热不追。
- 观察区：等待回踩 MA20 / BOLL 中轨。
- 止盈区：当前价到上方阻力区。
- 入场时机分应被压缩。

### 下跌趋势

触发条件：

- 收盘价低于 MA20 和 MA60，均线空头排列，或 `forecastScore` 明显为负。

输出：

- 动作：风险预警或逻辑失效。
- 观察区：不直接给积极买入，只给“企稳观察区”。
- 止盈区：反弹到 MA20 / 前支撑变压力时减仓。
- 入场时机分应被压缩。

### 数据不足

触发条件：

- K 线不足 20 根或收盘价无效。

输出：

- 动作：观察。
- 点位为空或使用策略兜底，不影响主评分。

## TDD 任务拆分

### 任务 1：核心点位分析器

1. 编写 `PriceLevelAnalyzerSmoke`，先验证编译失败或行为失败。
2. 新增 `PriceLevelAnalyzer` 和 `PriceLevelPlan`，让测试通过。
3. 验证命令：

```powershell
cmake --build build --config Release --target InvestInsightPriceLevelSmoke -- /m
.\build\Release\InvestInsightPriceLevelSmoke.exe
```

### 任务 2：主流程和推荐生命周期接入

1. 在 `InsightOrchestrator` 中调用 `PriceLevelAnalyzer::analyze`。
2. 在 `RecommendationTracker` 中读取 `riskRewardRatio` 和 `actionLabel`，校正 `entryTimingScore`。
3. 扩展 `RecommendationTrackerSmoke`，验证过热/风险收益比不足会压缩入场时机。
4. 验证命令：

```powershell
cmake --build build --config Release --target InvestInsightRecommendationSmoke -- /m
.\build\Release\InvestInsightRecommendationSmoke.exe
```

### 任务 3：UI 展示

1. 板块机会页增加点位计划摘要，优先展示动作、观察区、风险收益比和周期。
2. 板块详情页增加“技术点位计划”区块，展示买入区、止损位、止盈区、失效条件。
3. 扩展 UI smoke，验证关键文案存在。
4. 验证命令：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\verify_ui_smoke.ps1
```

### 任务 4：端到端构建和截图

1. 运行 Release 构建。
2. 运行无 AI 自动分析和截图命令。
3. 截图保存到 `docs/versions/v2.4/screenshots/technical-entry-exit/`。
4. 验证命令：

```powershell
cmake --build build --config Release -- /m
.\build\Release\InvestInsight.exe --auto-analyze-no-ai
.\build\Release\InvestInsight.exe --capture-ui-screenshots docs\versions\v2.4\screenshots\technical-entry-exit
```

## 验收标准

- `InvestInsightPriceLevelSmoke`、`InvestInsightRecommendationSmoke`、`verify_ui_smoke.ps1` 均通过。
- 有 K 线的板块详情页能看到观察买入区、止损失效位、止盈区、风险收益比和持有周期。
- 过热板块不会因为今日涨幅高直接强化入场时机。
- 风险收益比不足时，推荐生命周期会提示等待或压缩入场分。
- 所有新增说明同步写入项目上下文和产品说明。
