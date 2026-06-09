#pragma once

#include <QList>
#include <memory>
#include <vector>

#include "domain/RawInsight.h"
#include "providers/IDataProvider.h"

class DataCollector
{
public:
    void addProvider(std::unique_ptr<IDataProvider> provider);
    QList<RawInsight> collectAll() const;
    const std::vector<std::unique_ptr<IDataProvider>> &providers() const { return m_providers; }

private:
    std::vector<std::unique_ptr<IDataProvider>> m_providers;
};
