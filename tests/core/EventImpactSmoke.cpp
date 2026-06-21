#include "core/EventExtractionEngine.h"
#include "core/ImpactGraphEngine.h"
#include "core/SectorImpactAnalyzer.h"
#include "domain/AnalysisResult.h"
#include "domain/MacroEvent.h"
#include "providers/RealFinanceNewsProvider.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QMap>
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

const SectorEventImpact *findImpact(const QList<SectorEventImpact> &impacts, const QString &sector)
{
    for (const SectorEventImpact &impact : impacts) {
        if (impact.sector == sector) {
            return &impact;
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

void verifyImpactPaths()
{
    EventExtractionEngine extractor;
    const QList<MacroEvent> events = extractor.extractFromText(
        QString::fromUtf8("美联储降息预期升温，市场关注下次 FOMC 会议"),
        QString::fromUtf8("测试新闻源"),
        QDateTime(QDate(2026, 6, 21), QTime(10, 0), Qt::UTC));
    const MacroEvent *fedEvent = findFirst(events, MacroEventType::MonetaryPolicy);
    expect(fedEvent != nullptr, "Fed event exists for impact analysis");
    if (!fedEvent) return;

    ImpactGraphEngine graph;
    const QList<SectorEventImpact> impacts = graph.analyze(*fedEvent);

    const SectorEventImpact *gold = findImpact(impacts, QString::fromUtf8("黄金"));
    const SectorEventImpact *metals = findImpact(impacts, QString::fromUtf8("有色金属"));
    const SectorEventImpact *semiconductor = findImpact(impacts, QString::fromUtf8("半导体"));
    const SectorEventImpact *biotech = findImpact(impacts, QString::fromUtf8("创新药"));
    const SectorEventImpact *broker = findImpact(impacts, QString::fromUtf8("证券"));

    expect(gold && gold->direction == EventImpactDirection::Positive, "gold is directly positive");
    expect(gold && gold->relation == EventImpactRelation::Direct, "gold relation is direct");
    expect(metals != nullptr, "non-ferrous metals are included");
    expect(semiconductor && semiconductor->relation == EventImpactRelation::Indirect,
           "semiconductor relation is indirect");
    expect(biotech != nullptr, "biotech is included");
    expect(broker != nullptr, "brokerage is included");
    expect(semiconductor && !semiconductor->explanation.isEmpty(), "impact keeps explanation");

    SectorImpactAnalyzer analyzer;
    const QMap<QString, double> scores = analyzer.eventCatalystScores(impacts);
    expect(scores.value(QString::fromUtf8("半导体")) > 0.0, "semiconductor catalyst score is positive");
    expect(scores.value(QString::fromUtf8("黄金")) > scores.value(QString::fromUtf8("半导体")) * 0.6,
           "direct gold impact keeps meaningful strength");
}

void verifyAnalysisResultFields()
{
    AnalysisResult result;
    result.macroEvents.push_back(MacroEvent{});

    SectorSnapshot sector;
    sector.industry = QString::fromUtf8("半导体");
    sector.eventImpacts.push_back(SectorEventImpact{});
    sector.eventCatalystScore = 0.18;
    sector.eventSummary = QString::fromUtf8("美联储降息预期通过成长估值链条形成间接催化");

    expect(result.macroEvents.size() == 1, "analysis result stores macro events");
    expect(sector.eventImpacts.size() == 1, "sector snapshot stores event impacts");
    expect(sector.eventCatalystScore > 0.0, "sector snapshot stores event catalyst score");
    expect(sector.eventSummary.contains(QString::fromUtf8("间接催化")), "sector snapshot stores event summary");
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    verifyExtraction();
    verifyImpactPaths();
    verifyAnalysisResultFields();

    if (failures > 0) {
        QTextStream(stderr) << failures << " event impact smoke check(s) failed.\n";
        return 1;
    }

    QTextStream(stdout) << "Event impact smoke passed.\n";
    return 0;
}
