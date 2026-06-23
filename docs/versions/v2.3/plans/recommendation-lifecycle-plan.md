# 推荐生命周期与信任机制开发方案

最后更新：2026-06-23

## 实施进度

- 已新增 `RecommendationState`、`RecommendationRecord`、`RecommendationTracker` 和 `InvestInsightRecommendationSmoke`，覆盖过热不追、回调观察、风险预警、失效移除和 JSON 往返。
- 已接入 `InsightOrchestrator` 最终一致性校验之后，分析结果会生成 `AnalysisResult::recommendationRecords`，并回写 `SectorSnapshot` 的方向分、时机分、生命周期状态和失效条件。
- 已在策略跟踪页新增“推荐跟踪 / 信号复盘”模块，展示当前状态、今日涨跌、推荐后表现、方向分、时机分、动作、状态原因、预警和失效条件。
- 已通过 Release 构建、推荐生命周期 smoke、事件 smoke、AI smoke、UI smoke 和自动截图验证；截图保存于 `docs/versions/v2.3/screenshots/recommendation-lifecycle/`。

## 背景

当前推荐更接近“当日结果排序”：强势上涨的板块容易被推到前面，一旦回落又可能快速消失。用户看到的体验会变成“涨了才推荐，跌了就不见”，尤其是基金申购存在成交延后时，这会明显削弱信任。

v2.3 的核心目标不是让下跌板块继续被强推，而是让系统保留推荐后的状态变化：昨天为什么推荐，今天为什么变成风险预警，后续什么条件会恢复观察或确认失效，都需要被看见。

## 产品目标

- 推荐从单次排序结果升级为生命周期记录。
- 区分“方向仍成立”和“当前不适合追”的入场时机判断。
- 板块大涨后进入“过热不追”，而不是继续用涨幅强化推荐。
- 板块回落时保留在跟踪列表中，并给出“回调观察”“风险预警”或“逻辑失效”的原因。
- 用户能在策略跟踪页看到历史推荐、当前状态、状态变化原因和失效条件。
- 核心评分仍由规则、行情、资金、技术面和事件引擎主导，AI 只做解释增强和分歧提示。

## 当前问题诊断

### 推荐容易追涨

`forecastScore` 当前包含动量、今日涨幅、热度、资金、技术面、新闻和事件催化等因子。在强势行情中，今日涨幅和短期动量会共同推高分数，导致系统更容易推荐已经大幅上涨的板块。

### 下跌后的状态缺失

当板块由涨转跌时，如果综合分回落，板块可能直接跌出推荐区。用户看不到它是“正常回调”“风险预警”还是“逻辑已经失效”，也看不到系统是否承认前一日信号需要复核。

### 方向与时机混在一起

一个板块可以同时满足：

- 中期方向仍然成立。
- 当日已经过热，不适合追入。
- 回调后值得继续观察。
- 单日大跌需要风险预警。

当前一个 `AdviceAction` 很难表达这些差异。

## 设计原则

- 不用下跌自动抄底，也不用上涨自动追高。
- 方向判断和入场时机分开计算、分开展示。
- 推荐记录必须可追踪、可解释、可复盘。
- 状态变化要比排名变化更重要。
- 风险预警不能从列表中静默消失。
- UI 先给用户结论，再给证据和失效条件。

## 推荐生命周期状态

| 状态 | 含义 | 典型触发 |
| --- | --- | --- |
| 新信号 | 首次进入跟踪池 | 方向分达标，且此前没有活跃记录 |
| 推荐中 | 方向和时机都相对匹配 | 方向分为正，涨幅和拥挤度不过热 |
| 过热不追 | 方向可能成立，但当前追入风险高 | 当日涨幅、5 日涨幅、RSI 或拥挤度过高 |
| 回调观察 | 方向仍未破坏，但价格进入回调 | 今日下跌或短期动量回落，事件/资金逻辑仍在 |
| 风险预警 | 推荐后出现明显不利波动 | 单日大跌、推荐后回撤过大或风险因子抬升 |
| 逻辑复核 | 方向与证据开始冲突 | 事件失效、资金转弱、技术面破位但未完全确认 |
| 失效移除 | 推荐逻辑已经失效 | 方向分转负、动作变为减配、事件失效或数据质量不足 |

## 核心实现方案

### 数据模型

新增推荐跟踪结构，建议放在 `src/domain/AnalysisResult.h` 或独立 domain 文件中：

- `RecommendationState`：生命周期状态枚举。
- `RecommendationRecord`：记录板块名称、首次发现时间、最近更新时间、首次动作、当前动作、方向分、入场时机分、今日涨跌、推荐后表现、状态原因、风险原因和失效条件。
- `AnalysisResult::recommendationRecords`：UI 直接使用的推荐跟踪列表。

同时在 `SectorSnapshot` 中补充展示字段：

- `directionScore`：方向判断分，尽量减少今日涨幅和短期动量的放大影响。
- `entryTimingScore`：入场时机分，用于识别过热、回调和风险。
- `recommendationStateLabel`：当前生命周期状态。
- `recommendationReason`：状态变化原因。
- `recommendationWarning`：风险预警说明。
- `recommendationInvalidation`：失效条件。

