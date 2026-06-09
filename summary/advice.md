InvestInsight 投资决策系统升级方案（趋势生命周期与可解释性增强）

文档目标

本文档用于指导 InvestInsight 从“信息聚合 + 多因子打分系统”升级为“具备趋势生命周期识别、上涨持续性判断、风险状态识别、可解释推理链输出”的高级投资决策系统。

本文不仅包含理念层设计，还包含：

* 架构级改造方向
* 新增核心模块说明
* 因子体系升级方案
* 数据结构设计建议
* 评分逻辑调整方案
* UI 展示建议
* AI 推理链生成规范
* 可直接用于代码实现的结构定义
* 推荐的模块优先级

目标是让 AI 或开发者能够直接基于本文进行代码层次修改与系统升级。

⸻

一、当前系统的问题本质

当前 InvestInsight 已经具备：

* 多源数据获取
* 技术指标计算
* 板块轮动检测
* 新闻情绪分析
* 资金流分析
* 策略回测
* AI 深度分析

从工程角度来说，已经是一个完整的“多因子分析平台”。

但是当前系统存在一个核心问题：

推荐结果往往集中于“已经大涨”的板块。

用户会天然产生如下疑问：

* 为什么已经涨很多了还要推荐？
* 现在买会不会接盘？
* 这到底是主升浪还是顶部？
* 系统为什么认为它还能涨？

这说明当前系统存在：

“后验确认”强

“前瞻判断”弱

即：

系统擅长识别：

* 已经很强
* 已经有资金
* 已经有热度
* 已经有动量

但不擅长判断：

* 还能不能继续涨
* 当前是主升还是末期
* 是健康上涨还是情绪泡沫
* 是机构趋势还是游资博弈

这意味着当前系统本质仍偏向：

Trend Following（趋势跟随）

而不是：

Trend Understanding（趋势理解）

这是系统升级的核心方向。

⸻

二、当前系统为什么天然会“推荐已经大涨的板块”

当前主要因子：

因子	本质
MACD	趋势确认
RSI	动量强弱
资金流	资金追涨
新闻情绪	热度强化
板块轮动	相对强弱
回测胜率	历史统计
动量因子	已发生趋势

这些因子有一个共同特点：

它们都是“已经发生”的数据。

因此系统天然会：

* 越涨评分越高
* 越热评分越高
* 越强越推荐

这是典型的：

Momentum Bias（动量偏置）

问题在于：

市场里的“强”并不都一样。

⸻

三、系统缺失的核心能力

当前系统最大缺口：

“上涨持续性的解释能力”

系统不仅需要知道：

哪个板块强

更需要知道：

这个板块为什么还能继续强

这是系统升级的关键。

⸻

四、最重要的升级方向：趋势生命周期模型

当前系统已有：

* 周期分析
* 峰谷检测
* 趋势判断

但这些更偏统计学。

需要升级为：

投资语义化趋势阶段识别

即：

系统不再只是输出：

* 上涨
* 下跌
* 横盘

而是输出：

* 底部吸筹
* 初期突破
* 主升阶段
* 加速冲顶
* 高位派发
* 下跌趋势

这会极大提升系统可信度。

⸻

五、建议新增核心枚举：TrendStage

#pragma once
enum class TrendStage {
    BottomAccumulation,   // 底部吸筹
    EarlyBreakout,        // 初期突破
    MainUptrend,          // 主升阶段
    Acceleration,         // 加速冲顶
    Distribution,         // 高位派发
    Downtrend,            // 下跌趋势
    Sideways              // 横盘整理
};

⸻

六、趋势阶段识别逻辑（核心）

这是系统升级中最重要的模块之一。

建议新增：

class TrendStageDetector

路径建议：

src/core/TrendStageDetector.h
src/core/TrendStageDetector.cpp

⸻

七、阶段识别因子设计

1. MA结构

条件	含义
MA5 > MA20 > MA60	强趋势
MA5快速远离MA20	加速阶段
MA20走平	横盘
MA60下行	长期弱势

⸻

2. 涨幅位置

建议引入：

float distanceFromMA60;
float distanceFromYearLow;
float distanceFromYearHigh;

用于识别：

* 是否低位启动
* 是否已经高位泡沫
* 是否接近顶部区域

⸻

3. 波动率结构

新增：

float volatilityExpansion;

用于识别：

波动情况	含义
温和放大	健康趋势
急剧放大	情绪化上涨
波动塌缩	趋势衰减

⸻

4. 成交量结构

不要只看：

* 放量

而是看：

情况	含义
温和放量上涨	健康
放量滞涨	派发
缩量上涨	末期
巨量长上影	高风险

建议新增：

float volumeTrendScore;
float distributionRisk;

⸻

5. 回撤质量（极重要）

真正强势趋势：

* 会回踩
* 会震荡
* 会重新走强

而不是：

