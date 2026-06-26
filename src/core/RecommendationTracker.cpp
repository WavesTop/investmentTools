#include "core/RecommendationTracker.h"

#include <algorithm>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QSettings>
#include <QtGlobal>

namespace {

QString actionKey(AdviceAction action)
{
    switch (action) {
    case AdviceAction::Increase:
        return QStringLiteral("increase");
    case AdviceAction::Decrease:
        return QStringLiteral("decrease");
    case AdviceAction::Hold:
    default:
        return QStringLiteral("hold");
    }
}

AdviceAction actionFromKey(const QString &key)
{
    if (key == QStringLiteral("increase")) return AdviceAction::Increase;
    if (key == QStringLiteral("decrease")) return AdviceAction::Decrease;
    return AdviceAction::Hold;
}

QJsonObject toJson(const RecommendationRecord &record)
{
    QJsonObject object;
    object.insert(QStringLiteral("sector"), record.sector);
    object.insert(QStringLiteral("state"), recommendationStateKey(record.state));
    object.insert(QStringLiteral("firstAction"), actionKey(record.firstAction));
    object.insert(QStringLiteral("currentAction"), actionKey(record.currentAction));
    object.insert(QStringLiteral("firstSeenAt"), record.firstSeenAt.toString(Qt::ISODate));
    object.insert(QStringLiteral("lastSeenAt"), record.lastSeenAt.toString(Qt::ISODate));
    object.insert(QStringLiteral("trackingDays"), record.trackingDays);
    object.insert(QStringLiteral("initialScore"), record.initialScore);
    object.insert(QStringLiteral("currentScore"), record.currentScore);
    object.insert(QStringLiteral("initialTodayChangePct"), record.initialTodayChangePct);
    object.insert(QStringLiteral("todayChangePct"), record.todayChangePct);
    object.insert(QStringLiteral("fiveDayMomentum"), record.fiveDayMomentum);
    object.insert(QStringLiteral("directionScore"), record.directionScore);
    object.insert(QStringLiteral("entryTimingScore"), record.entryTimingScore);
    object.insert(QStringLiteral("returnSinceFirst"), record.returnSinceFirst);
    object.insert(QStringLiteral("stateReason"), record.stateReason);
    object.insert(QStringLiteral("warningReason"), record.warningReason);
    object.insert(QStringLiteral("invalidationCondition"), record.invalidationCondition);
    object.insert(QStringLiteral("active"), record.active);
    return object;
}

RecommendationRecord fromJson(const QJsonObject &object)
{
    RecommendationRecord record;
    record.sector = object.value(QStringLiteral("sector")).toString();
    record.state = recommendationStateFromKey(object.value(QStringLiteral("state")).toString());
    record.firstAction = actionFromKey(object.value(QStringLiteral("firstAction")).toString());
    record.currentAction = actionFromKey(object.value(QStringLiteral("currentAction")).toString());
    record.firstSeenAt = QDateTime::fromString(object.value(QStringLiteral("firstSeenAt")).toString(), Qt::ISODate);
    record.lastSeenAt = QDateTime::fromString(object.value(QStringLiteral("lastSeenAt")).toString(), Qt::ISODate);
    record.trackingDays = object.value(QStringLiteral("trackingDays")).toInt();
    record.initialScore = object.value(QStringLiteral("initialScore")).toDouble();
    record.currentScore = object.value(QStringLiteral("currentScore")).toDouble();
    record.initialTodayChangePct = object.value(QStringLiteral("initialTodayChangePct")).toDouble();
    record.todayChangePct = object.value(QStringLiteral("todayChangePct")).toDouble();
    record.fiveDayMomentum = object.value(QStringLiteral("fiveDayMomentum")).toDouble();
    record.directionScore = object.value(QStringLiteral("directionScore")).toDouble();
    record.entryTimingScore = object.value(QStringLiteral("entryTimingScore")).toDouble();
    record.returnSinceFirst = object.value(QStringLiteral("returnSinceFirst")).toDouble();
    record.stateReason = object.value(QStringLiteral("stateReason")).toString();
    record.warningReason = object.value(QStringLiteral("warningReason")).toString();
    record.invalidationCondition = object.value(QStringLiteral("invalidationCondition")).toString();
    record.active = object.value(QStringLiteral("active")).toBool();
    return record;
}

bool isOverheated(const SectorSnapshot &sector)
{
    const bool singleDayHot = sector.todayChangePctValid && sector.todayChangePct >= 4.0;
    return singleDayHot
        || sector.fiveDayMomentum >= 8.0
        || sector.overheat.compositeOverheat >= 65.0
        || sector.tech.rsiOverbought
        || sector.crowdingIndex >= 75.0;
}

int statePriority(RecommendationState state)
{
    switch (state) {
    case RecommendationState::RiskWarning:
        return 0;
    case RecommendationState::PullbackWatch:
        return 1;
    case RecommendationState::OverheatedNoChase:
        return 2;
    case RecommendationState::NewSignal:
        return 3;
    case RecommendationState::Active:
        return 4;
    case RecommendationState::LogicReview:
        return 5;
    case RecommendationState::Invalidated:
        return 6;
    case RecommendationState::None:
    default:
        return 9;
    }
}

} // namespace

