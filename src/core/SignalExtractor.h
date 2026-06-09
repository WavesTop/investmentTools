#pragma once

#include <QList>

#include "domain/IndustrySignal.h"
#include "domain/RawInsight.h"

class SignalExtractor
{
public:
    QList<IndustrySignal> extract(const QList<RawInsight> &rawData) const;
};
