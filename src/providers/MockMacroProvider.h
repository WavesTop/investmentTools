#pragma once

#include "providers/IDataProvider.h"

class MockMacroProvider : public IDataProvider
{
public:
    QList<RawInsight> fetchInsights() override;
    QString providerName() const override;
};