RecommendationTracker::RecommendationTracker(const QString &settingsKey)
    : m_settingsKey(settingsKey)
{
}

double RecommendationTracker::computeDirectionScore(const SectorSnapshot &sector)
{
    double score = sector.forecastScore;
    score -= sector.formula.todayFactor;
    score -= sector.formula.hotnessFactor * 0.6;
    score -= sector.formula.momentumFactor * 0.35;
    score += qBound(-0.08, sector.eventCatalystScore * 0.08, 0.08);
    score += qBound(-0.05, sector.formula.fundFlowFactor * 0.5, 0.05);
    return qBound(-1.0, score, 1.0);
}

double RecommendationTracker::computeEntryTimingScore(const SectorSnapshot &sector, double directionScore)
{
    double score = directionScore * 0.45 + sector.formula.techFactor * 0.35
        + sector.formula.fundFlowFactor * 0.25;

    if (sector.todayChangePctValid) {
        if (sector.todayChangePct >= 5.0) score -= 0.45;
        else if (sector.todayChangePct >= 3.0) score -= 0.28;
        else if (sector.todayChangePct < 0.0 && directionScore > 0.10) score += 0.08;

        if (sector.todayChangePct <= -6.0) score -= 0.40;
        else if (sector.todayChangePct <= -3.0) score -= 0.15;
    }

    if (sector.fiveDayMomentum >= 10.0) score -= 0.28;
    else if (sector.fiveDayMomentum >= 8.0) score -= 0.18;
    else if (sector.fiveDayMomentum < -2.0 && directionScore > 0.10) score += 0.06;

    if (sector.overheat.compositeOverheat >= 65.0) score -= 0.22;
    if (sector.tech.rsiOverbought) score -= 0.18;
    if (sector.crowdingIndex >= 75.0) score -= 0.16;
    if (sector.dataQualityScore > 0.0 && sector.dataQualityScore < 55.0) score -= 0.12;

    if (sector.priceLevelPlan.valid) {
        const QString action = sector.priceLevelPlan.actionLabel;
        if (action.contains(QString::fromUtf8("过热不追"))
            || action.contains(QString::fromUtf8("风险"))
            || action.contains(QString::fromUtf8("失效"))) {
            score -= 0.22;
        } else if (sector.priceLevelPlan.riskRewardRatio > 0.0
                   && sector.priceLevelPlan.riskRewardRatio < 1.5) {
            score -= 0.18;
        } else if (action.contains(QString::fromUtf8("回调分批"))
                   && sector.priceLevelPlan.riskRewardRatio >= 1.8) {
            score += 0.06;
        } else if (action.contains(QString::fromUtf8("观察"))) {
            score -= 0.04;
        }
    }

    return qBound(-1.0, score, 1.0);
}

