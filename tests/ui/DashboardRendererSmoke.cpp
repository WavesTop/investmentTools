#include "ui/AppTheme.h"
#include "ui/renderers/DashboardRenderer.h"

#include "domain/AnalysisResult.h"

#include <QMap>
#include <QString>
#include <QTextStream>

namespace {

int failures = 0;

void expect(bool condition, const QString &message)
{
    if (condition) return;
    QTextStream(stderr) << "FAIL: " << message << '\n';
    ++failures;
}

SectorSnapshot makeSector(const QString &name, AdviceAction action, double forecast, double change)
{
    SectorSnapshot snap;
    snap.industry = name;
    snap.action = action;
    snap.forecastScore = forecast;
    snap.todayChangePct = change;
    snap.todayChangePctValid = true;
    snap.newsSentiment = forecast / 2.0;
    snap.confidence = 0.78;
    snap.dataQualityScore = 86.0;
    snap.newsHitCount = 9;
    snap.positiveNewsCount = 6;
    snap.negativeNewsCount = 1;
    snap.trendSummary = QString::fromUtf8("趋势确认，资金温和流入");
    snap.personalAdvice = QString::fromUtf8("关注回调后的分批机会");
    snap.positiveFactors << QString::fromUtf8("政策催化") << QString::fromUtf8("资金流改善");
    snap.negativeFactors << QString::fromUtf8("短线涨幅偏高");
    snap.newsHeadlines << QString::fromUtf8("产业链订单出现边际改善");
    return snap;
}

AnalysisResult makeAnalysis()
{
    AnalysisResult analysis;
    analysis.aiAvailable = true;
    analysis.aiOverallSummary = QString::fromUtf8("AI 综合判断：半导体具备趋势延续条件。");
    analysis.aiMethodologyNote = QString::fromUtf8("规则引擎与 AI 协同判断。");
    analysis.sectors << makeSector(QString::fromUtf8("半导体"), AdviceAction::Increase, 0.38, 2.19)
                     << makeSector(QString::fromUtf8("有色金属"), AdviceAction::Hold, 0.08, -0.77)
                     << makeSector(QString::fromUtf8("锂电池"), AdviceAction::Increase, 0.31, 5.06);
    analysis.marketCtx.valid = true;
    analysis.marketCtx.shanghai.name = QString::fromUtf8("上证指数");
    analysis.marketCtx.shanghai.changePct = 0.42;
    analysis.marketCtx.marketRiskScore = 63.0;
    analysis.marketCtx.riskLevel = QString::fromUtf8("风险偏好回暖");
    return analysis;
}

} // namespace

int runDashboardRendererSmoke()
{
    const auto theme = InvestInsight::Ui::lightTheme();
    InvestInsight::Ui::DashboardRenderOptions options;
    options.simpleMode = false;
    options.portfolio.insert(QString::fromUtf8("半导体"), 8000.0);

    const QString html = InvestInsight::Ui::DashboardRenderer::render(makeAnalysis(), theme, options);

    expect(html.contains("<html>"), "dashboard html contains document root");
    expect(html.contains("table.overview"), "dashboard html keeps shared overview table class");
    expect(html.contains(QString::fromUtf8("市场仪表盘")), "dashboard html contains market dashboard");
    expect(html.contains(QString::fromUtf8("关键机会")), "dashboard html contains opportunity section");
    expect(html.contains(QString::fromUtf8("半导体")), "dashboard html renders sector names");
    expect(html.contains(QString::fromUtf8("AI 综合判断")), "dashboard html renders AI summary");
    expect(html.contains(theme.bodyBg), "dashboard html applies theme css");

    if (failures > 0) {
        QTextStream(stderr) << failures << " dashboard renderer smoke check(s) failed.\n";
        return 1;
    }

    QTextStream(stdout) << "DashboardRenderer smoke passed.\n";
    return 0;
}
