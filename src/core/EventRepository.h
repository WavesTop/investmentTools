#pragma once

#include "domain/MacroEvent.h"

#include <QDateTime>
#include <QList>
#include <QMap>
#include <QStringList>

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
};

class EventRepository
{
public:
    explicit EventRepository(const QString &filePath = QString());

    QList<MacroEvent> trackEvents(const QList<MacroEvent> &events,
                                  const QDateTime &seenAt = QDateTime());
    QList<TrackedEventRecord> records() const;

private:
    QString defaultFilePath() const;
    QString recordKey(const MacroEvent &event) const;
    void load();
    void save() const;

    QString m_filePath;
    QMap<QString, TrackedEventRecord> m_records;
};
