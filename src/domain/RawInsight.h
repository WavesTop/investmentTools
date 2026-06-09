#pragma once

#include <QDateTime>
#include <QString>

struct RawInsight
{
    QString sourceName;
    QString sourceUrl;
    QString industry;
    QString headline;
    QString detail;
    QDateTime timestamp;
    double dataQuality = 0.0; // [0.0, 1.0]
};