* 一路垂直上涨

建议新增：

float pullbackHealth;

判定逻辑：

回撤情况	含义
回踩MA20后继续上涨	健康
回撤极浅持续逼空	可能末期
大阴线连续破位	趋势恶化

⸻

八、趋势健康度系统（核心升级）

当前系统判断：

涨不涨

未来系统应该判断：

“涨得健康吗？”

建议新增：

struct TrendHealth {
    float structureScore;
    float momentumQuality;
    float pullbackQuality;
    float volumeHealth;
    float volatilityHealth;
    float sustainability;
};

⸻

九、板块内部宽度（Breadth）

这是专业系统非常重要的一层。

当前系统缺失：

板块内部扩散度分析

即：

不是看：

板块涨没涨

而是看：

有多少个股一起涨

⸻

十、为什么 Breadth 很重要

健康上涨

例如：

AI板块上涨5%

但：

* 80%个股上涨
* 大部分个股站上MA20
* 龙头与跟风同步上涨

说明：

趋势健康

⸻

风险上涨

AI板块上涨5%

但：

* 只有2个龙头上涨
* 大部分个股下跌
* 小票开始掉队

说明：

高位抱团

趋势末期风险上升

⸻

十一、建议新增 Breadth 数据结构

struct BreadthMetrics {
    float advancingRatio;        // 上涨个股比例
    float ma20AboveRatio;        // 站上MA20比例
    float ma60AboveRatio;        // 站上MA60比例
    float newHighRatio;          // 创新高比例
    float limitUpRatio;          // 涨停比例
    float divergenceScore;       // 龙头与板块背离程度
};

⸻

十二、资金结构升级（不要只看净流入）

当前系统：

flowFactor

问题：

无法区分：

* 机构资金
* 游资资金
* 抄底资金
* 高位接盘资金

⸻

十三、建议新增资金结构分析

1. 连续性分析

建议新增：

float continuousInflowDays;

含义：

情况	含义
连续温和流入	机构吸筹
单日爆量流入	情绪高潮

⸻

2. 波动型资金识别

新增：

float speculativeFlowRisk;

用于识别：

* 游资炒作
* 高位接力
* 情绪博弈

⸻

3. ETF资金联动

当前项目已有：

RealLinkageProvider

但未正式接入编排器。

建议：

正式纳入：

InsightOrchestrator

用于识别：

* 中线资金配置
* ETF持续增仓
* 行业配置趋势

⸻

十四、反身性风险系统（必须新增）

当前系统最大风险：

越一致看多 → 越推荐

但市场往往：

越一致 → 越危险

因此必须新增：

Overheat Detection（过热检测）

⸻

十五、建议新增过热指标

struct OverheatMetrics {
    float sentimentOverheat;
    float volumeClimax;
    float volatilityExplosion;
    float maDeviation;
    float turnoverRisk;
    float crowdingRisk;
};

⸻

十六、过热判定逻辑

指标	风险含义
新闻数量暴增	全市场关注
连续涨停	情绪高潮
偏离MA60过大	泡沫化
波动率急升	博弈化
高换手	高位接力

⸻

十七、评分系统升级

当前：

综合因子 → 单一评分 → 建议

问题：

缺少“阶段理解”。

⸻

十八、建议改为双层评分

第一层：趋势强度

float trendStrength;

判断：

* 强不强

⸻

第二层：趋势质量

float trendQuality;

判断：

* 健不健康
* 能否持续

⸻

第三层：风险状态

float exhaustionRisk;

判断：

* 是否接近顶部

⸻

十九、最终输出建议不再简单二元化

当前：

* 增配
* 持有
* 减仓

建议升级为：

enum class InvestmentState {
    EarlyTrendOpportunity,
    HealthyUptrend,
    LateStageMomentum,
    OverheatedRisk,
    DistributionWarning,
    DefensiveHold,
    DowntrendAvoid
};

⸻

二十、建议输出语义化投资结论

不要只输出：

推荐增配

而要输出：

当前板块处于主升阶段中期。
趋势结构健康：
- MA20持续抬升
- 回撤深度较小
- 板块内部78%个股站上MA20
- 连续6日主力净流入
当前更像趋势中继，而非顶部加速。
短期存在技术性回调风险，但中期趋势仍维持偏强。

这会显著增强可信度。

⸻

二十一、Explainable Investing（可解释投资）

这是未来系统的核心方向。

当前系统：

输入 → 打分 → 推荐

用户感知：

黑盒

未来系统应该：

数据 → 因子 → 推理链 → 风险分析 → 结论

⸻

二十二、建议新增 Explainability 模块

class ExplainabilityEngine

路径建议：

src/core/ExplainabilityEngine.h
src/core/ExplainabilityEngine.cpp

⸻

二十三、Explainability 输出结构

