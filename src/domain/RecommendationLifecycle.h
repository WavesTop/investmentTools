#pragma once

#include <QDateTime>
#include <QString>

#include "domain/InvestmentAdvice.h"

enum class RecommendationState
{
    None,
    NewSignal,
    Active,
    OverheatedNoChase,
    PullbackWatch,
    RiskWarning,
    LogicReview,
    Invalidated
};

struct RecommendationRecord
{
    QString sector;
    RecommendationState state = RecommendationState::None;
    AdviceAction firstAction = AdviceAction::Hold;
    AdviceAction currentAction = AdviceAction::Hold;
    QDateTime firstSeenAt;
    QDateTime lastSeenAt;
    int trackingDays = 0;
    double initialScore = 0.0;
    double currentScore = 0.0;
    double initialTodayChangePct = 0.0;
    double todayChangePct = 0.0;
    double fiveDayMomentum = 0.0;
    double directionScore = 0.0;
    double entryTimingScore = 0.0;
    double returnSinceFirst = 0.0;
    QString stateReason;
    QString warningReason;
    QString invalidationCondition;
    bool active = false;
};

inline QString recommendationStateKey(RecommendationState state)
{
    switch (state) {
    case RecommendationState::NewSignal:
        return QStringLiteral("new_signal");
    case RecommendationState::Active:
        return QStringLiteral("active");
    case RecommendationState::OverheatedNoChase:
        return QStringLiteral("overheated_no_chase");
    case RecommendationState::PullbackWatch:
        return QStringLiteral("pullback_watch");
    case RecommendationState::RiskWarning:
        return QStringLiteral("risk_warning");
    case RecommendationState::LogicReview:
        return QStringLiteral("logic_review");
    case RecommendationState::Invalidated:
        return QStringLiteral("invalidated");
    case RecommendationState::None:
    default:
        return QStringLiteral("none");
    }
}

inline RecommendationState recommendationStateFromKey(const QString &key)
{
    if (key == QStringLiteral("new_signal")) return RecommendationState::NewSignal;
    if (key == QStringLiteral("active")) return RecommendationState::Active;
    if (key == QStringLiteral("overheated_no_chase")) return RecommendationState::OverheatedNoChase;
    if (key == QStringLiteral("pullback_watch")) return RecommendationState::PullbackWatch;
    if (key == QStringLiteral("risk_warning")) return RecommendationState::RiskWarning;
    if (key == QStringLiteral("logic_review")) return RecommendationState::LogicReview;
    if (key == QStringLiteral("invalidated")) return RecommendationState::Invalidated;
    return RecommendationState::None;
}

inline QString recommendationStateLabel(RecommendationState state)
{
    switch (state) {
    case RecommendationState::NewSignal:
        return QString::fromUtf8("新信号");
    case RecommendationState::Active:
        return QString::fromUtf8("推荐中");
    case RecommendationState::OverheatedNoChase:
        return QString::fromUtf8("过热不追");
    case RecommendationState::PullbackWatch:
        return QString::fromUtf8("回调观察");
    case RecommendationState::RiskWarning:
        return QString::fromUtf8("风险预警");
    case RecommendationState::LogicReview:
        return QString::fromUtf8("逻辑复核");
    case RecommendationState::Invalidated:
        return QString::fromUtf8("失效移除");
    case RecommendationState::None:
    default:
        return QString::fromUtf8("未跟踪");
    }
}
