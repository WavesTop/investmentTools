#include "core/TechIndicators.h"
#include <QtMath>

QVector<double> TechIndicators::ema(const QVector<double> &data, int period)
{
    QVector<double> result(data.size(), 0.0);
    if (data.isEmpty() || period <= 0) return result;
    const double k = 2.0 / (period + 1.0);
    result[0] = data[0];
    for (int i = 1; i < data.size(); ++i)
        result[i] = data[i] * k + result[i - 1] * (1.0 - k);
    return result;
}

QVector<double> TechIndicators::sma(const QVector<double> &data, int period)
{
    QVector<double> result(data.size(), 0.0);
    if (data.isEmpty() || period <= 0) return result;
    double sum = 0;
    for (int i = 0; i < data.size(); ++i) {
        sum += data[i];
        if (i >= period) sum -= data[i - period];
        const int cnt = qMin(i + 1, period);
        result[i] = sum / cnt;
    }
    return result;
}

MACDResult TechIndicators::calcMACD(const QVector<KBar> &bars, int fast, int slow, int signal)
{
    MACDResult r;
    if (bars.size() < 2) return r;
    QVector<double> closes;
    closes.reserve(bars.size());
    for (const KBar &b : bars) closes.push_back(b.close);

    const QVector<double> emaFast = ema(closes, fast);
    const QVector<double> emaSlow = ema(closes, slow);
    r.dif.resize(closes.size());
    for (int i = 0; i < closes.size(); ++i)
        r.dif[i] = emaFast[i] - emaSlow[i];
    r.dea = ema(r.dif, signal);
    r.hist.resize(closes.size());
    for (int i = 0; i < closes.size(); ++i)
        r.hist[i] = 2.0 * (r.dif[i] - r.dea[i]);
    return r;
}

RSIResult TechIndicators::calcRSI(const QVector<KBar> &bars)
{
    RSIResult r;
    const int n = bars.size();
    if (n < 2) return r;

    auto calcOne = [&](int period) -> QVector<double> {
        QVector<double> result(n, 50.0);
        double avgGain = 0, avgLoss = 0;
        for (int i = 1; i < n; ++i) {
            double change = bars[i].close - bars[i - 1].close;
            double gain = change > 0 ? change : 0;
            double loss = change < 0 ? -change : 0;
            if (i < period) {
                avgGain += gain;
                avgLoss += loss;
                if (i == period - 1) {
                    avgGain /= period;
                    avgLoss /= period;
                    double rs = avgLoss > 1e-12 ? avgGain / avgLoss : 100.0;
                    result[i] = 100.0 - 100.0 / (1.0 + rs);
                }
            } else {
                avgGain = (avgGain * (period - 1) + gain) / period;
                avgLoss = (avgLoss * (period - 1) + loss) / period;
                double rs = avgLoss > 1e-12 ? avgGain / avgLoss : 100.0;
                result[i] = 100.0 - 100.0 / (1.0 + rs);
            }
        }
        return result;
    };

    r.rsi6  = calcOne(6);
    r.rsi12 = calcOne(12);
    r.rsi24 = calcOne(24);
    return r;
}

KDJResult TechIndicators::calcKDJ(const QVector<KBar> &bars, int n, int m1, int m2)
{
    KDJResult r;
    const int sz = bars.size();
    if (sz < n) return r;
    r.k.resize(sz); std::fill(r.k.begin(), r.k.end(), 50.0);
    r.d.resize(sz); std::fill(r.d.begin(), r.d.end(), 50.0);
    r.j.resize(sz); std::fill(r.j.begin(), r.j.end(), 50.0);
    double prevK = 50.0, prevD = 50.0;
    for (int i = n - 1; i < sz; ++i) {
        double highest = bars[i].high, lowest = bars[i].low;
        for (int j = i - n + 1; j < i; ++j) {
            if (bars[j].high > highest) highest = bars[j].high;
            if (bars[j].low < lowest) lowest = bars[j].low;
        }
        double rsv = (highest - lowest) > 1e-12
            ? (bars[i].close - lowest) / (highest - lowest) * 100.0
            : 50.0;
        double K = (prevK * (m1 - 1) + rsv) / m1;
        double D = (prevD * (m2 - 1) + K) / m2;
        double J = 3.0 * K - 2.0 * D;
        r.k[i] = K; r.d[i] = D; r.j[i] = J;
        prevK = K; prevD = D;
    }
    return r;
}

MAResult TechIndicators::calcMA(const QVector<KBar> &bars)
{
    MAResult r;
    QVector<double> closes;
    closes.reserve(bars.size());
    for (const KBar &b : bars) closes.push_back(b.close);
    r.ma5  = sma(closes, 5);
    r.ma10 = sma(closes, 10);
    r.ma20 = sma(closes, 20);
    r.ma60 = sma(closes, 60);
    return r;
}

BOLLResult TechIndicators::calcBOLL(const QVector<KBar> &bars, int period, double mult)
{
    BOLLResult r;
    QVector<double> closes;
    closes.reserve(bars.size());
    for (const KBar &b : bars) closes.push_back(b.close);
    r.mid = sma(closes, period);
    r.upper.resize(closes.size());
    r.lower.resize(closes.size());
    for (int i = 0; i < closes.size(); ++i) {
        int cnt = qMin(i + 1, period);
        double sumSq = 0;
        for (int j = i - cnt + 1; j <= i; ++j) {
            double diff = closes[j] - r.mid[i];
            sumSq += diff * diff;
        }
        double sd = qSqrt(sumSq / cnt);
        r.upper[i] = r.mid[i] + mult * sd;
        r.lower[i] = r.mid[i] - mult * sd;
    }
    return r;
}

QVector<double> TechIndicators::calcVolMA(const QVector<KBar> &bars, int period)
{
    QVector<double> vols;
    vols.reserve(bars.size());
    for (const KBar &b : bars) vols.push_back(b.volume);
    return sma(vols, period);
}
