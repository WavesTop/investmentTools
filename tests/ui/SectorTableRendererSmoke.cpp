#include "ui/AppTheme.h"
#include "ui/renderers/SectorTableRenderer.h"

#include "domain/AnalysisResult.h"

#include <QTextStream>

namespace {

int failures = 0;

void expect(bool condition, const QString &message)
{
    if (condition) return;
    QTextStream(stderr) << "FAIL: " << message << '\n';
    ++failures;
}

SectorSnapshot makeSector(const QString &name, AdviceAction action, double score, double change)
{
    SectorSnapshot snap;
    snap.industry = name;
    snap.action = action;
    snap.todayChangePct = change;
    snap.todayChangePctValid = true;
    snap.forecastScore = score;
    snap.confidence = 0.76;
    snap.dataQualityScore = 84.0;
    snap.sourceConsistencyScore = 91.0;
    snap.fiveDayMomentum = change * 1.4;
    snap.trendSummary = change > 0 ? QString::fromUtf8("偏多") : QString::fromUtf8("偏空");
    snap.sectorTierLabel = QString::fromUtf8("核心");
    snap.strategy.actionLabel = action == AdviceAction::Increase
        ? QString::fromUtf8("回调分批关注") : QString::fromUtf8("控制仓位");
    snap.newsHitCount = 5;
    snap.tech.techScore = score;
    snap.tech.macdGoldenCross = action == AdviceAction::Increase;
    snap.peRatio = 18.0;
    snap.pePercentile = 42.0;
    snap.crowdingIndex = 58.0;
    return snap;
}

AnalysisResult makeAnalysis()
{
    AnalysisResult analysis;
    analysis.sectors << makeSector(QString::fromUtf8("半导体"), AdviceAction::Increase, 0.42, 2.19)
                     << makeSector(QString::fromUtf8("有色金属"), AdviceAction::Hold, 0.05, -0.77)
                     << makeSector(QString::fromUtf8("锂电池"), AdviceAction::Increase, 0.35, 5.06);
    analysis.marketCtx.shanghai.name = QString::fromUtf8("上证指数");
    analysis.marketCtx.shanghai.changePct = 0.46;
    analysis.marketCtx.shanghai.changePctValid = true;
    analysis.marketCtx.shanghai.klineSeries = {3000, 3010, 3025, 3038, 3044, 3050};
    return analysis;
}

} // namespace

int runSectorTableRendererSmoke()
{
    const auto theme = InvestInsight::Ui::lightTheme();
    InvestInsight::Ui::SectorTableRenderOptions options;
    options.simpleMode = false;
    options.sortIndex = 1;

    const QString html = InvestInsight::Ui::SectorTableRenderer::render(makeAnalysis(), theme, options);
    expect(html.contains("<html>"), "sector table html contains document root");
    expect(html.contains("table.overview"), "sector table html keeps shared overview table class");
    expect(html.contains(QString::fromUtf8("板块机会")), "sector table html contains page title");
    expect(html.contains(QString::fromUtf8("事件催化")), "sector table html contains event catalyst column");
    expect(html.contains(QString::fromUtf8("风险提示")), "sector table html contains risk hint column");
    expect(html.contains("jump-1"), "sector table html keeps sector jump links");
    expect(html.contains("jumpi-SH"), "sector table html keeps index jump links");
    expect(html.contains(QString::fromUtf8("半导体")), "sector table html renders sector names");
    expect(html.contains(theme.bodyBg), "sector table html applies theme css");

    InvestInsight::Ui::SectorTableRenderOptions filtered;
    filtered.searchText = QString::fromUtf8("半导体");
    const QString filteredHtml = InvestInsight::Ui::SectorTableRenderer::render(makeAnalysis(), theme, filtered);
    expect(filteredHtml.contains(QString::fromUtf8("半导体")), "sector table filter keeps matching sector");
    expect(!filteredHtml.contains(QString::fromUtf8("有色金属")), "sector table filter removes non-matching sector");

    if (failures > 0) {
        QTextStream(stderr) << failures << " sector table renderer smoke check(s) failed.\n";
        return 1;
    }

    QTextStream(stdout) << "SectorTableRenderer smoke passed.\n";
    return 0;
}
