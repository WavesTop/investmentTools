#include "core/OverheatDetector.h"
#include <algorithm>
#include <cmath>

OverheatMetrics OverheatDetector::detect(const QVector<KBar> &bars,
                                          double newsSentiment, int newsCount,
                                          double crowdingIndex, double hotScore)
{
    OverheatMetrics m;
    const int n = bars.size();

    // 1. Sentiment overheat: news volume spike + strong positive sentiment
    m.sentimentOverheat = 0;
    if (newsCount > 15) m.sentimentOverheat += qMin(double(newsCount - 10) * 3.0, 40.0);
    if (newsSentiment > 0.3) m.sentimentOverheat += newsSentiment * 60.0;
    if (hotScore > 80) m.sentimentOverheat += (hotScore - 70) * 1.5;
    m.sentimentOverheat = qBound(0.0, m.sentimentOverheat, 100.0);
    if (m.sentimentOverheat > 60)
        m.warnings.append(QString::fromUtf8("新闻情绪过热，全市场关注度极高"));

    if (n < 20) {
        m.compositeOverheat = m.sentimentOverheat * 0.3;
        return m;
    }

    QVector<double> closes(n), volumes(n);
    for (int i = 0; i < n; ++i) {
        closes[i] = bars[i].close;
        volumes[i] = bars[i].volume;
    }
    const int last = n - 1;

    // 2. Volume climax: recent volume vs 30-day average
    double avgVol30 = 0;
    int cnt30 = qMin(30, n);
    for (int i = n - cnt30; i < n; ++i) avgVol30 += volumes[i];
    avgVol30 /= cnt30;
    double maxRecentVol = *std::max_element(volumes.begin() + qMax(n - 5, 0), volumes.end());
    double volClimax = (avgVol30 > 0) ? (maxRecentVol / avgVol30) : 1.0;
    m.volumeClimax = qBound(0.0, (volClimax - 1.0) * 30.0, 100.0);
    if (volClimax > 3.0)
        m.warnings.append(QString::fromUtf8("近期出现天量成交，可能是情绪高潮信号"));

    // 3. Volatility explosion
    double recentMaxChg = 0;
    for (int i = qMax(n - 5, 0); i < n; ++i)
        recentMaxChg = qMax(recentMaxChg, qAbs(bars[i].changePct));
    double avg20Chg = 0;
    int cnt20 = qMin(20, n - 1);
    for (int i = n - 1 - cnt20; i < n - 1; ++i)
        avg20Chg += qAbs(bars[i].changePct);
    avg20Chg /= qMax(cnt20, 1);
    double volExpRatio = (avg20Chg > 0.1) ? recentMaxChg / avg20Chg : 1.0;
    m.volatilityExplosion = qBound(0.0, (volExpRatio - 1.0) * 25.0, 100.0);
    if (volExpRatio > 3.0)
        m.warnings.append(QString::fromUtf8("波动率急剧放大，市场进入博弈化交易"));

    // 4. MA deviation: price far from MA60
    double ma60 = 0;
    if (n >= 60) {
        for (int i = last - 59; i <= last; ++i) ma60 += closes[i];
        ma60 /= 60.0;
    }
    double devPct = (ma60 > 0) ? (closes[last] - ma60) / ma60 * 100.0 : 0;
    m.maDeviation = qBound(0.0, qMax(devPct - 5.0, 0.0) * 4.0, 100.0);
    if (devPct > 20)
        m.warnings.append(QString::fromUtf8("价格严重偏离MA60，存在泡沫化风险"));

    // 5. Turnover risk (proxy from volume and price range)
    double avgTurnover = 0;
    for (int i = qMax(n - 5, 0); i < n; ++i) {
        double range = bars[i].high - bars[i].low;
        double mid = (bars[i].high + bars[i].low) / 2.0;
        avgTurnover += (mid > 0) ? range / mid * 100.0 : 0;
    }
    avgTurnover /= qMin(5, n);
    m.turnoverRisk = qBound(0.0, avgTurnover * 10.0, 100.0);

    // 6. Crowding risk (from pre-computed index)
    m.crowdingRisk = qBound(0.0, crowdingIndex, 100.0);
    if (crowdingIndex > 75)
        m.warnings.append(QString::fromUtf8("板块拥挤度过高，资金高度集中"));

    // Composite
    m.compositeOverheat = m.sentimentOverheat * 0.20
                        + m.volumeClimax * 0.20
                        + m.volatilityExplosion * 0.15
                        + m.maDeviation * 0.20
                        + m.turnoverRisk * 0.10
                        + m.crowdingRisk * 0.15;
    m.compositeOverheat = qBound(0.0, m.compositeOverheat, 100.0);

    if (m.compositeOverheat > 70 && m.warnings.isEmpty())
        m.warnings.append(QString::fromUtf8("综合过热指数偏高，注意风险"));

    return m;
}
