#pragma once

#include "domain/AnalysisResult.h"

class StrategyEngine
{
public:
    static TradingStrategy generate(const SectorSnapshot &snap);
};
