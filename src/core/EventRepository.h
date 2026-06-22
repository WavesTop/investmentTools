#pragma once

#include "domain/MacroEvent.h"

#include <QDateTime>
#include <QList>
#include <QMap>
#include <QStringList>

struct TrackedImpactPerformance
{
    QString sector;
    int windowDays = 0;
    double returnPct = 0.0;
    QDateTime capturedAt;
};

struct TrackedEventRecord
{
    QString key;
    QString eventId;
    QString normalizedKey;
    QString title;
    MacroEventState currentState = MacroEventState::Expected;
    QDateTime firstSeenAt;
    QDateTime lastSeenAt;
    int seenCount = 0;
    QStringList stateHistory;
    QList<TrackedImpactPerformance> performances;
};

class EventRepository
{
public:
    explicit EventRepository(const QString &filePath = QString());

    QList<MacroEvent> trackEvents(const QList<MacroEvent> &events,
                                  const QDateTime &seenAt = QDateTime());
    void recordPerformance(const QString &eventKey,
                           const QString &sector,
                           int windowDays,
                           double returnPct,
                           const QDateTime &capturedAt = QDateTime());
    QList<TrackedEventRecord> records() const;

private:
    QString defaultFilePath() const;
    QString recordKey(const MacroEvent &event) const;
    QString findRecordKey(const QString &eventKey) const;
    void load();
    void save() const;

    QString m_filePath;
    QMap<QString, TrackedEventRecord> m_records;
};
