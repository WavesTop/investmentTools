#pragma once

#include <QString>
#include <QStringList>
#include "core/TrendStageDetector.h"
#include "core/TrendHealthAnalyzer.h"
#include "core/BreadthAnalyzer.h"
#include "core/OverheatDetector.h"
#include "core/FlowStructureAnalyzer.h"
#include "core/MarketRegimeDetector.h"

enum class InvestmentState {
    EarlyTrendOpportunity,  // 早期趋势机会
    HealthyUptrend,         // 健康上涨趋势
    LateStageMomentum,      // 末期动量
    OverheatedRisk,         // 过热风险
    DistributionWarning,    // 派发警告
    DefensiveHold,          // 防御持有
    DowntrendAvoid          // 下跌回避
};

struct ExplanationChain {
    InvestmentState state = InvestmentState::DefensiveHold;
    QString stateName;
    QString trendStageDesc;
    QString strengthReason;
    QString sustainabilityReason;
    QString riskReason;
    QString conclusion;
    QStringList bullishFactors;
    QStringList bearishFactors;
    QStringList riskWarnings;

    double trendStrength = 0.0;     // 趋势强度 [0-100]
    double trendQuality = 0.0;      // 趋势质量 [0-100]
    double exhaustionRisk = 0.0;    // 耗竭风险 [0-100]
};

class ExplainabilityEngine {
public:
    static ExplanationChain build(const TrendStageResult &stage,
                                  const TrendHealth &health,
                                  const BreadthMetrics &breadth,
                                  const OverheatMetrics &overheat,
                                  const FlowStructure &flow,
                                  double forecastScore,
                                  double todayChangePct);
    static QString stateName(InvestmentState s);
};
