#include "core/RotationDetector.h"
#include "domain/AnalysisResult.h"

#include <algorithm>
#include <QtMath>

QList<RotationSignal> RotationDetector::detect(const QList<SectorSnapshot> &sectors)
{
    QList<RotationSignal> result;
    if (sectors.isEmpty()) return result;

    for (const SectorSnapshot &s : sectors) {
        RotationSignal rs;
        rs.sector = s.industry;
        rs.momentum5d = s.fiveDayMomentum;
        rs.momentum20d = s.twentyDayMomentum;

        rs.momentumDelta = s.fiveDayMomentum - s.twentyDayMomentum;

        if (s.fundFlowSeries.size() >= 10) {
            double recent5 = 0, prev5 = 0;
            int sz = s.fundFlowSeries.size();
            for (int i = sz - 5; i < sz; ++i) recent5 += s.fundFlowSeries[i];
            for (int i = sz - 10; i < sz - 5; ++i) prev5 += s.fundFlowSeries[i];
            rs.fundFlowAccel = recent5 - prev5;
        }

        rs.rotationScore = rs.momentumDelta * 0.5 + rs.fundFlowAccel * 0.3 + rs.momentum5d * 0.2;

        rs.isRotatingIn = (rs.momentumDelta > 1.0 && rs.momentum5d > 0.5);
        rs.isRotatingOut = (rs.momentumDelta < -1.0 && rs.momentum5d < -0.5);
        result.push_back(rs);
    }

    std::sort(result.begin(), result.end(),
              [](const RotationSignal &a, const RotationSignal &b) {
                  return a.rotationScore > b.rotationScore;
              });

    return result;
}

MarketRiskRadar RotationDetector::computeRisk(const QList<SectorSnapshot> &sectors,
                                               const MarketContext &ctx)
{
    MarketRiskRadar radar;
    if (sectors.isEmpty()) return radar;

    // 1) 动量风险：有多少板块处于极端状态
    int extremeUp = 0, extremeDown = 0;
    double avgMom = 0, avgTech = 0;
    for (const SectorSnapshot &s : sectors) {
        avgMom += s.fiveDayMomentum;
        avgTech += s.tech.techScore;
        if (s.fiveDayMomentum > 3.0) ++extremeUp;
        if (s.fiveDayMomentum < -3.0) ++extremeDown;
    }
    avgMom /= sectors.size();
    avgTech /= sectors.size();
    double extremePct = static_cast<double>(extremeUp + extremeDown) / sectors.size();
    radar.momentumRisk = qBound(0.0, 50.0 + extremePct * 200 + qAbs(avgMom) * 5, 100.0);

    // 2) 市场广度风险
    if (ctx.valid) {
        double adRatio = ctx.advanceDeclineRatio;
        if (adRatio > 2.0) radar.breadthRisk = 25;
        else if (adRatio > 1.2) radar.breadthRisk = 35;
        else if (adRatio > 0.8) radar.breadthRisk = 50;
        else if (adRatio > 0.5) radar.breadthRisk = 65;
        else radar.breadthRisk = 80;

        if (ctx.limitDownCount > 30) radar.breadthRisk += 15;
        radar.breadthRisk = qBound(0.0, radar.breadthRisk, 100.0);
    }

    // 3) 资金面风险
    if (ctx.northboundFlowValid) {
        if (ctx.northboundNetBuy > 50) radar.capitalFlowRisk = 25;
        else if (ctx.northboundNetBuy > 10) radar.capitalFlowRisk = 35;
        else if (ctx.northboundNetBuy > -10) radar.capitalFlowRisk = 50;
        else if (ctx.northboundNetBuy > -50) radar.capitalFlowRisk = 65;
        else radar.capitalFlowRisk = 80;
    }

    // 4) 集中度风险：少数板块涨幅过大
    QVector<double> changes;
    for (const SectorSnapshot &s : sectors) changes.push_back(s.todayChangePct);
    std::sort(changes.begin(), changes.end(), [](double a, double b) { return qAbs(a) > qAbs(b); });
    double top5Avg = 0;
    int top5Count = qMin(5, changes.size());
    for (int i = 0; i < top5Count; ++i) top5Avg += qAbs(changes[i]);
    if (top5Count > 0) top5Avg /= top5Count;
    double totalAvg = 0;
    for (double c : changes) totalAvg += qAbs(c);
    if (!changes.isEmpty()) totalAvg /= changes.size();
    double concRatio = (totalAvg > 0.01) ? top5Avg / totalAvg : 1.0;
    radar.concentrationRisk = qBound(0.0, 30.0 + concRatio * 20.0, 100.0);

    // 5) 波动性风险
    double avgVol = 0;
    int volCount = 0;
    for (const SectorSnapshot &s : sectors) {
        if (s.tech.bollWidth > 0) {
            avgVol += s.tech.bollWidth;
            ++volCount;
        }
    }
    if (volCount > 0) avgVol /= volCount;
    radar.volatilityRisk = qBound(0.0, 30.0 + avgVol * 200.0, 100.0);

    // 6) 估值风险（通过RSI分布近似）
    int overbought = 0, oversold = 0;
    for (const SectorSnapshot &s : sectors) {
        if (s.tech.rsiOverbought) ++overbought;
        if (s.tech.rsiOversold) ++oversold;
    }
    double obPct = static_cast<double>(overbought) / sectors.size();
    radar.valuationRisk = qBound(0.0, 40.0 + obPct * 200.0 - static_cast<double>(oversold) / sectors.size() * 100.0, 100.0);

    // 综合风险
    radar.compositeRisk = (radar.valuationRisk * 0.15
                         + radar.momentumRisk * 0.20
                         + radar.breadthRisk * 0.20
                         + radar.capitalFlowRisk * 0.20
                         + radar.concentrationRisk * 0.10
                         + radar.volatilityRisk * 0.15);

    if (radar.compositeRisk >= 75)
        radar.riskAdvice = "市场风险较高，建议控制仓位至30%以下，重点关注防御性板块（银行、消费、电力），规避高波动题材。";
    else if (radar.compositeRisk >= 60)
        radar.riskAdvice = "市场风险偏高，建议将仓位控制在50%左右，优选基本面扎实的板块，适当配置避险资产。";
    else if (radar.compositeRisk >= 40)
        radar.riskAdvice = "市场风险中性，可维持正常仓位60-80%，关注板块轮动信号，均衡配置成长与价值。";
    else if (radar.compositeRisk >= 25)
        radar.riskAdvice = "市场风险较低，可适当提升仓位至80%以上，积极把握板块轮动带来的结构性机会。";
    else
        radar.riskAdvice = "市场情绪极度低迷，历史上往往是中长期布局良机。建议分批建仓，重点关注超跌优质板块。";

    return radar;
}
