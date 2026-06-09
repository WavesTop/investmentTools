#pragma once

#include <QVector>

struct FlowStructure {
    double continuousInflowDays = 0;    // 连续净流入天数
    double speculativeFlowRisk = 0.0;   // 游资/投机资金风险 [0-100]
    double institutionalScore = 0.0;    // 机构资金特征评分 [0-100]
    double flowMomentum = 0.0;          // 资金流动量 [-100, +100]
    double flowAcceleration = 0.0;      // 资金加速度
    QString flowPattern;                // "机构吸筹" / "情绪高潮" / "持续流出" / "中性"
};

class FlowStructureAnalyzer {
public:
    static FlowStructure analyze(const QVector<double> &flowSeries,
                                  double todayFlow, double todayChangePct);
};
