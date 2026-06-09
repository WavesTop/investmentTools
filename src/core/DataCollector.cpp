#include "core/DataCollector.h"

void DataCollector::addProvider(std::unique_ptr<IDataProvider> provider)
{
    if (provider) {
        m_providers.push_back(std::move(provider));
    }
}

QList<RawInsight> DataCollector::collectAll() const
{
    QList<RawInsight> all;
    for (const auto &provider : m_providers) {
        const QList<RawInsight> fromProvider = provider->fetchInsights();
        for (const auto &item : fromProvider) {
            all.push_back(item);
        }
    }
    return all;
}
