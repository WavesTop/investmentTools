#include "core/StrategyEngine.h"
#include <QDebug>
#include <QtMath>

namespace {

QString buildShortTerm(const TechSignals &t, double changePct)
{
    QStringList pts;
    if (t.macdGoldenCross) pts << "MACD金叉形成，短期动能偏多";
    else if (t.macdDeadCross) pts << "MACD死叉形成，短期动能偏空";
    else if (t.macdHist > 0) pts << "MACD柱线为正，多方占优";
    else pts << "MACD柱线为负，空方占优";

    if (t.rsiOverbought) pts << "RSI超买(>80)，短期回调风险较大";
    else if (t.rsiOversold) pts << "RSI超卖(<20)，短期反弹概率较高";

    if (t.kdjOverbought) pts << "KDJ-J超买(>100)，注意高位风险";
    else if (t.kdjOversold) pts << "KDJ-J超卖(<0)，可能见底反弹";

    if (t.volExpansion) pts << "成交量放大，市场关注度提升";
    else if (t.volShrink) pts << "成交量萎缩，观望情绪浓厚";

    if (pts.isEmpty()) pts << "短期技术面信号中性，建议观望";
    return pts.join("；") + "。";
}

QString buildMediumTerm(const TechSignals &t, double weekMom, double forecastScore)
{
    QStringList pts;
    if (t.maLongArrange)
        pts << "均线多头排列(MA5>MA10>MA20>MA60)，中期趋势向好";
    else if (t.maShortArrange)
        pts << "均线空头排列，中期趋势偏弱";
    else
        pts << "均线交织，中期方向不明，需等待突破";

    if (t.priceBelowLower)
        pts << "价格跌破布林带下轨，超跌特征明显";
    else if (t.priceAboveUpper)
        pts << "价格突破布林带上轨，上涨动能较强但需警惕回落";

    if (weekMom > 3.0) pts << QString("周级别动量 %1%，中期上行趋势确立").arg(weekMom, 0, 'f', 1);
    else if (weekMom < -3.0) pts << QString("周级别动量 %1%，中期下行压力明显").arg(weekMom, 0, 'f', 1);

    if (pts.isEmpty()) pts << "中期趋势尚未明确，建议关注均线排列变化";
    return pts.join("；") + "。";
}

QString buildLongTerm(double monthMom, double forecastScore, double fundFlowFactor)
{
    QStringList pts;
    if (monthMom > 5.0)
        pts << QString("月级别动量 %1%，长期趋势稳健向上").arg(monthMom, 0, 'f', 1);
    else if (monthMom < -5.0)
        pts << QString("月级别动量 %1%，长期处于下行通道").arg(monthMom, 0, 'f', 1);
    else
        pts << "月级别动量表现平稳，长期趋势中性";

    if (fundFlowFactor > 0.1) pts << "近期主力资金持续流入，长期配置价值凸显";
    else if (fundFlowFactor < -0.1) pts << "近期主力资金持续流出，长期需谨慎";

    if (forecastScore > 0.3) pts << "综合预测偏乐观，可逢低布局";
    else if (forecastScore < -0.3) pts << "综合预测偏悲观，建议等待企稳信号";

    if (pts.isEmpty()) pts << "长期维度缺乏明确信号，建议以定投策略参与";
    return pts.join("；") + "。";
}

// 将 AdviceAction 映射为策略标签，确保与 action 一致
QString actionToStrategyLabel(AdviceAction action, int techNet)
{
    switch (action) {
    case AdviceAction::Increase:
        return techNet >= 4 ? "建议买入" : "谨慎增配";
    case AdviceAction::Decrease:
        return techNet <= -4 ? "建议止损" : "建议减仓";
    case AdviceAction::Hold:
    default:
        if (techNet >= 2) return "持有偏多";
        if (techNet <= -2) return "持有偏空";
        return "持有观望";
    }
}

} // namespace

