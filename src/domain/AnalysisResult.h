#pragma once

#include <QList>
#include <QPair>
#include <QString>
#include <QStringList>
#include <QVector>

#include "domain/InvestmentAdvice.h"
#include "core/SectorFetcher.h"
#include "core/MarketContext.h"
#include "core/RotationDetector.h"
#include "core/TrendStageDetector.h"
#include "core/TrendHealthAnalyzer.h"
#include "core/BreadthAnalyzer.h"
#include "core/OverheatDetector.h"
#include "core/FlowStructureAnalyzer.h"
#include "core/ExplainabilityEngine.h"
#include "core/MarketRegimeDetector.h"

struct EventMarker
{
    QString date;
    QString text;
    double sentiment = 0.0; // >0 positive, <0 negative
};

struct NewsEntry
{
    QString date;
    QString title;
    QString source;
    QString url;
    double sentiment = 0.0;
};

struct CapitalAnomaly
{
    QString date;
    double flowValue = 0.0;
    double zScore = 0.0;
    QString desc;
};

struct StrategyBacktest
{
    QString name;
    int totalTrades = 0;
    int wins = 0;
    double winRate = 0.0;
    double avgReturn = 0.0;
    double maxDrawdown = 0.0;
    QString currentSignal;
};

struct CycleAnalysis
{
    bool isCyclical = false;
    double cyclicScore = 0.0;      // 0-100，周期性强度
    int estimatedPeriodDays = 0;   // 估算的周期长度（交易日）
    double amplitude = 0.0;        // 周期振幅%
    // 五阶段：底部→复苏→扩张→顶部→收缩
    QString phaseName;             // "底部蓄力"/"复苏上行"/"扩张加速"/"顶部滞涨"/"收缩回调"
    int phaseIndex = 0;            // 0-4
    double phaseProgress = 0.0;    // 当前阶段已完成的比例 0-100%
    QString phaseAdvice;
    int peakCount = 0;
    int troughCount = 0;
    double lastPeakPrice = 0.0;
    double lastTroughPrice = 0.0;
    QString lastPeakDate;
    QString lastTroughDate;
};

struct TechSignals
{
    double macdDIF = 0, macdDEA = 0, macdHist = 0;
    bool macdGoldenCross = false;
    bool macdDeadCross = false;

    double rsi6 = 50, rsi12 = 50, rsi24 = 50;
    bool rsiOverbought = false;
    bool rsiOversold = false;

    double kdjK = 50, kdjD = 50, kdjJ = 50;
    bool kdjGoldenCross = false;
    bool kdjOverbought = false;
    bool kdjOversold = false;

    double bollUpper = 0, bollMid = 0, bollLower = 0;
    double bollWidth = 0;
    bool priceAboveUpper = false;
    bool priceBelowLower = false;

    double ma5 = 0, ma10 = 0, ma20 = 0, ma60 = 0;
    bool maLongArrange = false;
    bool maShortArrange = false;

    double volRatio = 1.0;
    bool volExpansion = false;
    bool volShrink = false;

    double techScore = 0.0;
};

struct TradingStrategy
{
    QString actionLabel;
    QString shortTermView;
    QString mediumTermView;
    QString longTermView;

    double supportLevel1 = 0;
    double supportLevel2 = 0;
    double resistLevel1 = 0;
    double resistLevel2 = 0;

    double stopLossPct = 0;
    double takeProfitPct = 0;
    QString stopLossReason;
    QString takeProfitReason;

    QString operationAdvice;
};

struct FormulaBreakdown
{
    double momentumFactor = 0.0;
    double todayFactor = 0.0;
    double sentimentFactor = 0.0;
    double newsIntensityFactor = 0.0;
    double fundFlowFactor = 0.0;
    double hotnessFactor = 0.0;
    double meanReversionPenalty = 0.0;
    double techFactor = 0.0;
    double valuationFactor = 0.0;
    double crowdingFactor = 0.0;
    double rawForecast = 0.0;
};