struct ExplanationChain {
    QString trendStage;
    QString strengthReason;
    QString sustainabilityReason;
    QString riskReason;
    QString conclusion;
    QStringList bullishFactors;
    QStringList bearishFactors;
    QStringList riskWarnings;
};

⸻

二十四、AI 模块升级方向

当前 AI 更偏：

* 摘要
* 总结
* 描述

建议升级为：

“结构化推理生成”

AI 输入不再只是新闻。

应该输入：

* 趋势阶段
* Breadth
* 趋势健康度
* 风险指标
* 资金结构
* 市场状态

然后 AI 输出：

解释链

风险分析

推理过程

而不是单纯情绪总结。

⸻

二十五、建议新增市场状态机（重要）

不同市场环境：

* 因子有效性不同
* 板块偏好不同
* 风险偏好不同

⸻

二十六、建议新增：MarketRegime

enum class MarketRegime {
    RiskOn,
    RiskOff,
    BullMarket,
    BearMarket,
    HighVolatility,
    LowVolatility,
    RotationMarket
};

⸻

二十七、不同市场状态应动态调整因子权重

例如：

牛市

提高：

* 动量因子
* 趋势因子

降低：

* 估值约束

⸻

熊市

提高：

* 防御权重
* 风险控制
* 估值权重

降低：

* 趋势追涨

⸻

二十八、建议新增动态权重系统

struct DynamicFactorWeights {
    float momentumWeight;
    float valuationWeight;
    float sentimentWeight;
    float riskWeight;
    float breadthWeight;
    float sustainabilityWeight;
};

⸻

二十九、最终推荐系统架构（升级版）

数据获取层
    ↓
技术指标层
    ↓
趋势阶段识别
    ↓
趋势健康度分析
    ↓
Breadth 分析
    ↓
资金结构分析
    ↓
风险过热分析
    ↓
市场状态分析
    ↓
动态权重评分
    ↓
Explainability 推理链生成
    ↓
AI 深度解释
    ↓
最终投资建议

⸻

三十、推荐新增文件结构

src/core/
├── TrendStageDetector.h/.cpp
├── TrendHealthAnalyzer.h/.cpp
├── BreadthAnalyzer.h/.cpp
├── FlowStructureAnalyzer.h/.cpp
├── OverheatDetector.h/.cpp
├── MarketRegimeDetector.h/.cpp
├── ExplainabilityEngine.h/.cpp
└── DynamicWeightEngine.h/.cpp

⸻

三十一、建议新增 SectorSnapshot 字段

TrendStage trendStage;
TrendHealth trendHealth;
BreadthMetrics breadth;
OverheatMetrics overheat;
ExplanationChain explanation;
float trendStrength;
float trendQuality;
float sustainabilityScore;
float exhaustionRisk;
float institutionalFlowScore;
float speculativeRisk;

⸻

三十二、UI 层升级建议

当前 UI 更偏：

* 数据展示
* 图表展示

未来应该强化：

“推理展示”

⸻

三十三、建议新增 UI 区块

1. 趋势生命周期卡片

显示：

* 当前阶段
* 阶段概率
* 历史相似阶段

⸻

2. 趋势健康度雷达图

显示：

* 结构
* 动量
* 波动
* Breadth
* 风险

⸻

3. 风险警告区

例如：

⚠ 当前已进入高波动区域
⚠ 板块内部开始分化
⚠ 龙头与中位股背离加大

⸻

4. AI 推理链展示

显示：

* 为什么推荐
* 为什么还能涨
* 为什么有风险
* 当前更像什么阶段

⸻

三十四、真正高级的投资系统是什么

不是：

找到涨得最好的板块

而是：

判断：

“这个趋势还能不能持续”

这是系统从：

* 数据系统

升级为：

* 决策系统

最关键的一步。

⸻

三十五、最终优先级建议

P0（必须优先）

1. 趋势阶段识别

原因：

这是整个系统可信度提升的核心。

⸻

2. 趋势健康度

原因：

解决“已经涨很多还能不能买”的问题。

⸻

3. Explainability 推理链

原因：

解决系统黑盒问题。

⸻

4. 板块 Breadth

原因：

识别主升 vs 抱团顶部。

⸻

P1（重要升级）

* 市场状态机
* 动态因子权重
* 风险过热系统
* 资金结构识别

⸻

P2（高级量化方向）

* 因子衰减模型
* 历史相似行情匹配
* AI 自动推理链生成
* 风格轮动识别
* 宏观周期联动

⸻

三十六、最终总结

InvestInsight 当前已经具备：

“发现强势板块”的能力

下一阶段需要构建的是：

“理解趋势生命周期”的能力

真正高级的投资系统：

不是会告诉用户：

什么涨了

而是会告诉用户：

为什么还能继续涨

以及：

什么情况下不应该再追。

这才是系统真正从：

Information System（信息系统）

升级为：

Decision Intelligence System（决策智能系统）

的关键。