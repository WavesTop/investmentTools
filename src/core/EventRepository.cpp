#include "core/EventRepository.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QStandardPaths>

namespace {

MacroEventState stateFromString(const QString &state)
{
    if (state == QStringLiteral("Scheduled")) return MacroEventState::Scheduled;
    if (state == QStringLiteral("Confirmed")) return MacroEventState::Confirmed;
    if (state == QStringLiteral("Revised")) return MacroEventState::Revised;
    return MacroEventState::Expected;
}

QJsonArray stringListToJson(const QStringList &items)
{
    QJsonArray array;
    for (const QString &item : items) array.push_back(item);
    return array;
}

QStringList jsonToStringList(const QJsonArray &array)
{
    QStringList items;
    for (const QJsonValue &value : array) items.push_back(value.toString());
    return items;
}

} // namespace

EventRepository::EventRepository(const QString &filePath)
    : m_filePath(filePath.isEmpty() ? defaultFilePath() : filePath)
{
    load();
}

QList<MacroEvent> EventRepository::trackEvents(const QList<MacroEvent> &events,
                                               const QDateTime &seenAt)
{
    QList<MacroEvent> uniqueEvents;
    QSet<QString> returnedKeys;
    const QDateTime effectiveSeenAt = seenAt.isValid()
        ? seenAt
        : QDateTime::currentDateTimeUtc();

    for (const MacroEvent &event : events) {
        const QString key = recordKey(event);
        if (key.isEmpty()) continue;

        TrackedEventRecord record = m_records.value(key);
        if (record.key.isEmpty()) {
            record.key = key;
            record.eventId = event.id;
            record.normalizedKey = event.normalizedKey;
            record.title = event.title;
            record.firstSeenAt = effectiveSeenAt;
            record.stateHistory.push_back(toString(event.state));
        } else if (record.stateHistory.isEmpty()
                   || record.stateHistory.last() != toString(event.state)) {
            record.stateHistory.push_back(toString(event.state));
        }

        record.currentState = event.state;
        record.lastSeenAt = effectiveSeenAt;
        record.seenCount += 1;
        if (!event.title.isEmpty()) record.title = event.title;
        m_records[key] = record;

        if (!returnedKeys.contains(key)) {
            uniqueEvents.push_back(event);
            returnedKeys.insert(key);
        }
    }

    save();
    return uniqueEvents;
}

QList<TrackedEventRecord> EventRepository::records() const
{
    return m_records.values();
}

QString EventRepository::defaultFilePath() const
{
    QString dirPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (dirPath.isEmpty()) {
        dirPath = QDir::tempPath() + QStringLiteral("/InvestInsight");
    }
    return QDir(dirPath).filePath(QStringLiteral("tracked-events.json"));
}

QString EventRepository::recordKey(const MacroEvent &event) const
{
    if (!event.id.trimmed().isEmpty()) return event.id.trimmed();
    if (!event.normalizedKey.trimmed().isEmpty()) {
        return event.normalizedKey.trimmed() + QStringLiteral(":") + event.title.left(40).trimmed();
    }
    return event.title.left(60).trimmed();
}

void EventRepository::load()
{
    QFile file(m_filePath);
    if (!file.exists() || !file.open(QIODevice::ReadOnly)) return;

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) return;

    const QJsonArray array = doc.object().value(QStringLiteral("events")).toArray();
    for (const QJsonValue &value : array) {
        const QJsonObject obj = value.toObject();
        TrackedEventRecord record;
        record.key = obj.value(QStringLiteral("key")).toString();
        record.eventId = obj.value(QStringLiteral("eventId")).toString();
        record.normalizedKey = obj.value(QStringLiteral("normalizedKey")).toString();
        record.title = obj.value(QStringLiteral("title")).toString();
        record.currentState = stateFromString(obj.value(QStringLiteral("currentState")).toString());
        record.firstSeenAt = QDateTime::fromString(obj.value(QStringLiteral("firstSeenAt")).toString(), Qt::ISODate);
        record.lastSeenAt = QDateTime::fromString(obj.value(QStringLiteral("lastSeenAt")).toString(), Qt::ISODate);
        record.seenCount = obj.value(QStringLiteral("seenCount")).toInt();
        record.stateHistory = jsonToStringList(obj.value(QStringLiteral("stateHistory")).toArray());
        if (!record.key.isEmpty()) m_records[record.key] = record;
    }
}

void EventRepository::save() const
{
    QDir dir(QFileInfo(m_filePath).absolutePath());
    if (!dir.exists()) dir.mkpath(QStringLiteral("."));

    QJsonArray array;
    for (const TrackedEventRecord &record : m_records) {
        QJsonObject obj;
        obj.insert(QStringLiteral("key"), record.key);
        obj.insert(QStringLiteral("eventId"), record.eventId);
        obj.insert(QStringLiteral("normalizedKey"), record.normalizedKey);
        obj.insert(QStringLiteral("title"), record.title);
        obj.insert(QStringLiteral("currentState"), toString(record.currentState));
        obj.insert(QStringLiteral("firstSeenAt"), record.firstSeenAt.toString(Qt::ISODate));
        obj.insert(QStringLiteral("lastSeenAt"), record.lastSeenAt.toString(Qt::ISODate));
        obj.insert(QStringLiteral("seenCount"), record.seenCount);
        obj.insert(QStringLiteral("stateHistory"), stringListToJson(record.stateHistory));
        array.push_back(obj);
    }

    QJsonObject root;
    root.insert(QStringLiteral("events"), array);
    QFile file(m_filePath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
    }
}