TradingStrategy StrategyEngine::generate(const SectorSnapshot &snap)
{
    TradingStrategy s;
    const TechSignals &t = snap.tech;
    const double score = snap.forecastScore;

    // 技术面打分（仅用于辅助细分策略标签和生成建议文字）
    int bullPoints = 0, bearPoints = 0;
    if (t.macdGoldenCross) ++bullPoints;
    if (t.macdDeadCross) ++bearPoints;
    if (t.macdHist > 0) ++bullPoints; else if (t.macdHist < 0) ++bearPoints;
    if (t.rsiOversold) ++bullPoints;
    if (t.rsiOverbought) ++bearPoints;
    if (t.kdjGoldenCross) ++bullPoints;
    if (t.kdjOverbought) ++bearPoints;
    if (t.kdjOversold) ++bullPoints;
    if (t.maLongArrange) bullPoints += 2;
    if (t.maShortArrange) bearPoints += 2;
    if (t.priceAboveUpper) { ++bullPoints; ++bearPoints; }
    if (t.priceBelowLower) { ++bullPoints; ++bearPoints; }
    if (t.volExpansion && snap.todayChangePct > 0) ++bullPoints;
    if (t.volExpansion && snap.todayChangePct < 0) ++bearPoints;
    if (snap.fundFlowFactor > 0.1) ++bullPoints;
    if (snap.fundFlowFactor < -0.1) ++bearPoints;

    int techNet = bullPoints - bearPoints;

    // actionLabel 以 snap.action（综合模型的统一决策）为基准，
    // 用技术面 techNet 做强度细分
    s.actionLabel = actionToStrategyLabel(snap.action, techNet);

    s.shortTermView  = buildShortTerm(t, snap.todayChangePct);
    s.mediumTermView = buildMediumTerm(t, snap.weekMomentum, score);
    s.longTermView   = buildLongTerm(snap.monthMomentum, score, snap.fundFlowFactor);

    double lastClose = 0;
    if (!snap.dailyBars.isEmpty()) lastClose = snap.dailyBars.last().close;

    if (lastClose > 1e-6) {
        s.supportLevel1 = qMin(t.ma20, t.bollLower);
        s.supportLevel2 = qMin(t.ma60, t.bollLower * 0.98);
        s.resistLevel1  = qMax(t.ma20, t.bollUpper);
        s.resistLevel2  = qMax(t.ma60, t.bollUpper * 1.02);

        if (s.supportLevel1 <= 0) s.supportLevel1 = lastClose * 0.95;
        if (s.supportLevel2 <= 0) s.supportLevel2 = lastClose * 0.90;
        if (s.resistLevel1 <= 0)  s.resistLevel1  = lastClose * 1.05;
        if (s.resistLevel2 <= 0)  s.resistLevel2  = lastClose * 1.10;
    }

    // ATR(14) based volatility-aware stop/take-profit
    double atr14Pct = 0;
    if (snap.dailyBars.size() >= 15 && lastClose > 1e-6) {
        double atrSum = 0;
        for (int bi = snap.dailyBars.size() - 14; bi < snap.dailyBars.size(); ++bi) {
            const KBar &cur = snap.dailyBars[bi];
            const KBar &prev = snap.dailyBars[bi - 1];
            double tr = qMax(cur.high - cur.low,
                        qMax(qAbs(cur.high - prev.close), qAbs(cur.low - prev.close)));
            atrSum += tr;
        }
        atr14Pct = (atrSum / 14.0) / lastClose * 100.0;
    }

    double distToSupport = (lastClose > 1e-6 && s.supportLevel1 > 1e-6)
        ? (s.supportLevel1 - lastClose) / lastClose * 100.0 : -5.0;
    double distToResist = (lastClose > 1e-6 && s.resistLevel1 > 1e-6)
        ? (s.resistLevel1 - lastClose) / lastClose * 100.0 : 8.0;

    if (atr14Pct > 0.5) {
        double atrStop = -(atr14Pct * 2.0);
        double atrTake = atr14Pct * 2.5;
        s.stopLossPct = qMax(qMin(distToSupport - 1.0, atrStop), -12.0);
        s.stopLossPct = qMin(s.stopLossPct, -1.5);
        s.takeProfitPct = qMin(qMax(distToResist + 1.0, atrTake), 20.0);
        s.takeProfitPct = qMax(s.takeProfitPct, 2.0);
    } else {
        s.stopLossPct = qMax(distToSupport - 2.0, -8.0);
        s.stopLossPct = qMin(s.stopLossPct, -3.0);
        s.takeProfitPct = qMin(distToResist + 2.0, 15.0);
        s.takeProfitPct = qMax(s.takeProfitPct, 3.0);
    }

    QString methodNote = atr14Pct > 0.5
        ? QString("ATR(14)=%1%").arg(atr14Pct, 0, 'f', 2)
        : QString::fromUtf8("支撑压力法");
    s.stopLossReason = QString("%1 支撑位 %2 → 止损 %3%")
        .arg(methodNote).arg(s.supportLevel1, 0, 'f', 2).arg(s.stopLossPct, 0, 'f', 1);
    s.takeProfitReason = QString("%1 压力位 %2 → 止盈 +%3%")
        .arg(methodNote).arg(s.resistLevel1, 0, 'f', 2).arg(s.takeProfitPct, 0, 'f', 1);

    QStringList advice;
    advice << QString("【操作方向】%1").arg(s.actionLabel);

    // 根据统一的 action 生成一致的建议文字
    switch (snap.action) {
    case AdviceAction::Increase:
        if (techNet >= 4) {
            advice << "技术面多项指标共振看多，综合预测评分偏强，建议积极参与";
            if (t.volExpansion) advice << "放量上涨进一步确认多头信号，可适当加大仓位";
        } else {
            advice << "综合预测评分偏多，但技术面信号强度一般，建议小仓位谨慎增配";
        }
        break;
    case AdviceAction::Decrease:
        if (techNet <= -4) {
            advice << "技术面多项指标发出警告，综合预测偏空，建议果断止损离场";
            if (t.maShortArrange) advice << "均线空头排列，下跌趋势已确立，不宜抄底";
        } else {
            advice << "综合预测评分偏空，建议逐步减仓控制风险";
        }
        break;
    case AdviceAction::Hold:
    default:
        if (techNet >= 2)
            advice << "综合预测中性但技术面偏多，可小仓位关注，等待更强确认信号";
        else if (techNet <= -2)
            advice << "综合预测中性但技术面偏弱，建议观望为主，不宜追涨";
        else
            advice << "多空分歧较大，综合预测和技术面均无明确方向，建议暂时观望";
        break;
    }

    if (snap.fundFlowFactor > 0.1) advice << "主力资金持续流入，为做多提供支撑";
    else if (snap.fundFlowFactor < -0.1) advice << "主力资金持续流出，反弹空间受限";

    advice << QString("止盈目标 +%1%，止损线 %2%").arg(s.takeProfitPct, 0, 'f', 1).arg(s.stopLossPct, 0, 'f', 1);

    s.operationAdvice = advice.join("。") + "。";
    return s;
}
