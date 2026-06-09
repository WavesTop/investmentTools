#pragma once

#include <QList>

#include "domain/RawInsight.h"

class IDataProvider
{
public:
    virtual ~IDataProvider() = default;
    virtual QList<RawInsight> fetchInsights() = 0;
    virtual QString providerName() const = 0;
};
