#pragma once

#include "domain/MacroEvent.h"

#include <QList>
#include <QStringList>

struct EventRule
{
    QString key;
    MacroEventType type = MacroEventType::Unknown;
    MacroEventRegion region = MacroEventRegion::Unknown;
    QStringList requiredAny;
    QStringList contextAny;
    QString checkpoint;
    double confidence = 0.7;
};

class EventRuleBook
{
public:
    EventRuleBook();

    QList<EventRule> matchingRules(const QString &text) const;
    MacroEventState resolveState(const QString &text) const;
    QList<MacroEventCheckpoint> resolveCheckpoints(const QString &text,
                                                   MacroEventType type,
                                                   MacroEventRegion region) const;

private:
    QList<EventRule> m_rules;
};
