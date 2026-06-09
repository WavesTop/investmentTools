#pragma once

#include <QVector>
#include "core/SectorFetcher.h"

struct TrendHealth {
    double structureScore = 0.0;     // MA排列与斜率
    double momentumQuality = 0.0;    // 动量连续性与加速度
    double pullbackQuality = 0.0;    // 回撤健康度
    double volumeHealth = 0.0;       // 量价配合度
    double volatilityHealth = 0.0;   // 波动率合理性
    double sustainability = 0.0;     // 综合可持续性评分 [0-100]
};

class TrendHealthAnalyzer {
public:
    static TrendHealth analyze(const QVector<KBar> &bars);
};
