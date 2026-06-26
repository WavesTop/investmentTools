#include "core/PriceLevelAnalyzer.h"

#include <algorithm>

#include <QtGlobal>

namespace {

QString money(double value)
{
    return QString::number(value, 'f', 2);
}

double averageTrueRange(const QVector<KBar> &bars, int period = 14)
{
    if (bars.size() < 2) return 0.0;
    const int first = qMax(1, bars.size() - period);
    double sum = 0.0;
    int count = 0;
    for (int i = first; i < bars.size(); ++i) {
        const KBar &cur = bars[i];
        const KBar &prev = bars[i - 1];
        const double highLow = cur.high - cur.low;
        const double highClose = qAbs(cur.high - prev.close);
        const double lowClose = qAbs(cur.low - prev.close);
        sum += qMax(highLow, qMax(highClose, lowClose));
        ++count;
    }
    return count > 0 ? sum / count : 0.0;
}

double recentLow(const QVector<KBar> &bars, int window = 60)
{
    if (bars.isEmpty()) return 0.0;
    const int first = qMax(0, bars.size() - window);
    double value = bars[first].low;
    for (int i = first + 1; i < bars.size(); ++i) value = qMin(value, bars[i].low);
    return value;
}

double recentHigh(const QVector<KBar> &bars, int window = 60)
{
    if (bars.isEmpty()) return 0.0;
    const int first = qMax(0, bars.size() - window);
    double value = bars[first].high;
    for (int i = first + 1; i < bars.size(); ++i) value = qMax(value, bars[i].high);
    return value;
}

double nearestSupport(const SectorSnapshot &sector, double current, double atr)
{
    QVector<double> candidates;
    const TechSignals &tech = sector.tech;
    if (tech.ma20 > 0.0) candidates << tech.ma20;
    if (tech.ma60 > 0.0) candidates << tech.ma60;
    if (tech.bollMid > 0.0) candidates << tech.bollMid;
    if (tech.bollLower > 0.0) candidates << tech.bollLower;
    const double low = recentLow(sector.dailyBars);
    if (low > 0.0) candidates << low;

    double support = 0.0;
    for (double candidate : candidates) {
        if (candidate <= current * 1.015) support = qMax(support, candidate);
    }
    if (support <= 0.0) support = current - atr * 2.0;
    return qMax(0.01, support);
}

double nearestResistance(const SectorSnapshot &sector, double current, double atr)
{
    QVector<double> candidates;
    const TechSignals &tech = sector.tech;
    if (tech.bollUpper > 0.0) candidates << tech.bollUpper;
    if (tech.ma20 > current) candidates << tech.ma20;
    if (tech.ma60 > current) candidates << tech.ma60;
    const double high = recentHigh(sector.dailyBars);
    if (high > 0.0) candidates << high;

    double resistance = 0.0;
    for (double candidate : candidates) {
        if (candidate >= current * 0.985) {
            resistance = resistance <= 0.0 ? candidate : qMin(resistance, candidate);
        }
    }
    if (resistance <= current) resistance = current + atr * 3.0;
    return resistance;
}

bool isOverheated(const SectorSnapshot &sector)
{
    return sector.tech.rsiOverbought
        || sector.tech.kdjOverbought
        || sector.tech.priceAboveUpper
        || sector.fiveDayMomentum >= 8.0
        || (sector.todayChangePctValid && sector.todayChangePct >= 4.0)
        || sector.crowdingIndex >= 75.0;
}

bool isDowntrend(const SectorSnapshot &sector, double current)
{
    const TechSignals &tech = sector.tech;
    return tech.maShortArrange
        || (tech.ma20 > 0.0 && tech.ma60 > 0.0 && current < tech.ma20 && tech.ma20 < tech.ma60)
        || sector.forecastScore <= -0.18
        || sector.twentyDayMomentum <= -8.0;
}

bool isUptrend(const SectorSnapshot &sector, double current)
{
    const TechSignals &tech = sector.tech;
    return tech.maLongArrange
        || (tech.ma20 > 0.0 && current > tech.ma20 && (tech.ma60 <= 0.0 || tech.ma20 >= tech.ma60))
        || sector.forecastScore >= 0.22;
}

void computeRiskReward(PriceLevelPlan &plan)
{
    const double entryMid = (plan.entryZoneLow + plan.entryZoneHigh) / 2.0;
    const double reward = plan.takeProfitLow - entryMid;
    const double risk = entryMid - plan.stopLossLevel;
    plan.riskRewardRatio = risk > 1e-6 ? qMax(0.0, reward / risk) : 0.0;
}

void fillCommonText(PriceLevelPlan &plan)
{
    plan.entryReason = QString::fromUtf8("观察区 %1-%2，接近支撑后再看量能和事件确认")
        .arg(money(plan.entryZoneLow), money(plan.entryZoneHigh));
    plan.exitReason = QString::fromUtf8("止盈/减仓区 %1-%2，接近压力位时分批兑现")
        .arg(money(plan.takeProfitLow), money(plan.takeProfitHigh));
    plan.invalidationReason = QString::fromUtf8("跌破 %1 且 2-3 个交易日无法收回，则点位计划失效")
        .arg(money(plan.stopLossLevel));
}

} // namespace