### 跟踪器

新增 `RecommendationTracker`，职责如下：

- 从当前 `SectorSnapshot` 中计算方向分和入场时机分。
- 读取本地历史推荐记录。
- 根据当前行情、技术面、事件和历史记录决定生命周期状态。
- 将仍需跟踪的记录写回本地。
- 输出给 `AnalysisResult`，供策略页和详情页展示。

本地存储建议先使用 `QSettings` JSON，避免引入新的数据库迁移成本。后续如果要做多周期回测，再迁移到独立 JSON 仓库或 SQLite。

### 状态决策规则

第一版采用透明规则，不依赖 AI 裁决：

- 如果方向分转负或最终动作变为减配，进入“失效移除”或“逻辑复核”。
- 如果推荐后单日跌幅过大，或相对首次记录出现明显回撤，进入“风险预警”。
- 如果方向分仍为正，但当日涨幅、5 日涨幅或技术指标过热，进入“过热不追”。
- 如果方向分仍为正，但价格回落且核心事件/资金逻辑未破坏，进入“回调观察”。
- 如果方向分和入场时机都较好，进入“新信号”或“推荐中”。

### UI 调整

策略跟踪页新增“推荐跟踪 / 信号复盘”模块，展示：

- 板块。
- 首次推荐时间。
- 当前状态。
- 今日涨跌和推荐后表现。
- 方向分与入场时机分。
- 当前动作。
- 状态原因。
- 风险预警和失效条件。

总览页可在后续切片增加简版摘要，例如“今日 3 个风险预警、5 个回调观察、2 个过热不追”。第一版优先把完整信息放在策略跟踪页，减少 UI 改动面。

### AI 使用边界

AI 可以参与：

- 把状态原因改写成更容易理解的一句话。
- 对“方向仍在但时机不好”的场景做解释。
- 标记 AI 与规则判断存在分歧。

AI 不可以：

- 直接把风险预警改回推荐。
- 覆盖 `forecastScore`、`directionScore`、`entryTimingScore`。
- 删除历史推荐记录。

## 实施切片

### 切片 1：文档与测试框架

- 新增 v2.3 计划文档。
- 更新文档入口。
- 新增推荐生命周期 smoke 测试目标。
- 先覆盖“上涨过热不追”“下跌风险预警”“回调观察”“逻辑失效”四类场景。

验证：

```powershell
cmake --build build --config Release -- /m
.\build\Release\InvestInsightRecommendationSmoke.exe
```

### 切片 2：推荐生命周期核心能力

- 新增 `RecommendationTracker`。
- 接入 `InsightOrchestrator`，在 `AnalysisResult` 生成前写入推荐记录。
- 使用 `QSettings` 保存和读取历史推荐记录。
- 给 `SectorSnapshot` 补充方向分、时机分和状态解释字段。

验证：

```powershell
cmake --build build --config Release -- /m
.\build\Release\InvestInsightRecommendationSmoke.exe
.\build\Release\InvestInsightEventSmoke.exe
.\build\Release\InvestInsightAIAnalyzerSmoke.exe
```

### 切片 3：策略跟踪页展示

- 在 `StrategyRenderer` 中新增“推荐跟踪 / 信号复盘”。
- 展示生命周期状态、推荐后表现、风险预警和失效条件。
- 更新 UI smoke 断言。

验证：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\verify_ui_smoke.ps1
.\build\Release\InvestInsight.exe --capture-ui-screenshots docs\versions\v2.3\screenshots\recommendation-lifecycle
```

### 切片 4：文档同步与本地提交

- 更新 `docs/codex/PROJECT_CONTEXT.md`。
- 更新 `docs/product/InvestInsight-product-overview.md`。
- 记录新增测试命令和截图路径。
- 按职责拆分本地 commit，不 push 远端。

## 测试要求

核心测试至少覆盖：

- 大涨板块不能只因涨幅高而继续强推，应进入“过热不追”。
- 前一日推荐的板块次日大跌时仍保留跟踪记录，并进入“风险预警”。
- 方向分仍为正但价格回落时进入“回调观察”，不应静默消失。
- 方向分转负或动作变为减配时进入“失效移除”，并说明失效条件。
- 历史记录可以序列化和反序列化。

UI 测试至少覆盖：

- 策略跟踪页出现“推荐跟踪 / 信号复盘”。
- 能看到状态、方向分、入场时机分、状态原因、风险预警和失效条件。
- UI smoke 通过。
- 完成截图验证。

## 完成标准

- 推荐列表不再只反映当日涨跌排名。
- 昨日推荐、今日回落的板块可以被标记为“风险预警”或“回调观察”，不会无解释消失。
- 大涨板块会在必要时进入“过热不追”，降低追高感。
- 策略跟踪页能解释推荐状态变化。
- 相关核心 smoke、UI smoke、构建和截图验证通过。
