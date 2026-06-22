# 事件传导引擎补完实现计划

> 面向 AI 代理的工作者：执行本计划时使用 `executing-plans` 或按 TDD 小步执行。每个切片完成后先运行对应验证，再本地 commit，不能 push 远端。

最后更新：2026-06-22

## 目标

把当前 v2.0 的事件传导 MVP 升级为 v2.1：新闻先被结构化为事件，再判断事件状态和时间窗口，解释影响路径，映射到板块，并把结果以可验证、可追踪、可展示的方式进入评分和 UI。

## 当前基线

已存在能力：

- `MacroEvent`、`SectorEventImpact` 基础结构。
- `EventRuleBook`、`EventExtractionEngine`、`ImpactGraphEngine`、`SectorImpactAnalyzer`、`EventRepository`。
- `InsightOrchestrator` 已把 `eventImpacts`、`eventCatalystScore`、`eventSummary` 注入 `SectorSnapshot`。
- `EventRadarRenderer` 和 `SectorDetailRenderer` 已展示事件、路径和板块详情。
- `InvestInsightEventSmoke` 覆盖抽取、路径、评分字段和仓库追踪。

当前进度：

- 切片 1 已完成：事件模型已补齐财政政策、地缘贸易、金融市场制度类型，以及传闻、已发生、失效状态；`MacroEvent` 已支持检测时间、发布时间、预期时间、确认时间、结构化观察点、新鲜度和重要性；证据已支持 URL 和可信度；板块影响已支持影响周期。
- 切片 2 已完成：事件状态解析已支持传闻、已发生和失效的基础关键词；`EventRuleBook` 已能输出 FOMC、CPI、PCE、非农、LPR、MLF 等模板观察点；事件仓库可跨重启还原新增状态。
- 切片 3 已完成：高频事件抽取规则已覆盖美联储鹰派/加息、CPI/PCE/非农、国内货币政策、财政刺激/专项债、半导体出口限制、原油供给扰动和市场制度事件。
- 切片 4 已完成：影响路径已补齐美联储鹰派/加息、财政稳增长、半导体出口限制、原油供给扰动和市场制度事件，并为关键路径写入影响周期。
- 切片 5 已完成：事件催化分已纳入来源可信度、新鲜度/重复度和证据时间衰减，失效事件贡献归零。
- 切片 6 已完成：事件仓库已能记录受影响板块在不同窗口的后续收益，并随本地 JSON 持久化。
- 切片 7 已完成：事件雷达和板块详情已展示状态、观察点、证据可信度、影响周期和失效条件。
- 切片 8 尚未完成：诊断命令仍按下方切片推进。

尚未完成：

- 财政政策、地缘贸易、金融市场制度已有基础抽取规则，后续仍需继续扩充样本和路径。
- 传闻、已发生、失效已有基础状态解析，但仍需要更多语义样本补强。
- 时间字段模型已具备，模板观察点已接入，真实日期解析和官方日历仍未完成。
- 状态、时间和日历仍耦合在 `EventRuleBook` 的关键词判断中。
- 影响路径规则已覆盖第一批高频事件，但仍需要继续扩展更多行业链条、失效条件和评分因子。
- 评分已包含方向、强度、置信度、状态权重、来源可信度、新鲜度/重复衰减和证据时间衰减，后续仍可继续补充市场确认因子。
- 事件仓库已记录状态变化和事后窗口表现，后续仍需要把命中率校准接入分析闭环。

## 文件边界

计划主要修改：

- `src/domain/MacroEvent.h`：补齐事件枚举、状态枚举、时间字段、观察点和影响周期。
- `src/core/EventRuleBook.h/.cpp`：逐步从单一规则簿拆出状态和时间判断，先保持兼容。
- `src/core/EventExtractionEngine.cpp`：填充事件时间、证据可信度和默认观察点。
- `src/core/ImpactGraphEngine.h/.cpp`：扩展规则路径和影响周期。
- `src/core/SectorImpactAnalyzer.h/.cpp`：升级事件催化评分公式。
- `src/core/EventRepository.h/.cpp`：扩展追踪字段和事后表现记录。
- `tests/core/EventImpactSmoke.cpp`：每个切片先写失败断言，再实现。
- `docs/codex/PROJECT_CONTEXT.md`、`docs/product/InvestInsight-product-overview.md`：行为或流程变更后同步更新。

不在本轮直接处理：

- 不接入实时官方经济日历接口。
- 不做完整量化回测平台。
- 不把 AI 作为唯一事件判断来源。
- 不重写 UI 技术栈。

## 实施切片

### 切片 1：补齐事件模型

目标：先让领域模型能表达 v2.1 需要的信息，且不破坏现有调用。

测试：

- 在 `tests/core/EventImpactSmoke.cpp` 增加断言：
  - `toString(MacroEventType::FiscalPolicy)`、`GeopoliticsTrade`、`MarketInstitution` 返回稳定字符串。
  - `toString(MacroEventState::Rumor)`、`Occurred`、`Invalidated` 返回稳定字符串。
  - `MacroEvent` 可保存 `detectedAt`、`expectedAt`、`confirmedAt` 和 `nextCheckpoints`。
  - `SectorEventImpact` 可保存 `horizon`。

实现：

- 在 `MacroEvent.h` 新增枚举值和 `ImpactHorizon`。
- 新增 `MacroEventCheckpoint`，包含 `name`、`time`、`reason`。
- 给 `MacroEventEvidence` 增加 `url` 和 `reliability`，默认保持旧代码可用。
- 给 `MacroEvent` 增加时间字段、观察点、`novelty`、`importance`。
- 给 `SectorEventImpact` 增加 `horizon`。
- 更新 `toString`。

