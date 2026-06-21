#include "core/InsightOrchestrator.h"

#include <algorithm>
#include <cmath>
#include <QDate>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutex>
#include <QSet>
#include <QSettings>
#include <QtConcurrent>
#include <QtGlobal>
#include <QtMath>
#include <memory>

#include "core/TechIndicators.h"
#include "core/StrategyEngine.h"
#include "core/RotationDetector.h"
#include "core/TrendStageDetector.h"
#include "core/TrendHealthAnalyzer.h"
#include "core/BreadthAnalyzer.h"
#include "core/OverheatDetector.h"
#include "core/FlowStructureAnalyzer.h"
#include "core/MarketRegimeDetector.h"
#include "core/ExplainabilityEngine.h"
#include "core/EventExtractionEngine.h"
#include "core/EventRepository.h"
#include "core/ImpactGraphEngine.h"
#include "core/SectorImpactAnalyzer.h"
#include "providers/RealFinanceNewsProvider.h"
#include "providers/RealLinkageProvider.h"

InsightOrchestrator::InsightOrchestrator()
{
    m_dataCollector.addProvider(std::make_unique<RealFinanceNewsProvider>());
}

void InsightOrchestrator::setAIEnabled(bool enabled)
{
    m_aiAnalyzer.setEnabled(enabled);
    m_aiAnalyzer.saveEnabledToConfig(enabled);
}

void InsightOrchestrator::configureAI(const QList<AIProvider> &providers, bool enabled, int deepAnalysisTopN)
{
    m_aiAnalyzer.configureProviders(providers, enabled, deepAnalysisTopN);
}

QString InsightOrchestrator::chatQuery(const QString &question, const AnalysisResult &context) const
{
    return m_aiAnalyzer.chatQuery(question, context);
}

namespace {
void reportProgress(const ProgressCallback &cb, int pct, const QString &stage)
{
    if (cb) cb(pct, stage);
}

AdviceAction chooseAction(double forecast)
{
    // 阈值加宽+对称，减少盘中微小波动导致的标签跳变
    if (forecast >= 0.22) return AdviceAction::Increase;
    if (forecast <= -0.22) return AdviceAction::Decrease;
    return AdviceAction::Hold;
}

QString toTrendSummary(double forecast, double todayPct, double mom5d)
{
    if (forecast >= 0.40 && mom5d > 1.5) return "强势看多";
    if (forecast >= 0.22) return "偏多";
    if (forecast <= -0.40 && mom5d < -1.5) return "强势看空";
    if (forecast <= -0.22) return "偏空";
    if (qAbs(forecast) < 0.05 && qAbs(todayPct) < 0.5 && qAbs(mom5d) < 1.0) return "横盘震荡";
    return "方向不明";
}

double computeDataQualityScore(const SectorInfo &si, int newsCount)
{
    const double klineCoverage = qMin(1.0, si.klineSeries.size() / 60.0);
    const double weeklyCoverage = qMin(1.0, si.weekSeries.size() / 8.0);
    const double monthlyCoverage = qMin(1.0, si.monthSeries.size() / 6.0);
    const double fundFlowCoverage = qMin(1.0, si.fundFlowSeries.size() / 20.0);
    const double valuationCoverage = (si.peRatio > 0 || si.pbRatio > 0 || si.totalMarketCap > 0) ? 1.0 : 0.0;
    const double newsCoverage = qMin(1.0, newsCount / 6.0);
    const double mtfCoverage = (weeklyCoverage + monthlyCoverage) / 2.0;

    const double score = (
        klineCoverage * 0.30 +
        fundFlowCoverage * 0.20 +
        valuationCoverage * 0.20 +
        newsCoverage * 0.20 +
        mtfCoverage * 0.10) * 100.0;
    return qBound(0.0, score, 100.0);
}

QString dataQualityNote(double score)
{
    if (score >= 80.0) return "高质量：数据覆盖完整，信号可靠性较高";
    if (score >= 60.0) return "中等质量：核心数据可用，建议结合风控使用";
    if (score >= 40.0) return "一般质量：部分关键数据缺失，建议谨慎解读";
    return "低质量：数据缺口较大，本次结论仅供参考";
}

double sourceConsistencyWeight(double score)
{
    // 一致性越低，评分越要保守
    return qBound(0.70, 0.70 + 0.30 * (score / 100.0), 1.00);
}

// 统一的 action → 中文映射
QString actionToLabel(AdviceAction act)
{
    switch (act) {
    case AdviceAction::Increase: return "增配";
    case AdviceAction::Decrease: return "减配";
    case AdviceAction::Hold:
    default: return "持有";
    }
}

QString buildNarrative(const QString &industry, double todayPct, double mom5d, double mom20d,
    double sentiment, int newsHits, int posNews, int negNews,
    double forecast, double hotScore, int stockCount)
{
    QString text;
    text += industry + "板块今日";
    text += todayPct >= 0 ? "上涨" : "下跌";
    text += QString::number(qAbs(todayPct), 'f', 2) + "%。";

    if (qAbs(mom5d) > 0.5) {
        text += "近5个交易日累计";
        text += mom5d > 0 ? "上涨" : "下跌";
        text += QString::number(qAbs(mom5d), 'f', 2) + "%，";
        text += "短期动量" + QString(mom5d > 0 ? "偏强" : "偏弱") + "。";
    }

    if (qAbs(mom20d) > 1.0) {
        text += "近20个交易日" + QString(mom20d > 0 ? "上涨" : "下跌");
        text += QString::number(qAbs(mom20d), 'f', 2) + "%，中期趋势" + QString(mom20d > 0 ? "向好" : "承压") + "。";
    }

    if (newsHits > 0) {
        text += "\n\n本次共匹配到 " + QString::number(newsHits) + " 条相关新闻，其中";
        text += "正面信号 " + QString::number(posNews) + " 条、";
        text += "负面信号 " + QString::number(negNews) + " 条、";
        text += "中性 " + QString::number(newsHits - posNews - negNews) + " 条。";
        if (posNews > negNews) text += "整体新闻面偏积极。";
        else if (negNews > posNews) text += "整体新闻面偏消极。";
        else text += "新闻面多空交织。";
    }

    text += "\n\n综合短期动量（5日）、中期趋势（20日）、今日涨跌、新闻情绪四个维度";
    if (stockCount > 0) {
        text += "，以及板块内 " + QString::number(stockCount) + " 只成分股的活跃度";
    }
    text += "，";
    if (forecast >= 0.15) {
        text += "模型判断短期内该板块大概率延续上行，建议适当增加配置。";
    } else if (forecast <= -0.12) {
        text += "模型判断短期内该板块面临调整压力，建议适当降低配置或观望。";
    } else {
        text += "模型判断短期方向不明确，建议维持现有仓位，等待更清晰的信号。";
    }

    return text;
}

QString eventDirectionLabel(EventImpactDirection direction)
{
    switch (direction) {
    case EventImpactDirection::Positive:
        return QString::fromUtf8("偏正面");
    case EventImpactDirection::Negative:
        return QString::fromUtf8("偏负面");
    case EventImpactDirection::Mixed:
        return QString::fromUtf8("多空交织");
    case EventImpactDirection::Neutral:
    default:
        return QString::fromUtf8("中性");
    }
}

QString buildEventSummary(const QList<SectorEventImpact> &impacts)
{
    if (impacts.isEmpty()) return QString();
    const SectorEventImpact &impact = impacts.first();
    return QString::fromUtf8("%1：%2，%3")
        .arg(impact.eventTitle, eventDirectionLabel(impact.direction), impact.path);
}
} // namespace

