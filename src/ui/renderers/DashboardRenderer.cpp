#include "ui/renderers/DashboardRenderer.h"

#include "domain/AnalysisResult.h"

#include <algorithm>

namespace InvestInsight::Ui {
namespace {

QString num(double value, int digits = 2)
{
    return QString::number(value, 'f', digits);
}

QString pct(double value)
{
    return (value >= 0.0 ? "+" : "") + num(value, 2) + "%";
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
    case AdviceAction::Increase: return QString::fromUtf8("增配");
    case AdviceAction::Decrease: return QString::fromUtf8("减配");
    default: return QString::fromUtf8("持有");
    }
}

QString tagClass(AdviceAction action)
{
    switch (action) {
    case AdviceAction::Increase: return "tag-up";
    case AdviceAction::Decrease: return "tag-down";
    default: return "tag-hold";
    }
}

QString escaped(const QString &text)
{
    return text.toHtmlEscaped();
}

QString firstNonEmpty(const QStringList &items, const QString &fallback = {})
{
    for (const QString &item : items) {
        const QString text = item.trimmed();
        if (!text.isEmpty()) return text;
    }
    return fallback;
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

QString evidenceSource(const MacroEvent &event)
{
    for (const MacroEventEvidence &evidence : event.evidence) {
        if (!evidence.source.trimmed().isEmpty()) return evidence.source.trimmed();
    }
    return QString::fromUtf8("规则事件");
}

QString evidenceDate(const MacroEvent &event)
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
    if (event.aiInsight.valid && !event.aiInsight.whyItMatters.isEmpty()) return event.aiInsight.whyItMatters;
    if (event.aiInsight.valid && !event.aiInsight.summary.isEmpty()) return event.aiInsight.summary;
    for (const MacroEventEvidence &evidence : event.evidence) {
        if (!evidence.summary.trimmed().isEmpty()) return evidence.summary.trimmed();
    }
    return event.checkpoint.isEmpty()
        ? QString::fromUtf8("等待更多新闻证据确认事件影响。")
        : QString::fromUtf8("下一步关注：") + event.checkpoint;
}

QString sectorPrimaryReason(const SectorSnapshot &sector)
{
    if (sector.aiInsight.valid && !sector.aiInsight.primaryReason.isEmpty()) return sector.aiInsight.primaryReason;
    if (!sector.eventSummary.isEmpty()) return sector.eventSummary;
    if (!sector.trendSummary.isEmpty()) return sector.trendSummary;
    return firstNonEmpty(sector.positiveFactors, sector.personalAdvice);
}

QString sectorPrimaryRisk(const SectorSnapshot &sector)
{
    if (sector.aiInsight.valid && !sector.aiInsight.primaryRisk.isEmpty()) return sector.aiInsight.primaryRisk;
    return firstNonEmpty(sector.negativeFactors, QString::fromUtf8("暂无突出风险，继续观察资金和涨幅是否背离。"));
}

QString sectorNextCheckpoint(const SectorSnapshot &sector)
{
    if (sector.aiInsight.valid && !sector.aiInsight.nextCheckpoint.isEmpty()) return sector.aiInsight.nextCheckpoint;
    return firstNonEmpty(sector.upcomingEvents + sector.futureEventsAI,
                         QString::fromUtf8("观察新闻延续性、资金流和回调确认。"));
}

QString metricCard(const QString &label,
                   const QString &value,
                   const QString &hint,
                   const QString &color,
                   const ThemeColors &theme)
{
    return "<td style='width:25%;padding:12px 10px;background:" + theme.narrativeBg
        + ";border:1px solid " + theme.cardBorder
        + ";border-radius:8px;text-align:center;'>"
        + "<div style='font-size:10px;color:" + theme.mutedColor + ";'>" + escaped(label) + "</div>"
        + "<div style='font-size:20px;font-weight:800;color:" + color + ";margin:4px 0;'>"
        + escaped(value) + "</div>"
        + "<div style='font-size:10px;color:" + theme.subtleColor + ";'>" + escaped(hint) + "</div>"
        + "</td>";
}

int indexCount(const AnalysisResult &analysis)
{
    int count = 0;
    const auto add = [&count](const IndexSnapshot &idx) {
        if (!idx.name.isEmpty()) ++count;
    };
    add(analysis.marketCtx.shanghai);
    add(analysis.marketCtx.shenzhen);
    add(analysis.marketCtx.chinext);
    add(analysis.marketCtx.csi300);
    add(analysis.marketCtx.csi500);
    add(analysis.marketCtx.nasdaq);
    add(analysis.marketCtx.sp500);
    add(analysis.marketCtx.dowjones);
    return count;
}

QString renderIndexCell(const IndexSnapshot &idx, const ThemeColors &theme)
{
    if (idx.name.isEmpty()) return {};
    const QString change = idx.changePctValid || !qFuzzyIsNull(idx.changePct)
        ? pct(idx.changePct) : QString::fromUtf8("暂无");
    return "<td style='padding:10px;border-bottom:1px solid " + theme.tableBorder + ";'>"
        + "<b>" + escaped(idx.name) + "</b>"
        + "<div style='font-size:11px;color:" + changeColor(idx.changePct, theme) + ";'>"
        + escaped(change) + "</div></td>";
}

QList<const SectorSnapshot *> topSectors(const AnalysisResult &analysis, int limit)
{
    QList<const SectorSnapshot *> sectors;
    for (const SectorSnapshot &sector : analysis.sectors) {
        sectors.push_back(&sector);
    }
    std::sort(sectors.begin(), sectors.end(), [](const SectorSnapshot *a, const SectorSnapshot *b) {
        return a->forecastScore > b->forecastScore;
    });
    while (sectors.size() > limit) sectors.removeLast();
    return sectors;
}

QString renderDashboardStatusBand(const AnalysisResult &analysis, const ThemeColors &theme)
{
    const QString riskLevel = analysis.marketCtx.riskLevel.isEmpty()
        ? QString::fromUtf8("等待市场风险评级")
        : analysis.marketCtx.riskLevel;
    QString topEvent = QString::fromUtf8("等待新闻与事件雷达刷新");
    if (!analysis.macroEvents.isEmpty()) {
        topEvent = eventReadableTitle(analysis.macroEvents.first());
    } else {
        const QList<const SectorSnapshot *> sectors = topSectors(analysis, 1);
        if (!sectors.isEmpty()) topEvent = sectorNextCheckpoint(*sectors.first());
    }

    QString h = "<div class='workspace-status-band insight-card' data-design='overview-status-band'>";
    h += "<table width='100%' cellspacing='0' cellpadding='0'><tr>";
    h += "<td width='34%' valign='top'><div class='status-title'>今日工作台</div>"
        "<div class='status-meta'>市场风险 "
        + escaped(num(analysis.marketCtx.marketRiskScore, 0))
        + " / " + escaped(riskLevel)
        + "</div></td>";
    h += "<td width='42%' valign='top'><div class='status-title'>下一观察点</div>"
        "<div class='status-meta'>" + escaped(shortText(topEvent, 86)) + "</div></td>";
    h += "<td width='24%' valign='top' align='right'><span class='state-chip'>"
        + (analysis.aiAvailable ? QStringLiteral("AI + Rules") : QStringLiteral("Rules"))
        + "</span><div class='status-meta' style='margin-top:6px;'>"
        + escaped(num(analysis.sectors.size(), 0)) + QString::fromUtf8(" 个板块 / ")
        + escaped(num(indexCount(analysis), 0)) + QString::fromUtf8(" 个指数</div></td>");
    h += "</tr></table></div>";
    Q_UNUSED(theme);
    return h;
}

QString renderMarketDashboard(const AnalysisResult &analysis, const ThemeColors &theme)
{
    QString h;
    h += "<div class='section-title'>市场仪表盘</div>";
    h += "<table style='width:100%;border-collapse:separate;border-spacing:6px 0;margin-bottom:10px;'><tr>";
    h += metricCard(QString::fromUtf8("市场风险"),
                    num(analysis.marketCtx.marketRiskScore, 0),
                    analysis.marketCtx.riskLevel.isEmpty() ? QString::fromUtf8("暂无评级") : analysis.marketCtx.riskLevel,
                    changeColor(analysis.marketCtx.marketRiskScore - 50.0, theme),
                    theme);
    h += metricCard(QString::fromUtf8("板块数量"),
                    num(analysis.sectors.size(), 0),
                    QString::fromUtf8("本轮分析覆盖"),
                    theme.headingColor,
                    theme);
    h += metricCard(QString::fromUtf8("指数数量"),
                    num(indexCount(analysis), 0),
                    QString::fromUtf8("市场环境样本"),
                    theme.headingColor,
                    theme);
    h += metricCard(QString::fromUtf8("北向资金"),
                    analysis.marketCtx.northboundFlowValid ? num(analysis.marketCtx.northboundNetBuy, 1) : QString::fromUtf8("暂无"),
                    QString::fromUtf8("亿元"),
                    changeColor(analysis.marketCtx.northboundNetBuy, theme),
                    theme);
    h += "</tr></table>";

    h += "<table class='overview'><tr>";
    h += renderIndexCell(analysis.marketCtx.shanghai, theme);
    h += renderIndexCell(analysis.marketCtx.shenzhen, theme);
    h += renderIndexCell(analysis.marketCtx.chinext, theme);
    h += renderIndexCell(analysis.marketCtx.csi300, theme);
    h += "</tr></table>";
    return h;
}

QString renderPortfolio(const AnalysisResult &analysis,
                        const DashboardRenderOptions &options,
                        const ThemeColors &theme)
{
    if (options.portfolio.isEmpty()) return {};

    double total = 0.0;
    double weightedForecast = 0.0;
    int matched = 0;
    for (const SectorSnapshot &sector : analysis.sectors) {
        const double amount = options.portfolio.value(sector.industry, 0.0);
        if (amount <= 0.0) continue;
        total += amount;
        weightedForecast += amount * sector.forecastScore;
        ++matched;
    }
    if (total > 0.0) weightedForecast /= total;

    QString h;
    h += "<div class='section-title'>我的持仓</div>";
    h += "<table style='width:100%;border-collapse:separate;border-spacing:6px 0;margin-bottom:10px;'><tr>";
    h += metricCard(QString::fromUtf8("持仓总额"), num(total, 0), QString::fromUtf8("元"), theme.headingColor, theme);
    h += metricCard(QString::fromUtf8("匹配板块"), num(matched, 0), QString::fromUtf8("已进入本轮分析"), theme.headingColor, theme);
    h += metricCard(QString::fromUtf8("加权预测"), num(weightedForecast, 3), QString::fromUtf8("按持仓金额加权"), changeColor(weightedForecast, theme), theme);
    h += metricCard(QString::fromUtf8("视图模式"),
                    options.simpleMode ? QString::fromUtf8("简洁") : QString::fromUtf8("完整"),
                    QString::fromUtf8("影响展示密度"),
                    theme.mutedColor,
                    theme);
    h += "</tr></table>";
    return h;
}

QString renderOpportunities(const AnalysisResult &analysis, const ThemeColors &theme)
{
    QString h;
    h += "<div class='section-title'>板块机会与风险</div>";
    h += "<table class='overview'><tr>"
         "<th>板块</th><th style='text-align:right;'>今日</th>"
         "<th style='text-align:right;'>事件</th><th style='text-align:right;'>趋势</th>"
         "<th style='text-align:center;'>动作</th><th>首要理由</th><th>首要风险</th><th>下一观察</th></tr>";
    for (const SectorSnapshot *sector : topSectors(analysis, 8)) {
        const QString change = sector->todayChangePctValid ? pct(sector->todayChangePct) : QString::fromUtf8("暂无");
        const QString reason = shortText(sectorPrimaryReason(*sector), 56);
        const QString risk = shortText(sectorPrimaryRisk(*sector), 56);
        const QString checkpoint = shortText(sectorNextCheckpoint(*sector), 46);
        h += "<tr><td><b>" + escaped(sector->industry) + "</b>"
            + "<div class='meta'>" + num(sector->dataQualityScore, 0) + QString::fromUtf8(" 数据质量</div></td>")
            + "<td style='text-align:right;color:" + changeColor(sector->todayChangePct, theme) + ";'>"
            + escaped(change) + "</td>"
            + "<td style='text-align:right;color:" + changeColor(sector->eventCatalystScore, theme) + ";'>"
            + num(sector->eventCatalystScore, 2) + "</td>"
            + "<td style='text-align:right;color:" + changeColor(sector->forecastScore, theme) + ";'>"
            + num(sector->forecastScore, 2) + "</td>"
            + "<td style='text-align:center;'><span class='tag " + tagClass(sector->action) + "'>"
            + actionText(sector->action) + "</span></td>"
            + "<td>" + escaped(reason) + "</td>"
            + "<td style='color:" + theme.warningColor + ";'>" + escaped(risk) + "</td>"
            + "<td class='meta' style='font-size:11px;'>" + escaped(checkpoint) + "</td></tr>";
    }
    h += "</table>";
    return h;
}

QString renderKeyEventRadar(const AnalysisResult &analysis, const ThemeColors &theme)
{
    QString h;
    h += "<div class='section-title'>关键事件雷达</div>";
    h += "<div class='split-grid'>";
    h += "<div>";
    int rows = 0;
    for (const MacroEvent &event : analysis.macroEvents) {
        const QString dateText = evidenceDate(event);
        const QString meta = evidenceSource(event)
            + (dateText.isEmpty() ? QString() : QString::fromUtf8(" · ") + dateText)
            + QString::fromUtf8(" · ") + toString(event.state)
            + QString::fromUtf8(" · 可信度 ") + num(event.confidence, 2);
        h += QStringLiteral("<div class='callout' style='margin-bottom:10px;'>")
            + "<div style='font-weight:800;color:" + theme.headingColor + ";'>"
            + linkHtml(eventReadableTitle(event), eventUrl(event)) + "</div>"
            + "<div class='meta' style='margin:5px 0 6px 0;'>" + escaped(meta) + "</div>"
            + "<div style='font-size:12px;color:" + theme.bodyColor + ";line-height:1.65;'>"
            + escaped(shortText(eventReadableSummary(event), 82)) + "</div>"
            + "<div class='meta' style='margin:6px 0 0 0;'>"
            + escaped(event.checkpoint.isEmpty() ? QString::fromUtf8("等待后续观察点") : event.checkpoint) + "</div>"
            + "</div>";
        if (++rows >= 5) break;
    }
    if (rows == 0) {
        for (const SectorSnapshot *sector : topSectors(analysis, 5)) {
            const QString eventText = sector->aiInsight.valid && !sector->aiInsight.readableTitle.isEmpty()
                ? sector->aiInsight.readableTitle
                : (!sector->eventSummary.isEmpty()
                    ? sector->eventSummary
                    : (!sector->newsHeadlines.isEmpty() ? sector->newsHeadlines.first() : sector->trendSummary));
            QString url;
            for (const NewsEntry &news : sector->newsEntries) {
                if (!news.url.trimmed().isEmpty()) { url = news.url.trimmed(); break; }
            }
            h += QStringLiteral("<div class='callout' style='margin-bottom:10px;'>")
                + "<div style='font-weight:800;color:" + theme.headingColor + ";'>"
                + linkHtml(eventText.isEmpty() ? sector->industry : shortText(eventText, 64), url) + "</div>"
                + "<div class='meta'>" + escaped(sector->industry)
                + QString::fromUtf8(" · 事件催化 ") + num(sector->eventCatalystScore, 2)
                + "</div><div style='font-size:12px;line-height:1.65;'>"
                + escaped(shortText(sectorPrimaryReason(*sector), 82)) + "</div></div>";
        }
    }
    h += "</div>";
    QString checkpoint = QString::fromUtf8("美联储议息会议 · CPI/PCE 数据");
    for (const MacroEvent &event : analysis.macroEvents) {
        if (!event.aiInsight.nextCheckpoint.isEmpty()) { checkpoint = event.aiInsight.nextCheckpoint; break; }
        if (!event.checkpoint.isEmpty()) { checkpoint = event.checkpoint; break; }
    }
    if (analysis.macroEvents.isEmpty()) {
        for (const SectorSnapshot *sector : topSectors(analysis, 1)) {
            checkpoint = sectorNextCheckpoint(*sector);
        }
    }
    h += "<div class='callout' style='border-color:" + theme.warningBorder + ";background:" + theme.warningBg + ";'>"
        + "<b>" + QString::fromUtf8("下一观察点") + "</b>"
        + "<div style='margin-top:6px;color:" + theme.warningColor + ";font-weight:700;'>"
        + escaped(shortText(checkpoint, 76)) + "</div>"
        + "<div class='meta' style='margin-top:6px;'>"
        + QString::fromUtf8("未确认事件只作为观察和仓位约束，不直接改写规则评分或买入动作。") + "</div>"
        + "</div>";
    h += "</div>";
    return h;
}

QString renderAiSummary(const AnalysisResult &analysis, const ThemeColors &theme)
{
    QString h;
    h += "<div class='section-title'>AI 与规则摘要</div>";
    if (analysis.aiAvailable && !analysis.aiOverallSummary.isEmpty()) {
        h += "<div class='narrative'>" + escaped(analysis.aiOverallSummary) + "</div>";
    } else {
        h += "<div class='narrative'>" + QString::fromUtf8("当前使用规则引擎生成摘要，AI 未启用或暂无有效输出。") + "</div>";
    }
    if (!analysis.aiMethodologyNote.isEmpty()) {
        h += "<div class='meta' style='margin-top:6px;'>" + escaped(analysis.aiMethodologyNote) + "</div>";
    }
    if (!analysis.aiErrors.isEmpty()) {
        h += "<div style='margin-top:8px;padding:8px;border:1px solid " + theme.warningBorder
            + ";background:" + theme.warningBg + ";color:" + theme.warningColor + ";border-radius:8px;'>";
        for (const QString &error : analysis.aiErrors) {
            h += "<div>" + escaped(error) + "</div>";
        }
        h += "</div>";
    }
    return h;
}

} // namespace

