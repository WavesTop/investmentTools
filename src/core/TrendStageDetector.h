#pragma once

#include <QString>
#include <QVector>
#include "core/SectorFetcher.h"

enum class TrendStage {
    BottomAccumulation,   // 底部吸筹
    EarlyBreakout,        // 初期突破
    MainUptrend,          // 主升阶段
    Acceleration,         // 加速冲顶
    Distribution,         // 高位派发
    Downtrend,            // 下跌趋势
    Sideways              // 横盘整理
};

struct TrendStageResult {
    TrendStage stage = TrendStage::Sideways;
    QString stageName;
    QString stageAdvice;
    double stageConfidence = 0.0;

    double distFromMA60 = 0.0;
    double distFromYearLow = 0.0;
    double distFromYearHigh = 0.0;
    double volatilityExpansion = 0.0;
    double volumeTrendScore = 0.0;
    double distributionRisk = 0.0;
    double pullbackHealth = 0.0;
};

class TrendStageDetector {
public:
    static TrendStageResult detect(const QVector<KBar> &bars);
    static QString stageName(TrendStage s);
};
