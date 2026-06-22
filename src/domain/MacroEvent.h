#pragma once

#include <QDateTime>
#include <QList>
#include <QString>

enum class MacroEventType
{
    Unknown,
    MonetaryPolicy,
    InflationEmployment,
    FiscalPolicy,
    IndustrialPolicy,
    CommoditySupplyDemand,
    GeopoliticsTrade,
    MarketInstitution
};

enum class MacroEventState
{
    Rumor,
    Expected,
    Scheduled,
    Confirmed,
    Occurred,
    Revised,
    Invalidated
};

enum class MacroEventRegion
{
    Unknown,
    US,
    China,
    Global
};

enum class EventImpactDirection
{
    Negative,
    Neutral,
    Positive,
    Mixed
};

enum class EventImpactRelation
{
    Direct,
    Indirect,
    Conditional
};

enum class ImpactHorizon
{
    Intraday,
    ShortTerm,
    MediumTerm,
    LongTerm
};

struct MacroEventCheckpoint
{
    QString name;
    QDateTime time;
    QString reason;
};

struct MacroEventEvidence
{
    QString source;
    QString url;
    QString title;
    QString summary;
    QDateTime publishedAt;
    double reliability = 0.0;
};

struct MacroEvent
{
    QString id;
    QString title;
    QString normalizedKey;
    QString checkpoint;
    MacroEventType type = MacroEventType::Unknown;
    MacroEventState state = MacroEventState::Expected;
    MacroEventRegion region = MacroEventRegion::Unknown;
    QDateTime detectedAt;
    QDateTime publishedAt;
    QDateTime expectedAt;
    QDateTime confirmedAt;
    QString effectiveWindow;
    QList<MacroEventCheckpoint> nextCheckpoints;
    double novelty = 0.0;
    double importance = 0.0;
    double confidence = 0.0;
    QList<MacroEventEvidence> evidence;
};

struct SectorEventImpact
{
    QString eventId;
    QString eventTitle;
    QString sector;
    QString path;
    QString explanation;
    QString condition;
    EventImpactDirection direction = EventImpactDirection::Neutral;
    EventImpactRelation relation = EventImpactRelation::Indirect;
    MacroEventState state = MacroEventState::Expected;
    ImpactHorizon horizon = ImpactHorizon::ShortTerm;
    double strength = 0.0;
    double confidence = 0.0;
    double sourceReliability = 0.0;
    double noveltyWeight = 1.0;
    double timeDecay = 1.0;
    QDateTime latestEvidenceAt;
};

inline QString toString(MacroEventType type)
{
    switch (type) {
    case MacroEventType::MonetaryPolicy:
        return QStringLiteral("MonetaryPolicy");
    case MacroEventType::InflationEmployment:
        return QStringLiteral("InflationEmployment");
    case MacroEventType::FiscalPolicy:
        return QStringLiteral("FiscalPolicy");
    case MacroEventType::IndustrialPolicy:
        return QStringLiteral("IndustrialPolicy");
    case MacroEventType::CommoditySupplyDemand:
        return QStringLiteral("CommoditySupplyDemand");
    case MacroEventType::GeopoliticsTrade:
        return QStringLiteral("GeopoliticsTrade");
    case MacroEventType::MarketInstitution:
        return QStringLiteral("MarketInstitution");
    case MacroEventType::Unknown:
    default:
        return QStringLiteral("Unknown");
    }
}

inline QString toString(EventImpactDirection direction)
{
    switch (direction) {
    case EventImpactDirection::Negative:
        return QStringLiteral("negative");
    case EventImpactDirection::Positive:
        return QStringLiteral("positive");
    case EventImpactDirection::Mixed:
        return QStringLiteral("mixed");
    case EventImpactDirection::Neutral:
    default:
        return QStringLiteral("neutral");
    }
}

inline QString toString(EventImpactRelation relation)
{
    switch (relation) {
    case EventImpactRelation::Direct:
        return QStringLiteral("direct");
    case EventImpactRelation::Conditional:
        return QStringLiteral("conditional");
    case EventImpactRelation::Indirect:
    default:
        return QStringLiteral("indirect");
    }
}

inline QString toString(MacroEventState state)
{
    switch (state) {
    case MacroEventState::Rumor:
        return QStringLiteral("Rumor");
    case MacroEventState::Expected:
        return QStringLiteral("Expected");
    case MacroEventState::Scheduled:
        return QStringLiteral("Scheduled");
    case MacroEventState::Confirmed:
        return QStringLiteral("Confirmed");
    case MacroEventState::Occurred:
        return QStringLiteral("Occurred");
    case MacroEventState::Revised:
        return QStringLiteral("Revised");
    case MacroEventState::Invalidated:
        return QStringLiteral("Invalidated");
    default:
        return QStringLiteral("Expected");
    }
}

inline QString toString(ImpactHorizon horizon)
{
    switch (horizon) {
    case ImpactHorizon::Intraday:
        return QStringLiteral("Intraday");
    case ImpactHorizon::MediumTerm:
        return QStringLiteral("MediumTerm");
    case ImpactHorizon::LongTerm:
        return QStringLiteral("LongTerm");
    case ImpactHorizon::ShortTerm:
    default:
        return QStringLiteral("ShortTerm");
    }
}

inline QString toString(MacroEventRegion region)
{
    switch (region) {
    case MacroEventRegion::US:
        return QStringLiteral("US");
    case MacroEventRegion::China:
        return QStringLiteral("China");
    case MacroEventRegion::Global:
        return QStringLiteral("Global");
    case MacroEventRegion::Unknown:
    default:
        return QStringLiteral("Unknown");
    }
}
