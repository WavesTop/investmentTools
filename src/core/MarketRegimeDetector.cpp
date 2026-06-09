#include "core/MarketRegimeDetector.h"
#include <cmath>
#include <algorithm>

QString MarketRegimeDetector::regimeName(MarketRegime r)
{
    switch (r) {
    case MarketRegime::RiskOn:         return QString::fromUtf8("风险偏好上升");
    case MarketRegime::RiskOff:        return QString::fromUtf8("风险偏好下降");
    case MarketRegime::BullMarket:     return QString::fromUtf8("牛市行情");
    case MarketRegime::BearMarket:     return QString::fromUtf8("熊市行情");
    case MarketRegime::HighVolatility: return QString::fromUtf8("高波动市场");
    case MarketRegime::LowVolatility:  return QString::fromUtf8("低波动市场");
    case MarketRegime::RotationMarket: return QString::fromUtf8("轮动行情");
    }
    return QString::fromUtf8("未知");
}

MarketRegimeResult MarketRegimeDetector::detect(const MarketContext &ctx)
{
    MarketRegimeResult r;
    DynamicFactorWeights &w = r.weights;

    const auto &sh = ctx.shanghai;
    double shChangePct = sh.changePct;
    double shMA20Distance = 0;
    {
        const int bn = sh.dailyBars.size();
        if (bn >= 20) {
            double ma20 = 0;
            for (int i = bn - 20; i < bn; ++i) ma20 += sh.dailyBars[i].close;
            ma20 /= 20.0;
            if (ma20 > 0) shMA20Distance = (sh.dailyBars.last().close - ma20) / ma20 * 100.0;
        }
    }

    double fearGreed = ctx.marketRiskScore;
    double advanceDecline = (ctx.totalStocks > 0) ? double(ctx.advanceCount) / ctx.totalStocks * 100.0 : 50.0;
    double limitUpPct = (ctx.totalStocks > 0) ? double(ctx.limitUpCount) / ctx.totalStocks * 100.0 : 0;
    double limitDownPct = (ctx.totalStocks > 0) ? double(ctx.limitDownCount) / ctx.totalStocks * 100.0 : 0;

    // Score each regime
    double bullScore = 0, bearScore = 0, hiVolScore = 0, loVolScore = 0, riskOnScore = 0, riskOffScore = 0, rotScore = 0;

    // Bull market
    if (shMA20Distance > 3) bullScore += 25;
    if (advanceDecline > 60) bullScore += 20;
    if (fearGreed > 60) bullScore += 15;
    if (limitUpPct > 2) bullScore += 15;
    if (shChangePct > 1) bullScore += 10;

    // Bear market
    if (shMA20Distance < -3) bearScore += 25;
    if (advanceDecline < 35) bearScore += 20;
    if (fearGreed < 30) bearScore += 15;
    if (limitDownPct > 2) bearScore += 15;
    if (shChangePct < -1) bearScore += 10;

    // High volatility
    double recentVol = qAbs(shChangePct);
    if (recentVol > 2.5) hiVolScore += 30;
    if (limitUpPct + limitDownPct > 4) hiVolScore += 25;
    if (fearGreed < 25 || fearGreed > 80) hiVolScore += 20;

    // Low volatility
    if (recentVol < 0.5) loVolScore += 30;
    if (advanceDecline > 40 && advanceDecline < 60) loVolScore += 20;
    if (fearGreed > 40 && fearGreed < 60) loVolScore += 15;

    // Risk on
    if (fearGreed > 55) riskOnScore += 25;
    if (advanceDecline > 55) riskOnScore += 20;
    if (shChangePct > 0.5) riskOnScore += 15;

    // Risk off
    if (fearGreed < 40) riskOffScore += 25;
    if (advanceDecline < 40) riskOffScore += 20;
    if (shChangePct < -0.5) riskOffScore += 15;

    // Rotation
    if (advanceDecline > 40 && advanceDecline < 65) rotScore += 20;
    if (qAbs(shChangePct) < 1.5) rotScore += 15;
    rotScore += 15; // baseline

    struct { double s; MarketRegime reg; } cands[] = {
        {bullScore, MarketRegime::BullMarket},
        {bearScore, MarketRegime::BearMarket},
        {hiVolScore, MarketRegime::HighVolatility},
        {loVolScore, MarketRegime::LowVolatility},
        {riskOnScore, MarketRegime::RiskOn},
        {riskOffScore, MarketRegime::RiskOff},
        {rotScore, MarketRegime::RotationMarket},
    };

    double maxS = 0, totalS = 0;
    for (auto &c : cands) {
        totalS += c.s;
        if (c.s > maxS) { maxS = c.s; r.regime = c.reg; }
    }
    r.regimeConfidence = (totalS > 0) ? maxS / totalS * 100.0 : 0;
    r.regimeName = regimeName(r.regime);

    // Adjust factor weights based on regime
    switch (r.regime) {
    case MarketRegime::BullMarket:
    case MarketRegime::RiskOn:
        w.momentumWeight = 0.25; w.valuationWeight = 0.08; w.sentimentWeight = 0.15;
        w.riskWeight = 0.10; w.breadthWeight = 0.12; w.sustainabilityWeight = 0.18; w.flowWeight = 0.12;
        break;
    case MarketRegime::BearMarket:
    case MarketRegime::RiskOff:
        w.momentumWeight = 0.10; w.valuationWeight = 0.22; w.sentimentWeight = 0.10;
        w.riskWeight = 0.25; w.breadthWeight = 0.10; w.sustainabilityWeight = 0.13; w.flowWeight = 0.10;
        break;
    case MarketRegime::HighVolatility:
        w.momentumWeight = 0.12; w.valuationWeight = 0.15; w.sentimentWeight = 0.12;
        w.riskWeight = 0.25; w.breadthWeight = 0.10; w.sustainabilityWeight = 0.16; w.flowWeight = 0.10;
        break;
    case MarketRegime::LowVolatility:
        w.momentumWeight = 0.20; w.valuationWeight = 0.18; w.sentimentWeight = 0.15;
        w.riskWeight = 0.10; w.breadthWeight = 0.12; w.sustainabilityWeight = 0.15; w.flowWeight = 0.10;
        break;
    case MarketRegime::RotationMarket:
        // balanced defaults
        break;
    }

    return r;
}
