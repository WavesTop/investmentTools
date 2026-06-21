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
         "<th style='text-align:center;'>动作</th><th>主要理由 / 风险</th></tr>";
    for (const SectorSnapshot *sector : topSectors(analysis, 8)) {
        const QString change = sector->todayChangePctValid ? pct(sector->todayChangePct) : QString::fromUtf8("暂无");
        QString reason = !sector->trendSummary.isEmpty() ? sector->trendSummary : sector->personalAdvice;
        if (reason.isEmpty() && !sector->positiveFactors.isEmpty()) reason = sector->positiveFactors.first();
        if (!sector->negativeFactors.isEmpty()) reason += QString::fromUtf8("；风险：") + sector->negativeFactors.first();
        h += "<tr><td><b>" + escaped(sector->industry) + "</b>"
            + "<div class='meta'>" + num(sector->dataQualityScore, 0) + QString::fromUtf8(" 数据质量</div></td>")
            + "<td style='text-align:right;color:" + changeColor(sector->todayChangePct, theme) + ";'>"
            + escaped(change) + "</td>"
            + "<td style='text-align:right;color:" + changeColor(sector->eventCatalystScore, theme) + ";'>"
            + num(sector->eventCatalystScore, 2) + "</td>"
            + "<td style='text-align:right;color:" + changeColor(sector->forecastScore, theme) + ";'>"
            + num(sector->forecastScore, 2) + "</td>"
            + "<td style='text-align:center;'><span class='" + tagClass(sector->action) + "'>"
            + actionText(sector->action) + "</span></td>"
            + "<td>" + escaped(reason) + "</td></tr>";
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
        h += QStringLiteral("<div class='callout' style='margin-bottom:10px;'>")
            + "<b>" + escaped(event.title) + "</b>"
            + "<div class='meta'>" + escaped(toString(event.state)) + " · "
            + escaped(toString(event.type)) + " · " + escaped(event.checkpoint) + "</div>"
            + "</div>";
        if (++rows >= 3) break;
    }
    if (rows == 0) {
        for (const SectorSnapshot *sector : topSectors(analysis, 3)) {
            const QString eventText = !sector->eventSummary.isEmpty()
                ? sector->eventSummary
                : (!sector->newsHeadlines.isEmpty() ? sector->newsHeadlines.first() : sector->trendSummary);
            h += QStringLiteral("<div class='callout' style='margin-bottom:10px;'>")
                + "<b>" + escaped(eventText.isEmpty() ? sector->industry : eventText) + "</b>"
                + "<div class='meta'>" + escaped(sector->industry)
                + QString::fromUtf8(" · 事件催化 ") + num(sector->eventCatalystScore, 2)
                + "</div></div>";
        }
    }
    h += "</div>";
    h += "<div class='callout' style='border-color:" + theme.warningBorder + ";background:" + theme.warningBg + ";'>"
        + "<b>" + QString::fromUtf8("下一观察点") + "</b>"
        + "<div style='margin-top:6px;color:" + theme.warningColor + ";font-weight:700;'>"
        + QString::fromUtf8("美联储议息会议 · CPI/PCE 数据") + "</div>"
        + "<div class='meta' style='margin-top:6px;'>"
        + QString::fromUtf8("未发生事件以观察和仓位约束为主，不直接推动买入。") + "</div>"
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
    h += "<div class='workspace-shell'>";
    h += "<h1 style='font-size:18px;'>综合总览</h1>";
    h += "<div class='meta'>" + num(analysis.sectors.size(), 0)
        + QString::fromUtf8(" 个板块 · ") + num(indexCount(analysis), 0)
        + QString::fromUtf8(" 个指数");
    if (analysis.aiAvailable) h += " · <span class='ai-badge'>AI</span>";
    h += "</div>";

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
