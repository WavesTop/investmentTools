#pragma once

#include <QDateTime>
#include <QString>

enum class SignalDirection
{
    Positive,
    Negative,
    Neutral
};

struct IndustrySignal
{
    QString sourceName;
    QString sourceUrl;
    QString industry;
    QString event;
    SignalDirection direction = SignalDirection::Neutral;
    double strength = 0.0; // [-1.0, 1.0]
    double dataQuality = 0.0;
    QString reason;
    QDateTime timestamp;
};