RecommendationRecord RecommendationTracker::evaluate(const SectorSnapshot &sector,
                                                     const RecommendationRecord *previous,
                                                     const QDateTime &now)
{
    const bool hasPrevious = previous && !previous->sector.isEmpty();
    const bool previousWorthTracking = hasPrevious
        && previous->state != RecommendationState::None
        && previous->state != RecommendationState::Invalidated;

    RecommendationRecord record;
    record.sector = sector.industry;
    record.firstSeenAt = hasPrevious ? previous->firstSeenAt : now;
    record.lastSeenAt = now;
    record.firstAction = hasPrevious ? previous->firstAction : sector.action;
    record.currentAction = sector.action;
    record.initialScore = hasPrevious ? previous->initialScore : sector.forecastScore;
    record.currentScore = sector.forecastScore;
    record.initialTodayChangePct = hasPrevious ? previous->initialTodayChangePct : sector.todayChangePct;
    record.todayChangePct = sector.todayChangePct;
    record.fiveDayMomentum = sector.fiveDayMomentum;
    record.directionScore = computeDirectionScore(sector);
    record.entryTimingScore = computeEntryTimingScore(sector, record.directionScore);
    record.returnSinceFirst = hasPrevious
        ? sector.todayChangePct - previous->initialTodayChangePct
        : 0.0;
    record.trackingDays = record.firstSeenAt.isValid()
        ? static_cast<int>(qMax<qint64>(1, record.firstSeenAt.date().daysTo(now.date()) + 1))
        : 1;
    record.invalidationCondition = QString::fromUtf8(
        "方向分低于 -0.12、动作降为减配、事件催化失效或资金/技术面继续转弱");

    const bool shouldTrack = previousWorthTracking
        || sector.action == AdviceAction::Increase
        || sector.forecastScore >= 0.18
        || record.directionScore >= 0.12;
    if (!shouldTrack) {
        record.state = RecommendationState::None;
        record.stateReason = QString::fromUtf8("未达到推荐跟踪条件");
        return record;
    }

    const bool violentDrop = sector.todayChangePctValid && sector.todayChangePct <= -6.0;
    const bool trackedDrawdown = hasPrevious && record.returnSinceFirst <= -8.0;
    const bool invalidated = record.directionScore <= -0.12 || sector.action == AdviceAction::Decrease;
    const bool overheated = isOverheated(sector);
    const bool pullback = record.directionScore >= 0.10
        && sector.todayChangePctValid
        && sector.todayChangePct <= -0.8;
    const bool needsReview = hasPrevious
        && (record.directionScore < 0.04
            || (sector.dataQualityScore > 0.0 && sector.dataQualityScore < 45.0));

    if (hasPrevious && (violentDrop || trackedDrawdown) && record.directionScore > -0.18) {
        record.state = RecommendationState::RiskWarning;
        record.warningReason = violentDrop
            ? QString::fromUtf8("今日跌幅达到 %1%，需要复核推荐逻辑和仓位风险")
                  .arg(sector.todayChangePct, 0, 'f', 2)
            : QString::fromUtf8("推荐后相对首次记录回撤 %1%，需要进入风险复核")
                  .arg(qAbs(record.returnSinceFirst), 0, 'f', 2);
        record.stateReason = QString::fromUtf8("保留跟踪记录，不因下跌静默移除");
    } else if (invalidated) {
        record.state = RecommendationState::Invalidated;
        record.stateReason = QString::fromUtf8("方向分或最终动作已经转弱，推荐逻辑失效");
    } else if (overheated) {
        record.state = RecommendationState::OverheatedNoChase;
        record.stateReason = QString::fromUtf8("方向仍有支撑，但短期涨幅、拥挤度或技术指标偏热");
        record.warningReason = QString::fromUtf8("等待涨幅消化或回踩确认，避免把涨幅当成追入理由");
    } else if (pullback) {
        record.state = RecommendationState::PullbackWatch;
        record.stateReason = QString::fromUtf8("方向分仍为正，价格回落后进入观察队列");
    } else if (needsReview) {
        record.state = RecommendationState::LogicReview;
        record.stateReason = QString::fromUtf8("方向证据变弱或数据质量不足，需要复核后再行动");
    } else if (hasPrevious) {
        record.state = RecommendationState::Active;
        record.stateReason = QString::fromUtf8("方向和入场时机暂未触发降级条件，继续跟踪");
    } else {
        record.state = RecommendationState::NewSignal;
        record.stateReason = QString::fromUtf8("首次进入推荐跟踪池，等待后续表现验证");
    }

    record.active = record.state != RecommendationState::Invalidated
        && record.state != RecommendationState::None;
    return record;
}