验证：

```powershell
cmake --build build --config Release -- /m
.\build\Release\InvestInsightEventSmoke.exe
```

提交建议：

```text
功能(事件): 补齐事件模型字段
```

### 切片 2：状态和时间解析解耦

目标：支持传闻、失效、已发生和结构化观察点。

测试：

- 传闻文本识别为 `Rumor`。
- 否认、辟谣、落空识别为 `Invalidated`。
- 已公布并进入结果观察的文本识别为 `Occurred`。
- FOMC、CPI、PCE、非农、LPR、MLF、财政会议生成 `nextCheckpoints`。

实现：

- 先在 `EventRuleBook` 增加 `resolveCheckpoints()`，保持改动小。
- 后续如文件变大，再拆出 `EventStateResolver` 和 `EventCalendarResolver`。
- `EventExtractionEngine` 调用新解析结果填充 `MacroEvent`。

验证：

```powershell
cmake --build build --config Release -- /m
.\build\Release\InvestInsightEventSmoke.exe
.\build\Release\InvestInsight.exe --debug-event-impact "市场传闻美联储可能提前降息，交易员关注下次 FOMC"
```

### 切片 3：扩展高频事件规则

目标：覆盖第一批高频宏观和政策事件。

规则范围：

- 美联储降息、加息、鹰派和鸽派表态。
- CPI/PCE 高于或低于预期。
- 非农强弱和失业率变化。
- 国内降准、LPR、MLF、财政刺激、专项债。
- 半导体出口限制、国产替代、产业补贴。
- 铜、铝、锂、黄金、原油的价格和供需扰动。
- IPO、印花税、融资融券、减持规则等市场制度。

验证：

```powershell
.\build\Release\InvestInsightEventSmoke.exe
.\build\Release\InvestInsight.exe --debug-event-impact "美国 CPI 低于预期，降息交易升温"
.\build\Release\InvestInsight.exe --debug-event-impact "专项债发行加速，财政稳增长预期升温"
```

### 切片 4：升级影响路径和影响周期

目标：每条路径同时说明方向、强度、关系、周期和失效条件。

实现：

- 给路径补充 `ImpactHorizon`。
- 针对正负分支拆分解释，例如“预防式降息”和“衰退式降息”。
- 增加市场制度、财政、地缘贸易、商品供需路径。

验证：

```powershell
.\build\Release\InvestInsightEventSmoke.exe
.\build\Release\InvestInsight.exe --debug-event-impact "美联储释放鹰派信号，美元指数走强"
```

### 切片 5：升级事件催化评分

目标：让事件评分能区分新事件、重复报道、可靠来源和过期事件。

公式：

```text
score = direction * strength * confidence * stateWeight * sourceReliability * noveltyWeight * timeDecay
```

实现：

- `sourceReliability` 从证据均值兜底到 `confidence`。
- `noveltyWeight` 基于事件仓库的 `seenCount`。
- `timeDecay` 基于最新证据时间。
- 继续把 `forecastScore` 影响限制在小范围。

验证：

```powershell
.\build\Release\InvestInsightEventSmoke.exe
cmake --build build --config Release -- /m
```

### 切片 6：事件追踪和事后表现

目标：记录事件首次发现、状态变化、受影响板块的后续表现，用于校准延迟和命中率。

实现：

- 扩展本地 JSON 记录 1、3、5、20 日窗口。
- 先记录结构和占位计算，不接完整回测平台。
- 输出诊断字段，供后续 UI 使用。

验证：

```powershell
.\build\Release\InvestInsightEventSmoke.exe
```

### 切片 7：UI 展示补齐

目标：事件雷达和板块详情能展示状态时间线、观察点、证据质量和失效条件。

实现：

- `EventRadarRenderer` 增加事件状态时间线和下一观察点。
- `SectorDetailRenderer` 增加影响周期、证据来源、失效条件。
- 不把大段 HTML 回填到 `MainWindow.cpp`。

验证：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\verify_ui_smoke.ps1
```

### 切片 8：诊断命令和回归样本

目标：后续每次修改事件规则都能快速验证。

实现：

- 扩展 `--debug-event-impact` 输出时间字段、观察点、周期和评分因子。
- 如需要新增 `--dump-event-rules`，列出规则 key、类型、状态关键词和路径数量。
- 在 smoke 中保留代表性样本。

验证：

```powershell
.\build\Release\InvestInsightEventSmoke.exe
.\build\Release\InvestInsight.exe --debug-event-impact "美联储降息预期升温，市场关注 CPI 和 FOMC"
```

## 提交节奏

- 文档计划单独提交。
- 每个切片一个本地 commit，尽量 200 到 300 行，原则上不超过 500 行。
- 每个 commit 前必须运行切片对应验证。
- 不直接 push 远端。

## 完成标准

v2.1 完成时，软件应能对一条宏观新闻输出：

- 它属于哪类事件。
- 它处于传闻、预期、排期、确认、已发生、修正或失效中的哪种状态。
- 关键观察点是什么，影响窗口大致在哪里。
- 通过哪些宏观变量和资产变量影响哪些板块。
- 哪些板块是短期催化，哪些是中期趋势或长期逻辑。
- 这个判断的证据来源、可信度、新鲜度和重复程度如何。