struct SectorSnapshot
{
    QString industry;
    AdviceAction action = AdviceAction::Hold;

    double todayChangePct = 0.0;
    double fiveDayMomentum = 0.0;
    double twentyDayMomentum = 0.0;
    double newsSentiment = 0.0;
    double forecastScore = 0.0;
    double confidence = 0.0;
    double dataQualityScore = 0.0;   // 数据质量分 [0,100]
    double dataQualityWeight = 1.0;  // 评分权重 [0.55,1.00]
    QString dataQualityNote;
    double sectorHotScore = 0.0;
    int newsHitCount = 0;
    int positiveNewsCount = 0;
    int negativeNewsCount = 0;
    int sectorStockCount = 0;
    FormulaBreakdown formula;
    TechSignals tech;
    TradingStrategy strategy;

    QString trendSummary;
    QString personalAdvice;
    QString analysisNarrative;
    QString aiAnalysis;
    QString aiPredictionReason;
    QString aiTrendSummary;
    QStringList positiveFactors;
    QStringList negativeFactors;
    QStringList newsHeadlines;
    QVector<NewsEntry> newsEntries;
    QVector<KBar> dailyBars;
    QVector<double> trendSeries;
    QVector<double> weekSeries;
    QVector<double> monthSeries;
    QVector<double> fundFlowSeries;
    QString listSource;
    QString klineSource;
    QString fundFlowSource;
    QString valuationSource;
    QString lastDataDate;
    QStringList missingDataItems;
    int sectorTier = 1;
    QString sectorTierLabel;
    bool crossSourceValidated = false;
    double sourceConsistencyScore = 70.0;
    double sourceConsistencyWeight = 1.0;
    double weekMomentum = 0.0;
    double monthMomentum = 0.0;
    double fundFlowFactor = 0.0;

    // 估值与拥挤度
    double peRatio = 0.0;
    double pbRatio = 0.0;
    double pePercentile = 50.0;
    double crowdingIndex = 50.0;
    double totalMarketCap = 0.0;
    int stockCount = 0;

    // 事件标注
    QList<EventMarker> eventMarkers;
    // 事件驱动分析：大涨/大跌日的事件归因 (date, changePct, eventText)
    struct EventDrivenMove { QString date; double changePct; QString eventText; };
    QList<EventDrivenMove> eventDrivenMoves;
    // 主力资金异动
    QList<CapitalAnomaly> capitalAnomalies;
    // 未来规划事件（规则提取）
    QStringList upcomingEvents;
    // AI 前瞻事件预测
    QStringList futureEventsAI;
    // 休市标记
    bool marketClosed = false;
    // 累计追踪
    double cumulativeReturn = 0.0;
    double trackingDays = 0;
    double trackingStartPrice = 0.0;
    // 策略回测
    QList<StrategyBacktest> backtestResults;
    QString bestStrategyName;
    double bestStrategyWinRate = 0.0;
    // 策略追踪：最佳策略在近期的实际收益
    double bestStrategyTrackedReturn = 0.0;
    QString bestStrategyCurrentSignal;
    QString investmentRecommendation;
    // 周期分析
    CycleAnalysis cycle;

    // ---- 趋势生命周期系统（升级版） ----
    TrendStageResult trendStageResult;
    TrendHealth trendHealth;
    BreadthMetrics breadth;
    OverheatMetrics overheat;
    FlowStructure flowStructure;
    ExplanationChain explanation;
};

struct AnalysisResult
{
    QString reportText;
    QString aiOverallSummary;
    QString aiMethodologyNote;
    QStringList aiErrors;
    bool aiAvailable = false;
    QList<SectorSnapshot> sectors;

    MarketContext marketCtx;
    QList<RotationSignal> rotationSignals;
    MarketRiskRadar riskRadar;
    MarketRegimeResult marketRegime;
};
