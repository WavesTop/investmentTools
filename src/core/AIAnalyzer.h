#pragma once

#include <QString>
#include <QList>
#include <QMap>

#include "domain/AnalysisResult.h"
#include "providers/RealFinanceNewsProvider.h"

struct AIProvider
{
    QString name;
    QString apiUrl;
    QString apiKey;
    QString model;
};

struct AINewsSignal
{
    QString sector;
    double sentiment = 0.0;
    int impactLevel = 0;
    QString keyEvent;
    QString originalHeadline;
};

struct AINewsDigestResult
{
    QList<AINewsSignal> newsSignals;
    bool valid = false;
    QString errorMessage;
};

struct AICollaborationParseResult
{
    bool valid = false;
    QString errorMessage;
    QMap<QString, AIReadableInsight> sectors;
};

class AIAnalyzer
{
public:
    AIAnalyzer();

    bool isAvailable() const;
    bool isEnabled() const { return m_enabled; }
    void setEnabled(bool enabled) { m_enabled = enabled; }
    int providerCount() const;
    int deepAnalysisTopN() const { return m_deepAnalysisTopN; }
    QList<AIProvider> providers() const { return m_providers; }

    AINewsDigestResult digestNews(const QList<RawHeadline> &headlines,
                                  const QStringList &sectorNames) const;

    void enhance(AnalysisResult &result, int maxSectors = 0) const;

    QString chatQuery(const QString &userQuestion, const AnalysisResult &context) const;

    AICollaborationParseResult parseCollaborationResponse(const QString &content) const;

    void saveEnabledToConfig(bool enabled) const;
    void configureProviders(const QList<AIProvider> &providers, bool enabled, int deepAnalysisTopN);

    struct SectorAIResult
    {
        QString analysis;
        QString predictionReason;
        QString suggestedAction;
        QString trendSummary;
        QString investmentStrategy;
        QStringList positiveFactors;
        QStringList negativeFactors;
        QStringList futureEvents;
        AIReadableInsight readable;
    };

private:
    struct ProviderResult
    {
        QString providerName;
        QString overallSummary;
        QString methodologyNote;
        QString errorMessage;
        bool valid = false;
        QMap<QString, SectorAIResult> sectors;
    };

    QString buildDigestPrompt(const QList<RawHeadline> &headlines,
                              const QStringList &sectorNames) const;
    AINewsDigestResult parseDigestResponse(const QString &content) const;

    QString buildDeepPrompt(const AnalysisResult &result, int maxSectors) const;
    ProviderResult callProvider(const AIProvider &provider, const QString &prompt, int maxTokens = 6000) const;
    void aggregateResults(const QList<ProviderResult> &results, AnalysisResult &output, int maxSectors) const;

    QList<AIProvider> m_providers;
    bool m_enabled = true;
    int m_deepAnalysisTopN = 20;
};
