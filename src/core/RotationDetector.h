#pragma once

#include <QList>
#include <QString>
#include <QVector>

#include "core/MarketContext.h"

struct SectorSnapshot;  // forward declaration

struct RotationSignal
{
    QString sector;
    double momentum5d = 0;
    double momentum20d = 0;
    double momentumDelta = 0;
    double fundFlowAccel = 0;
    double rotationScore = 0;
    bool isRotatingIn = false;
    bool isRotatingOut = false;
};

struct MarketRiskRadar
{
    // 六维风险雷达
    double valuationRisk = 50;   // 估值风险 [0,100]
    double momentumRisk = 50;    // 动量背离 [0,100]
    double breadthRisk = 50;     // 市场广度 [0,100]
    double capitalFlowRisk = 50; // 资金面 [0,100]
    double concentrationRisk = 50; // 集中度 [0,100]
    double volatilityRisk = 50;  // 波动性 [0,100]

    double compositeRisk = 50;   // 综合风险 [0,100]
    QString riskAdvice;
};

class RotationDetector
{
public:
    static QList<RotationSignal> detect(const QList<SectorSnapshot> &sectors);
    static MarketRiskRadar computeRisk(const QList<SectorSnapshot> &sectors,
                                       const MarketContext &ctx);
};