PriceLevelPlan PriceLevelAnalyzer::analyze(const SectorSnapshot &sector)
{
    PriceLevelPlan plan;
    plan.actionLabel = QString::fromUtf8("观察");
    plan.holdingHorizonLabel = QString::fromUtf8("短期观察");

    if (sector.dailyBars.size() < 20 || sector.dailyBars.last().close <= 0.0) {
        plan.summary = QString::fromUtf8("K 线不足，暂不生成明确买卖点位，仅保留观察。");
        return plan;
    }

    plan.valid = true;
    plan.currentPrice = sector.dailyBars.last().close;
    const double current = plan.currentPrice;
    const double atr = qMax(averageTrueRange(sector.dailyBars), current * 0.015);
    const double support = nearestSupport(sector, current, atr);
    const double resistance = nearestResistance(sector, current, atr);
    const bool overheated = isOverheated(sector);
    const bool downtrend = isDowntrend(sector, current);
    const bool uptrend = isUptrend(sector, current);

    if (downtrend) {
        plan.trendStateLabel = QString::fromUtf8("下降趋势");
        plan.actionLabel = sector.forecastScore <= -0.22
            ? QString::fromUtf8("逻辑失效") : QString::fromUtf8("风险预警");
        plan.holdingHorizonLabel = QString::fromUtf8("短期风险观察");
        plan.entryZoneHigh = qMin(current * 0.99, support + atr * 0.4);
        plan.entryZoneLow = qMax(0.01, plan.entryZoneHigh - atr * 1.2);
        plan.stopLossLevel = qMax(0.01, plan.entryZoneLow - atr);
        plan.takeProfitLow = qMax(current, qMin(resistance, current + atr * 1.2));
        plan.takeProfitHigh = qMax(plan.takeProfitLow, plan.takeProfitLow + atr);
        plan.riskRewardRatio = 0.8;
        plan.entryReason = QString::fromUtf8("下降趋势中不直接给积极买入，等待放量企稳和重新站回 MA20。");
        plan.exitReason = QString::fromUtf8("反弹到 %1-%2 附近优先减仓或复核。")
            .arg(money(plan.takeProfitLow), money(plan.takeProfitHigh));
        plan.invalidationReason = QString::fromUtf8("若继续跌破 %1，说明趋势仍未修复。")
            .arg(money(plan.stopLossLevel));
        plan.summary = QString::fromUtf8("下降趋势，先处理风险，不因短线反弹提前推荐。");
        return plan;
    }

    plan.entryZoneLow = qMax(0.01, support - atr * 0.6);
    plan.entryZoneHigh = qMin(current * 0.995, support + atr * 0.6);
    if (plan.entryZoneHigh < plan.entryZoneLow) plan.entryZoneHigh = plan.entryZoneLow + atr * 0.3;
    plan.stopLossLevel = qMax(0.01, plan.entryZoneLow - atr * 1.3);
    plan.takeProfitLow = qMax(current + atr * 2.4, resistance - atr * 0.3);
    plan.takeProfitHigh = qMax(plan.takeProfitLow + atr * 0.8, resistance + atr * 0.7);
    computeRiskReward(plan);
    fillCommonText(plan);

    if (overheated) {
        plan.trendStateLabel = QString::fromUtf8("过热拉升");
        plan.actionLabel = QString::fromUtf8("过热不追");
        plan.holdingHorizonLabel = QString::fromUtf8("短期观察");
        plan.summary = QString::fromUtf8("逻辑可能仍在，但价格偏离支撑，避免把涨幅当成追高理由。");
        plan.entryReason = QString::fromUtf8("等待回踩 %1-%2 后再观察，当前不适合直接追高。")
            .arg(money(plan.entryZoneLow), money(plan.entryZoneHigh));
        return plan;
    }

    if (uptrend) {
        plan.trendStateLabel = QString::fromUtf8("上升趋势");
        plan.holdingHorizonLabel = sector.twentyDayMomentum >= 5.0 || sector.eventCatalystScore > 0.15
            ? QString::fromUtf8("中期跟踪") : QString::fromUtf8("短期到中期");
        const bool nearEntry = current <= plan.entryZoneHigh * 1.03
            || (sector.todayChangePctValid && sector.todayChangePct <= 0.0);
        plan.actionLabel = nearEntry ? QString::fromUtf8("回调分批") : QString::fromUtf8("观察");
        plan.summary = nearEntry
            ? QString::fromUtf8("趋势仍在，价格接近观察区，可等待确认后分批。")
            : QString::fromUtf8("趋势向上但未到观察区，等待回踩或突破确认。");
        return plan;
    }

    plan.trendStateLabel = QString::fromUtf8("震荡区间");
    plan.actionLabel = QString::fromUtf8("观察");
    plan.holdingHorizonLabel = QString::fromUtf8("短期观察");
    plan.summary = QString::fromUtf8("区间震荡，优先在下沿观察、上沿减仓，不做趋势外推。");
    return plan;
}