AnalysisResult InsightOrchestrator::runAnalysis(ProgressCallback progress) const
{
    AnalysisResult result;
    const bool useAI = m_aiAnalyzer.isEnabled() && m_aiAnalyzer.isAvailable();

    // ========== 阶段 1: 板块列表（极快 ~2s）==========
    reportProgress(progress, 3, "正在获取行业板块列表...");
    QList<SectorInfo> liveSectors = m_sectorFetcher.fetchSectorList();
    if (liveSectors.isEmpty()) {
        liveSectors = {
            { "new_bdtj", "半导体", "BK1036", 0, 0, 0, 0, 0 },
            { "new_xny", "新能源", "BK0478", 0, 0, 0, 0, 0 },
            { "new_yy", "医药", "BK0465", 0, 0, 0, 0, 0 },
            { "new_sp", "消费", "BK0438", 0, 0, 0, 0, 0 },
        };
    }

    QStringList sectorNames;
    for (const SectorInfo &s : liveSectors) sectorNames.push_back(s.name);

    // ========== 阶段 2: K线+资金流 与 新闻拉取 与 大盘指数 并行 ==========
    reportProgress(progress, 5, "正在并行拉取行情数据、新闻和大盘数据...");

    RealFinanceNewsProvider *newsProvider = nullptr;
    for (const auto &provider : m_dataCollector.providers()) {
        newsProvider = dynamic_cast<RealFinanceNewsProvider *>(provider.get());
        if (newsProvider) { newsProvider->setSectorNames(sectorNames); break; }
    }

    // 路线 A: K线+资金流
    QFuture<void> klineFuture = QtConcurrent::run([this, &liveSectors]() {
        m_sectorFetcher.fetchMarketData(liveSectors);
    });

    // 路线 B: 新闻（内部已并行化）
    QList<RawInsight> rawData;
    QFuture<QList<RawInsight>> newsFuture = QtConcurrent::run([this]() {
        return m_dataCollector.collectAll();
    });

    // 路线 C: 大盘指数 + 北向资金 + 涨跌家数
    QFuture<MarketContext> marketCtxFuture = QtConcurrent::run([this]() {
        return m_marketCtxFetcher.fetch();
    });

    // 等待新闻先完成（通常比 K线快），然后立即启动 AI Stage 1
    newsFuture.waitForFinished();
    rawData = newsFuture.result();
    reportProgress(progress, 20, "新闻数据就绪，正在分析...");

    QList<RawHeadline> eventHeadlines;
    QSet<QString> eventHeadlineKeys;
    for (const RawInsight &ins : rawData) {
        const QString key = ins.sourceName + "|" + ins.headline + "|" + ins.detail;
        if (eventHeadlineKeys.contains(key)) continue;
        eventHeadlineKeys.insert(key);
        eventHeadlines.push_back({ins.sourceName, ins.headline, ins.detail, ins.timestamp});
    }
    if (newsProvider) {
        for (const RawHeadline &headline : newsProvider->rawHeadlines()) {
            const QString key = headline.source + "|" + headline.title + "|" + headline.summary;
            if (eventHeadlineKeys.contains(key)) continue;
            eventHeadlineKeys.insert(key);
            eventHeadlines.push_back(headline);
        }
    }

    // ========== 阶段 3: 信号提取 + AI Stage 1（与 K线并行）==========
    struct NewsAggregate {
        double sentimentSum = 0.0;
        int count = 0;
        int positiveCount = 0;
        int negativeCount = 0;
        QStringList headlines;
        QStringList keyEvents;
        QList<EventMarker> eventMarkers;
        QStringList upcomingEvents;
        QVector<NewsEntry> entries;
    };

    QHash<QString, NewsAggregate> newsGrouped;
    for (const QString &name : sectorNames) newsGrouped[name] = NewsAggregate{};

    const QList<IndustrySignal> extractedSignals = m_signalExtractor.extract(rawData);
    for (const IndustrySignal &signal : extractedSignals) {
        if (!newsGrouped.contains(signal.industry)) continue;
        NewsAggregate &na = newsGrouped[signal.industry];
        na.sentimentSum += signal.strength;
        na.count += 1;
        if (signal.direction == SignalDirection::Positive) na.positiveCount += 1;
        else if (signal.direction == SignalDirection::Negative) na.negativeCount += 1;
        if (na.headlines.size() < 10)
            na.headlines.push_back(signal.event + "（" + signal.sourceName + "）");

        if (na.entries.size() < 20) {
            NewsEntry ne;
            ne.date = signal.timestamp.isValid()
                ? signal.timestamp.toString("yyyy-MM-dd HH:mm")
                : QString();
            ne.title = signal.event;
            ne.source = signal.sourceName;
            ne.url = signal.sourceUrl;
            ne.sentiment = signal.strength;
            na.entries.push_back(ne);
        }

        if (signal.timestamp.isValid() && na.eventMarkers.size() < 20) {
            EventMarker em;
            em.date = signal.timestamp.toString("yyyy-MM-dd");
            em.text = signal.event;
            em.sentiment = signal.strength;
            na.eventMarkers.push_back(em);
        }

        static const QStringList futureKeywords = {
            "将", "预计", "即将", "计划", "拟", "有望", "规划",
            "发布会", "峰会", "展览", "招标", "发射", "上线"
        };
        bool isFutureEvent = false;
        for (const QString &kw : futureKeywords) {
            if (signal.event.contains(kw)) { isFutureEvent = true; break; }
        }
        if (isFutureEvent && na.upcomingEvents.size() < 8)
            na.upcomingEvents.push_back(signal.event);
    }

    // AI Stage 1: 与 K线拉取并行执行
    AINewsDigestResult aiDigest;
    bool aiStage1Running = false;
    QFuture<AINewsDigestResult> aiStage1Future;

    if (useAI && newsProvider && !newsProvider->rawHeadlines().isEmpty()) {
        QList<RawHeadline> aiHeadlines;
        QSet<QString> seenHeadlineKeys;
        for (const RawInsight &ins : rawData) {
            const QString key = ins.headline + "|" + ins.detail;
            if (seenHeadlineKeys.contains(key)) continue;
            seenHeadlineKeys.insert(key);
            aiHeadlines.push_back({ ins.sourceName, ins.headline, ins.detail, ins.timestamp });
        }
        if (aiHeadlines.isEmpty()) aiHeadlines = newsProvider->rawHeadlines();

        reportProgress(progress, 22, "AI 正在智能分析新闻影响（与行情拉取并行）...");
        const AIAnalyzer *analyzer = &m_aiAnalyzer;
        aiStage1Future = QtConcurrent::run([analyzer, aiHeadlines, sectorNames]() {
            return analyzer->digestNews(aiHeadlines, sectorNames);
        });
        aiStage1Running = true;
    }

    // 等待 K线数据完成
    klineFuture.waitForFinished();
    reportProgress(progress, 38, "行情数据就绪");

    // 拉取估值与拥挤数据（依赖K线历史用于历史分位）
    reportProgress(progress, 39, "正在拉取估值与拥挤数据...");
    m_sectorFetcher.fetchValuationData(liveSectors);

    // 等待大盘数据
    marketCtxFuture.waitForFinished();
    result.marketCtx = marketCtxFuture.result();
    reportProgress(progress, 40, result.marketCtx.valid
        ? QString("大盘数据就绪：上证 %1%").arg(result.marketCtx.shanghai.changePct, 0, 'f', 2)
        : "大盘数据获取失败，继续分析...");

    // 等待 AI Stage 1 完成
    if (aiStage1Running) {
        reportProgress(progress, 42, "等待 AI 新闻分析完成...");
        aiStage1Future.waitForFinished();
        aiDigest = aiStage1Future.result();

        if (aiDigest.valid) {
            qDebug() << "[Orchestrator] AI Stage 1 returned" << aiDigest.newsSignals.size() << "signals";
            for (const AINewsSignal &sig : aiDigest.newsSignals) {
                if (!newsGrouped.contains(sig.sector)) continue;
                NewsAggregate &na = newsGrouped[sig.sector];
                na.sentimentSum += sig.sentiment;
                na.count += 1;
                if (sig.sentiment > 0.1) na.positiveCount += 1;
                else if (sig.sentiment < -0.1) na.negativeCount += 1;
                if (!sig.keyEvent.isEmpty() && !na.keyEvents.contains(sig.keyEvent)) {
                    na.keyEvents.push_back(sig.keyEvent);
                    if (na.headlines.size() < 15)
                        na.headlines.push_back(sig.keyEvent + "（AI识别）");
                }
            }
            reportProgress(progress, 48, "AI 新闻分析完成: " + QString::number(aiDigest.newsSignals.size()) + " 条信号");
        } else {
            QString errMsg = aiDigest.errorMessage.isEmpty() ? "AI 新闻分析未返回有效结果" : aiDigest.errorMessage;
            result.aiErrors.push_back(errMsg);
            reportProgress(progress, 48, "AI 新闻分析未成功，使用关键词分析");
        }
    } else {
        reportProgress(progress, 48, useAI ? "无原始新闻可供AI分析" : "AI 已关闭，使用关键词分析");
    }

    // ========== 阶段 3.2: 结构化事件抽取与影响路径 ==========
    EventExtractionEngine eventExtractor;
    ImpactGraphEngine impactGraph;
    SectorImpactAnalyzer sectorImpactAnalyzer;
    EventRepository eventRepository;
    result.macroEvents = eventRepository.trackEvents(eventExtractor.extractFromHeadlines(eventHeadlines));

    QList<SectorEventImpact> allEventImpacts;
    for (const MacroEvent &event : result.macroEvents) {
        allEventImpacts += impactGraph.analyze(event, sectorNames);
    }
    const QMap<QString, double> eventCatalystScores =
        sectorImpactAnalyzer.eventCatalystScores(allEventImpacts);
    QHash<QString, QList<SectorEventImpact>> eventImpactsBySector;
    for (const SectorEventImpact &impact : allEventImpacts) {
        eventImpactsBySector[impact.sector].push_back(impact);
    }

    // ========== 阶段 3.5: 市场状态识别 + 动态权重 ==========
    result.marketRegime = MarketRegimeDetector::detect(result.marketCtx);
    const DynamicFactorWeights &dynW = result.marketRegime.weights;
    qDebug() << "[Regime]" << result.marketRegime.regimeName
             << "conf=" << result.marketRegime.regimeConfidence
             << "momW=" << dynW.momentumWeight << "riskW=" << dynW.riskWeight
             << "valW=" << dynW.valuationWeight;

    // ========== 阶段 4: 生成报告 + 逐板块分析 ==========
    reportProgress(progress, 50, "正在生成分析报告...");
    const QList<FundImpact> impacts = m_impactAnalyzer.analyze(extractedSignals);
    const QList<InvestmentAdvice> advices = m_adviceEngine.generate(impacts);
    result.reportText = m_reportComposer.compose(advices);

    const int totalSectors = liveSectors.size();
    for (int i = 0; i < totalSectors; ++i) {
        const SectorInfo &si = liveSectors[i];
        const NewsAggregate &na = newsGrouped.value(si.name);

        QVector<double> trend = si.klineSeries;
        double mom5d = 0.0, mom20d = 0.0;
        if (trend.size() >= 6) mom5d = (trend.last() - trend[trend.size() - 6]) / trend[trend.size() - 6] * 100.0;
        if (trend.size() >= 21) mom20d = (trend.last() - trend[trend.size() - 21]) / trend[trend.size() - 21] * 100.0;

        double weekMom = 0.0;
        if (si.weekSeries.size() >= 6)
            weekMom = (si.weekSeries.last() - si.weekSeries[si.weekSeries.size() - 6]) / si.weekSeries[si.weekSeries.size() - 6] * 100.0;
        double monthMom = 0.0;
        if (si.monthSeries.size() >= 4)
            monthMom = (si.monthSeries.last() - si.monthSeries[si.monthSeries.size() - 4]) / si.monthSeries[si.monthSeries.size() - 4] * 100.0;

        const double todayPct = si.changePctValid ? si.changePct : 0.0;
        const bool hasTodayPct = si.changePctValid;
        const double newsSentiment = na.count > 0 ? na.sentimentSum / static_cast<double>(na.count) : 0.0;
        const double trendBias = hasTodayPct ? (mom5d * 0.6 + todayPct * 0.4) : (mom5d * 1.0);
        const double trendDirection = trendBias > 0.3 ? 1.0 : (trendBias < -0.3 ? -1.0 : 0.0);
        const double newsRatio = na.count >= 2 ? static_cast<double>(na.positiveCount - na.negativeCount) / na.count : 0.0;

        double flowSum5d = 0.0;
        const QVector<double> &ff = si.fundFlowSeries;
        const int ffCount = qMin(5, ff.size());
        for (int k = ff.size() - ffCount; k < ff.size(); ++k) flowSum5d += ff[k];
        const double flow5dAvg = ffCount > 0 ? flowSum5d / ffCount : 0.0;
        const double tanhArg = flow5dAvg / 30.0;
        const double flowSignal = (qExp(2.0 * tanhArg) - 1.0) / (qExp(2.0 * tanhArg) + 1.0);

        FormulaBreakdown fb;
        fb.momentumFactor = mom5d * 0.015 + mom20d * 0.008;
        fb.todayFactor = hasTodayPct ? (todayPct * 0.015) : 0.0;
        fb.sentimentFactor = newsSentiment * 0.6 + newsRatio * 0.15;
        fb.newsIntensityFactor = qMin(na.count / 8.0, 1.0) * 0.08 * (newsSentiment > 0 ? 1.0 : -1.0);
        fb.fundFlowFactor = flowSignal * 0.10;
        fb.hotnessFactor = qMin(si.hotScore / 120.0, 0.08) * trendDirection;

        fb.meanReversionPenalty = 0.0;
        if (mom5d > 8.0) fb.meanReversionPenalty = -(mom5d - 8.0) * 0.025;
        if (mom5d < -8.0) fb.meanReversionPenalty = -(mom5d + 8.0) * 0.025;
        if (mom20d > 20.0) fb.meanReversionPenalty += -(mom20d - 20.0) * 0.008;
        if (mom20d < -20.0) fb.meanReversionPenalty += -(mom20d + 20.0) * 0.008;

        // 估值因子：PE分位越低越有价值（分位<30为低估，>70为高估）
        if (si.peRatio > 0) {
            const double valScore = (50.0 - si.pePercentile) / 50.0;
            fb.valuationFactor = valScore * 0.08;
        }
        // 拥挤度因子：拥挤度越高，回调风险越大（>70为拥挤，<30为冷门）
        if (si.crowdingIndex > 0) {
            const double crowdScore = (50.0 - si.crowdingIndex) / 50.0;
            fb.crowdingFactor = crowdScore * 0.06;
        }

        fb.rawForecast = fb.momentumFactor + fb.todayFactor + fb.sentimentFactor
            + fb.newsIntensityFactor + fb.fundFlowFactor + fb.hotnessFactor
            + fb.meanReversionPenalty + fb.valuationFactor + fb.crowdingFactor;
        const double qualityScore = computeDataQualityScore(si, na.count);
        const double qualityWeight = 0.55 + 0.45 * (qualityScore / 100.0);
        const double consistencyWeight = sourceConsistencyWeight(si.sourceConsistencyScore);
        double forecast = qBound(-1.0, fb.rawForecast * qualityWeight * consistencyWeight, 1.0);

        const double dataRichness = qMin(1.0, na.count / 5.0);
        const double trendDataRichness = trend.size() >= 20 ? 1.0 : trend.size() / 20.0;
        const double confidenceBase = qBound(0.10,
            0.15 * qAbs(forecast) + 0.40 * dataRichness + 0.25 * trendDataRichness
            + 0.20 * qMin(1.0, si.hotScore / 40.0), 0.95);
        const double confidence = qBound(0.10,
            confidenceBase * (0.70 + 0.30 * (qualityScore / 100.0))
            * (0.75 + 0.25 * (si.sourceConsistencyScore / 100.0)), 0.95);

        SectorSnapshot snap;
        snap.industry = si.name;
        snap.todayChangePct = todayPct;
        snap.todayChangePctValid = hasTodayPct;
        snap.fiveDayMomentum = mom5d;
        snap.twentyDayMomentum = mom20d;
        snap.newsSentiment = newsSentiment;
        snap.forecastScore = forecast;
        snap.confidence = confidence;
        snap.dataQualityScore = qualityScore;
        snap.dataQualityWeight = qualityWeight;
        snap.dataQualityNote = dataQualityNote(qualityScore);
        snap.sectorTier = si.sectorTier;
        snap.sectorTierLabel = si.sectorTierLabel;
        snap.crossSourceValidated = si.crossSourceValidated;
        snap.sourceConsistencyScore = si.sourceConsistencyScore;
        snap.sourceConsistencyWeight = consistencyWeight;
        snap.sectorHotScore = si.hotScore;
        snap.newsHitCount = na.count;
        snap.positiveNewsCount = na.positiveCount;
        snap.negativeNewsCount = na.negativeCount;
        snap.sectorStockCount = si.stockCount > 0 ? si.stockCount : si.upCount;
        snap.peRatio = si.peRatio;
        snap.pbRatio = si.pbRatio;
        snap.pePercentile = si.pePercentile;
        snap.crowdingIndex = si.crowdingIndex;
        snap.totalMarketCap = si.totalMarketCap;
        snap.stockCount = si.stockCount;
        snap.formula = fb;
        snap.action = chooseAction(forecast);
        snap.trendSummary = toTrendSummary(forecast, todayPct, mom5d);
        snap.trendSeries    = trend;
        snap.weekSeries     = si.weekSeries;
        snap.monthSeries    = si.monthSeries;
        snap.fundFlowSeries = si.fundFlowSeries;
        snap.listSource     = si.sourceTag;
        snap.klineSource    = si.klineSource;
        snap.fundFlowSource = si.fundFlowSource;
        snap.valuationSource = si.valuationSource;
        snap.lastDataDate   = si.lastDataDate;
        {
            const QDate today = QDate::currentDate();
            const QTime now = QTime::currentTime();
            const int dow = today.dayOfWeek();
            const bool isWeekend = (dow >= 6);
            const bool hasDataToday = (si.lastDataDate == today.toString("yyyy-MM-dd"));
            if (hasDataToday) {
                snap.marketClosed = false;
            } else if (isWeekend) {
                snap.marketClosed = true;
            } else if (now >= QTime(9, 25)) {
                snap.marketClosed = false;
            } else {
                snap.marketClosed = false;
            }
        }
        snap.missingDataItems = si.missingDataItems;
        snap.fundFlowFactor = fb.fundFlowFactor;
        snap.weekMomentum   = weekMom;
        snap.monthMomentum  = monthMom;
        snap.newsHeadlines  = na.headlines;
        snap.newsEntries    = na.entries;
        std::sort(snap.newsEntries.begin(), snap.newsEntries.end(),
            [](const NewsEntry &a, const NewsEntry &b) { return a.date > b.date; });
        snap.eventImpacts = eventImpactsBySector.value(si.name);
        snap.eventCatalystScore = qBound(-1.0, eventCatalystScores.value(si.name), 1.0);
        snap.eventSummary = buildEventSummary(snap.eventImpacts);
        if (!snap.eventSummary.isEmpty()) {
            if (snap.eventCatalystScore > 0.05)
                snap.positiveFactors.push_back(QString::fromUtf8("事件催化：") + snap.eventSummary);
            else if (snap.eventCatalystScore < -0.05)
                snap.negativeFactors.push_back(QString::fromUtf8("事件风险：") + snap.eventSummary);
        }

        if (!na.keyEvents.isEmpty())
            snap.newsHeadlines.push_front("【AI关键事件】" + na.keyEvents.join("、"));

        if (flow5dAvg > 2.0) snap.positiveFactors.push_back("近5日主力净流入均值 " + QString::number(flow5dAvg, 'f', 1) + " 亿元，资金积极入场");
        if (flow5dAvg < -2.0) snap.negativeFactors.push_back("近5日主力净流出均值 " + QString::number(qAbs(flow5dAvg), 'f', 1) + " 亿元，资金持续离场");
        if (mom5d > 1.0) snap.positiveFactors.push_back("近5日上涨 " + QString::number(mom5d, 'f', 2) + "%，短期动量偏强");
        if (mom20d > 3.0) snap.positiveFactors.push_back("近20日上涨 " + QString::number(mom20d, 'f', 2) + "%，中期趋势向好");
        if (weekMom > 2.0) snap.positiveFactors.push_back("近5周上涨 " + QString::number(weekMom, 'f', 2) + "%，周线走势偏多");
        if (monthMom > 5.0) snap.positiveFactors.push_back("近3月上涨 " + QString::number(monthMom, 'f', 2) + "%，月线趋势向好");
        if (hasTodayPct && todayPct > 1.0) snap.positiveFactors.push_back("今日涨幅 " + QString::number(todayPct, 'f', 2) + "%，市场情绪积极");
        if (na.positiveCount > na.negativeCount && na.count >= 2) snap.positiveFactors.push_back("正面新闻（" + QString::number(na.positiveCount) + " 条）多于负面（" + QString::number(na.negativeCount) + " 条）");
        if (si.hotScore > 20) snap.positiveFactors.push_back("板块热度较高（" + QString::number(si.hotScore, 'f', 1) + "），市场关注度集中");
        if (si.pePercentile < 30 && si.peRatio > 0)
            snap.positiveFactors.push_back("PE估值分位 " + QString::number(si.pePercentile, 'f', 0) + "%，处于历史低位，可能被低估");
        if (si.crowdingIndex < 30)
            snap.positiveFactors.push_back("拥挤度 " + QString::number(si.crowdingIndex, 'f', 0) + "%，交易冷清，存在预期差机会");

        if (mom5d < -1.0) snap.negativeFactors.push_back("近5日下跌 " + QString::number(qAbs(mom5d), 'f', 2) + "%，短期动量偏弱");
        if (mom20d < -3.0) snap.negativeFactors.push_back("近20日下跌 " + QString::number(qAbs(mom20d), 'f', 2) + "%，中期趋势承压");
        if (weekMom < -2.0) snap.negativeFactors.push_back("近5周下跌 " + QString::number(qAbs(weekMom), 'f', 2) + "%，周线走势偏空");
        if (monthMom < -5.0) snap.negativeFactors.push_back("近3月下跌 " + QString::number(qAbs(monthMom), 'f', 2) + "%，月线趋势承压");
        if (hasTodayPct && todayPct < -1.0) snap.negativeFactors.push_back("今日跌幅 " + QString::number(qAbs(todayPct), 'f', 2) + "%，市场情绪谨慎");
        if (na.negativeCount > na.positiveCount && na.count >= 2) snap.negativeFactors.push_back("负面新闻（" + QString::number(na.negativeCount) + " 条）多于正面（" + QString::number(na.positiveCount) + " 条）");
        if (mom5d > 5.0) snap.negativeFactors.push_back("近5日涨幅 " + QString::number(mom5d, 'f', 2) + "%，短期累积涨幅较大，存在回调风险");
        if (mom20d > 15.0) snap.negativeFactors.push_back("近20日涨幅 " + QString::number(mom20d, 'f', 2) + "%，中期累积涨幅过大，需警惕获利回吐");
        if (weekMom > 8.0) snap.negativeFactors.push_back("近5周涨幅 " + QString::number(weekMom, 'f', 2) + "%，周线短期过热");
        if (monthMom > 20.0) snap.negativeFactors.push_back("近3月涨幅 " + QString::number(monthMom, 'f', 2) + "%，中长期涨幅过大，需注意高位风险");
        if (hasTodayPct && todayPct > 3.0) snap.negativeFactors.push_back("今日涨幅 " + QString::number(todayPct, 'f', 2) + "%，单日波动过大");
        if (si.pePercentile > 70 && si.peRatio > 0)
            snap.negativeFactors.push_back("PE估值分位 " + QString::number(si.pePercentile, 'f', 0) + "%，处于历史高位，估值偏贵");
        if (si.crowdingIndex > 70)
            snap.negativeFactors.push_back("拥挤度 " + QString::number(si.crowdingIndex, 'f', 0) + "%，交易过热，回调概率增大");

        if (snap.positiveFactors.isEmpty() && snap.negativeFactors.isEmpty()) {
            if (!hasTodayPct)
                snap.positiveFactors.push_back("今日涨跌数据缺失，判断依据不足");
            else {
                snap.positiveFactors.push_back("暂无显著看多信号");
                snap.negativeFactors.push_back("暂无显著看空信号");
            }
        }

        snap.dailyBars = si.dailyBars;
        snap.eventMarkers = na.eventMarkers;
        snap.upcomingEvents = na.upcomingEvents;

        // ---- 事件驱动分析：关联价格大幅波动与新闻事件 ----
        if (si.dailyBars.size() >= 2) {
            QMap<QString, QStringList> dateEventMap;
            for (const EventMarker &em : na.eventMarkers) {
                dateEventMap[em.date].push_back(em.text);
            }
            for (int bi = 1; bi < si.dailyBars.size(); ++bi) {
                double dayChg = si.dailyBars[bi].changePct;
                if (qAbs(dayChg) < 1.5) continue;
                SectorSnapshot::EventDrivenMove edm;
                edm.date = si.dailyBars[bi].date;
                edm.changePct = dayChg;
                if (dateEventMap.contains(edm.date)) {
                    edm.eventText = dateEventMap[edm.date].join("；");
                } else {
                    const int searchRange = 2;
                    QString nearbyEvent;
                    for (int delta = -searchRange; delta <= searchRange; ++delta) {
                        int ni = bi + delta;
                        if (ni < 0 || ni >= si.dailyBars.size() || delta == 0) continue;
                        const QString &nd = si.dailyBars[ni].date;
                        if (dateEventMap.contains(nd)) {
                            nearbyEvent = dateEventMap[nd].first();
                            break;
                        }
                    }
                    edm.eventText = nearbyEvent.isEmpty()
                        ? (dayChg > 0 ? "技术性反弹/资金推动" : "获利回吐/市场情绪波动")
                        : nearbyEvent;
                }
                snap.eventDrivenMoves.push_back(edm);
            }
        }

        // ---- 将事件驱动数据纳入投资建议因素 ----
        if (!snap.eventDrivenMoves.isEmpty()) {
            int bigUpDays = 0, bigDownDays = 0;
            double maxUp = 0, maxDown = 0;
            QString latestEventUp, latestEventDown;
            for (const auto &edm : snap.eventDrivenMoves) {
                if (edm.changePct > 0) {
                    ++bigUpDays;
                    if (edm.changePct > maxUp) { maxUp = edm.changePct; latestEventUp = edm.eventText; }
                } else {
                    ++bigDownDays;
                    if (edm.changePct < maxDown) { maxDown = edm.changePct; latestEventDown = edm.eventText; }
                }
            }
            if (bigUpDays > bigDownDays && bigUpDays >= 2) {
                QString hint = QString::fromUtf8("近期事件驱动偏多（%1次大涨 vs %2次大跌）")
                    .arg(bigUpDays).arg(bigDownDays);
                if (!latestEventUp.isEmpty())
                    hint += QString::fromUtf8("，最大涨幅+%1%（%2）").arg(maxUp, 0, 'f', 2).arg(latestEventUp);
                snap.positiveFactors.push_back(hint);
            } else if (bigDownDays > bigUpDays && bigDownDays >= 2) {
                QString hint = QString::fromUtf8("近期事件驱动偏空（%1次大跌 vs %2次大涨）")
                    .arg(bigDownDays).arg(bigUpDays);
                if (!latestEventDown.isEmpty())
                    hint += QString::fromUtf8("，最大跌幅%1%（%2）").arg(maxDown, 0, 'f', 2).arg(latestEventDown);
                snap.negativeFactors.push_back(hint);
            }
        }

        // ---- 主力资金异动检测 ----
        if (si.fundFlowSeries.size() >= 5) {
            double flowMean = 0, flowStd = 0;
            for (double v : si.fundFlowSeries) flowMean += v;
            flowMean /= si.fundFlowSeries.size();
            for (double v : si.fundFlowSeries) flowStd += (v - flowMean) * (v - flowMean);
            flowStd = std::sqrt(flowStd / si.fundFlowSeries.size());
            if (flowStd > 0.01) {
                const int offset = si.dailyBars.size() - si.fundFlowSeries.size();
                for (int fi = 0; fi < si.fundFlowSeries.size(); ++fi) {
                    double z = (si.fundFlowSeries[fi] - flowMean) / flowStd;
                    if (qAbs(z) >= 2.0) {
                        CapitalAnomaly ca;
                        int barIdx = offset + fi;
                        ca.date = (barIdx >= 0 && barIdx < si.dailyBars.size()) ? si.dailyBars[barIdx].date : "";
                        ca.flowValue = si.fundFlowSeries[fi];
                        ca.zScore = z;
                        ca.desc = z > 0
                            ? QString("主力异常大额流入 %1 亿（%2σ）").arg(ca.flowValue, 0, 'f', 1).arg(z, 0, 'f', 1)
                            : QString("主力异常大额流出 %1 亿（%2σ）").arg(qAbs(ca.flowValue), 0, 'f', 1).arg(qAbs(z), 0, 'f', 1);
                        snap.capitalAnomalies.push_back(ca);
                    }
                }
            }
        }

        // ---- 累计涨降幅追踪（基于K线数据+用户首次收益日） ----
        if (!si.dailyBars.isEmpty()) {
            QSettings settings("InvestInsight", "InvestInsight");
            const double currentPrice = si.dailyBars.last().close;
            const QString klineSource = si.klineSource;

            // 查找用户持仓中此板块的最早首次收益日
            QDate userEarnDate;
            {
                const QString batchKey = settings.contains("portfolio/batches_json")
                    ? "portfolio/batches_json" : "portfolio/entries_json";
                const QJsonDocument pfDoc = QJsonDocument::fromJson(
                    settings.value(batchKey).toString().toUtf8());
                if (pfDoc.isArray()) {
                    for (const QJsonValue &v : pfDoc.array()) {
                        const QJsonObject o = v.toObject();
                        if (o.value("sector").toString() != si.name) continue;
                        if (o.value("sold").toBool(false)) continue;
                        QString earnStr = o.value("firstEarnDate").toString();
                        if (earnStr.isEmpty()) earnStr = o.value("date").toString();
                        const QDate d = QDate::fromString(earnStr, "yyyy-MM-dd");
                        if (d.isValid() && (!userEarnDate.isValid() || d < userEarnDate))
                            userEarnDate = d;
                    }
                }
            }

            // 从K线中找对应日期的价格
            double startPrice = 0;
            QString startDateStr;
            const QDate today = QDate::currentDate();
            if (userEarnDate.isValid()) {
                if (userEarnDate > today) {
                    // 首次收益日还没到，累计收益为0
                    startPrice = currentPrice;
                    startDateStr = si.dailyBars.last().date;
                } else {
                    const QString targetDate = userEarnDate.toString("yyyy-MM-dd");
                    for (const KBar &bar : si.dailyBars) {
                        if (bar.date >= targetDate && bar.close > 0) {
                            startPrice = bar.close;
                            startDateStr = bar.date;
                            break;
                        }
                    }
                    if (startPrice <= 0 && !si.dailyBars.isEmpty()) {
                        startPrice = si.dailyBars.last().close;
                        startDateStr = si.dailyBars.last().date;
                        qDebug() << "[CumulReturn] WARN: firstEarnDate" << targetDate
                                 << "not found in bars, using last bar as fallback -> return~0%";
                    }
                }
            }

            // 回退：无用户持仓时，用K线起始点
            if (startPrice <= 0) {
                startPrice = si.dailyBars.first().close;
                startDateStr = si.dailyBars.first().date;
            }

            snap.trackingStartPrice = startPrice;
            if (startPrice > 0) {
                snap.cumulativeReturn = (currentPrice - startPrice) / startPrice * 100.0;
            } else {
                snap.cumulativeReturn = 0.0;
            }
            const QDate sd = QDate::fromString(startDateStr, "yyyy-MM-dd");
            snap.trackingDays = sd.isValid() ? sd.daysTo(QDate::currentDate()) : 0;

            qDebug() << "[CumulReturn]" << si.name
                     << "startPrice=" << startPrice << "currentPrice=" << currentPrice
                     << "return=" << snap.cumulativeReturn << "%"
                     << "earnDate=" << (userEarnDate.isValid() ? userEarnDate.toString("yyyy-MM-dd") : "none")
                     << "klineStart=" << startDateStr;
        }

        // ---- 策略回测 ----
        if (si.dailyBars.size() >= 30) {
            const auto &bars = si.dailyBars;
            const int n = bars.size();

            auto runBacktest = [&](const QString &name, auto signalFn) -> StrategyBacktest {
                StrategyBacktest bt;
                bt.name = name;
                int holdDays = 0;
                double entryPrice = 0;
                double maxPeak = 0;
                double maxDd = 0;
                double totalRet = 0;

                for (int bi = 1; bi < n; ++bi) {
                    bool buySignal = false, sellSignal = false;
                    signalFn(bi, buySignal, sellSignal);

                    if (buySignal && entryPrice <= 0) {
                        entryPrice = bars[bi].close;
                        maxPeak = entryPrice;
                        holdDays = 0;
                    } else if (entryPrice > 0) {
                        ++holdDays;
                        if (bars[bi].close > maxPeak) maxPeak = bars[bi].close;
                        double dd = (maxPeak - bars[bi].close) / maxPeak * 100.0;
                        if (dd > maxDd) maxDd = dd;

                        if (sellSignal || holdDays >= 20) {
                            double ret = (bars[bi].close - entryPrice) / entryPrice * 100.0;
                            bt.totalTrades++;
                            if (ret > 0) bt.wins++;
                            totalRet += ret;
                            entryPrice = 0;
                        }
                    }
                }
                bt.winRate = bt.totalTrades > 0 ? static_cast<double>(bt.wins) / bt.totalTrades * 100.0 : 0;
                bt.avgReturn = bt.totalTrades > 0 ? totalRet / bt.totalTrades : 0;
                bt.maxDrawdown = maxDd;

                // 当前信号：基于最新K线状态而非历史最后一笔交易
                if (entryPrice > 0) {
                    bt.currentSignal = QString::fromUtf8("持仓中");
                } else {
                    bool latestBuy = false, latestSell = false;
                    if (n >= 2) signalFn(n - 1, latestBuy, latestSell);
                    bool prevBuy = false, prevSell = false;
                    if (n >= 3) signalFn(n - 2, prevBuy, prevSell);
                    if (latestBuy)
                        bt.currentSignal = QString::fromUtf8("买入信号");
                    else if (latestSell)
                        bt.currentSignal = QString::fromUtf8("卖出信号");
                    else if (prevBuy)
                        bt.currentSignal = QString::fromUtf8("近期买入");
                    else
                        bt.currentSignal = QString::fromUtf8("观望");
                }
                return bt;
            };

            const auto macd = TechIndicators::calcMACD(bars);
            const auto rsi = TechIndicators::calcRSI(bars);
            const auto ma = TechIndicators::calcMA(bars);

            snap.backtestResults.push_back(runBacktest("MACD金叉策略", [&](int bi, bool &buy, bool &sell) {
                if (macd.dif.size() <= bi || bi < 1) return;
                buy = macd.dif[bi - 1] < macd.dea[bi - 1] && macd.dif[bi] >= macd.dea[bi];
                sell = macd.dif[bi - 1] > macd.dea[bi - 1] && macd.dif[bi] <= macd.dea[bi];
            }));

            snap.backtestResults.push_back(runBacktest("RSI超卖反弹", [&](int bi, bool &buy, bool &sell) {
                if (rsi.rsi6.size() <= bi || bi < 1) return;
                buy = rsi.rsi6[bi - 1] < 25 && rsi.rsi6[bi] >= 25;
                sell = rsi.rsi6[bi] > 75;
            }));

            snap.backtestResults.push_back(runBacktest("均线多头突破", [&](int bi, bool &buy, bool &sell) {
                if (ma.ma5.size() <= bi || bi < 1) return;
                buy = bars[bi - 1].close < ma.ma20[bi - 1] && bars[bi].close >= ma.ma20[bi];
                sell = bars[bi].close < ma.ma10[bi];
            }));

            snap.backtestResults.push_back(runBacktest("放量突破策略", [&](int bi, bool &buy, bool &sell) {
                if (bi < 5 || ma.ma5.size() <= bi) return;
                double avgVol = 0;
                for (int vi = bi - 5; vi < bi; ++vi) avgVol += bars[vi].volume;
                avgVol /= 5.0;
                buy = bars[bi].volume > avgVol * 1.8 && bars[bi].changePct > 1.0;
                sell = bars[bi].changePct < -2.0;
            }));

            snap.backtestResults.push_back(runBacktest("布林带回归策略", [&](int bi, bool &buy, bool &sell) {
                const auto boll = TechIndicators::calcBOLL(bars);
                if (boll.lower.size() <= bi) return;
                buy = bars[bi].close <= boll.lower[bi] * 1.005;
                sell = bars[bi].close >= boll.upper[bi] * 0.995;
            }));

            double bestWR = 0;
            for (const StrategyBacktest &bt : snap.backtestResults) {
                if (bt.totalTrades >= 2 && bt.winRate > bestWR) {
                    bestWR = bt.winRate;
                    snap.bestStrategyName = bt.name;
                    snap.bestStrategyWinRate = bt.winRate;
                    snap.bestStrategyCurrentSignal = bt.currentSignal;
                }
            }

            // 策略追踪：记录最佳策略最近一次信号后的收益
            if (!snap.bestStrategyName.isEmpty()) {
                QSettings stSettings("InvestInsight", "InvestInsight");
                const QString stKey = "strategy/" + si.name;
                const QString savedStrategy = stSettings.value(stKey + "/name").toString();
                const double savedEntryPrice = stSettings.value(stKey + "/entryPrice", 0.0).toDouble();
                const double curPrice = bars.last().close;

                if (savedStrategy == snap.bestStrategyName && savedEntryPrice > 0) {
                    snap.bestStrategyTrackedReturn = (curPrice - savedEntryPrice) / savedEntryPrice * 100.0;
                } else {
                    stSettings.setValue(stKey + "/name", snap.bestStrategyName);
                    stSettings.setValue(stKey + "/entryPrice", curPrice);
                    stSettings.setValue(stKey + "/date", bars.last().date);
                    snap.bestStrategyTrackedReturn = 0.0;
                }

                // 综合投资建议（与主策略 snap.action 保持一致）
                const QString &sig = snap.bestStrategyCurrentSignal;
                const QString sName = snap.bestStrategyName;
                const QString sWR = QString::number(snap.bestStrategyWinRate, 'f', 0);
                QString rec;
                QString mainAction;
                switch (snap.action) {
                case AdviceAction::Increase: mainAction = QString::fromUtf8("增配"); break;
                case AdviceAction::Decrease: mainAction = QString::fromUtf8("减仓"); break;
                default: mainAction = QString::fromUtf8("持有观望"); break;
                }

                if (sig.contains(QString::fromUtf8("买入"))) {
                    rec = QString::fromUtf8("最佳策略「%1」(胜率%2%)发出买入信号。综合建议：%3")
                        .arg(sName, sWR, mainAction);
                } else if (sig.contains(QString::fromUtf8("持仓"))) {
                    const QString trStr = (snap.bestStrategyTrackedReturn >= 0 ? "+" : "")
                        + QString::number(snap.bestStrategyTrackedReturn, 'f', 2) + "%";
                    rec = QString::fromUtf8("最佳策略「%1」(胜率%2%)当前持仓中，追踪收益%3。综合建议：%4")
                        .arg(sName, sWR, trStr, mainAction);
                } else if (sig.contains(QString::fromUtf8("卖出"))) {
                    rec = QString::fromUtf8("最佳策略「%1」(胜率%2%)发出卖出信号。综合建议：%3")
                        .arg(sName, sWR, mainAction);
                } else {
                    rec = QString::fromUtf8("最佳策略「%1」(胜率%2%)暂无明确信号。综合建议：%3")
                        .arg(sName, sWR, mainAction);
                }
                snap.investmentRecommendation = rec;
            }
        }

        // ---- 周期性分析 ----
        if (si.dailyBars.size() >= 30) {
            const auto &bars = si.dailyBars;
            const int n = bars.size();

            // 1) 用 MA20 平滑价格，检测局部极值（峰/谷）
            QVector<double> smoothed(n, 0);
            const int smWin = qMin(10, n / 3);
            for (int bi = 0; bi < n; ++bi) {
                double sum = 0; int cnt = 0;
                for (int j = qMax(0, bi - smWin); j <= qMin(n - 1, bi + smWin); ++j) {
                    sum += bars[j].close; ++cnt;
                }
                smoothed[bi] = sum / cnt;
            }

            struct Extremum { int idx; double price; bool isPeak; QString date; };
            QList<Extremum> extrema;
            const int lookback = qMax(3, n / 15);
            for (int bi = lookback; bi < n - lookback; ++bi) {
                bool isPeak = true, isTrough = true;
                for (int j = bi - lookback; j <= bi + lookback; ++j) {
                    if (j == bi) continue;
                    if (smoothed[j] >= smoothed[bi]) isPeak = false;
                    if (smoothed[j] <= smoothed[bi]) isTrough = false;
                }
                if (isPeak) extrema.push_back({bi, bars[bi].close, true, bars[bi].date});
                else if (isTrough) extrema.push_back({bi, bars[bi].close, false, bars[bi].date});
            }

            CycleAnalysis &ca = snap.cycle;
            int peakCnt = 0, troughCnt = 0;
            for (const Extremum &e : extrema) {
                if (e.isPeak) { ++peakCnt; ca.lastPeakPrice = e.price; ca.lastPeakDate = e.date; }
                else { ++troughCnt; ca.lastTroughPrice = e.price; ca.lastTroughDate = e.date; }
            }
            ca.peakCount = peakCnt;
            ca.troughCount = troughCnt;

            // 2) 计算周期性强度：至少 2 个完整波（2 峰 + 2 谷）才视为有周期
            const int fullWaves = qMin(peakCnt, troughCnt);
            if (fullWaves >= 2) {
                ca.isCyclical = true;

                // 估算周期长度：相邻同类极值间距均值
                QVector<int> peakDists, troughDists;
                int prevPeakIdx = -1, prevTroughIdx = -1;
                double ampSum = 0; int ampCnt = 0;
                for (const Extremum &e : extrema) {
                    if (e.isPeak) {
                        if (prevPeakIdx >= 0) peakDists.push_back(e.idx - prevPeakIdx);
                        prevPeakIdx = e.idx;
                    } else {
                        if (prevTroughIdx >= 0) troughDists.push_back(e.idx - prevTroughIdx);
                        prevTroughIdx = e.idx;
                    }
                }
                // 峰谷间振幅
                for (int ei = 1; ei < extrema.size(); ++ei) {
                    double diff = qAbs(extrema[ei].price - extrema[ei - 1].price);
                    double mid = (extrema[ei].price + extrema[ei - 1].price) / 2.0;
                    if (mid > 0) { ampSum += diff / mid * 100.0; ++ampCnt; }
                }

                double avgPeriod = 0; int periodCnt = 0;
                for (int d : peakDists) { avgPeriod += d; ++periodCnt; }
                for (int d : troughDists) { avgPeriod += d; ++periodCnt; }
                if (periodCnt > 0) avgPeriod /= periodCnt;
                ca.estimatedPeriodDays = static_cast<int>(avgPeriod);
                ca.amplitude = ampCnt > 0 ? ampSum / ampCnt : 0;

                // 周期规律性评分：用周期长度的变异系数衡量
                double varSum = 0;
                for (int d : peakDists) varSum += (d - avgPeriod) * (d - avgPeriod);
                for (int d : troughDists) varSum += (d - avgPeriod) * (d - avgPeriod);
                double cv = (periodCnt > 0 && avgPeriod > 0)
                    ? std::sqrt(varSum / periodCnt) / avgPeriod : 1.0;
                ca.cyclicScore = qBound(0.0, (1.0 - cv) * 100.0, 100.0);

                // 3) 判断当前阶段
                // 找到最近的极值
                if (!extrema.isEmpty()) {
                    const Extremum &lastE = extrema.last();
                    int daysSinceLast = n - 1 - lastE.idx;
                    double halfPeriod = avgPeriod / 2.0;
                    double currentPrice = bars.last().close;

                    if (lastE.isPeak) {
                        // 上一个极值是峰：之后进入下降→底部→复苏
                        double progress = halfPeriod > 0 ? daysSinceLast / halfPeriod : 0;
                        if (currentPrice < lastE.price * 0.97 && progress < 0.5) {
                            ca.phaseIndex = 4; ca.phaseName = "收缩回调";
                            ca.phaseProgress = progress * 100;
                            ca.phaseAdvice = "板块处于峰值后回调阶段，建议减仓或观望，等待企稳信号。";
                        } else if (progress >= 0.5 && progress < 1.0) {
                            ca.phaseIndex = 0; ca.phaseName = "底部蓄力";
                            ca.phaseProgress = (progress - 0.5) * 200;
                            ca.phaseAdvice = "板块可能正接近周期底部，可关注左侧布局机会，分批建仓。";
                        } else {
                            ca.phaseIndex = 1; ca.phaseName = "复苏上行";
                            ca.phaseProgress = qBound(0.0, (progress - 1.0) * 100, 100.0);
                            ca.phaseAdvice = "板块已进入新一轮复苏周期，可适度加仓，关注放量突破确认。";
                        }
                    } else {
                        // 上一个极值是谷：之后进入上升→顶部→收缩
                        double progress = halfPeriod > 0 ? daysSinceLast / halfPeriod : 0;
                        if (currentPrice > lastE.price * 1.03 && progress < 0.5) {
                            ca.phaseIndex = 1; ca.phaseName = "复苏上行";
                            ca.phaseProgress = progress * 100;
                            ca.phaseAdvice = "板块处于谷底后反弹阶段，趋势向好，适合持有或加仓。";
                        } else if (progress >= 0.5 && progress < 1.0) {
                            ca.phaseIndex = 2; ca.phaseName = "扩张加速";
                            ca.phaseProgress = (progress - 0.5) * 200;
                            ca.phaseAdvice = "板块处于上升周期中后段，涨幅可能加速，但需警惕见顶信号。";
                        } else {
                            ca.phaseIndex = 3; ca.phaseName = "顶部滞涨";
                            ca.phaseProgress = qBound(0.0, (progress - 1.0) * 100, 100.0);
                            ca.phaseAdvice = "板块可能接近周期顶部，建议逐步止盈，降低仓位。";
                        }
                    }
                }
            } else {
                ca.isCyclical = false;
                ca.cyclicScore = fullWaves >= 1 ? 20 : 0;

                const double curPrice = bars.last().close;
                const double ma5  = [&]() { double s=0; int c=qMin(5,n); for(int k=n-c;k<n;++k) s+=bars[k].close; return c>0?s/c:curPrice; }();
                const double ma20 = [&]() { double s=0; int c=qMin(20,n); for(int k=n-c;k<n;++k) s+=bars[k].close; return c>0?s/c:curPrice; }();
                const double ma60 = [&]() { double s=0; int c=qMin(60,n); for(int k=n-c;k<n;++k) s+=bars[k].close; return c>0?s/c:curPrice; }();
                const double momPct = n >= 20 && bars[n-20].close > 0
                    ? (curPrice - bars[n-20].close) / bars[n-20].close * 100.0 : 0.0;

                double hi = curPrice, lo = curPrice;
                for (int k = qMax(0, n-60); k < n; ++k) {
                    if (bars[k].close > hi) hi = bars[k].close;
                    if (bars[k].close < lo) lo = bars[k].close;
                }
                double posInRange = (hi > lo) ? (curPrice - lo) / (hi - lo) : 0.5;

                if (curPrice > ma5 && ma5 > ma20 && ma20 > ma60 && momPct > 3.0) {
                    ca.phaseIndex = 2; ca.phaseName = QString::fromUtf8("上升趋势");
                    ca.phaseProgress = qBound(0.0, posInRange * 100.0, 100.0);
                    ca.phaseAdvice = QString::fromUtf8("均线多头排列，近20日上涨") + QString::number(momPct, 'f', 1)
                        + QString::fromUtf8("%，趋势向好。适合持有或顺势加仓，注意设置止盈。");
                } else if (curPrice < ma5 && ma5 < ma20 && ma20 < ma60 && momPct < -3.0) {
                    ca.phaseIndex = 4; ca.phaseName = QString::fromUtf8("下降趋势");
                    ca.phaseProgress = qBound(0.0, (1.0 - posInRange) * 100.0, 100.0);
                    ca.phaseAdvice = QString::fromUtf8("均线空头排列，近20日下跌") + QString::number(qAbs(momPct), 'f', 1)
                        + QString::fromUtf8("%，弱势运行。建议观望或减仓，等待企稳信号。");
                } else if (posInRange < 0.25 && momPct < 0) {
                    ca.phaseIndex = 0; ca.phaseName = QString::fromUtf8("低位震荡");
                    ca.phaseProgress = posInRange * 100.0;
                    ca.phaseAdvice = QString::fromUtf8("价格处于近期低位区间，可能正在筑底。可小仓位关注，等待放量突破确认。");
                } else if (posInRange > 0.75 && momPct > 0) {
                    ca.phaseIndex = 3; ca.phaseName = QString::fromUtf8("高位震荡");
                    ca.phaseProgress = posInRange * 100.0;
                    ca.phaseAdvice = QString::fromUtf8("价格处于近期高位区间，上方空间有限。建议逐步止盈，注意回调风险。");
                } else {
                    ca.phaseIndex = 1; ca.phaseName = QString::fromUtf8("横盘整理");
                    ca.phaseProgress = posInRange * 100.0;
                    ca.phaseAdvice = QString::fromUtf8("趋势不明朗，均线交织，建议观望为主，等待方向明确后再操作。");
                }
            }
        }

        // ---- 技术指标计算 ----
        if (si.dailyBars.size() >= 20) {
            const auto macd = TechIndicators::calcMACD(si.dailyBars);
            const auto rsi  = TechIndicators::calcRSI(si.dailyBars);
            const auto kdj  = TechIndicators::calcKDJ(si.dailyBars);
            const auto ma   = TechIndicators::calcMA(si.dailyBars);
            const auto boll = TechIndicators::calcBOLL(si.dailyBars);

            const int last = si.dailyBars.size() - 1;
            const int prev = last - 1;
            TechSignals &ts = snap.tech;

            if (!macd.dif.isEmpty()) {
                ts.macdDIF = macd.dif[last]; ts.macdDEA = macd.dea[last]; ts.macdHist = macd.hist[last];
                if (prev >= 0) {
                    ts.macdGoldenCross = macd.dif[prev] < macd.dea[prev] && macd.dif[last] >= macd.dea[last];
                    ts.macdDeadCross   = macd.dif[prev] > macd.dea[prev] && macd.dif[last] <= macd.dea[last];
                }
            }
            if (!rsi.rsi6.isEmpty()) {
                ts.rsi6 = rsi.rsi6[last]; ts.rsi12 = rsi.rsi12[last]; ts.rsi24 = rsi.rsi24[last];
                ts.rsiOverbought = ts.rsi6 > 80;
                ts.rsiOversold = ts.rsi6 < 20;
            }
            if (!kdj.k.isEmpty()) {
                ts.kdjK = kdj.k[last]; ts.kdjD = kdj.d[last]; ts.kdjJ = kdj.j[last];
                if (prev >= 0)
                    ts.kdjGoldenCross = kdj.k[prev] < kdj.d[prev] && kdj.k[last] >= kdj.d[last];
                ts.kdjOverbought = ts.kdjJ > 100;
                ts.kdjOversold = ts.kdjJ < 0;
            }
            if (!boll.upper.isEmpty()) {
                ts.bollUpper = boll.upper[last]; ts.bollMid = boll.mid[last]; ts.bollLower = boll.lower[last];
                ts.bollWidth = ts.bollMid > 1e-6 ? (ts.bollUpper - ts.bollLower) / ts.bollMid * 100.0 : 0;
                ts.priceAboveUpper = si.dailyBars[last].close > ts.bollUpper;
                ts.priceBelowLower = si.dailyBars[last].close < ts.bollLower;
            }
            if (!ma.ma5.isEmpty()) {
                ts.ma5 = ma.ma5[last]; ts.ma10 = ma.ma10[last]; ts.ma20 = ma.ma20[last]; ts.ma60 = ma.ma60[last];
                ts.maLongArrange = ts.ma5 > ts.ma10 && ts.ma10 > ts.ma20 && ts.ma20 > ts.ma60 && ts.ma60 > 0;
                ts.maShortArrange = ts.ma5 < ts.ma10 && ts.ma10 < ts.ma20 && ts.ma20 < ts.ma60 && ts.ma60 > 0;
            }
            const auto volMA5 = TechIndicators::calcVolMA(si.dailyBars, 5);
            if (!volMA5.isEmpty() && volMA5[last] > 1e-6) {
                ts.volRatio = si.dailyBars[last].volume / volMA5[last];
                ts.volExpansion = ts.volRatio > 1.5;
                ts.volShrink = ts.volRatio < 0.7;
            }

            double techScore = 0;
            if (ts.macdGoldenCross) techScore += 0.20; else if (ts.macdDeadCross) techScore -= 0.20;
            if (ts.macdHist > 0) techScore += 0.05; else techScore -= 0.05;
            if (ts.rsiOversold) techScore += 0.15; else if (ts.rsiOverbought) techScore -= 0.15;
            if (ts.kdjGoldenCross) techScore += 0.10;
            if (ts.kdjOversold) techScore += 0.10; else if (ts.kdjOverbought) techScore -= 0.10;
            if (ts.maLongArrange) techScore += 0.20; else if (ts.maShortArrange) techScore -= 0.20;
            if (ts.priceAboveUpper) techScore += 0.05;
            if (ts.priceBelowLower) techScore += 0.10;
            if (ts.volExpansion && todayPct > 0) techScore += 0.05;
            if (ts.volExpansion && todayPct < 0) techScore -= 0.05;
            ts.techScore = qBound(-1.0, techScore, 1.0);

            fb.techFactor = ts.techScore * 0.12;
        }

        // 应用市场状态动态权重缩放因子
        {
            const double mw = dynW.momentumWeight / 0.20;
            const double sw = dynW.sentimentWeight / 0.15;
            const double rw = dynW.riskWeight / 0.15;
            const double vw = dynW.valuationWeight / 0.15;
            const double fw = dynW.flowWeight / 0.10;
            fb.momentumFactor *= mw;
            fb.todayFactor *= mw;
            fb.sentimentFactor *= sw;
            fb.newsIntensityFactor *= sw;
            fb.hotnessFactor *= sw;
            fb.fundFlowFactor *= fw;
            fb.valuationFactor *= vw;
            fb.crowdingFactor *= rw;
            fb.meanReversionPenalty *= rw;
        }

        fb.rawForecast = fb.momentumFactor + fb.todayFactor + fb.sentimentFactor
            + fb.newsIntensityFactor + fb.fundFlowFactor + fb.hotnessFactor
            + fb.meanReversionPenalty + fb.techFactor + fb.valuationFactor + fb.crowdingFactor;
        fb.eventCatalystFactor = qBound(-0.06, snap.eventCatalystScore * 0.06, 0.06);
        fb.rawForecast += fb.eventCatalystFactor;
        const double forecastFinal = qBound(-1.0,
            fb.rawForecast * snap.dataQualityWeight * snap.sourceConsistencyWeight, 1.0);
        snap.forecastScore = forecastFinal;
        snap.formula = fb;
        snap.action = chooseAction(forecastFinal);
        snap.trendSummary = toTrendSummary(forecastFinal, todayPct, mom5d);

        snap.strategy = StrategyEngine::generate(snap);

        snap.analysisNarrative = buildNarrative(si.name, todayPct, mom5d, mom20d,
            newsSentiment, na.count, na.positiveCount, na.negativeCount,
            forecastFinal, si.hotScore, si.upCount);

        if (m_portfolio.contains(si.name)) {
            const double amount = m_portfolio.value(si.name);
            const QString amtStr = (amount >= 10000)
                ? QString::number(amount / 10000.0, 'f', 2) + QString::fromUtf8(" 万元")
                : QString::number(amount, 'f', 0) + QString::fromUtf8(" 元");
            QString advice;
            if (forecastFinal >= 0.15) {
                advice = QString::fromUtf8("您持有该板块 %1。当前预测评分 %2（偏多），建议考虑适当增配，预计短期有望延续上行趋势。")
                    .arg(amtStr).arg(forecastFinal, 0, 'f', 2);
                if (weekMom > 2.0) advice += QString::fromUtf8("周线近5周上涨 %1%，中期动量支撑较强。").arg(weekMom, 0, 'f', 2);
            } else if (forecastFinal <= -0.12) {
                advice = QString::fromUtf8("您持有该板块 %1。当前预测评分 %2（偏空），建议考虑分批减仓，注意控制风险。")
                    .arg(amtStr).arg(forecastFinal, 0, 'f', 2);
                if (weekMom < -2.0) advice += QString::fromUtf8("周线近5周下跌 %1%，中期趋势偏弱，可适当降低仓位。").arg(qAbs(weekMom), 0, 'f', 2);
            } else {
                advice = QString::fromUtf8("您持有该板块 %1。当前预测评分 %2（中性），建议维持现有仓位，等待更明确信号后再行动。")
                    .arg(amtStr).arg(forecastFinal, 0, 'f', 2);
            }
            if (na.count >= 3) advice += QString::fromUtf8("（本次共匹配 %1 条相关新闻，信号可信度较高）").arg(na.count);
            snap.personalAdvice = advice;
        }

        // ---- 趋势生命周期分析（升级版核心） ----
        if (si.dailyBars.size() >= 30) {
            snap.trendStageResult = TrendStageDetector::detect(si.dailyBars);
            snap.trendHealth = TrendHealthAnalyzer::analyze(si.dailyBars);
            snap.breadth = BreadthAnalyzer::analyze(si.dailyBars, todayPct, si.upCount, si.stockCount);
            snap.overheat = OverheatDetector::detect(si.dailyBars, newsSentiment, na.count,
                                                      snap.crowdingIndex, si.hotScore);
            const double todayNetFlow = si.fundFlowSeries.isEmpty() ? 0.0 : si.fundFlowSeries.last();
            snap.flowStructure = FlowStructureAnalyzer::analyze(si.fundFlowSeries,
                                                                 todayNetFlow, todayPct);
            snap.explanation = ExplainabilityEngine::build(
                snap.trendStageResult, snap.trendHealth, snap.breadth,
                snap.overheat, snap.flowStructure, forecastFinal, todayPct);

            // 根据 InvestmentState 校正 action（趋势理解覆盖纯动量追随）
            const InvestmentState iState = snap.explanation.state;
            if (iState == InvestmentState::DistributionWarning || iState == InvestmentState::DowntrendAvoid) {
                if (snap.action == AdviceAction::Increase) snap.action = AdviceAction::Decrease;
            } else if (iState == InvestmentState::OverheatedRisk || iState == InvestmentState::LateStageMomentum) {
                if (snap.action == AdviceAction::Increase) snap.action = AdviceAction::Hold;
            } else if (iState == InvestmentState::EarlyTrendOpportunity) {
                if (snap.action == AdviceAction::Hold && forecastFinal > 0.05) snap.action = AdviceAction::Increase;
            }

            // 将 ExplanationChain 融入 personalAdvice
            if (!snap.personalAdvice.isEmpty()) {
                snap.personalAdvice += QString::fromUtf8("\n【趋势阶段】%1 — %2")
                    .arg(snap.trendStageResult.stageName, snap.explanation.stateName);
                if (snap.explanation.exhaustionRisk > 50)
                    snap.personalAdvice += QString::fromUtf8("\n⚠ 耗竭风险 %1/100，注意控制仓位")
                        .arg(snap.explanation.exhaustionRisk, 0, 'f', 0);
                if (snap.trendHealth.sustainability > 65)
                    snap.personalAdvice += QString::fromUtf8("\n趋势可持续性评分 %1/100，结构健康")
                        .arg(snap.trendHealth.sustainability, 0, 'f', 0);
            }
        }

        result.sectors.push_back(snap);

        const int pct = 50 + (i + 1) * 15 / totalSectors;
        reportProgress(progress, pct, "正在分析：" + si.name + "...");
    }

    // ========== 阶段 4.5: 板块轮动检测 + 风险雷达 ==========
    reportProgress(progress, 66, "正在计算板块轮动信号和市场风险...");
    result.rotationSignals = RotationDetector::detect(result.sectors);
    result.riskRadar = RotationDetector::computeRisk(result.sectors, result.marketCtx);

    // 将轮动信号注入到 sector 的 positive/negative factors
    for (const RotationSignal &rs : result.rotationSignals) {
        for (SectorSnapshot &s : result.sectors) {
            if (s.industry != rs.sector) continue;
            if (rs.isRotatingIn)
                s.positiveFactors.push_back("板块轮动信号：资金和动量加速流入，短期可能成为市场热点");
            if (rs.isRotatingOut)
                s.negativeFactors.push_back("板块轮动信号：资金和动量正在流出，市场关注度下降");
            break;
        }
    }

    // ========== 阶段 5: AI Stage 2 深度分析 ==========
    if (useAI) {
        const int topN = m_aiAnalyzer.deepAnalysisTopN();
        const int cnt = m_aiAnalyzer.providerCount();
        reportProgress(progress, 68, QString("正在调用 %1 个 AI 模型深度分析 Top %2 板块...")
            .arg(cnt).arg(topN));
        m_aiAnalyzer.enhance(result, topN);
        if (result.aiAvailable) {
            reportProgress(progress, 95, cnt > 1 ? "多模型对比分析完成" : "AI 深度分析完成");
        } else {
            reportProgress(progress, 95, "AI 调用未成功，使用规则引擎分析");
        }
    } else {
        reportProgress(progress, 95, m_aiAnalyzer.isAvailable()
            ? "AI 已关闭，使用规则引擎分析"
            : "未配置 AI Key，使用规则引擎分析");
    }

    // ========== 阶段 5.5: 将AI前瞻事件纳入预测评分微调 ==========
    for (SectorSnapshot &s : result.sectors) {
        if (s.futureEventsAI.isEmpty()) continue;
        int positiveEvents = 0, negativeEvents = 0;
        for (const QString &ev : s.futureEventsAI) {
            bool hasPositive = ev.contains("利好") || ev.contains("增长") || ev.contains("突破")
                || ev.contains("提升") || ev.contains("扩大") || ev.contains("催化");
            bool hasNegative = ev.contains("风险") || ev.contains("收缩") || ev.contains("下行")
                || ev.contains("压力") || ev.contains("限制");
            if (hasPositive) ++positiveEvents;
            if (hasNegative) ++negativeEvents;
        }
        double eventBias = (positiveEvents - negativeEvents) * 0.02;
        eventBias = qBound(-0.05, eventBias, 0.05);
        s.forecastScore = qBound(-1.0, s.forecastScore + eventBias, 1.0);
    }

    // ========== 阶段 6: 最终一致性校验 ==========
    reportProgress(progress, 97, "正在做最终一致性校验...");
    for (SectorSnapshot &s : result.sectors) {
        // 如果 AI 提供了趋势描述，使用它；但必须确保与 action 不矛盾
        if (!s.aiTrendSummary.isEmpty()) {
            const bool aiTrendBullish = s.aiTrendSummary.contains("多") ||
                s.aiTrendSummary.contains("涨") || s.aiTrendSummary.contains("突破");
            const bool aiTrendBearish = s.aiTrendSummary.contains("空") ||
                s.aiTrendSummary.contains("跌") || s.aiTrendSummary.contains("回调");

            // 只有 AI 趋势描述与 action 方向不矛盾时才采用
            bool consistent = true;
            if (s.action == AdviceAction::Increase && aiTrendBearish && !aiTrendBullish)
                consistent = false;
            if (s.action == AdviceAction::Decrease && aiTrendBullish && !aiTrendBearish)
                consistent = false;

            if (consistent)
                s.trendSummary = s.aiTrendSummary;
            // else 保持规则引擎的 trendSummary
        }

        // 保存 AI 生成的操作建议（如果有）
        const QString aiOpAdvice = s.strategy.operationAdvice;

        // 重新生成 strategy 使其 actionLabel 与 action 一致
        s.strategy = StrategyEngine::generate(s);

        // 恢复 AI 的操作建议并追加到规则引擎建议之后
        if (!aiOpAdvice.isEmpty() && aiOpAdvice != s.strategy.operationAdvice) {
            s.strategy.operationAdvice += "\n\n【AI策略建议】" + aiOpAdvice;
        }
        if (!s.aiPredictionReason.isEmpty()) {
            s.strategy.operationAdvice += "\n\n【AI研判】" + s.aiPredictionReason;
        }
    }

    reportProgress(progress, 100, "分析完成");
    return result;
}
