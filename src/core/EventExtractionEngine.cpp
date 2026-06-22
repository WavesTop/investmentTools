#include "core/EventExtractionEngine.h"

#include <QCryptographicHash>

namespace {

QString normalizedId(const QString &key, const QString &title)
{
    const QByteArray digest = QCryptographicHash::hash((key + title.left(24)).toUtf8(),
                                                       QCryptographicHash::Sha1)
                                  .toHex()
                                  .left(10);
    return key + QStringLiteral("-") + QString::fromLatin1(digest);
}

QString joinedText(const RawHeadline &headline)
{
    return headline.title + QStringLiteral(" ") + headline.summary;
}

} // namespace

QList<MacroEvent> EventExtractionEngine::extractFromHeadlines(const QList<RawHeadline> &headlines) const
{
    QList<MacroEvent> events;
    for (const RawHeadline &headline : headlines) {
        const QString text = joinedText(headline);
        const QList<EventRule> rules = m_ruleBook.matchingRules(text);
        for (const EventRule &rule : rules) {
            MacroEvent event;
            event.id = normalizedId(rule.key, headline.title);
            event.title = headline.title;
            event.normalizedKey = rule.key;
            event.type = rule.type;
            event.state = m_ruleBook.resolveState(text);
            event.region = rule.region;
            event.checkpoint = rule.checkpoint;
            event.confidence = rule.confidence;
            event.detectedAt = QDateTime::currentDateTimeUtc();
            event.publishedAt = headline.timestamp;
            event.nextCheckpoints = m_ruleBook.resolveCheckpoints(text, event.type, event.region);

            MacroEventEvidence evidence;
            evidence.source = headline.source;
            evidence.title = headline.title;
            evidence.summary = headline.summary;
            evidence.publishedAt = headline.timestamp;
            evidence.reliability = rule.confidence;
            event.evidence.push_back(evidence);
            events.push_back(event);
        }
    }
    return events;
}

QList<MacroEvent> EventExtractionEngine::extractFromText(const QString &text,
                                                         const QString &source,
                                                         const QDateTime &publishedAt) const
{
    RawHeadline headline;
    headline.source = source;
    headline.title = text;
    headline.timestamp = publishedAt;
    return extractFromHeadlines({headline});
}
