#pragma once

#include "domain/MacroEvent.h"

#include <QDateTime>
#include <QMap>

class SectorImpactAnalyzer
{
public:
    QMap<QString, double> eventCatalystScores(const QList<SectorEventImpact> &impacts) const;
    QMap<QString, double> eventCatalystScores(const QList<SectorEventImpact> &impacts,
                                             const QDateTime &now) const;
    double scoreImpact(const SectorEventImpact &impact, const QDateTime &now) const;

private:
    double stateWeight(MacroEventState state) const;
    double directionWeight(EventImpactDirection direction) const;
    double reliabilityWeight(const SectorEventImpact &impact) const;
    double noveltyWeight(const SectorEventImpact &impact) const;
    double timeDecayWeight(const SectorEventImpact &impact, const QDateTime &now) const;
};
