#pragma once

#include "domain/AnalysisResult.h"

class PriceLevelAnalyzer
{
public:
    static PriceLevelPlan analyze(const SectorSnapshot &sector);
};
