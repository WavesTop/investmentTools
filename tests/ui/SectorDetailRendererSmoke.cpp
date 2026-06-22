#include "ui/AppTheme.h"
#include "ui/renderers/SectorDetailRenderer.h"

#include "domain/AnalysisResult.h"

#include <QDate>
#include <QTextStream>

namespace {

int failures = 0;

void expect(bool condition, const QString &message)
{
    if (condition) return;
    QTextStream(stderr) << "FAIL: " << message << '\n';
    ++failures;
}

SectorSnapshot makeSector()
{
    SectorSnapshot snap;
    snap.industry = QString::fromUtf8("半导体");
    snap.action = AdviceAction::Increase;
    snap.todayChangePct = 2.19;
    snap.todayChangePctValid = true;
    snap.forecastScore = 0.42;
    snap.confidence = 0.81;
    snap.dataQualityScore = 88;
    snap.dataQualityNote = QString::fromUtf8("多源行情完整");
    snap.sourceConsistencyScore = 92;
    snap.trendSummary = QString::fromUtf8("趋势向上");
    snap.sectorHotScore = 75;
    snap.stockCount = 128;
    snap.fundFlowFactor = 0.18;
    snap.fundFlowSource = QString::fromUtf8("新浪资金流");
    snap.fiveDayMomentum = 10.2;
    snap.twentyDayMomentum = -12.6;
    snap.monthMomentum = 71.8;
    snap.cumulativeReturn = 54.87;
    snap.positiveFactors << QString::fromUtf8("政策催化") << QString::fromUtf8("订单改善");
    snap.negativeFactors << QString::fromUtf8("短线涨幅偏高");
    snap.newsHeadlines << QString::fromUtf8("半导体设备订单持续改善");
    snap.eventCatalystScore = 0.24;
    snap.eventSummary = QString::fromUtf8("美联储降息预期通过成长估值链条形成间接催化");
    SectorEventImpact impact;
    impact.eventTitle = QString::fromUtf8("美联储降息预期升温");
    impact.sector = snap.industry;
    impact.path = QString::fromUtf8("美债收益率下行 -> 成长股估值压力缓解");
    impact.explanation = QString::fromUtf8("半导体受益于成长风格估值修复");
    impact.direction = EventImpactDirection::Positive;
    impact.relation = EventImpactRelation::Indirect;
    impact.horizon = ImpactHorizon::MediumTerm;
    impact.condition = QString::fromUtf8("若 FOMC 转鹰则路径失效");
    impact.strength = 0.68;
    impact.confidence = 0.76;
    impact.sourceReliability = 0.91;
    snap.eventImpacts << impact;
    snap.strategy.actionLabel = QString::fromUtf8("回调分批加仓");
    snap.strategy.shortTermView = QString::fromUtf8("短期维持强势");
    snap.strategy.mediumTermView = QString::fromUtf8("中期关注业绩兑现");
    snap.strategy.longTermView = QString::fromUtf8("长期看国产替代");
    snap.strategy.stopLossPct = 4.5;
    snap.strategy.takeProfitPct = 9.8;
    snap.strategy.operationAdvice = QString::fromUtf8("分批执行，避免追高");
    snap.tech.techScore = 0.36;
    snap.tech.macdGoldenCross = true;
    snap.tech.rsi6 = 62;
    snap.tech.kdjK = 68;
    snap.tech.maLongArrange = true;

    StrategyBacktest bt;
    bt.name = QString::fromUtf8("趋势跟踪");
    bt.totalTrades = 8;
    bt.winRate = 62;
    bt.avgReturn = 4.8;
    snap.backtestResults << bt;

    NewsEntry news;
    news.date = "2026-06-20";
    news.source = QString::fromUtf8("财联社");
    news.title = QString::fromUtf8("先进封装需求继续提升");
    snap.newsEntries << news;

    const QDate start(2026, 1, 2);
    double close = 100.0;
    for (int i = 0; i < 80; ++i) {
        KBar bar;
        bar.date = start.addDays(i).toString("yyyy-MM-dd");
        bar.open = close;
        close += ((i % 7) - 2) * 0.22 + 0.16;
        bar.close = close;
        bar.high = close + 1.0;
        bar.low = bar.open - 1.0;
        bar.volume = 100000000 + i * 300000;
        snap.dailyBars << bar;
        snap.trendSeries << close;
        snap.fundFlowSeries << ((i % 5) - 2) * 0.6;
    }
    return snap;
}

} // namespace

int runSectorDetailRendererSmoke()
{
    const auto theme = InvestInsight::Ui::lightTheme();
    InvestInsight::Ui::SectorDetailRenderOptions options;
    options.aiAvailable = true;
    const QString html = InvestInsight::Ui::SectorDetailRenderer::render(makeSector(), theme, options);

    expect(html.contains("<html>"), "sector detail html contains document root");
    expect(html.contains(QString::fromUtf8("投资结论")), "sector detail contains investment conclusion");
    expect(html.contains("metric-grid"), "sector detail uses first-screen metric grid");
    expect(html.contains(QString::fromUtf8("核心评分")), "sector detail contains core score block");
    expect(html.contains(QString::fromUtf8("信号解释")), "sector detail contains signal explanation block");
    expect(html.contains(QString::fromUtf8("影响路径")), "sector detail contains impact path block");
    expect(html.contains(QString::fromUtf8("阶段收益与回测")), "sector detail contains staged return block");
    expect(html.contains(QString::fromUtf8("资金流与相关板块")), "sector detail contains fund flow relation block");
    expect(html.contains("data:image/png;base64"), "sector detail embeds chart image");
    expect(html.contains(QString::fromUtf8("技术指标")), "sector detail contains technical indicators");
    expect(html.contains(QString::fromUtf8("资金流")), "sector detail contains fund flow");
    expect(html.contains(QString::fromUtf8("事件驱动")), "sector detail contains event impact section");
    expect(html.contains(QString::fromUtf8("美债收益率下行")), "sector detail renders structured event path");
    expect(html.contains(QStringLiteral("MediumTerm")), "sector detail renders impact horizon");
    expect(html.contains(QStringLiteral("0.91")), "sector detail renders source reliability");
    expect(html.contains(QString::fromUtf8("若 FOMC 转鹰则路径失效")), "sector detail renders invalidation condition");
    expect(html.contains(QString::fromUtf8("策略回测")), "sector detail contains backtest");
    expect(html.contains(QString::fromUtf8("新闻证据")), "sector detail contains news evidence");
    expect(html.contains(QString::fromUtf8("数据质量")), "sector detail contains data quality");
    expect(html.contains(theme.bodyBg), "sector detail applies theme css");

    if (failures > 0) {
        QTextStream(stderr) << failures << " sector detail renderer smoke check(s) failed.\n";
        return 1;
    }

    QTextStream(stdout) << "SectorDetailRenderer smoke passed.\n";
    return 0;
}
