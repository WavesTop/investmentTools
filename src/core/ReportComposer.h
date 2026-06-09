#pragma once

#include <QList>
#include <QString>

#include "domain/InvestmentAdvice.h"

class ReportComposer
{
public:
    QString compose(const QList<InvestmentAdvice> &advices) const;
};
