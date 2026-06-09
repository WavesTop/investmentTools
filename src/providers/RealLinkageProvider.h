#pragma once

#include "providers/IDataProvider.h"

class RealLinkageProvider : public IDataProvider
{
public:
    QList<RawInsight> fetchInsights() override;
    QString providerName() const override;
};
