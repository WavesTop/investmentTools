#pragma once

#include <QVector>
#include "core/SectorFetcher.h"

struct BreadthMetrics {
    double advancingRatio = 0.0;     // 板块内上涨个股比例 (由API提供或估算)
    double ma20AboveRatio = 0.0;     // 站上MA20比例 (估算)
    double ma60AboveRatio = 0.0;     // 站上MA60比例 (估算)
    double newHighRatio = 0.0;       // 创新高比例
    double limitUpRatio = 0.0;       // 涨停比例
    double divergenceScore = 0.0;    // 龙头与板块背离程度 [0-100, higher = more divergent]
    double breadthHealth = 0.0;      // 综合宽度健康度 [0-100]
};

class BreadthAnalyzer {
public:
    static BreadthMetrics analyze(const QVector<KBar> &bars,
                                  double todayChangePct,
                                  int upCount, int totalCount);
};
