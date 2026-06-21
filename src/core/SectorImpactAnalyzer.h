#pragma once

#include "domain/MacroEvent.h"

#include <QMap>

class SectorImpactAnalyzer
{
public:
    QMap<QString, double> eventCatalystScores(const QList<SectorEventImpact> &impacts) const;

private:
    double stateWeight(MacroEventState state) const;
    double directionWeight(EventImpactDirection direction) const;
};
