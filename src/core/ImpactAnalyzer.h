#pragma once

#include <QList>

#include "domain/FundImpact.h"
#include "domain/IndustrySignal.h"

class ImpactAnalyzer
{
public:
    QList<FundImpact> analyze(const QList<IndustrySignal> &industrySignals) const;
};
