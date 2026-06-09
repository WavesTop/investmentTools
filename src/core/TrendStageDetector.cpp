#include "core/TrendStageDetector.h"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <QDebug>

namespace {

double sma(const QVector<double> &v, int end, int period) {
    if (end < period - 1 || v.isEmpty()) return 0;
    double s = 0;
    for (int i = end - period + 1; i <= end; ++i) s += v[i];
    return s / period;
}

double stddev(const QVector<double> &v, int end, int period) {
    if (end < period - 1) return 0;
    double m = sma(v, end, period);
    double ss = 0;
    for (int i = end - period + 1; i <= end; ++i) {
        double d = v[i] - m;
        ss += d * d;
    }
    return std::sqrt(ss / period);
}

} // namespace

QString TrendStageDetector::stageName(TrendStage s)
{
    switch (s) {
    case TrendStage::BottomAccumulation: return QString::fromUtf8("底部吸筹");
    case TrendStage::EarlyBreakout:     return QString::fromUtf8("初期突破");
    case TrendStage::MainUptrend:       return QString::fromUtf8("主升阶段");
    case TrendStage::Acceleration:      return QString::fromUtf8("加速冲顶");
    case TrendStage::Distribution:      return QString::fromUtf8("高位派发");
    case TrendStage::Downtrend:         return QString::fromUtf8("下跌趋势");
    case TrendStage::Sideways:          return QString::fromUtf8("横盘整理");
    }
    return QString::fromUtf8("未知");
}

