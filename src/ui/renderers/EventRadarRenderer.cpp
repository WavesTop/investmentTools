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

QString shortText(const QString &text, int maxChars)
{
    const QString cleaned = text.simplified();
    if (cleaned.size() <= maxChars) return cleaned;
    return cleaned.left(maxChars - 1) + QString::fromUtf8("…");
}

QString linkHtml(const QString &title, const QString &url)
{
    const QString safeTitle = escaped(title);
    if (url.trimmed().isEmpty()) return safeTitle;
    return "<a href='" + escaped(url.trimmed()) + "'>" + safeTitle + "</a>";
}

QString eventUrl(const MacroEvent &event)
{
    for (const MacroEventEvidence &evidence : event.evidence) {
        if (!evidence.url.trimmed().isEmpty()) return evidence.url.trimmed();
    }
    return {};
}

QString eventSource(const MacroEvent &event)
{
    for (const MacroEventEvidence &evidence : event.evidence) {
        if (!evidence.source.trimmed().isEmpty()) return evidence.source.trimmed();
    }
    return QString::fromUtf8("规则事件");
}

QString eventPublishedAt(const MacroEvent &event)
{
    for (const MacroEventEvidence &evidence : event.evidence) {
        if (evidence.publishedAt.isValid()) return evidence.publishedAt.toString("MM-dd HH:mm");
    }
    if (event.publishedAt.isValid()) return event.publishedAt.toString("MM-dd HH:mm");
    if (event.detectedAt.isValid()) return event.detectedAt.toString("MM-dd HH:mm");
    return {};
}

QString eventReadableTitle(const MacroEvent &event)
{
    if (event.aiInsight.valid && !event.aiInsight.readableTitle.isEmpty()) return event.aiInsight.readableTitle;
    for (const MacroEventEvidence &evidence : event.evidence) {
        if (!evidence.title.trimmed().isEmpty()) return evidence.title.trimmed();
    }
    return event.title;
}

