#include "core/SectorImpactAnalyzer.h"

#include <QtGlobal>

QMap<QString, double> SectorImpactAnalyzer::eventCatalystScores(const QList<SectorEventImpact> &impacts) const
{
    QMap<QString, double> scores;
    for (const SectorEventImpact &impact : impacts) {
        const double contribution = directionWeight(impact.direction)
            * impact.strength
            * impact.confidence
            * stateWeight(impact.state);
        scores[impact.sector] = qBound(-1.0, scores.value(impact.sector) + contribution, 1.0);
    }
    return scores;
}

double SectorImpactAnalyzer::stateWeight(MacroEventState state) const
{
    switch (state) {
    case MacroEventState::Confirmed:
        return 0.86;
    case MacroEventState::Occurred:
        return 0.72;
    case MacroEventState::Scheduled:
        return 0.56;
    case MacroEventState::Revised:
        return 0.64;
    case MacroEventState::Rumor:
        return 0.28;
    case MacroEventState::Invalidated:
        return 0.0;
    case MacroEventState::Expected:
    default:
        return 0.46;
    }
}

double SectorImpactAnalyzer::directionWeight(EventImpactDirection direction) const
{
    switch (direction) {
    case EventImpactDirection::Negative:
        return -1.0;
    case EventImpactDirection::Positive:
        return 1.0;
    case EventImpactDirection::Mixed:
        return 0.35;
    case EventImpactDirection::Neutral:
    default:
        return 0.0;
    }
}
