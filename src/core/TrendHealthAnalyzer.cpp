#include "core/TrendHealthAnalyzer.h"
#include <algorithm>
#include <cmath>

namespace {
double sma(const QVector<double> &v, int end, int period) {
    if (end < period - 1 || v.isEmpty()) return 0;
    double s = 0;
    for (int i = end - period + 1; i <= end; ++i) s += v[i];
    return s / period;
}
} // namespace

TrendHealth TrendHealthAnalyzer::analyze(const QVector<KBar> &bars)
{
    TrendHealth h;
    const int n = bars.size();
    if (n < 60) {
        h.sustainability = 30.0;
        return h;
    }

    QVector<double> closes(n), volumes(n);
    for (int i = 0; i < n; ++i) {
        closes[i] = bars[i].close;
        volumes[i] = bars[i].volume;
    }
    const int last = n - 1;

    // 1. Structure score: MA alignment quality
    const double ma5 = sma(closes, last, 5);
    const double ma20 = sma(closes, last, 20);
    const double ma60 = sma(closes, last, 60);
    double structS = 0;
    if (ma5 > ma20 && ma20 > ma60) structS = 80;
    else if (ma5 > ma20) structS = 55;
    else if (ma5 < ma20 && ma20 < ma60) structS = 15;
    else structS = 40;

    double ma20now = sma(closes, last, 20);
    double ma20prev = sma(closes, qMax(last - 10, 19), 20);
    double slope = (ma20prev > 0) ? (ma20now - ma20prev) / ma20prev * 100.0 : 0;
    if (slope > 1.0) structS += 15;
    else if (slope > 0.3) structS += 8;
    else if (slope < -0.5) structS -= 15;
    h.structureScore = qBound(0.0, structS, 100.0);

    // 2. Momentum quality: consistency of returns
    int upDays = 0, totalDays = qMin(20, n - 1);
    double sumPosMom = 0, sumNegMom = 0;
    for (int i = last; i > last - totalDays && i > 0; --i) {
        double chg = bars[i].changePct;
        if (chg > 0) { ++upDays; sumPosMom += chg; }
        else sumNegMom += qAbs(chg);
    }
    double winRatio = double(upDays) / qMax(totalDays, 1);
    double momBalance = (sumPosMom + sumNegMom > 0) ? sumPosMom / (sumPosMom + sumNegMom) : 0.5;
    h.momentumQuality = qBound(0.0, (winRatio * 50.0 + momBalance * 50.0), 100.0);

    // 3. Pullback quality
    int pullbacks = 0, healthyPb = 0;
    for (int i = qMax(n - 30, 1); i < n; ++i) {
        if (bars[i].changePct < -1.5) {
            ++pullbacks;
            double localMA20 = sma(closes, i, qMin(i + 1, 20));
            if (closes[i] >= localMA20 * 0.97) ++healthyPb;
        }
    }
    if (pullbacks == 0)
        h.pullbackQuality = 60.0; // no pullbacks: ambiguous
    else
        h.pullbackQuality = qBound(0.0, double(healthyPb) / pullbacks * 100.0, 100.0);

    // 4. Volume health: price-volume agreement
    double volH = 50.0;
    int priceUpVolUp = 0, priceUpVolDown = 0, priceDownVolUp = 0;
    for (int i = qMax(n - 15, 1); i < n; ++i) {
        double avgVol = sma(volumes, qMax(i - 1, 4), 5);
        bool volUp = (avgVol > 0) && (volumes[i] > avgVol * 1.1);
        bool volDown = (avgVol > 0) && (volumes[i] < avgVol * 0.8);
        if (bars[i].changePct > 0.5 && volUp) ++priceUpVolUp;
        if (bars[i].changePct > 0.5 && volDown) ++priceUpVolDown;
        if (bars[i].changePct < -0.5 && volUp) ++priceDownVolUp;
    }
    volH += priceUpVolUp * 6;
    volH -= priceUpVolDown * 8;
    volH -= priceDownVolUp * 10;
    h.volumeHealth = qBound(0.0, volH, 100.0);

    // 5. Volatility health: moderate is healthy, extremes are not
    QVector<double> returns(n - 1);
    for (int i = 1; i < n; ++i) returns[i - 1] = bars[i].changePct;
    double recentStd = 0, baseStd = 0;
    {
        int rn = qMin(10, returns.size());
        double sum = 0, sum2 = 0;
        for (int i = returns.size() - rn; i < returns.size(); ++i) {
            sum += returns[i]; sum2 += returns[i] * returns[i];
        }
        double m = sum / rn;
        recentStd = std::sqrt(sum2 / rn - m * m);
    }
    {
        int bn = qMin(30, returns.size());
        double sum = 0, sum2 = 0;
        for (int i = returns.size() - bn; i < returns.size(); ++i) {
            sum += returns[i]; sum2 += returns[i] * returns[i];
        }
        double m = sum / bn;
        baseStd = std::sqrt(qMax(sum2 / bn - m * m, 0.0));
    }
    double volRatio = (baseStd > 0.1) ? recentStd / baseStd : 1.0;
    if (volRatio < 0.7) h.volatilityHealth = 50; // too quiet
    else if (volRatio < 1.3) h.volatilityHealth = 85; // healthy
    else if (volRatio < 2.0) h.volatilityHealth = 55; // elevated
    else h.volatilityHealth = 25; // dangerous

    // 6. Sustainability composite
    h.sustainability = h.structureScore * 0.25
                     + h.momentumQuality * 0.20
                     + h.pullbackQuality * 0.20
                     + h.volumeHealth * 0.20
                     + h.volatilityHealth * 0.15;

    return h;
}
