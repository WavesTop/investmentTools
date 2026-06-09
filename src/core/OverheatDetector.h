#pragma once

#include <QVector>
#include "core/SectorFetcher.h"

struct OverheatMetrics {
    double sentimentOverheat = 0.0;     // 新闻情绪过热 [0-100]
    double volumeClimax = 0.0;          // 成交量高潮 [0-100]
    double volatilityExplosion = 0.0;   // 波动率爆发 [0-100]
    double maDeviation = 0.0;           // 均线偏离度 [0-100]
    double turnoverRisk = 0.0;          // 换手率风险 [0-100]
    double crowdingRisk = 0.0;          // 拥挤度风险 [0-100]
    double compositeOverheat = 0.0;     // 综合过热指数 [0-100]
    QStringList warnings;               // 风险警告列表
};

class OverheatDetector {
public:
    static OverheatMetrics detect(const QVector<KBar> &bars,
                                   double newsSentiment, int newsCount,
                                   double crowdingIndex, double hotScore);
};
