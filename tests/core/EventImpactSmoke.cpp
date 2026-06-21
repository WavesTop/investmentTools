#include "core/EventExtractionEngine.h"
#include "domain/MacroEvent.h"
#include "providers/RealFinanceNewsProvider.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QTextStream>

namespace {

int failures = 0;

void expect(bool condition, const QString &message)
{
    if (condition) return;
    QTextStream(stderr) << "FAIL: " << message << '\n';
    ++failures;
}

RawHeadline makeHeadline(const QString &title, const QString &summary = QString())
{
    RawHeadline headline;
    headline.source = QString::fromUtf8("测试新闻源");
    headline.title = title;
    headline.summary = summary;
    headline.timestamp = QDateTime(QDate(2026, 6, 21), QTime(9, 30), Qt::UTC);
    return headline;
}

const MacroEvent *findFirst(const QList<MacroEvent> &events, MacroEventType type)
{
    for (const MacroEvent &event : events) {
        if (event.type == type) {
            return &event;
        }
    }
    return nullptr;
}

void verifyExtraction()
{
    EventExtractionEngine engine;
    const QList<MacroEvent> events = engine.extractFromHeadlines({
        makeHeadline(QString::fromUtf8("美联储降息预期升温，市场关注下次 FOMC 会议")),
        makeHeadline(QString::fromUtf8("美联储将于下周召开议息会议")),
        makeHeadline(QString::fromUtf8("美联储宣布降息 25 个基点")),
        makeHeadline(QString::fromUtf8("美国 CPI 高于预期，交易员下修降息概率"))
    });

    const MacroEvent *fedEvent = findFirst(events, MacroEventType::MonetaryPolicy);
    expect(fedEvent != nullptr, "Fed rate cut headline creates a monetary policy event");
    if (!fedEvent) return;

    expect(fedEvent->state == MacroEventState::Expected, "rate cut expectation resolves to Expected");
    expect(fedEvent->region == MacroEventRegion::US, "Fed event region resolves to US");
    expect(fedEvent->checkpoint.contains(QString::fromUtf8("FOMC")), "FOMC checkpoint is preserved");
    expect(!fedEvent->evidence.isEmpty(), "event keeps evidence");
    expect(fedEvent->evidence.first().source == QString::fromUtf8("测试新闻源"), "evidence keeps source");
    expect(fedEvent->evidence.first().publishedAt.isValid(), "evidence keeps timestamp");

    bool hasScheduled = false;
    bool hasConfirmed = false;
    bool hasRevised = false;
    for (const MacroEvent &event : events) {
        hasScheduled = hasScheduled || event.state == MacroEventState::Scheduled;
        hasConfirmed = hasConfirmed || event.state == MacroEventState::Confirmed;
        hasRevised = hasRevised || event.state == MacroEventState::Revised;
    }

    expect(hasScheduled, "scheduled policy meetings are recognized");
    expect(hasConfirmed, "confirmed policy actions are recognized");
    expect(hasRevised, "revised expectations are recognized");
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    verifyExtraction();

    if (failures > 0) {
        QTextStream(stderr) << failures << " event impact smoke check(s) failed.\n";
        return 1;
    }

    QTextStream(stdout) << "Event impact smoke passed.\n";
    return 0;
}
