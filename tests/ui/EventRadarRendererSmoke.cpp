#include "ui/AppTheme.h"
#include "ui/renderers/EventRadarRenderer.h"

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

SectorSnapshot makeSector(const QString &name, AdviceAction action, double forecast, double change)
{
    SectorSnapshot snap;
    snap.industry = name;
    snap.action = action;
    snap.forecastScore = forecast;
    snap.todayChangePct = change;
    snap.todayChangePctValid = true;
    snap.newsHitCount = 8;
    snap.dataQualityScore = 88.0;
    snap.trendSummary = QString::fromUtf8("趋势延续，等待二次确认");
    snap.upcomingEvents << QString::fromUtf8("降息预期升温，关注流动性窗口");
    snap.futureEventsAI << QString::fromUtf8("未来两个月政策落地节奏可能影响估值");
    snap.positiveFactors << QString::fromUtf8("订单边际改善");
    snap.negativeFactors << QString::fromUtf8("短线涨幅偏高");
    snap.newsHeadlines << QString::fromUtf8("产业链催化继续发酵");

    SectorEventImpact impact;
    impact.eventTitle = QString::fromUtf8("美联储降息预期升温");
    impact.sector = name;
    impact.path = QString::fromUtf8("美债收益率下行 -> 成长股估值压力缓解");
    impact.explanation = QString::fromUtf8("半导体受益于成长风格估值修复");
    impact.direction = EventImpactDirection::Positive;
    impact.relation = EventImpactRelation::Indirect;
    impact.strength = 0.68;
    impact.confidence = 0.76;
    snap.eventImpacts << impact;
    snap.eventCatalystScore = 0.24;
    snap.eventSummary = QString::fromUtf8("美联储降息预期通过成长估值链条形成间接催化");
    return snap;
}

AnalysisResult makeAnalysis()
{
    AnalysisResult analysis;
    analysis.aiAvailable = true;
    analysis.sectors << makeSector(QString::fromUtf8("半导体"), AdviceAction::Increase, 0.42, 2.19)
                     << makeSector(QString::fromUtf8("有色金属"), AdviceAction::Hold, 0.10, -0.77);
    analysis.marketCtx.valid = true;
    analysis.marketCtx.marketRiskScore = 62.0;
    analysis.marketCtx.riskLevel = QString::fromUtf8("风险偏好回暖");
    analysis.riskRadar.compositeRisk = 58.0;
    analysis.riskRadar.riskAdvice = QString::fromUtf8("控制追高，等待回撤确认");
    return analysis;
}

} // namespace

int runEventRadarRendererSmoke()
{
    const auto theme = InvestInsight::Ui::lightTheme();
    InvestInsight::Ui::EventRadarRenderOptions options;
    options.simpleMode = false;

    const QString html = InvestInsight::Ui::EventRadarRenderer::render(makeAnalysis(), theme, options);

    expect(html.contains("<html>"), "event radar html contains document root");
    expect(html.contains(QString::fromUtf8("事件雷达")), "event radar html contains title");
    expect(html.contains(QString::fromUtf8("关键事件队列")), "event radar html contains event queue");
    expect(html.contains(QString::fromUtf8("事件传导路径")), "event radar html contains transmission path");
    expect(html.contains(QString::fromUtf8("风险与失效条件")), "event radar html contains risk section");
    expect(html.contains(QString::fromUtf8("事件催化分")), "event radar html renders catalyst score");
    expect(html.contains(QString::fromUtf8("美债收益率下行")), "event radar html renders structured impact path");
    expect(html.contains(QString::fromUtf8("半导体")), "event radar html renders sector names");
    expect(html.contains(QString::fromUtf8("降息预期")), "event radar html renders future catalyst");
    expect(html.contains(theme.bodyBg), "event radar html applies theme css");

    if (failures > 0) {
        QTextStream(stderr) << failures << " event radar renderer smoke check(s) failed.\n";
        return 1;
    }

    QTextStream(stdout) << "EventRadarRenderer smoke passed.\n";
    return 0;
}
