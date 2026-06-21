#pragma once

#include <QDateTime>
#include <QList>
#include <QString>

enum class MacroEventType
{
    Unknown,
    MonetaryPolicy,
    InflationEmployment,
    CommoditySupplyDemand,
    IndustrialPolicy
};

enum class MacroEventState
{
    Expected,
    Scheduled,
    Confirmed,
    Revised
};

enum class MacroEventRegion
{
    Unknown,
    US,
    China,
    Global
};

struct MacroEventEvidence
{
    QString source;
    QString title;
    QString summary;
    QDateTime publishedAt;
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
    double confidence = 0.0;
    QList<MacroEventEvidence> evidence;
};

inline QString toString(MacroEventType type)
{
    switch (type) {
    case MacroEventType::MonetaryPolicy:
        return QStringLiteral("MonetaryPolicy");
    case MacroEventType::InflationEmployment:
        return QStringLiteral("InflationEmployment");
    case MacroEventType::CommoditySupplyDemand:
        return QStringLiteral("CommoditySupplyDemand");
    case MacroEventType::IndustrialPolicy:
        return QStringLiteral("IndustrialPolicy");
    case MacroEventType::Unknown:
    default:
        return QStringLiteral("Unknown");
    }
}

inline QString toString(MacroEventState state)
{
    switch (state) {
    case MacroEventState::Expected:
        return QStringLiteral("Expected");
    case MacroEventState::Scheduled:
        return QStringLiteral("Scheduled");
    case MacroEventState::Confirmed:
        return QStringLiteral("Confirmed");
    case MacroEventState::Revised:
        return QStringLiteral("Revised");
    default:
        return QStringLiteral("Expected");
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
