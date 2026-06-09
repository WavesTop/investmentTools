#pragma once

#include <QVector>
#include "core/SectorFetcher.h"

struct MACDResult  { QVector<double> dif, dea, hist; };
struct RSIResult   { QVector<double> rsi6, rsi12, rsi24; };
struct KDJResult   { QVector<double> k, d, j; };
struct MAResult    { QVector<double> ma5, ma10, ma20, ma60; };
struct BOLLResult  { QVector<double> upper, mid, lower; };

class TechIndicators
{
public:
    static MACDResult  calcMACD(const QVector<KBar> &bars, int fast = 12, int slow = 26, int signal = 9);
    static RSIResult   calcRSI(const QVector<KBar> &bars);
    static KDJResult   calcKDJ(const QVector<KBar> &bars, int n = 9, int m1 = 3, int m2 = 3);
    static MAResult    calcMA(const QVector<KBar> &bars);
    static BOLLResult  calcBOLL(const QVector<KBar> &bars, int period = 20, double mult = 2.0);
    static QVector<double> calcVolMA(const QVector<KBar> &bars, int period);

private:
    static QVector<double> ema(const QVector<double> &data, int period);
    static QVector<double> sma(const QVector<double> &data, int period);
};