QString eventReadableSummary(const MacroEvent &event)
{
    if (event.aiInsight.valid && !event.aiInsight.summary.isEmpty()) return event.aiInsight.summary;
    if (event.aiInsight.valid && !event.aiInsight.whyItMatters.isEmpty()) return event.aiInsight.whyItMatters;
    for (const MacroEventEvidence &evidence : event.evidence) {
        if (!evidence.summary.trimmed().isEmpty()) return evidence.summary.trimmed();
    }
    return QString::fromUtf8("等待更多证据确认影响路径。");
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
    if (sector.aiInsight.valid && !sector.aiInsight.readableTitle.isEmpty()) {
        return sector.aiInsight.readableTitle;
    }
    if (sector.aiInsight.valid && !sector.aiInsight.summary.isEmpty()) {
        return sector.aiInsight.summary;
    }
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

QString checkpointText(const MacroEvent &event)
{
    QStringList checkpoints;
    if (!event.checkpoint.trimmed().isEmpty()) checkpoints << event.checkpoint.trimmed();
    for (const MacroEventCheckpoint &checkpoint : event.nextCheckpoints) {
        if (!checkpoint.name.trimmed().isEmpty()) checkpoints << checkpoint.name.trimmed();
    }
    checkpoints.removeDuplicates();
    return checkpoints.isEmpty() ? QString::fromUtf8("等待后续观察点") : checkpoints.join(QStringLiteral(" / "));
}

double evidenceReliability(const MacroEvent &event)
{
    double sum = 0.0;
    int count = 0;
    for (const MacroEventEvidence &evidence : event.evidence) {
        if (evidence.reliability <= 0.0) continue;
        sum += evidence.reliability;
        ++count;
    }
    return count > 0 ? sum / count : event.confidence;
}

QString renderEventTimeline(const AnalysisResult &analysis, const ThemeColors &theme)
{
    QString h = "<div class='section-title'>结构化事件时间线</div>";
    if (analysis.macroEvents.isEmpty()) {
        h += "<div class='narrative'>暂无结构化宏观事件，等待新闻抽取结果补充。</div>";
        return h;
    }

    h += "<table class='overview'><tr><th>事件</th><th>证据链</th><th>状态</th><th>观察点</th><th>影响说明</th></tr>";
    int rows = 0;
    for (const MacroEvent &event : analysis.macroEvents) {
        const QString dateText = eventPublishedAt(event);
        const QString source = eventSource(event)
            + (dateText.isEmpty() ? QString() : QString::fromUtf8(" · ") + dateText)
            + QString::fromUtf8(" · 可信度 ") + num(evidenceReliability(event), 2);
        h += "<tr><td><b>" + linkHtml(shortText(eventReadableTitle(event), 72), eventUrl(event)) + "</b>"
            + "<div class='meta'>" + escaped(toString(event.type)) + " · " + escaped(toString(event.region)) + "</div></td>"
            + "<td class='meta'>" + escaped(source) + "</td>"
            + "<td>" + escaped(toString(event.state)) + "</td>"
            + "<td>" + escaped(checkpointText(event)) + "</td>"
            + "<td>" + escaped(shortText(eventReadableSummary(event), 82)) + "</td></tr>";
        if (++rows >= 6) break;
    }
    h += "</table>";
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
    h += "<table class='overview'><tr><th>板块</th><th>可读事件</th><th>影响路径</th><th>首要理由</th><th>下一观察</th><th>信号</th></tr>";
    for (const SectorSnapshot *sector : items) {
        const QString path = sector->aiInsight.valid && !sector->aiInsight.impactPath.isEmpty()
            ? sector->aiInsight.impactPath
            : (sector->eventImpacts.isEmpty()
                ? sector->eventSummary
                : sector->eventImpacts.first().path);
        const QString reason = sector->aiInsight.valid && !sector->aiInsight.primaryReason.isEmpty()
            ? sector->aiInsight.primaryReason
            : firstNonEmpty(sector->positiveFactors + sector->negativeFactors,
                            sector->trendSummary);
        const QString checkpoint = sector->aiInsight.valid && !sector->aiInsight.nextCheckpoint.isEmpty()
            ? sector->aiInsight.nextCheckpoint
            : firstNonEmpty(sector->upcomingEvents + sector->futureEventsAI,
                            QString::fromUtf8("继续跟踪新闻和资金验证"));
        h += "<tr><td><b>" + escaped(sector->industry) + "</b></td>"
            + "<td>" + escaped(shortText(primaryEvent(*sector), 64)) + "</td>"
            + "<td>" + escaped(shortText(path, 72)) + "</td>"
            + "<td>" + escaped(shortText(reason, 60)) + "</td>"
            + "<td class='meta'>" + escaped(shortText(checkpoint, 54)) + "</td>"
            + "<td><span class='tag "
            + (sector->action == AdviceAction::Increase ? "tag-up" : (sector->action == AdviceAction::Decrease ? "tag-down" : "tag-hold"))
            + "'>" + escaped(actionText(sector->action)) + "</span>"
            + "<div class='meta' style='margin-top:4px;'>事件 " + num(sector->eventCatalystScore, 2)
            + " · 今日 " + escaped(sector->todayChangePctValid ? pct(sector->todayChangePct) : QString::fromUtf8("暂无"))
            + "</div></td></tr>";
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
                    + "<td>" + escaped(impact.path + QString::fromUtf8("；") + impact.explanation)
                    + "<br/><span class='meta'>" + escaped(toString(impact.horizon))
                    + QString::fromUtf8(" · 失效条件：")
                    + escaped(impact.condition.isEmpty() ? QString::fromUtf8("等待后续验证") : impact.condition)
                    + "</span></td></tr>";
                if (++rows >= 5) break;
            }
            if (rows >= 5) break;
            continue;
        }

        const QString trigger = primaryEvent(*sector);
        const QString effect = sector->aiInsight.valid && !sector->aiInsight.impactPath.isEmpty()
            ? sector->aiInsight.impactPath
            : firstNonEmpty(sector->positiveFactors + sector->negativeFactors,
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
        const QString risk = sector->aiInsight.valid && !sector->aiInsight.primaryRisk.isEmpty()
            ? sector->aiInsight.primaryRisk
            : firstNonEmpty(sector->negativeFactors,
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
    h += "<div class='workspace-shell event-radar-workbench' data-design='event-radar'>";
    h += "<h1>事件雷达</h1>";
    h += "<p class='meta'>将新闻、未来催化、行情确认和风险条件组织为一个可扫描的事件工作台。</p>";
    h += "<div class='workspace-status-band event-card' data-design='event-status-band'>";
    h += renderSummary(analysis, items, theme);
    h += "</div>";
    h += renderEventTimeline(analysis, theme);
    h += renderQueue(items, theme);
    if (!options.simpleMode) {
        h += renderPath(items);
        h += renderRisk(analysis, items, theme);
    }
    h += "</div></body></html>";
    return h;
}

} // namespace InvestInsight::Ui
