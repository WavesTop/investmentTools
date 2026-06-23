#include "core/RecommendationTracker.h"

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

SectorSnapshot makeSector(const QString &name,
                          AdviceAction action,
                          double forecast,
                          double todayChange,
                          double fiveDayMomentum)
{
    SectorSnapshot sector;
    sector.industry = name;
    sector.action = action;
    sector.forecastScore = forecast;
    sector.todayChangePct = todayChange;
    sector.todayChangePctValid = true;
    sector.fiveDayMomentum = fiveDayMomentum;
    sector.dataQualityScore = 82.0;
    sector.formula.todayFactor = todayChange * 0.015;
    sector.formula.momentumFactor = fiveDayMomentum * 0.015;
    sector.formula.fundFlowFactor = 0.04;
    sector.formula.techFactor = 0.05;
    sector.eventCatalystScore = 0.35;
    return sector;
}

RecommendationRecord previousActive(const QString &sectorName)
{
    RecommendationRecord record;
    record.sector = sectorName;
    record.state = RecommendationState::Active;
    record.firstAction = AdviceAction::Increase;
    record.currentAction = AdviceAction::Increase;
    record.firstSeenAt = QDateTime(QDate(2026, 6, 22), QTime(10, 0), Qt::UTC);
    record.lastSeenAt = record.firstSeenAt;
    record.initialScore = 0.31;
    record.currentScore = 0.31;
    record.initialTodayChangePct = 1.0;
    record.directionScore = 0.24;
    record.entryTimingScore = 0.18;
    record.active = true;
    return record;
}

void verifyOverheatedNoChase()
{
    SectorSnapshot sector = makeSector(QString::fromUtf8("有色金属"),
                                       AdviceAction::Increase, 0.42, 5.8, 11.2);
    sector.tech.rsiOverbought = true;

    const RecommendationRecord record = RecommendationTracker::evaluate(
        sector, nullptr, QDateTime(QDate(2026, 6, 23), QTime(10, 0), Qt::UTC));

    expect(record.state == RecommendationState::OverheatedNoChase,
           "large rise enters overheated no-chase state");
    expect(record.entryTimingScore < record.directionScore,
           "entry timing score is compressed when price is overheated");
    expect(record.warningReason.contains(QString::fromUtf8("避免")),
           "overheated state explains no-chase risk");
}

void verifyRiskWarningKeepsPreviousRecommendation()
{
    RecommendationRecord previous = previousActive(QString::fromUtf8("有色金属"));
    SectorSnapshot sector = makeSector(QString::fromUtf8("有色金属"),
                                       AdviceAction::Hold, 0.26, -8.0, -2.6);

    const RecommendationRecord record = RecommendationTracker::evaluate(
        sector, &previous, QDateTime(QDate(2026, 6, 23), QTime(10, 0), Qt::UTC));

    expect(record.state == RecommendationState::RiskWarning,
           "previous recommendation remains visible as risk warning after a sharp drop");
    expect(record.active, "risk warning remains an active tracking record");
    expect(record.warningReason.contains(QString::fromUtf8("复核")),
           "risk warning explains the review requirement");
}

void verifyPullbackWatch()
{
    SectorSnapshot sector = makeSector(QString::fromUtf8("半导体"),
                                       AdviceAction::Hold, 0.24, -1.5, -1.2);

    const RecommendationRecord record = RecommendationTracker::evaluate(
        sector, nullptr, QDateTime(QDate(2026, 6, 23), QTime(10, 0), Qt::UTC));

    expect(record.state == RecommendationState::PullbackWatch,
           "positive direction with mild pullback enters pullback watch");
    expect(record.stateReason.contains(QString::fromUtf8("观察")),
           "pullback watch keeps an observation reason");
}

void verifyInvalidated()
{
    RecommendationRecord previous = previousActive(QString::fromUtf8("锂电池"));
    SectorSnapshot sector = makeSector(QString::fromUtf8("锂电池"),
                                       AdviceAction::Decrease, -0.24, -1.1, -5.5);

    const RecommendationRecord record = RecommendationTracker::evaluate(
        sector, &previous, QDateTime(QDate(2026, 6, 23), QTime(10, 0), Qt::UTC));

    expect(record.state == RecommendationState::Invalidated,
           "negative direction invalidates a previous recommendation");
    expect(!record.active, "invalidated records are not active");
    expect(record.invalidationCondition.contains(QString::fromUtf8("方向分")),
           "invalidated record keeps explicit invalidation condition");
}

void verifyUpdateAndSerialization()
{
    QList<SectorSnapshot> sectors;
    sectors << makeSector(QString::fromUtf8("有色金属"), AdviceAction::Increase, 0.42, 5.8, 11.2)
            << makeSector(QString::fromUtf8("半导体"), AdviceAction::Hold, 0.24, -1.5, -1.2)
            << makeSector(QString::fromUtf8("低波红利"), AdviceAction::Hold, 0.02, 0.1, 0.4);

    RecommendationTracker tracker(QStringLiteral("tests/recommendation-smoke"));
    const QList<RecommendationRecord> records = tracker.update(
        sectors, QDateTime(QDate(2026, 6, 23), QTime(10, 0), Qt::UTC), false);

    expect(records.size() == 2, "tracker only emits meaningful lifecycle records");
    expect(sectors.first().recommendationStateLabel == recommendationStateLabel(RecommendationState::OverheatedNoChase),
           "tracker annotates sector with lifecycle label");
    expect(!sectors.first().recommendationReason.isEmpty(), "tracker annotates sector with reason");

    const QByteArray json = RecommendationTracker::recordsToJson(records);
    const QList<RecommendationRecord> restored = RecommendationTracker::recordsFromJson(json);
    expect(restored.size() == records.size(), "records round-trip through JSON");
    expect(restored.first().sector == records.first().sector, "serialization keeps sector");
    expect(restored.first().state == records.first().state, "serialization keeps state");
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    verifyOverheatedNoChase();
    verifyRiskWarningKeepsPreviousRecommendation();
    verifyPullbackWatch();
    verifyInvalidated();
    verifyUpdateAndSerialization();

    if (failures > 0) {
        QTextStream(stderr) << failures << " recommendation tracker smoke check(s) failed.\n";
        return 1;
    }

    QTextStream(stdout) << "RecommendationTracker smoke passed.\n";
    return 0;
}
