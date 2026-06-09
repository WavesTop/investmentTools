#pragma once

#include <QDateTime>
#include <QStringList>

#include "providers/IDataProvider.h"

struct RawHeadline
{
    QString source;
    QString title;
    QString summary;
    QDateTime timestamp;
};

class RealFinanceNewsProvider : public IDataProvider
{
public:
    void setSectorNames(const QStringList &sectorNames);
    QList<RawInsight> fetchInsights() override;
    QString providerName() const override;

    const QList<RawHeadline> &rawHeadlines() const { return m_rawHeadlines; }

private:
    QStringList m_sectorNames;
    QList<RawHeadline> m_rawHeadlines;
};
