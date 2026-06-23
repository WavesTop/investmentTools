#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QList>
#include <QString>

#include "domain/AnalysisResult.h"

class RecommendationTracker
{
public:
    explicit RecommendationTracker(const QString &settingsKey = QStringLiteral("recommendations/lifecycleRecords"));

    QList<RecommendationRecord> update(QList<SectorSnapshot> &sectors,
                                        const QDateTime &now = QDateTime::currentDateTime(),
                                        bool persist = true) const;

    static RecommendationRecord evaluate(const SectorSnapshot &sector,
                                         const RecommendationRecord *previous,
                                         const QDateTime &now);
    static double computeDirectionScore(const SectorSnapshot &sector);
    static double computeEntryTimingScore(const SectorSnapshot &sector, double directionScore);

    static QByteArray recordsToJson(const QList<RecommendationRecord> &records);
    static QList<RecommendationRecord> recordsFromJson(const QByteArray &json);

private:
    QList<RecommendationRecord> loadRecords() const;
    void saveRecords(const QList<RecommendationRecord> &records) const;

    QString m_settingsKey;
};
