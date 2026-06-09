#include "core/ExplainabilityEngine.h"
#include <cmath>

QString ExplainabilityEngine::stateName(InvestmentState s)
{
    switch (s) {
    case InvestmentState::EarlyTrendOpportunity: return QString::fromUtf8("早期趋势机会");
    case InvestmentState::HealthyUptrend:        return QString::fromUtf8("健康上涨趋势");
    case InvestmentState::LateStageMomentum:     return QString::fromUtf8("末期动量");
    case InvestmentState::OverheatedRisk:        return QString::fromUtf8("过热风险");
    case InvestmentState::DistributionWarning:   return QString::fromUtf8("派发警告");
    case InvestmentState::DefensiveHold:         return QString::fromUtf8("防御持有");
    case InvestmentState::DowntrendAvoid:        return QString::fromUtf8("下跌回避");
    }
    return QString::fromUtf8("未知");
}

ExplanationChain ExplainabilityEngine::build(
    const TrendStageResult &stage,
    const TrendHealth &health,
    const BreadthMetrics &breadth,
    const OverheatMetrics &overheat,
    const FlowStructure &flow,
    double forecastScore,
    double todayChangePct)
{
    ExplanationChain e;

    // --- Trend strength ---
    e.trendStrength = 0;
    if (stage.stage == TrendStage::MainUptrend || stage.stage == TrendStage::Acceleration)
        e.trendStrength = 70 + stage.stageConfidence * 0.3;
    else if (stage.stage == TrendStage::EarlyBreakout)
        e.trendStrength = 55 + stage.stageConfidence * 0.2;
    else if (stage.stage == TrendStage::Downtrend)
        e.trendStrength = 15;
    else if (stage.stage == TrendStage::Distribution)
        e.trendStrength = 35;
    else
        e.trendStrength = 40;
    e.trendStrength = qBound(0.0, e.trendStrength, 100.0);

    // --- Trend quality ---
    e.trendQuality = health.sustainability;

    // --- Exhaustion risk ---
    e.exhaustionRisk = overheat.compositeOverheat * 0.5
                     + stage.distributionRisk * 0.3
                     + (100.0 - breadth.breadthHealth) * 0.2;
    e.exhaustionRisk = qBound(0.0, e.exhaustionRisk, 100.0);

    // --- Determine InvestmentState ---
    if (stage.stage == TrendStage::Downtrend) {
        e.state = InvestmentState::DowntrendAvoid;
    } else if (stage.stage == TrendStage::Distribution && overheat.compositeOverheat > 50) {
        e.state = InvestmentState::DistributionWarning;
    } else if (overheat.compositeOverheat > 65) {
        e.state = InvestmentState::OverheatedRisk;
    } else if (stage.stage == TrendStage::Acceleration && e.exhaustionRisk > 50) {
        e.state = InvestmentState::LateStageMomentum;
    } else if ((stage.stage == TrendStage::EarlyBreakout || stage.stage == TrendStage::BottomAccumulation)
               && health.sustainability > 50) {
        e.state = InvestmentState::EarlyTrendOpportunity;
    } else if (stage.stage == TrendStage::MainUptrend && health.sustainability > 55) {
        e.state = InvestmentState::HealthyUptrend;
    } else {
        e.state = InvestmentState::DefensiveHold;
    }
    e.stateName = stateName(e.state);

    // --- Build textual descriptions ---
    e.trendStageDesc = QString::fromUtf8("当前板块处于「%1」阶段（置信度 %2%）")
        .arg(stage.stageName)
        .arg(stage.stageConfidence, 0, 'f', 0);

    // Strength reason
    {
        QStringList reasons;
        if (stage.distFromMA60 > 5)
            reasons << QString::fromUtf8("价格高于MA60 %1%，趋势偏强").arg(stage.distFromMA60, 0, 'f', 1);
        else if (stage.distFromMA60 < -5)
            reasons << QString::fromUtf8("价格低于MA60 %1%，趋势偏弱").arg(qAbs(stage.distFromMA60), 0, 'f', 1);
        if (flow.continuousInflowDays >= 3)
            reasons << QString::fromUtf8("连续%1日主力净流入，资金面支撑").arg(int(flow.continuousInflowDays));
        if (flow.flowPattern == QString::fromUtf8("机构吸筹"))
            reasons << QString::fromUtf8("资金呈机构吸筹特征");
        if (todayChangePct > 1.5)
            reasons << QString::fromUtf8("今日涨幅%1%，短期动量强劲").arg(todayChangePct, 0, 'f', 2);
        e.strengthReason = reasons.isEmpty() ? QString::fromUtf8("趋势信号不明显") : reasons.join(QString::fromUtf8("；"));
    }

    // Sustainability reason
    {
        QStringList reasons;
        if (health.structureScore > 70)
            reasons << QString::fromUtf8("均线多头排列，结构评分%1").arg(health.structureScore, 0, 'f', 0);
        if (health.pullbackQuality > 65)
            reasons << QString::fromUtf8("回撤健康，回踩均线后能有效企稳");
        else if (health.pullbackQuality < 35)
            reasons << QString::fromUtf8("回撤破位严重，趋势有恶化风险");
        if (health.volumeHealth > 65)
            reasons << QString::fromUtf8("量价配合良好");
        else if (health.volumeHealth < 35)
            reasons << QString::fromUtf8("量价背离，可能缩量上涨或放量下跌");
        if (breadth.breadthHealth > 65)
            reasons << QString::fromUtf8("板块内部%1%个股跟涨，扩散健康").arg(breadth.advancingRatio, 0, 'f', 0);
        else if (breadth.divergenceScore > 40)
            reasons << QString::fromUtf8("板块内部分化严重，龙头与中位股背离");
        e.sustainabilityReason = reasons.isEmpty() ? QString::fromUtf8("可持续性待观察") : reasons.join(QString::fromUtf8("；"));
    }

    // Risk reason
    {
        QStringList reasons;
        if (overheat.compositeOverheat > 50)
            reasons << QString::fromUtf8("过热指数%1，存在回调风险").arg(overheat.compositeOverheat, 0, 'f', 0);
        if (stage.distributionRisk > 40)
            reasons << QString::fromUtf8("高位放量滞涨，派发风险%1%").arg(stage.distributionRisk, 0, 'f', 0);
        if (stage.distFromYearHigh < 5 && stage.stage != TrendStage::BottomAccumulation)
            reasons << QString::fromUtf8("距年内高点仅%1%，接近压力区").arg(stage.distFromYearHigh, 0, 'f', 1);
        if (flow.speculativeFlowRisk > 50)
            reasons << QString::fromUtf8("投机资金风险%1，游资博弈特征明显").arg(flow.speculativeFlowRisk, 0, 'f', 0);
        e.riskWarnings = overheat.warnings;
        for (const auto &r : reasons) e.riskWarnings.append(r);
        e.riskReason = reasons.isEmpty() ? QString::fromUtf8("风险可控") : reasons.join(QString::fromUtf8("；"));
    }

    // Bullish / Bearish factors
    if (stage.stage == TrendStage::MainUptrend || stage.stage == TrendStage::EarlyBreakout) {
        e.bullishFactors << stage.stageAdvice;
        if (health.sustainability > 60)
            e.bullishFactors << QString::fromUtf8("趋势健康度评分 %1/100").arg(health.sustainability, 0, 'f', 0);
        if (flow.institutionalScore > 50)
            e.bullishFactors << QString::fromUtf8("机构资金特征评分 %1/100").arg(flow.institutionalScore, 0, 'f', 0);
    }
    if (stage.stage == TrendStage::Acceleration || stage.stage == TrendStage::Distribution) {
        e.bearishFactors << stage.stageAdvice;
        if (overheat.compositeOverheat > 40)
            e.bearishFactors << QString::fromUtf8("过热综合指数 %1/100").arg(overheat.compositeOverheat, 0, 'f', 0);
    }
    if (stage.stage == TrendStage::Downtrend) {
        e.bearishFactors << stage.stageAdvice;
        e.bearishFactors << QString::fromUtf8("均线空头排列，趋势偏弱");
    }

    // Final conclusion
    switch (e.state) {
    case InvestmentState::EarlyTrendOpportunity:
        e.conclusion = QString::fromUtf8("当前处于趋势早期，风险收益比良好。趋势结构正在构建，可适当试探性建仓，等待确认后加码。");
        break;
    case InvestmentState::HealthyUptrend:
        e.conclusion = QString::fromUtf8("趋势健康运行中。更像趋势中继而非顶部加速，短期存在技术性回调风险，但中期趋势维持偏强，可持有并回踩时加仓。");
        break;
    case InvestmentState::LateStageMomentum:
        e.conclusion = QString::fromUtf8("处于趋势末期加速阶段，短期仍有惯性但风险快速累积。建议逐步止盈，不宜追高，严格执行止损纪律。");
        break;
    case InvestmentState::OverheatedRisk:
        e.conclusion = QString::fromUtf8("市场情绪过热，多项指标发出警告。建议降低仓位，锁定部分利润，等待回调后重新评估。");
        break;
    case InvestmentState::DistributionWarning:
        e.conclusion = QString::fromUtf8("高位派发特征明显，主力可能正在出货。建议果断减仓，避免成为最后的接盘方。");
        break;
    case InvestmentState::DefensiveHold:
        e.conclusion = QString::fromUtf8("趋势方向不明确或处于调整中，建议防御性持有，等待更清晰的方向信号。不宜大举加仓或做空。");
        break;
    case InvestmentState::DowntrendAvoid:
        e.conclusion = QString::fromUtf8("下跌趋势尚未结束，不宜抄底。等待企稳信号（如放量阳线突破MA20）出现后再考虑参与。");
        break;
    }

    return e;
}
