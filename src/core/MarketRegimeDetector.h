#pragma once

#include "core/MarketContext.h"

enum class MarketRegime {
    RiskOn,           // 风险偏好上升
    RiskOff,          // 风险偏好下降
    BullMarket,       // 牛市
    BearMarket,       // 熊市
    HighVolatility,   // 高波动
    LowVolatility,    // 低波动
    RotationMarket    // 轮动行情
};

struct DynamicFactorWeights {
    double momentumWeight = 0.20;
    double valuationWeight = 0.15;
    double sentimentWeight = 0.15;
    double riskWeight = 0.15;
    double breadthWeight = 0.10;
    double sustainabilityWeight = 0.15;
    double flowWeight = 0.10;
};

struct MarketRegimeResult {
    MarketRegime regime = MarketRegime::RotationMarket;
    QString regimeName;
    double regimeConfidence = 0.0;
    DynamicFactorWeights weights;
};

class MarketRegimeDetector {
public:
    static MarketRegimeResult detect(const MarketContext &ctx);
    static QString regimeName(MarketRegime r);
};
