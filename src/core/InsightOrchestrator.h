#pragma once

#include <QMap>
#include <QString>
#include <functional>

#include "core/AIAnalyzer.h"
#include "core/AdviceEngine.h"
#include "core/DataCollector.h"
#include "core/ImpactAnalyzer.h"
#include "core/MarketContext.h"
#include "core/ReportComposer.h"
#include "core/RotationDetector.h"
#include "core/SectorFetcher.h"
#include "core/SignalExtractor.h"
#include "domain/AnalysisResult.h"

using ProgressCallback = std::function<void(int percent, const QString &stage)>;

class InsightOrchestrator
{
public:
    InsightOrchestrator();
    AnalysisResult runAnalysis(ProgressCallback progress = nullptr) const;
    QString chatQuery(const QString &question, const AnalysisResult &context) const;
    int aiProviderCount() const { return m_aiAnalyzer.providerCount(); }

    bool isAIEnabled() const { return m_aiAnalyzer.isEnabled(); }
    bool isAIAvailable() const { return m_aiAnalyzer.isAvailable(); }
    void setAIEnabled(bool enabled);
    QList<AIProvider> aiProviders() const { return m_aiAnalyzer.providers(); }
    void configureAI(const QList<AIProvider> &providers, bool enabled, int deepAnalysisTopN);

    // portfolio: sectorName -> total invested amount in 元
    void setPortfolio(const QMap<QString, double> &portfolio) { m_portfolio = portfolio; }
    const QMap<QString, double> &portfolio() const { return m_portfolio; }

private:
    QMap<QString, double> m_portfolio;
    DataCollector m_dataCollector;
    SectorFetcher m_sectorFetcher;
    MarketContextFetcher m_marketCtxFetcher;
    SignalExtractor m_signalExtractor;
    ImpactAnalyzer m_impactAnalyzer;
    AdviceEngine m_adviceEngine;
    ReportComposer m_reportComposer;
    mutable AIAnalyzer m_aiAnalyzer;
};
