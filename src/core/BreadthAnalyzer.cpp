#include "core/BreadthAnalyzer.h"
#include <algorithm>
#include <cmath>

BreadthMetrics BreadthAnalyzer::analyze(const QVector<KBar> &bars,
                                         double todayChangePct,
                                         int upCount, int totalCount)
{
    BreadthMetrics m;
    const int n = bars.size();

    // Advancing ratio from API-provided counts
    if (totalCount > 0 && upCount >= 0) {
        m.advancingRatio = double(upCount) / totalCount * 100.0;
    } else {
        // Estimate from recent bar pattern: if sector up, guess ~55-70%; if down, ~30-45%
        m.advancingRatio = qBound(10.0, 50.0 + todayChangePct * 8.0, 95.0);
    }

    if (n < 20) {
        m.breadthHealth = m.advancingRatio;
        return m;
    }

    // Estimate MA20/MA60 "above" ratios from price structure
    // Use proxy: how many of last N days closed above their own MA20-equivalent
    int above20 = 0, above60 = 0, newHighs = 0;
    double maxHigh = 0;
    for (int i = 0; i < n; ++i) {
        if (bars[i].high > maxHigh) maxHigh = bars[i].high;
    }

    for (int i = qMax(n - 20, 0); i < n; ++i) {
        double localMA20 = 0;
        int cnt = qMin(i + 1, 20);
        for (int j = i - cnt + 1; j <= i; ++j) localMA20 += bars[j].close;
        localMA20 /= cnt;

        if (bars[i].close > localMA20) ++above20;

        if (i >= 59) {
            double localMA60 = 0;
            for (int j = i - 59; j <= i; ++j) localMA60 += bars[j].close;
            localMA60 /= 60.0;
            if (bars[i].close > localMA60) ++above60;
        }

        if (bars[i].high >= maxHigh * 0.98) ++newHighs;
    }
    m.ma20AboveRatio = double(above20) / 20.0 * 100.0;
    int ma60Window = qMin(20, qMax(n - 60, 0) > 0 ? 20 : 0);
    m.ma60AboveRatio = (ma60Window > 0) ? double(above60) / ma60Window * 100.0 : 50.0;
    m.newHighRatio = double(newHighs) / 20.0 * 100.0;

    // Limit-up ratio estimate (if today's change > 9%, consider near-limit)
    m.limitUpRatio = (todayChangePct > 5.0) ? qMin(todayChangePct * 3.0, 30.0) : 0;

    // Divergence score: if sector index up a lot but breadth is narrow
    if (todayChangePct > 2.0 && m.advancingRatio < 50)
        m.divergenceScore = (2.0 - m.advancingRatio / 50.0) * 40.0 + todayChangePct * 5.0;
    else if (todayChangePct < -2.0 && m.advancingRatio > 60)
        m.divergenceScore = 30.0;
    else
        m.divergenceScore = qMax(0.0, 20.0 - m.advancingRatio * 0.2);

    // Composite breadth health
    m.breadthHealth = m.advancingRatio * 0.30
                    + m.ma20AboveRatio * 0.25
                    + m.ma60AboveRatio * 0.20
                    + (100.0 - m.divergenceScore) * 0.15
                    + m.newHighRatio * 0.10;
    m.breadthHealth = qBound(0.0, m.breadthHealth, 100.0);

    return m;
}