QList<RecommendationRecord> RecommendationTracker::update(QList<SectorSnapshot> &sectors,
                                                          const QDateTime &now,
                                                          bool persist) const
{
    QMap<QString, RecommendationRecord> previousBySector;
    for (const RecommendationRecord &record : loadRecords()) {
        if (!record.sector.isEmpty()) previousBySector.insert(record.sector, record);
    }

    QList<RecommendationRecord> records;
    for (SectorSnapshot &sector : sectors) {
        const RecommendationRecord previous = previousBySector.value(sector.industry);
        const bool hasPrevious = previousBySector.contains(sector.industry);
        const RecommendationRecord record = evaluate(sector, hasPrevious ? &previous : nullptr, now);
        sector.directionScore = record.directionScore;
        sector.entryTimingScore = record.entryTimingScore;
        sector.recommendationStateLabel = recommendationStateLabel(record.state);
        sector.recommendationReason = record.stateReason;
        sector.recommendationWarning = record.warningReason;
        sector.recommendationInvalidation = record.invalidationCondition;
        if (record.state != RecommendationState::None) records.push_back(record);
    }

    std::sort(records.begin(), records.end(), [](const RecommendationRecord &a, const RecommendationRecord &b) {
        const int pa = statePriority(a.state);
        const int pb = statePriority(b.state);
        if (pa != pb) return pa < pb;
        return a.directionScore > b.directionScore;
    });

    while (records.size() > 80) records.removeLast();
    if (persist) saveRecords(records);
    return records;
}

QByteArray RecommendationTracker::recordsToJson(const QList<RecommendationRecord> &records)
{
    QJsonArray array;
    for (const RecommendationRecord &record : records) {
        if (record.state != RecommendationState::None) array.append(toJson(record));
    }
    return QJsonDocument(array).toJson(QJsonDocument::Compact);
}

QList<RecommendationRecord> RecommendationTracker::recordsFromJson(const QByteArray &json)
{
    QList<RecommendationRecord> records;
    const QJsonDocument doc = QJsonDocument::fromJson(json);
    if (!doc.isArray()) return records;
    for (const QJsonValue &value : doc.array()) {
        const RecommendationRecord record = fromJson(value.toObject());
        if (!record.sector.isEmpty() && record.state != RecommendationState::None) {
            records.push_back(record);
        }
    }
    return records;
}

QList<RecommendationRecord> RecommendationTracker::loadRecords() const
{
    QSettings settings(QStringLiteral("InvestInsight"), QStringLiteral("InvestInsight"));
    return recordsFromJson(settings.value(m_settingsKey).toByteArray());
}

void RecommendationTracker::saveRecords(const QList<RecommendationRecord> &records) const
{
    QSettings settings(QStringLiteral("InvestInsight"), QStringLiteral("InvestInsight"));
    settings.setValue(m_settingsKey, recordsToJson(records));
}
