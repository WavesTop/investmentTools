#pragma once

#include "domain/MacroEvent.h"

#include <QList>
#include <QStringList>

class ImpactGraphEngine
{
public:
    QList<SectorEventImpact> analyze(const MacroEvent &event,
                                     const QStringList &sectorPool = QStringList()) const;
};
