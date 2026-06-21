#include "ui/renderers/EventRadarRenderer.h"

#include "domain/AnalysisResult.h"

#include <algorithm>
#include <cmath>

namespace InvestInsight::Ui {
namespace {

QString num(double value, int digits = 1)
{
    return QString::number(value, 'f', digits);
}

QString pct(double value)
{
    return (value >= 0.0 ? "+" : "") + QString::number(value, 'f', 2) + "%";
}

QString escaped(const QString &text)
{
    return text.toHtmlEscaped();
}

QString changeColor(double value, const ThemeColors &theme)
{
    if (value > 0.001) return "#EF4444";
    if (value < -0.001) return "#3B82F6";
    return theme.neutralColor;
}

QString actionText(AdviceAction action)
{
    switch (action) {
    case AdviceAction::Increase: return QString::fromUtf8("增配观察");
    case AdviceAction::Decrease: return QString::fromUtf8("风险回避");
    default: return QString::fromUtf8("继续观察");
    }
}

double eventScore(const SectorSnapshot &sector)
{
    return sector.newsHitCount
        + (sector.upcomingEvents.size() + sector.futureEventsAI.size()) * 2.0
        + sector.eventImpacts.size() * 3.0
        + std::abs(sector.eventCatalystScore) * 12.0
        + std::abs(sector.forecastScore) * 8.0
        + std::abs(sector.todayChangePct) * 0.5;
}

QString firstNonEmpty(const QStringList &items, const QString &fallback)
{
    for (const QString &item : items) {
        if (!item.trimmed().isEmpty()) return item;
    }
    return fallback;
}

QString primaryEvent(const SectorSnapshot &sector)
{
    if (!sector.eventSummary.isEmpty()) {
        return sector.eventSummary;
    }
    if (!sector.eventImpacts.isEmpty() && !sector.eventImpacts.first().eventTitle.isEmpty()) {
        return sector.eventImpacts.first().eventTitle;
    }
    if (!sector.upcomingEvents.isEmpty()) {
        return firstNonEmpty(sector.upcomingEvents, QString());
    }
    if (!sector.futureEventsAI.isEmpty()) {
        return firstNonEmpty(sector.futureEventsAI, QString());
    }
    if (!sector.newsHeadlines.isEmpty()) {
        return firstNonEmpty(sector.newsHeadlines, QString());
    }
    if (!sector.positiveFactors.isEmpty()) {
        return firstNonEmpty(sector.positiveFactors, QString());
    }
    if (!sector.negativeFactors.isEmpty()) {
        return firstNonEmpty(sector.negativeFactors, QString());
    }
    return QString::fromUtf8("暂无明确事件，等待新增新闻确认");
}

QList<const SectorSnapshot *> rankedEventSectors(const AnalysisResult &analysis, int maxItems)
{
    QList<const SectorSnapshot *> sectors;
    for (const SectorSnapshot &sector : analysis.sectors) {
        if (sector.newsHitCount <= 0
            && sector.upcomingEvents.isEmpty()
            && sector.futureEventsAI.isEmpty()
            && sector.eventImpacts.isEmpty()
            && sector.newsHeadlines.isEmpty()
            && sector.positiveFactors.isEmpty()
            && sector.negativeFactors.isEmpty()) {
            continue;
        }
        sectors.push_back(&sector);
    }
    std::sort(sectors.begin(), sectors.end(), [](const SectorSnapshot *a, const SectorSnapshot *b) {
        return eventScore(*a) > eventScore(*b);
    });
    while (sectors.size() > maxItems) sectors.removeLast();
    return sectors;
}

QString renderMetric(const QString &label, const QString &value, const QString &hint,
                     const QString &color, const ThemeColors &theme)
{
    return "<td style='width:25%;padding:10px;background:" + theme.narrativeBg
        + ";border:1px solid " + theme.cardBorder + ";border-radius:8px;'>"
        + "<div style='font-size:10px;color:" + theme.mutedColor + ";'>" + escaped(label) + "</div>"
        + "<div style='font-size:19px;font-weight:800;color:" + color + ";margin:3px 0;'>"
        + escaped(value) + "</div>"
        + "<div style='font-size:10px;color:" + theme.subtleColor + ";'>" + escaped(hint) + "</div>"
        + "</td>";
}

QString renderSummary(const AnalysisResult &analysis,
                      const QList<const SectorSnapshot *> &items,
                      const ThemeColors &theme)
{
    int futureCount = 0;
    int newsCount = 0;
    int structuredEvents = analysis.macroEvents.size();
    for (const SectorSnapshot *sector : items) {
        futureCount += sector->upcomingEvents.size() + sector->futureEventsAI.size();
        newsCount += sector->newsHitCount;
        structuredEvents += sector->eventImpacts.size();
    }
    const QString riskLevel = analysis.marketCtx.riskLevel.isEmpty()
        ? QString::fromUtf8("暂无评级") : analysis.marketCtx.riskLevel;

    QString h;
    h += "<table style='width:100%;border-collapse:separate;border-spacing:6px 0;margin-bottom:12px;'><tr>";
    h += renderMetric(QString::fromUtf8("事件板块"), num(items.size(), 0),
                      QString::fromUtf8("按新闻/未来事件排序"), theme.headingColor, theme);
    h += renderMetric(QString::fromUtf8("结构事件"), num(structuredEvents, 0),
                      QString::fromUtf8("MacroEvent + 影响路径"), theme.headingColor, theme);
    h += renderMetric(QString::fromUtf8("新闻热度"), num(newsCount, 0),
                      QString::fromUtf8("关联新闻条数"), theme.headingColor, theme);
    h += renderMetric(QString::fromUtf8("市场风险"), num(analysis.marketCtx.marketRiskScore, 0),
                      riskLevel, changeColor(analysis.marketCtx.marketRiskScore - 50.0, theme), theme);
    h += "</tr></table>";
    return h;
}

QString renderQueue(const QList<const SectorSnapshot *> &items, const ThemeColors &theme)
{
    QString h;
    h += "<div class='section-title'>关键事件队列</div>";
    if (items.isEmpty()) {
        h += "<div class='narrative'>暂无可排序事件。请先运行分析，或等待新的新闻、政策和未来事件进入系统。</div>";
        return h;
    }
    h += "<table class='overview'><tr><th>板块</th><th>事件/催化</th><th>事件催化分</th><th>信号</th><th>今日</th><th>评分</th></tr>";
    for (const SectorSnapshot *sector : items) {
        h += "<tr><td><b>" + escaped(sector->industry) + "</b></td>"
            + "<td>" + escaped(primaryEvent(*sector)) + "</td>"
            + "<td style='color:" + changeColor(sector->eventCatalystScore, theme) + ";font-weight:700;'>"
            + escaped(num(sector->eventCatalystScore, 2)) + "</td>"
            + "<td>" + escaped(actionText(sector->action)) + "</td>"
            + "<td style='color:" + changeColor(sector->todayChangePct, theme) + ";'>"
            + escaped(sector->todayChangePctValid ? pct(sector->todayChangePct) : QString::fromUtf8("暂无")) + "</td>"
            + "<td>" + escaped(num(sector->forecastScore * 100.0, 0)) + "</td></tr>";
    }
    h += "</table>";
    return h;
}

QString renderPath(const QList<const SectorSnapshot *> &items)
{
    QString h;
    h += "<div class='section-title'>事件传导路径</div>";
    h += "<table class='overview'><tr><th>上游触发</th><th>影响板块</th><th>观察口径</th></tr>";
    int rows = 0;
    for (const SectorSnapshot *sector : items) {
        if (!sector->eventImpacts.isEmpty()) {
            for (const SectorEventImpact &impact : sector->eventImpacts) {
                h += "<tr><td>" + escaped(impact.eventTitle) + "</td>"
                    + "<td><b>" + escaped(sector->industry) + "</b></td>"
                    + "<td>" + escaped(impact.path + QString::fromUtf8("；") + impact.explanation) + "</td></tr>";
                if (++rows >= 5) break;
            }
            if (rows >= 5) break;
            continue;
        }

        const QString trigger = primaryEvent(*sector);
        const QString effect = firstNonEmpty(sector->positiveFactors + sector->negativeFactors,
                                             sector->trendSummary.isEmpty()
                                                 ? QString::fromUtf8("等待行情与新闻交叉确认")
                                                 : sector->trendSummary);
        h += "<tr><td>" + escaped(trigger) + "</td>"
            + "<td><b>" + escaped(sector->industry) + "</b></td>"
            + "<td>" + escaped(effect) + "</td></tr>";
        if (++rows >= 5) break;
    }
    if (rows == 0) {
        h += "<tr><td colspan='3'>暂无事件传导路径，等待新闻归因或 AI 前瞻事件补充。</td></tr>";
    }
    h += "</table>";
    return h;
}

QString renderRisk(const AnalysisResult &analysis,
                   const QList<const SectorSnapshot *> &items,
                   const ThemeColors &theme)
{
    QString h;
    h += "<div class='section-title'>风险与失效条件</div>";
    h += "<div class='narrative'>";
    h += QString::fromUtf8("市场综合风险：") + escaped(num(analysis.riskRadar.compositeRisk, 0));
    if (!analysis.riskRadar.riskAdvice.isEmpty()) {
        h += " · " + escaped(analysis.riskRadar.riskAdvice);
    }
    h += "</div>";
    h += "<table class='overview'><tr><th>板块</th><th>主要风险</th><th>数据质量</th></tr>";
    int rows = 0;
    for (const SectorSnapshot *sector : items) {
        if (sector->negativeFactors.isEmpty() && sector->dataQualityScore >= 80.0) continue;
        const QString risk = firstNonEmpty(sector->negativeFactors,
                                           QString::fromUtf8("暂无显著看空因素，重点观察数据是否延续"));
        h += "<tr><td><b>" + escaped(sector->industry) + "</b></td>"
            + "<td>" + escaped(risk) + "</td>"
            + "<td style='color:" + changeColor(sector->dataQualityScore - 70.0, theme) + ";'>"
            + escaped(num(sector->dataQualityScore, 0)) + "</td></tr>";
        if (++rows >= 5) break;
    }
    if (rows == 0) {
        h += "<tr><td colspan='3'>当前事件队列暂无突出失效条件，仍需关注涨幅过快、资金转弱和新闻衰减。</td></tr>";
    }
    h += "</table>";
    return h;
}

} // namespace

QString EventRadarRenderer::render(const AnalysisResult &analysis,
                                   const ThemeColors &theme,
                                   const EventRadarRenderOptions &options)
{
    const QList<const SectorSnapshot *> items = rankedEventSectors(analysis, options.maxItems);
    QString h;
    h += "<html><head><meta charset='utf-8'><style>";
    h += buildHtmlCss(theme);
    h += "</style></head><body>";
    h += "<h1>事件雷达</h1>";
    h += "<p class='meta'>将新闻、未来催化、行情确认和风险条件组织为一个可扫描的事件工作台。</p>";
    h += renderSummary(analysis, items, theme);
    h += renderQueue(items, theme);
    if (!options.simpleMode) {
        h += renderPath(items);
        h += renderRisk(analysis, items, theme);
    }
    h += "</body></html>";
    return h;
}

} // namespace InvestInsight::Ui
