#pragma once

#include "core/EventRuleBook.h"
#include "domain/MacroEvent.h"
#include "providers/RealFinanceNewsProvider.h"

#include <QList>

class EventExtractionEngine
{
public:
    QList<MacroEvent> extractFromHeadlines(const QList<RawHeadline> &headlines) const;
    QList<MacroEvent> extractFromText(const QString &text,
                                      const QString &source = QString(),
                                      const QDateTime &publishedAt = QDateTime()) const;

private:
    EventRuleBook m_ruleBook;
};
