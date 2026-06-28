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
    snap.aiInsight.valid = true;
    snap.aiInsight.readableTitle = QString::fromUtf8("先进封装订单改善");
    snap.aiInsight.primaryReason = QString::fromUtf8("订单改善与资金流入同时出现，短期关注度提升。");
    snap.aiInsight.primaryRisk = QString::fromUtf8("若财报确认不足，前期涨幅可能回吐。");
    snap.aiInsight.nextCheckpoint = QString::fromUtf8("跟踪头部设备公司订单和一季报指引。");
    NewsEntry news;
    news.title = QString::fromUtf8("产业链订单出现边际改善");
    news.source = QString::fromUtf8("测试源");
    news.url = QStringLiteral("https://example.com/news");
    snap.newsEntries << news;
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
    expect(html.contains("workspace-shell"), "dashboard html uses workspace shell");
    expect(html.contains("overview-dashboard"), "dashboard html uses v2.5 dashboard marker");
    expect(html.contains("workspace-status-band"), "dashboard html renders v2.5 status band");
    expect(html.contains("insight-card"), "dashboard html renders v2.5 insight cards");
    expect(html.contains(QString::fromUtf8("市场仪表盘")), "dashboard html contains market dashboard");
    expect(html.contains(QString::fromUtf8("关键事件雷达")), "dashboard html contains event radar section");
    expect(html.contains(QString::fromUtf8("板块机会与风险")), "dashboard html contains opportunity and risk section");
    expect(html.contains(QString::fromUtf8("下一观察点")), "dashboard html contains next checkpoint");
    expect(html.contains(QString::fromUtf8("首要理由")), "dashboard html contains primary reason column");
    expect(html.contains(QString::fromUtf8("首要风险")), "dashboard html contains primary risk column");
    expect(html.contains(QStringLiteral("href='https://example.com/news'")), "dashboard html renders source link");
    expect(html.contains(QString::fromUtf8("订单改善与资金流入")), "dashboard html renders AI primary reason");
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
