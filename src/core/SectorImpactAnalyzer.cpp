#include "core/SectorImpactAnalyzer.h"

#include <cmath>

#include <QtGlobal>

QMap<QString, double> SectorImpactAnalyzer::eventCatalystScores(const QList<SectorEventImpact> &impacts) const
{
    return eventCatalystScores(impacts, QDateTime::currentDateTimeUtc());
}

QMap<QString, double> SectorImpactAnalyzer::eventCatalystScores(const QList<SectorEventImpact> &impacts,
                                                               const QDateTime &now) const
{
    QMap<QString, double> scores;
    for (const SectorEventImpact &impact : impacts) {
        const double contribution = scoreImpact(impact, now);
        scores[impact.sector] = qBound(-1.0, scores.value(impact.sector) + contribution, 1.0);
    }
    return scores;
}

double SectorImpactAnalyzer::scoreImpact(const SectorEventImpact &impact, const QDateTime &now) const
{
    return qBound(-1.0,
                  directionWeight(impact.direction)
                      * impact.strength
                      * impact.confidence
                      * stateWeight(impact.state)
                      * reliabilityWeight(impact)
                      * noveltyWeight(impact)
                      * timeDecayWeight(impact, now),
                  1.0);
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

double SectorImpactAnalyzer::reliabilityWeight(const SectorEventImpact &impact) const
{
    const double reliability = impact.sourceReliability > 0.0 ? impact.sourceReliability : impact.confidence;
    return qBound(0.0, reliability, 1.0);
}

double SectorImpactAnalyzer::noveltyWeight(const SectorEventImpact &impact) const
{
    return qBound(0.2, impact.noveltyWeight, 1.0);
}

double SectorImpactAnalyzer::timeDecayWeight(const SectorEventImpact &impact, const QDateTime &now) const
{
    double weight = qBound(0.15, impact.timeDecay, 1.0);
    if (impact.latestEvidenceAt.isValid() && now.isValid()) {
        const double ageDays = qMax(0.0, impact.latestEvidenceAt.secsTo(now) / 86400.0);
        weight *= qBound(0.2, std::exp(-ageDays / 7.0), 1.0);
    }
    return qBound(0.0, weight, 1.0);
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
