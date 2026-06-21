#include "ui/AppTheme.h"
#include "ui/renderers/StrategyRenderer.h"

#include "domain/AnalysisResult.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
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
    snap.forecastScore = score;
    snap.todayChangePct = change;
    snap.todayChangePctValid = true;
    snap.trendSummary = change > 0 ? QString::fromUtf8("趋势向上") : QString::fromUtf8("趋势走弱");
    snap.strategy.actionLabel = action == AdviceAction::Increase
        ? QString::fromUtf8("回调分批加仓") : QString::fromUtf8("降低仓位");
    snap.strategy.operationAdvice = QString::fromUtf8("按信号分批执行");
    snap.strategy.stopLossPct = 4.5;
    snap.strategy.supportLevel1 = 98.2;
    snap.cumulativeReturn = change * 2.0;
    snap.upcomingEvents << QString::fromUtf8("产业会议进入召开窗口");
    snap.futureEventsAI << QString::fromUtf8("AI 预计订单兑现节奏加快");
    StrategyBacktest bt;
    bt.name = QString::fromUtf8("趋势跟踪");
    bt.totalTrades = 8;
    bt.winRate = 62;
    snap.backtestResults << bt;
    return snap;
}

QString portfolioJson()
{
    QJsonArray arr;
    QJsonObject holding;
    holding.insert("sector", QString::fromUtf8("半导体"));
    holding.insert("amount", 12000.0);
    holding.insert("holdType", QString::fromUtf8("基金"));
    holding.insert("date", "2026-06-18");
    holding.insert("remark", QString::fromUtf8("核心仓"));
    arr.append(holding);
    return QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact));
}

AnalysisResult makeAnalysis()
{
    AnalysisResult analysis;
    analysis.sectors << makeSector(QString::fromUtf8("半导体"), AdviceAction::Increase, 0.42, 2.19)
                     << makeSector(QString::fromUtf8("有色金属"), AdviceAction::Hold, 0.04, -0.77)
                     << makeSector(QString::fromUtf8("锂电池"), AdviceAction::Decrease, -0.28, -1.85)
                     << makeSector(QString::fromUtf8("机器人"), AdviceAction::Increase, 0.31, 1.22)
                     << makeSector(QString::fromUtf8("证券"), AdviceAction::Hold, 0.02, 0.18);
    analysis.marketCtx.shanghai.name = QString::fromUtf8("上证指数");
    analysis.marketCtx.shanghai.changePct = 0.46;
    analysis.marketCtx.shanghai.changePctValid = true;
    return analysis;
}

} // namespace

int runStrategyRendererSmoke()
{
    const auto theme = InvestInsight::Ui::lightTheme();
    InvestInsight::Ui::StrategyRenderOptions options;
    options.simpleMode = false;
    options.portfolioBatchesJson = portfolioJson();

    const QString html = InvestInsight::Ui::StrategyRenderer::render(makeAnalysis(), theme, options);
    expect(html.contains("<html>"), "strategy html contains document root");
    expect(html.contains("table.fund"), "strategy html keeps shared fund table class");
    expect(html.contains(QString::fromUtf8("市场操作建议")), "strategy html contains market action section");
    expect(html.contains(QString::fromUtf8("跟踪状态")), "strategy html contains tracking state cards");
    expect(html.contains(QString::fromUtf8("推荐关注板块")), "strategy html contains top opportunity section");
    expect(html.contains(QString::fromUtf8("建议回避板块")), "strategy html contains avoid section");
    expect(html.contains(QString::fromUtf8("指数方向参考")), "strategy html contains index reference");
    expect(html.contains(QString::fromUtf8("我的持仓诊断")), "strategy html contains portfolio diagnostics");
    expect(html.contains(QString::fromUtf8("未来事件日历")), "strategy html contains future events");
    expect(html.contains("jump-1"), "strategy html keeps sector jump links");
    expect(html.contains("jumpi-SH"), "strategy html keeps index jump links");
    expect(html.contains(theme.bodyBg), "strategy html applies theme css");

    if (failures > 0) {
        QTextStream(stderr) << failures << " strategy renderer smoke check(s) failed.\n";
        return 1;
    }

    QTextStream(stdout) << "StrategyRenderer smoke passed.\n";
    return 0;
}
