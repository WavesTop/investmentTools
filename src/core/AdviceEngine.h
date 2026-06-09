#pragma once

#include <QList>

#include "domain/FundImpact.h"
#include "domain/InvestmentAdvice.h"

class AdviceEngine
{
public:
    QList<InvestmentAdvice> generate(const QList<FundImpact> &impacts) const;
};