QString DashboardRenderer::render(const AnalysisResult &analysis,
                                  const ThemeColors &theme,
                                  const DashboardRenderOptions &options)
{
    QString h = "<html><head><style>" + buildHtmlCss(theme) + "</style></head><body>";
    h += "<div class='workspace-shell overview-dashboard' data-design='overview-dashboard'>";
    h += "<h1 style='font-size:18px;'>综合总览</h1>";
    h += "<div class='meta'>" + num(analysis.sectors.size(), 0)
        + QString::fromUtf8(" 个板块 · ") + num(indexCount(analysis), 0)
        + QString::fromUtf8(" 个指数");
    if (analysis.aiAvailable) h += " · <span class='ai-badge'>AI</span>";
    h += "</div>";

    h += renderDashboardStatusBand(analysis, theme);
    h += renderMarketDashboard(analysis, theme);
    h += renderPortfolio(analysis, options, theme);
    h += renderKeyEventRadar(analysis, theme);
    h += renderOpportunities(analysis, theme);
    h += renderAiSummary(analysis, theme);
    h += "<div class='disclaimer'>" + QString::fromUtf8("以上内容仅用于投研辅助，不构成投资建议。") + "</div>";
    h += "</div>";
    h += "</body></html>";
    return h;
}

} // namespace InvestInsight::Ui