TrendStageResult TrendStageDetector::detect(const QVector<KBar> &bars)
{
    TrendStageResult r;
    const int n = bars.size();
    if (n < 60) {
        r.stage = TrendStage::Sideways;
        r.stageName = stageName(r.stage);
        r.stageAdvice = QString::fromUtf8("数据不足，无法判断趋势阶段");
        return r;
    }

    QVector<double> closes(n);
    QVector<double> volumes(n);
    for (int i = 0; i < n; ++i) {
        closes[i] = bars[i].close;
        volumes[i] = bars[i].volume;
    }

    const int last = n - 1;
    const double price = closes[last];
    const double ma5  = sma(closes, last, 5);
    const double ma20 = sma(closes, last, 20);
    const double ma60 = sma(closes, last, 60);

    // --- MA structure ---
    bool maLong = (ma5 > ma20) && (ma20 > ma60);
    bool maShort = (ma5 < ma20) && (ma20 < ma60);
    double ma20Slope = (sma(closes, last, 20) - sma(closes, last - 5, 20)) / qMax(sma(closes, last - 5, 20), 0.001) * 100.0;
    double ma60Slope = (sma(closes, last, 60) - sma(closes, qMax(last - 10, 59), 60)) / qMax(sma(closes, qMax(last - 10, 59), 60), 0.001) * 100.0;

    // --- Distance metrics ---
    r.distFromMA60 = (price - ma60) / qMax(ma60, 0.001) * 100.0;

    double yearLow = *std::min_element(closes.begin(), closes.end());
    double yearHigh = *std::max_element(closes.begin(), closes.end());
    r.distFromYearLow = (price - yearLow) / qMax(yearLow, 0.001) * 100.0;
    r.distFromYearHigh = (yearHigh - price) / qMax(yearHigh, 0.001) * 100.0;

    // --- Volatility expansion ---
    double recentVol = stddev(closes, last, 10);
    double baseVol = stddev(closes, qMax(last - 20, 19), 20);
    r.volatilityExpansion = (baseVol > 0.001) ? (recentVol / baseVol) : 1.0;

    // --- Volume trend score ---
    // Positive = healthy volume increase with price rise; negative = distribution
    double recentAvgVol = sma(volumes, last, 5);
    double baseAvgVol = sma(volumes, qMax(last - 10, 9), 10);
    double volRatio = (baseAvgVol > 0) ? (recentAvgVol / baseAvgVol) : 1.0;

    double recentReturn = (closes[last] - closes[qMax(last - 5, 0)]) / qMax(closes[qMax(last - 5, 0)], 0.001);
    if (recentReturn > 0 && volRatio > 1.2)
        r.volumeTrendScore = qMin(volRatio * 50.0, 100.0);
    else if (recentReturn > 0 && volRatio < 0.7)
        r.volumeTrendScore = -30.0; // shrinking volume on rise = late stage
    else if (recentReturn < -0.01 && volRatio > 1.5)
        r.volumeTrendScore = -60.0; // heavy selling
    else
        r.volumeTrendScore = 0.0;

    // --- Distribution risk ---
    // High volume + stagnant price = distribution
    int stagnantHighVol = 0;
    for (int i = qMax(n - 10, 1); i < n; ++i) {
        double chg = std::abs(bars[i].changePct);
        double vr = (baseAvgVol > 0) ? (volumes[i] / baseAvgVol) : 1.0;
        if (vr > 1.3 && chg < 1.0) ++stagnantHighVol;
    }
    r.distributionRisk = stagnantHighVol * 15.0;

    // --- Pullback health ---
    // Look at last 20 bars: count pullbacks that held MA20
    int pullbackCount = 0, healthyPullback = 0;
    for (int i = qMax(n - 20, 1); i < n; ++i) {
        if (bars[i].changePct < -1.0) {
            ++pullbackCount;
            if (closes[i] >= sma(closes, i, qMin(i + 1, 20)) * 0.98)
                ++healthyPullback;
        }
    }
    r.pullbackHealth = (pullbackCount > 0) ? (double(healthyPullback) / pullbackCount * 100.0) : 50.0;

    // --- Stage classification ---
    double positionInRange = (yearHigh > yearLow)
        ? (price - yearLow) / (yearHigh - yearLow) * 100.0
        : 50.0;

    double score = 0;
    // Accumulation indicators
    double accumScore = 0;
    if (positionInRange < 25) accumScore += 30;
    if (ma60Slope < 0.5 && ma60Slope > -1.0) accumScore += 20;
    if (r.volatilityExpansion < 0.8) accumScore += 20;
    if (volRatio > 1.1 && recentReturn > -0.005) accumScore += 15;

    // Breakout indicators
    double breakoutScore = 0;
    if (price > ma60 && closes[qMax(last - 5, 0)] <= ma60) breakoutScore += 35;
    if (maLong && ma20Slope > 0.3) breakoutScore += 25;
    if (volRatio > 1.5 && recentReturn > 0.02) breakoutScore += 20;
    if (positionInRange > 20 && positionInRange < 55) breakoutScore += 10;

    // Main uptrend indicators
    double mainScore = 0;
    if (maLong && ma20Slope > 0.2) mainScore += 25;
    if (r.distFromMA60 > 3 && r.distFromMA60 < 20) mainScore += 25;
    if (r.pullbackHealth > 60) mainScore += 20;
    if (r.volumeTrendScore > 20) mainScore += 15;
    if (positionInRange > 40 && positionInRange < 80) mainScore += 10;

    // Acceleration indicators
    double accelScore = 0;
    if (r.distFromMA60 > 15) accelScore += 25;
    if (r.volatilityExpansion > 1.5) accelScore += 25;
    if (ma5 > ma20 * 1.03) accelScore += 20;
    if (positionInRange > 80) accelScore += 15;

    // Distribution indicators
    double distScore = 0;
    if (r.distributionRisk > 30) distScore += 30;
    if (r.volumeTrendScore < -20) distScore += 25;
    if (positionInRange > 75 && ma20Slope < 0.1) distScore += 20;
    if (r.volatilityExpansion > 2.0) distScore += 15;

    // Downtrend indicators
    double downScore = 0;
    if (maShort) downScore += 30;
    if (ma20Slope < -0.3) downScore += 25;
    if (r.distFromMA60 < -5) downScore += 20;
    if (positionInRange < 30) downScore += 15;

    // Sideways indicators
    double sideScore = 0;
    if (std::abs(ma20Slope) < 0.15) sideScore += 30;
    if (r.volatilityExpansion < 1.0 && r.volatilityExpansion > 0.6) sideScore += 20;
    if (positionInRange > 30 && positionInRange < 70) sideScore += 15;

    struct { double score; TrendStage stage; } candidates[] = {
        {accumScore,    TrendStage::BottomAccumulation},
        {breakoutScore, TrendStage::EarlyBreakout},
        {mainScore,     TrendStage::MainUptrend},
        {accelScore,    TrendStage::Acceleration},
        {distScore,     TrendStage::Distribution},
        {downScore,     TrendStage::Downtrend},
        {sideScore,     TrendStage::Sideways},
    };

    double maxScore = 0;
    double totalScore = 0;
    for (auto &c : candidates) {
        totalScore += c.score;
        if (c.score > maxScore) {
            maxScore = c.score;
            r.stage = c.stage;
        }
    }
    r.stageConfidence = (totalScore > 0) ? (maxScore / totalScore * 100.0) : 0;
    r.stageName = stageName(r.stage);

    switch (r.stage) {
    case TrendStage::BottomAccumulation:
        r.stageAdvice = QString::fromUtf8("底部区域，可关注低吸机会。需等待放量突破确认信号后再加仓。");
        break;
    case TrendStage::EarlyBreakout:
        r.stageAdvice = QString::fromUtf8("初步突破，趋势有望启动。可适当试探建仓，突破确认后逐步加码。");
        break;
    case TrendStage::MainUptrend:
        r.stageAdvice = QString::fromUtf8("主升行情进行中，趋势结构健康。可持有并在回踩均线时加仓。");
        break;
    case TrendStage::Acceleration:
        r.stageAdvice = QString::fromUtf8("加速冲顶阶段，短期涨幅可观但风险加大。应逐步止盈，严格执行纪律。");
        break;
    case TrendStage::Distribution:
        r.stageAdvice = QString::fromUtf8("高位派发特征明显，主力可能在出货。建议减仓锁利，避免追高。");
        break;
    case TrendStage::Downtrend:
        r.stageAdvice = QString::fromUtf8("下跌趋势中，不宜抄底。等待趋势企稳信号后再考虑参与。");
        break;
    case TrendStage::Sideways:
        r.stageAdvice = QString::fromUtf8("横盘整理阶段，方向不明。可观望等待突破方向的确认。");
        break;
    }

    return r;
}
