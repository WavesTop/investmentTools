#include "core/EventExtractionEngine.h"
#include "core/EventRepository.h"
#include "core/ImpactGraphEngine.h"
#include "core/SectorImpactAnalyzer.h"
#include "domain/AnalysisResult.h"
#include "domain/MacroEvent.h"
#include "providers/RealFinanceNewsProvider.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QMap>
#include <QTemporaryDir>
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

bool hasCheckpoint(const MacroEvent &event, const QString &name)
{
    for (const MacroEventCheckpoint &checkpoint : event.nextCheckpoints) {
        if (checkpoint.name.contains(name, Qt::CaseInsensitive)) {
            return true;
        }
    }
    return false;
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

void verifyV21ModelFields()
{
    expect(toString(MacroEventType::FiscalPolicy) == QStringLiteral("FiscalPolicy"),
           "fiscal policy event type has a stable string");
    expect(toString(MacroEventType::GeopoliticsTrade) == QStringLiteral("GeopoliticsTrade"),
           "geopolitics and trade event type has a stable string");
    expect(toString(MacroEventType::MarketInstitution) == QStringLiteral("MarketInstitution"),
           "market institution event type has a stable string");
    expect(toString(MacroEventState::Rumor) == QStringLiteral("Rumor"), "rumor state has a stable string");
    expect(toString(MacroEventState::Occurred) == QStringLiteral("Occurred"),
           "occurred state has a stable string");
    expect(toString(MacroEventState::Invalidated) == QStringLiteral("Invalidated"),
           "invalidated state has a stable string");
    expect(toString(ImpactHorizon::MediumTerm) == QStringLiteral("MediumTerm"),
           "impact horizon has a stable string");

    MacroEvent event;
    const QDateTime detectedAt(QDate(2026, 6, 21), QTime(9, 30), Qt::UTC);
    const QDateTime expectedAt(QDate(2026, 7, 1), QTime(20, 0), Qt::UTC);
    const QDateTime confirmedAt(QDate(2026, 7, 2), QTime(2, 0), Qt::UTC);
    event.detectedAt = detectedAt;
    event.expectedAt = expectedAt;
    event.confirmedAt = confirmedAt;
    event.novelty = 0.82;
    event.importance = 0.74;

    MacroEventCheckpoint checkpoint;
    checkpoint.name = QStringLiteral("FOMC");
    checkpoint.time = expectedAt;
    checkpoint.reason = QStringLiteral("policy decision window");
    event.nextCheckpoints.push_back(checkpoint);

    MacroEventEvidence evidence;
    evidence.url = QStringLiteral("https://example.com/news");
    evidence.reliability = 0.91;
    event.evidence.push_back(evidence);

    expect(event.detectedAt == detectedAt, "event stores detected time");
    expect(event.expectedAt == expectedAt, "event stores expected time");
    expect(event.confirmedAt == confirmedAt, "event stores confirmed time");
    expect(event.nextCheckpoints.size() == 1, "event stores structured checkpoints");
    expect(event.nextCheckpoints.first().name == QStringLiteral("FOMC"), "checkpoint stores name");
    expect(event.evidence.first().url.contains(QStringLiteral("example.com")), "evidence stores URL");
    expect(event.evidence.first().reliability > 0.9, "evidence stores reliability");
    expect(event.novelty > 0.8, "event stores novelty");
    expect(event.importance > 0.7, "event stores importance");

    SectorEventImpact impact;
    impact.horizon = ImpactHorizon::MediumTerm;
    expect(impact.horizon == ImpactHorizon::MediumTerm, "sector impact stores horizon");
}

void verifyV21StateAndCheckpointParsing()
{
    EventExtractionEngine engine;
    const QDateTime publishedAt(QDate(2026, 6, 21), QTime(11, 0), Qt::UTC);

    const QList<MacroEvent> rumorEvents = engine.extractFromText(
        QString::fromUtf8("市场传闻美联储可能提前降息，交易员关注下次 FOMC"),
        QStringLiteral("state-test"),
        publishedAt);
    const MacroEvent *rumor = findFirst(rumorEvents, MacroEventType::MonetaryPolicy);
    expect(rumor != nullptr, "rumor Fed headline creates an event");
    if (rumor) {
        expect(rumor->state == MacroEventState::Rumor, "unconfirmed policy rumor resolves to Rumor");
        expect(hasCheckpoint(*rumor, QStringLiteral("FOMC")), "Fed rumor keeps FOMC checkpoint");
        expect(hasCheckpoint(*rumor, QStringLiteral("CPI")), "Fed rumor keeps CPI checkpoint");
    }

    const QList<MacroEvent> invalidatedEvents = engine.extractFromText(
        QString::fromUtf8("美联储官员否认近期降息可能，降息交易逻辑落空"),
        QStringLiteral("state-test"),
        publishedAt);
    const MacroEvent *invalidated = findFirst(invalidatedEvents, MacroEventType::MonetaryPolicy);
    expect(invalidated != nullptr, "invalidated Fed headline creates an event");
    if (invalidated) {
        expect(invalidated->state == MacroEventState::Invalidated,
               "denied policy expectation resolves to Invalidated");
    }

    const QList<MacroEvent> occurredEvents = engine.extractFromText(
        QString::fromUtf8("FOMC 会议落地后市场开始交易降息结果"),
        QStringLiteral("state-test"),
        publishedAt);
    const MacroEvent *occurred = findFirst(occurredEvents, MacroEventType::MonetaryPolicy);
    expect(occurred != nullptr, "occurred Fed meeting headline creates an event");
    if (occurred) {
        expect(occurred->state == MacroEventState::Occurred, "post-event policy text resolves to Occurred");
    }

    const QList<MacroEvent> chinaEvents = engine.extractFromText(
        QString::fromUtf8("央行 LPR 调整预期升温，市场关注下次报价和 MLF 操作"),
        QStringLiteral("state-test"),
        publishedAt);
    const MacroEvent *china = findFirst(chinaEvents, MacroEventType::MonetaryPolicy);
    expect(china != nullptr, "China monetary headline creates an event");
    if (china) {
        expect(china->region == MacroEventRegion::China, "China monetary headline keeps China region");
        expect(hasCheckpoint(*china, QStringLiteral("LPR")), "China monetary event keeps LPR checkpoint");
        expect(hasCheckpoint(*china, QStringLiteral("MLF")), "China monetary event keeps MLF checkpoint");
    }
}

void verifyV21HighFrequencyRules()
{
    EventExtractionEngine engine;
    const QDateTime publishedAt(QDate(2026, 6, 21), QTime(12, 0), Qt::UTC);

    auto firstEvent = [&](const QString &text) {
        const QList<MacroEvent> events = engine.extractFromText(text, QStringLiteral("rule-test"), publishedAt);
        return events.isEmpty() ? MacroEvent{} : events.first();
    };

    const MacroEvent cpi = firstEvent(QString::fromUtf8("美国 CPI 低于预期，降息交易升温"));
    expect(cpi.type == MacroEventType::InflationEmployment, "US CPI surprise maps to inflation employment");
    expect(cpi.region == MacroEventRegion::US, "US CPI surprise keeps US region");
    expect(cpi.normalizedKey == QStringLiteral("us_inflation_jobs"), "US CPI surprise uses inflation rule key");

    const MacroEvent hawkish = firstEvent(QString::fromUtf8("美联储释放鹰派信号，暗示仍可能加息"));
    expect(hawkish.type == MacroEventType::MonetaryPolicy, "hawkish Fed headline maps to monetary policy");
    expect(hawkish.normalizedKey == QStringLiteral("fed_hawkish_hike"), "hawkish Fed headline uses hike rule");

    const MacroEvent fiscal = firstEvent(QString::fromUtf8("专项债发行加速，财政稳增长预期升温"));
    expect(fiscal.type == MacroEventType::FiscalPolicy, "special bond headline maps to fiscal policy");
    expect(fiscal.region == MacroEventRegion::China, "fiscal policy headline keeps China region");
    expect(fiscal.normalizedKey == QStringLiteral("china_fiscal_stimulus"), "fiscal headline uses fiscal rule key");

    const QList<MacroEvent> exportLimitEvents = engine.extractFromText(
        QString::fromUtf8("美国扩大半导体出口限制，先进芯片供应链再受扰动"),
        QStringLiteral("rule-test"),
        publishedAt);
    const MacroEvent exportLimit = exportLimitEvents.isEmpty() ? MacroEvent{} : exportLimitEvents.first();
    expect(exportLimit.type == MacroEventType::GeopoliticsTrade, "export restriction maps to geopolitics trade");
    expect(exportLimit.normalizedKey == QStringLiteral("semiconductor_export_control"),
           "export restriction uses export control key");
    bool hasDuplicateIndustrialPolicy = false;
    for (const MacroEvent &event : exportLimitEvents) {
        hasDuplicateIndustrialPolicy = hasDuplicateIndustrialPolicy
            || event.normalizedKey == QStringLiteral("semiconductor_policy");
    }
    expect(!hasDuplicateIndustrialPolicy, "export restriction suppresses duplicate industrial policy event");

    const MacroEvent oil = firstEvent(QString::fromUtf8("OPEC 减产叠加地缘冲突，原油供给扰动推动油价上涨"));
    expect(oil.type == MacroEventType::CommoditySupplyDemand, "oil supply shock maps to commodity supply demand");
    expect(oil.normalizedKey == QStringLiteral("oil_supply_shock"), "oil supply shock uses oil rule key");

    const MacroEvent institution = firstEvent(QString::fromUtf8("证监会优化 IPO 和减持规则，印花税下调预期升温"));
    expect(institution.type == MacroEventType::MarketInstitution, "market rule headline maps to market institution");
    expect(institution.region == MacroEventRegion::China, "market institution headline keeps China region");
    expect(institution.normalizedKey == QStringLiteral("china_market_institution"),
           "market rule headline uses institution key");
}

void verifyV21ExpandedImpactPaths()
{
    EventExtractionEngine extractor;
    ImpactGraphEngine graph;
    const QDateTime publishedAt(QDate(2026, 6, 21), QTime(13, 0), Qt::UTC);

    auto firstImpactSet = [&](const QString &text) {
        const QList<MacroEvent> events = extractor.extractFromText(text, QStringLiteral("path-test"), publishedAt);
        expect(!events.isEmpty(), "path sample creates a macro event");
        return events.isEmpty() ? QList<SectorEventImpact>() : graph.analyze(events.first());
    };

    const QList<SectorEventImpact> hawkishImpacts = firstImpactSet(
        QString::fromUtf8("美联储释放鹰派信号，暗示仍可能加息"));
    const SectorEventImpact *hawkishSemi = findImpact(hawkishImpacts, QString::fromUtf8("半导体"));
    const SectorEventImpact *hawkishGold = findImpact(hawkishImpacts, QString::fromUtf8("黄金"));
    expect(hawkishSemi && hawkishSemi->direction == EventImpactDirection::Negative,
           "hawkish Fed signal pressures semiconductor growth valuation");
    expect(hawkishSemi && hawkishSemi->horizon == ImpactHorizon::MediumTerm,
           "hawkish Fed semiconductor impact uses medium-term horizon");
    expect(hawkishGold && hawkishGold->direction == EventImpactDirection::Negative,
           "hawkish Fed signal pressures gold via real rates");

    const QList<SectorEventImpact> fiscalImpacts = firstImpactSet(
        QString::fromUtf8("专项债发行加速，财政稳增长预期升温"));
    const SectorEventImpact *infrastructure = findImpact(fiscalImpacts, QString::fromUtf8("建筑建材"));
    const SectorEventImpact *fiscalBroker = findImpact(fiscalImpacts, QString::fromUtf8("证券"));
    expect(infrastructure && infrastructure->direction == EventImpactDirection::Positive,
           "fiscal stimulus supports infrastructure chain");
    expect(infrastructure && infrastructure->relation == EventImpactRelation::Direct,
           "fiscal infrastructure relation is direct");
    expect(fiscalBroker && fiscalBroker->relation == EventImpactRelation::Indirect,
           "fiscal policy also improves brokerage risk appetite indirectly");

    const QList<SectorEventImpact> exportImpacts = firstImpactSet(
        QString::fromUtf8("美国扩大半导体出口限制，先进芯片供应链再受扰动"));
    const SectorEventImpact *exportSemi = findImpact(exportImpacts, QString::fromUtf8("半导体"));
    expect(exportSemi && exportSemi->direction == EventImpactDirection::Mixed,
           "export controls keep semiconductor impact mixed");
    expect(exportSemi && exportSemi->relation == EventImpactRelation::Conditional,
           "export controls use conditional relation");
    expect(exportSemi && exportSemi->horizon == ImpactHorizon::MediumTerm,
           "export controls use medium-term horizon");
    expect(exportSemi && !exportSemi->condition.isEmpty(), "export controls keep invalidation condition");

    const QList<SectorEventImpact> oilImpacts = firstImpactSet(
        QString::fromUtf8("OPEC 减产叠加地缘冲突，原油供给扰动推动油价上涨"));
    const SectorEventImpact *oilUpstream = findImpact(oilImpacts, QString::fromUtf8("石油石化"));
    const SectorEventImpact *transport = findImpact(oilImpacts, QString::fromUtf8("交通运输"));
    expect(oilUpstream && oilUpstream->direction == EventImpactDirection::Positive,
           "oil supply shock supports upstream oil chain");
    expect(transport && transport->direction == EventImpactDirection::Negative,
           "oil supply shock pressures transport cost");

    const QList<SectorEventImpact> institutionImpacts = firstImpactSet(
        QString::fromUtf8("证监会优化 IPO 和减持规则，印花税下调预期升温"));
    const SectorEventImpact *institutionBroker = findImpact(institutionImpacts, QString::fromUtf8("证券"));
    expect(institutionBroker && institutionBroker->direction == EventImpactDirection::Positive,
           "market institution reform supports brokerage sector");
    expect(institutionBroker && institutionBroker->horizon == ImpactHorizon::ShortTerm,
           "market institution reform uses short-term trading horizon");
}

void verifyV21ScoringFactors()
{
    SectorImpactAnalyzer analyzer;
    const QDateTime now(QDate(2026, 6, 21), QTime(14, 0), Qt::UTC);

    SectorEventImpact base;
    base.sector = QString::fromUtf8("半导体");
    base.direction = EventImpactDirection::Positive;
    base.state = MacroEventState::Expected;
    base.strength = 0.8;
    base.confidence = 0.8;
    base.sourceReliability = 1.0;
    base.noveltyWeight = 1.0;
    base.latestEvidenceAt = now.addSecs(-3600);

    const double baseScore = analyzer.scoreImpact(base, now);

    SectorEventImpact weakSource = base;
    weakSource.sourceReliability = 0.35;
    expect(analyzer.scoreImpact(weakSource, now) < baseScore * 0.5,
           "low source reliability compresses event score");

    SectorEventImpact repeated = base;
    repeated.noveltyWeight = 0.4;
    expect(analyzer.scoreImpact(repeated, now) < baseScore * 0.5,
           "low novelty compresses repeated event score");

    SectorEventImpact oldEvidence = base;
    oldEvidence.latestEvidenceAt = now.addDays(-9);
    expect(analyzer.scoreImpact(oldEvidence, now) < baseScore * 0.7,
           "old evidence decays event score");

    SectorEventImpact invalidated = base;
    invalidated.state = MacroEventState::Invalidated;
    expect(analyzer.scoreImpact(invalidated, now) == 0.0,
           "invalidated event contributes no score");

    const QMap<QString, double> scores = analyzer.eventCatalystScores({base, weakSource}, now);
    expect(scores.value(QString::fromUtf8("半导体")) > baseScore,
           "aggregated sector score includes multiple impacts");
    expect(scores.value(QString::fromUtf8("半导体")) < baseScore * 1.5,
           "aggregated sector score respects reliability compression");
}

void verifyEventRepository()
{
    QTemporaryDir dir;
    expect(dir.isValid(), "temporary repository directory is valid");
    if (!dir.isValid()) return;

    MacroEvent event;
    event.id = QStringLiteral("fed_rate_cut-test");
    event.normalizedKey = QStringLiteral("fed_rate_cut");
    event.title = QString::fromUtf8("美联储降息预期升温");
    event.type = MacroEventType::MonetaryPolicy;
    event.state = MacroEventState::Expected;

    const QString path = dir.filePath(QStringLiteral("events.json"));
    EventRepository repository(path);
    const QDateTime firstSeen(QDate(2026, 6, 21), QTime(9, 30), Qt::UTC);
    const QDateTime secondSeen(QDate(2026, 6, 21), QTime(10, 0), Qt::UTC);
    const QDateTime thirdSeen(QDate(2026, 6, 21), QTime(10, 30), Qt::UTC);

    QList<MacroEvent> tracked = repository.trackEvents({event, event}, firstSeen);
    expect(tracked.size() == 1, "same event is returned once in one batch");

    event.state = MacroEventState::Confirmed;
    repository.trackEvents({event}, secondSeen);
    event.state = MacroEventState::Invalidated;
    repository.trackEvents({event}, thirdSeen);

    const QList<TrackedEventRecord> records = repository.records();
    expect(records.size() == 1, "repository keeps one record for repeated event");
    expect(records.first().seenCount == 4, "repository increments seen count");
    expect(records.first().firstSeenAt == firstSeen, "repository preserves first seen time");
    expect(records.first().lastSeenAt == thirdSeen, "repository updates last seen time");
    expect(records.first().stateHistory.contains(QStringLiteral("Expected")), "repository stores initial state");
    expect(records.first().stateHistory.contains(QStringLiteral("Confirmed")), "repository stores state change");
    expect(records.first().stateHistory.contains(QStringLiteral("Invalidated")), "repository stores invalidated state");

    repository.recordPerformance(event.id, QString::fromUtf8("半导体"), 1, 2.4,
                                 QDateTime(QDate(2026, 6, 22), QTime(15, 0), Qt::UTC));
    repository.recordPerformance(event.id, QString::fromUtf8("半导体"), 3, -1.2,
                                 QDateTime(QDate(2026, 6, 24), QTime(15, 0), Qt::UTC));
    repository.recordPerformance(event.id, QString::fromUtf8("黄金"), 20, 8.5,
                                 QDateTime(QDate(2026, 7, 21), QTime(15, 0), Qt::UTC));

    const QList<TrackedEventRecord> performanceRecords = repository.records();
    expect(performanceRecords.first().performances.size() == 3,
           "repository stores multiple impact performance windows");
    expect(performanceRecords.first().performances.first().windowDays == 1,
           "performance record stores window days");

    EventRepository reloaded(path);
    expect(reloaded.records().size() == 1, "repository reloads persisted records");
    expect(reloaded.records().first().currentState == MacroEventState::Invalidated,
           "repository reloads invalidated state");
    expect(reloaded.records().first().performances.size() == 3,
           "repository reloads persisted performance records");
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    verifyExtraction();
    verifyImpactPaths();
    verifyAnalysisResultFields();
    verifyV21ModelFields();
    verifyV21StateAndCheckpointParsing();
    verifyV21HighFrequencyRules();
    verifyV21ExpandedImpactPaths();
    verifyV21ScoringFactors();
    verifyEventRepository();

    if (failures > 0) {
        QTextStream(stderr) << failures << " event impact smoke check(s) failed.\n";
        return 1;
    }

    QTextStream(stdout) << "Event impact smoke passed.\n";
    return 0;
}
