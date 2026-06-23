#include "ui/renderers/SectorDetailRenderer.h"

#include "domain/AnalysisResult.h"
#include "ui/renderers/ChartRenderer.h"

#include <QBuffer>
#include <QPixmap>

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

QString escaped(const QString &text)
{
    return text.toHtmlEscaped();
}

QString linkHtml(const QString &title, const QString &url)
{
    const QString safeTitle = escaped(title);
    if (url.trimmed().isEmpty()) return safeTitle;
    return "<a href='" + escaped(url.trimmed()) + "'>" + safeTitle + "</a>";
}

QString colorFor(double value, const ThemeColors &theme)
{
    if (value > 0.001) return "#EF4444";
    if (value < -0.001) return "#3B82F6";
    return theme.neutralColor;
}

QString pixmapToBase64(const QPixmap &pixmap)
{
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    pixmap.save(&buffer, "PNG");
    return QString::fromLatin1(bytes.toBase64());
}

QString actionText(AdviceAction action)
{
    switch (action) {
    case AdviceAction::Increase: return QString::fromUtf8("增配");
    case AdviceAction::Decrease: return QString::fromUtf8("减配");
    default: return QString::fromUtf8("持有");
    }
}

QString renderFactorList(const QString &title, const QStringList &items, const QString &color)
{
    QString h = "<div style='flex:1;'><div style='font-size:12px;font-weight:700;color:" + color
        + ";margin-bottom:4px;'>" + title + "</div><ul class='factor-list'>";
    if (items.isEmpty()) {
        h += "<li>" + QString::fromUtf8("暂无") + "</li>";
    } else {
        for (const QString &item : items) h += "<li>" + escaped(item) + "</li>";
    }
    h += "</ul></div>";
    return h;
}

QString metricCard(const QString &label, const QString &value, const QString &color, const ThemeColors &theme)
{
    return "<td style='width:25%;padding:10px;border:1px solid " + theme.cardBorder
        + ";border-radius:8px;background:" + theme.narrativeBg + ";text-align:center;'>"
        + "<div style='font-size:10px;color:" + theme.subtleColor + ";'>" + label + "</div>"
        + "<div style='font-size:18px;font-weight:800;color:" + color + ";margin-top:3px;'>"
        + value + "</div></td>";
}

QString renderConclusion(const SectorSnapshot &sector, const ThemeColors &theme)
{
    const TradingStrategy &strategy = sector.strategy;
    const QString actionLabel = strategy.actionLabel.isEmpty() ? actionText(sector.action) : strategy.actionLabel;
    const QString actionColor = sector.action == AdviceAction::Increase ? "#EF4444"
        : (sector.action == AdviceAction::Decrease ? "#3B82F6" : "#059669");

    QString h = "<div class='section-title'>投资结论</div>";
    h += "<div style='margin:8px 0 12px;padding:14px 18px;border-radius:10px;background:" + theme.narrativeBg
        + ";border:2px solid " + actionColor + ";'>";
    h += "<div style='display:flex;align-items:center;gap:12px;margin-bottom:10px;'>"
        "<span style='padding:7px 20px;border-radius:20px;background:" + actionColor
        + ";color:#fff;font-size:16px;font-weight:800;'>" + escaped(actionLabel) + "</span>"
        "<span style='font-size:12px;color:" + theme.bodyColor + ";'>"
        + QString::fromUtf8("综合评分 <b style='color:") + colorFor(sector.forecastScore, theme) + ";'>"
        + num(sector.forecastScore) + QString::fromUtf8("</b> · 置信度 <b>")
        + num(sector.confidence) + "</b> · " + escaped(sector.trendSummary) + "</span></div>";
    h += "<table style='width:100%;border-collapse:separate;border-spacing:6px 0;margin-bottom:8px;'><tr>";
    h += metricCard(QString::fromUtf8("今日涨幅"), sector.todayChangePctValid ? pct(sector.todayChangePct) : QString::fromUtf8("缺失"), colorFor(sector.todayChangePct, theme), theme);
    h += metricCard(QString::fromUtf8("止盈目标"), "+" + num(strategy.takeProfitPct, 1) + "%", "#059669", theme);
    h += metricCard(QString::fromUtf8("止损线"), num(strategy.stopLossPct, 1) + "%", "#DC2626", theme);
    h += metricCard(QString::fromUtf8("热度"), num(sector.sectorHotScore, 0), theme.headingColor, theme);
    h += "</tr></table>";
    if (!strategy.operationAdvice.isEmpty()) {
        h += "<div style='font-size:12px;line-height:1.7;color:" + theme.bodyColor + ";'>"
            + escaped(strategy.operationAdvice).replace("\n", "<br/>") + "</div>";
    }
    h += "</div>";
    return h;
}

QString coreScoreCard(const QString &label,
                      const QString &value,
                      const QString &note,
                      const QString &color)
{
    return "<div class='metric-card'>"
        "<div class='label'>" + escaped(label) + "</div>"
        "<div class='value' style='color:" + color + ";'>" + escaped(value) + "</div>"
        "<div class='meta' style='margin:4px 0 0 0;'>" + escaped(note) + "</div>"
        "</div>";
}

QString renderCoreScores(const SectorSnapshot &sector, const ThemeColors &theme)
{
    const double confidencePct = sector.confidence <= 1.0 ? sector.confidence * 100.0 : sector.confidence;
    QString h = "<div class='section-title'>核心评分</div>";
    h += "<div class='metric-grid'>";
    h += coreScoreCard(QString::fromUtf8("综合评分"), num(sector.forecastScore),
                       QString::fromUtf8("置信度 ") + num(confidencePct, 0) + "%", colorFor(sector.forecastScore, theme));
    h += coreScoreCard(QString::fromUtf8("事件催化"), num(sector.eventCatalystScore),
                       sector.eventSummary.isEmpty() ? QString::fromUtf8("暂无结构化事件") : sector.eventSummary,
                       colorFor(sector.eventCatalystScore, theme));
    h += coreScoreCard(QString::fromUtf8("技术强度"), num(sector.tech.techScore),
                       sector.trendSummary.isEmpty() ? QString::fromUtf8("等待趋势信号") : sector.trendSummary,
                       colorFor(sector.tech.techScore, theme));
    h += coreScoreCard(QString::fromUtf8("数据质量"), num(sector.dataQualityScore, 0),
                       QString::fromUtf8("一致性 ") + num(sector.sourceConsistencyScore, 0),
                       colorFor(sector.dataQualityScore - 70.0, theme));
    h += "</div>";
    return h;
}

QString renderAiReadableInsight(const SectorSnapshot &sector, const ThemeColors &theme)
{
    if (!sector.aiInsight.valid) return {};

    QString h = "<div class='section-title'>AI 协同解读 <span class='ai-badge'>AI</span></div>";
    h += "<div class='callout'>";
    if (!sector.aiInsight.readableTitle.isEmpty()) {
        h += "<div style='font-size:14px;font-weight:800;color:" + theme.headingColor + ";margin-bottom:6px;'>"
            + escaped(sector.aiInsight.readableTitle) + "</div>";
    }
    if (!sector.aiInsight.summary.isEmpty()) {
        h += "<div style='font-size:12px;line-height:1.7;color:" + theme.bodyColor + ";'>"
            + escaped(sector.aiInsight.summary) + "</div>";
    }
    h += "<table class='overview' style='margin-top:10px;'><tr><th>项目</th><th>内容</th></tr>";
    if (!sector.aiInsight.whyItMatters.isEmpty())
        h += "<tr><td>为什么重要</td><td>" + escaped(sector.aiInsight.whyItMatters) + "</td></tr>";
    if (!sector.aiInsight.impactPath.isEmpty())
        h += "<tr><td>影响路径</td><td>" + escaped(sector.aiInsight.impactPath) + "</td></tr>";
    if (!sector.aiInsight.primaryReason.isEmpty())
        h += "<tr><td>首要理由</td><td>" + escaped(sector.aiInsight.primaryReason) + "</td></tr>";
    if (!sector.aiInsight.primaryRisk.isEmpty())
        h += "<tr><td>首要风险</td><td>" + escaped(sector.aiInsight.primaryRisk) + "</td></tr>";
    if (!sector.aiInsight.nextCheckpoint.isEmpty())
        h += "<tr><td>下一观察</td><td>" + escaped(sector.aiInsight.nextCheckpoint) + "</td></tr>";
    if (!sector.aiInsight.disagreementNotes.isEmpty())
        h += "<tr><td>规则分歧</td><td style='color:" + theme.warningColor + ";'>"
            + escaped(sector.aiInsight.disagreementNotes) + "</td></tr>";
    h += "</table>";
    h += "<div class='meta' style='margin-top:8px;'>"
        + QString::fromUtf8("AI 内容只用于解释、归纳和交叉验证，不直接改写规则评分或最终动作。")
        + "</div></div>";
    return h;
}

QString renderSignalExplanation(const SectorSnapshot &sector, const ThemeColors &theme)
{
    QString h = "<div class='section-title'>信号解释</div>";
    h += "<div class='split-grid'>";
    h += "<div class='callout'>";
    h += renderFactorList(QString::fromUtf8("正向因素"), sector.positiveFactors, "#059669");
    h += renderFactorList(QString::fromUtf8("风险因素"), sector.negativeFactors, "#DC2626");
    h += "</div>";
    h += "<div class='callout'>";
    h += "<div style='font-size:12px;font-weight:800;color:" + theme.headingColor + ";margin-bottom:8px;'>"
         + QString::fromUtf8("页面提示与失效条件") + "</div>";
    QStringList hints;
    if (!sector.strategy.operationAdvice.isEmpty()) hints << sector.strategy.operationAdvice;
    if (!sector.eventSummary.isEmpty()) hints << QString::fromUtf8("事件观察：") + sector.eventSummary;
    if (sector.dataQualityScore < 70.0) hints << QString::fromUtf8("数据质量偏低，需等待多源验证后再提高权重。");
    if (sector.todayChangePctValid && sector.todayChangePct > 3.0) hints << QString::fromUtf8("单日涨幅偏大，避免只按情绪追高。");
    if (hints.isEmpty()) hints << QString::fromUtf8("暂无额外失效条件，继续观察新闻与资金是否共振。");
    h += "<ul class='factor-list'>";
    for (const QString &hint : hints) h += "<li>" + escaped(hint) + "</li>";
    h += "</ul></div></div>";
    return h;
}

QString impactDirectionText(EventImpactDirection direction);
QString impactRelationText(EventImpactRelation relation);

QString renderImpactPath(const SectorSnapshot &sector, const ThemeColors &theme)
{
    QString h = "<div class='section-title'>影响路径</div>";
    h += "<div class='callout'>";
    if (sector.aiInsight.valid && !sector.aiInsight.impactPath.isEmpty()) {
        h += "<div style='padding:10px 12px;margin-bottom:8px;border:1px solid " + theme.cardBorder
            + ";border-radius:8px;background:" + theme.bodyBg + ";'>"
            + "<div style='font-weight:800;color:" + theme.headingColor + ";font-size:12px;'>"
            + QString::fromUtf8("AI 归纳路径") + "</div>"
            + "<div style='margin-top:4px;color:" + theme.bodyColor + ";font-size:12px;'>"
            + escaped(sector.aiInsight.impactPath) + "</div></div>";
    }
    if (sector.eventImpacts.isEmpty()) {
        h += "<div style='color:" + theme.mutedColor + ";font-size:12px;'>"
            + (sector.eventSummary.isEmpty()
                ? QString::fromUtf8("暂无结构化影响路径，等待事件引擎补充。")
                : escaped(sector.eventSummary))
            + "</div>";
    } else {
        for (const SectorEventImpact &impact : sector.eventImpacts) {
            h += "<div style='padding:10px 12px;margin-bottom:8px;border:1px solid " + theme.cardBorder
                + ";border-radius:8px;background:" + theme.bodyBg + ";'>";
            h += "<div style='font-weight:800;color:" + theme.headingColor + ";font-size:12px;'>"
                + escaped(impact.eventTitle) + "</div>";
            h += "<div style='margin-top:4px;color:" + theme.bodyColor + ";font-size:12px;'>"
                + escaped(impact.path) + "</div>";
            h += "<div class='meta' style='margin:4px 0 0 0;'>"
                + escaped(impactRelationText(impact.relation)) + QString::fromUtf8("影响 · ")
                + escaped(impactDirectionText(impact.direction)) + QString::fromUtf8(" · 置信度 ")
                + num(impact.confidence * 100.0, 0) + "%"
                + QString::fromUtf8(" · 周期 ") + escaped(toString(impact.horizon))
                + QString::fromUtf8(" · 来源可信度 ") + num(impact.sourceReliability, 2)
                + "</div>";
            if (!impact.condition.isEmpty()) {
                h += "<div class='meta' style='margin:4px 0 0 0;color:" + theme.warningColor + ";'>"
                    + QString::fromUtf8("失效条件：") + escaped(impact.condition) + "</div>";
            }
            h += "</div>";
        }
    }
    h += "<div style='margin-top:8px;font-size:11px;color:" + theme.warningColor + ";font-weight:700;'>"
        + QString::fromUtf8("失效条件：若事件无法传导到订单、价格、资金或业绩验证，需下调评分。")
        + "</div></div>";
    return h;
}

QString renderStageReturnsAndBacktests(const SectorSnapshot &sector, const ThemeColors &theme)
{
    QString h = "<div class='section-title'>阶段收益与回测</div>";
    h += "<div class='metric-grid'>";
    h += coreScoreCard(QString::fromUtf8("近5日"), pct(sector.fiveDayMomentum),
                       QString::fromUtf8("短线动量"), colorFor(sector.fiveDayMomentum, theme));
    h += coreScoreCard(QString::fromUtf8("近20日"), pct(sector.twentyDayMomentum),
                       QString::fromUtf8("中期趋势"), colorFor(sector.twentyDayMomentum, theme));
    h += coreScoreCard(QString::fromUtf8("近3月"), pct(sector.monthMomentum),
                       QString::fromUtf8("长期弹性"), colorFor(sector.monthMomentum, theme));
    h += coreScoreCard(QString::fromUtf8("累计收益"), pct(sector.cumulativeReturn),
                       sector.bestStrategyName.isEmpty() ? QString::fromUtf8("当前跟踪") : sector.bestStrategyName,
                       colorFor(sector.cumulativeReturn, theme));
    h += "</div>";
    if (!sector.backtestResults.isEmpty()) {
        h += "<table class='overview'><tr><th>策略</th><th>交易数</th><th>胜率</th><th>平均收益</th><th>最大回撤</th></tr>";
        for (const StrategyBacktest &bt : sector.backtestResults) {
            h += "<tr><td>" + escaped(bt.name) + "</td><td>" + QString::number(bt.totalTrades)
                + "</td><td>" + num(bt.winRate, 0) + "%</td><td style='color:" + colorFor(bt.avgReturn, theme)
                + ";font-weight:700;'>" + pct(bt.avgReturn) + "</td><td>" + pct(bt.maxDrawdown) + "</td></tr>";
        }
        h += "</table>";
    }
    return h;
}

QString renderFundFlowRelations(const SectorSnapshot &sector, const ThemeColors &theme)
{
    QString h = "<div class='section-title'>资金流与相关板块</div>";
    h += "<div class='split-grid'>";
    h += "<div class='callout'>";
    h += "<div class='kv'><span class='label'>资金流因子</span><span class='value' style='color:"
        + colorFor(sector.fundFlowFactor, theme) + ";'>" + num(sector.fundFlowFactor, 3) + "</span></div>";
    h += "<div class='kv'><span class='label'>样本数量</span><span class='value'>"
        + QString::number(sector.fundFlowSeries.size()) + QString::fromUtf8(" 条</span></div>");
    h += "<div class='kv'><span class='label'>数据来源</span><span class='value'>"
        + escaped(sector.fundFlowSource) + "</span></div>";
    h += "</div><div class='callout'>";
    h += "<div style='font-size:12px;font-weight:800;color:" + theme.headingColor + ";margin-bottom:8px;'>"
        + QString::fromUtf8("联动线索") + "</div>";
    QStringList relations;
    for (const SectorEventImpact &impact : sector.eventImpacts) {
        if (!impact.path.isEmpty()) relations << impact.path;
    }
    if (relations.isEmpty()) relations = sector.positiveFactors + sector.negativeFactors;
    if (relations.isEmpty()) relations << QString::fromUtf8("暂无明确联动板块，等待事件路径或资金共振。");
    h += "<ul class='factor-list'>";
    const int count = qMin(4, relations.size());
    for (int i = 0; i < count; ++i) h += "<li>" + escaped(relations[i]) + "</li>";
    h += "</ul></div></div>";
    return h;
}

QString renderViews(const SectorSnapshot &sector, const ThemeColors &theme)
{
    const TradingStrategy &strategy = sector.strategy;
    QString h = "<div class='section-title'>短中长期观点</div>";
    h += "<table class='overview'><tr><th>周期</th><th>观点</th></tr>";
    h += "<tr><td>短期</td><td>" + escaped(strategy.shortTermView) + "</td></tr>";
    h += "<tr><td>中期</td><td>" + escaped(strategy.mediumTermView) + "</td></tr>";
    h += "<tr><td>长期</td><td>" + escaped(strategy.longTermView) + "</td></tr>";
    h += "</table>";
    h += "<div style='display:flex;gap:16px;margin-top:10px;'>";
    h += renderFactorList(QString::fromUtf8("&#9650; 看多因素"), sector.positiveFactors, "#059669");
    h += renderFactorList(QString::fromUtf8("&#9660; 看空因素"), sector.negativeFactors, "#DC2626");
    h += "</div>";
    Q_UNUSED(theme);
    return h;
}

QString impactDirectionText(EventImpactDirection direction)
{
    switch (direction) {
    case EventImpactDirection::Positive:
        return QString::fromUtf8("正面");
    case EventImpactDirection::Negative:
        return QString::fromUtf8("负面");
    case EventImpactDirection::Mixed:
        return QString::fromUtf8("多空交织");
    case EventImpactDirection::Neutral:
    default:
        return QString::fromUtf8("中性");
    }
}

QString impactRelationText(EventImpactRelation relation)
{
    switch (relation) {
    case EventImpactRelation::Direct:
        return QString::fromUtf8("直接");
    case EventImpactRelation::Conditional:
        return QString::fromUtf8("条件");
    case EventImpactRelation::Indirect:
    default:
        return QString::fromUtf8("间接");
    }
}

QString renderEventImpacts(const SectorSnapshot &sector, const ThemeColors &theme)
{
    if (sector.eventImpacts.isEmpty() && sector.eventSummary.isEmpty()) return QString();

    QString h = "<div class='section-title'>事件驱动</div>";
    h += "<div class='narrative'>";
    h += QString::fromUtf8("事件催化分：<b style='color:") + colorFor(sector.eventCatalystScore, theme)
        + ";'>" + num(sector.eventCatalystScore) + "</b>";
    if (!sector.eventSummary.isEmpty()) {
        h += QString::fromUtf8(" · ") + escaped(sector.eventSummary);
    }
    h += "</div>";

    h += "<table class='overview'><tr><th>事件</th><th>方向</th><th>关系</th><th>路径与解释</th><th>周期/证据</th></tr>";
    if (sector.eventImpacts.isEmpty()) {
        h += "<tr><td colspan='5'>暂无结构化影响路径，等待事件引擎补充。</td></tr>";
    } else {
        for (const SectorEventImpact &impact : sector.eventImpacts) {
            h += "<tr><td>" + escaped(impact.eventTitle) + "</td>"
                + "<td>" + escaped(impactDirectionText(impact.direction)) + "</td>"
                + "<td>" + escaped(impactRelationText(impact.relation)) + "</td>"
                + "<td>" + escaped(impact.path) + "<br/><span class='meta'>"
                + escaped(impact.explanation) + "</span></td>"
                + "<td>" + escaped(toString(impact.horizon)) + "<br/><span class='meta'>"
                + QString::fromUtf8("来源可信度 ") + num(impact.sourceReliability, 2)
                + "</span><br/><span class='meta'>"
                + QString::fromUtf8("失效条件：")
                + escaped(impact.condition.isEmpty() ? QString::fromUtf8("等待后续验证") : impact.condition)
                + "</span></td></tr>";
        }
    }
    h += "</table>";
    return h;
}

QString renderTechnical(const SectorSnapshot &sector, const ThemeColors &theme)
{
    const TechSignals &tech = sector.tech;
    QString h = "<div class='section-title'>技术指标</div>";
    h += "<table class='overview'><tr><th>指标</th><th>数值</th><th>信号</th></tr>";
    h += "<tr><td>MACD</td><td>DIF " + num(tech.macdDIF, 3) + " / DEA " + num(tech.macdDEA, 3)
        + "</td><td>" + (tech.macdGoldenCross ? QString::fromUtf8("金叉") : (tech.macdDeadCross ? QString::fromUtf8("死叉") : QString::fromUtf8("观察"))) + "</td></tr>";
    h += "<tr><td>RSI</td><td>RSI6 " + num(tech.rsi6, 1) + " / RSI12 " + num(tech.rsi12, 1)
        + "</td><td>" + (tech.rsiOverbought ? QString::fromUtf8("超买") : (tech.rsiOversold ? QString::fromUtf8("超卖") : QString::fromUtf8("中性"))) + "</td></tr>";
    h += "<tr><td>KDJ</td><td>K " + num(tech.kdjK, 1) + " / D " + num(tech.kdjD, 1)
        + "</td><td>" + (tech.kdjGoldenCross ? QString::fromUtf8("金叉") : QString::fromUtf8("观察")) + "</td></tr>";
    h += "<tr><td>均线</td><td>MA5 " + num(tech.ma5, 2) + " / MA20 " + num(tech.ma20, 2)
        + "</td><td>" + (tech.maLongArrange ? QString::fromUtf8("多头排列") : (tech.maShortArrange ? QString::fromUtf8("空头排列") : QString::fromUtf8("震荡"))) + "</td></tr>";
    h += "<tr><td>技术综合</td><td colspan='2' style='color:" + colorFor(tech.techScore, theme)
        + ";font-weight:700;'>" + num(tech.techScore) + "</td></tr>";
    h += "</table>";
    return h;
}

QString renderDataQuality(const SectorSnapshot &sector, const ThemeColors &theme)
{
    QString h = "<div class='section-title'>数据质量</div>";
    h += "<table class='overview'><tr><th>项目</th><th>结果</th></tr>";
    h += "<tr><td>质量分</td><td><b>" + num(sector.dataQualityScore, 0) + "</b> / 100</td></tr>";
    h += "<tr><td>一致性</td><td><b>" + num(sector.sourceConsistencyScore, 0) + "</b> / 100</td></tr>";
    h += "<tr><td>来源</td><td>K 线：" + escaped(sector.klineSource)
        + "；资金流：" + escaped(sector.fundFlowSource) + "</td></tr>";
    h += "<tr><td>说明</td><td>" + escaped(sector.dataQualityNote) + "</td></tr>";
    if (!sector.missingDataItems.isEmpty()) {
        h += "<tr><td>缺失项</td><td style='color:" + theme.warningColor + ";'>"
            + escaped(sector.missingDataItems.join(QString::fromUtf8("、"))) + "</td></tr>";
    }
    h += "</table>";
    return h;
}

QString renderFundFlow(const SectorSnapshot &sector)
{
    QString h = "<div class='section-title'>资金流</div>";
    h += "<table class='overview'><tr><th>项目</th><th>数值</th></tr>";
    h += "<tr><td>资金流因子</td><td>" + num(sector.fundFlowFactor, 3) + "</td></tr>";
    h += "<tr><td>资金流样本</td><td>" + QString::number(sector.fundFlowSeries.size()) + QString::fromUtf8(" 条</td></tr>");
    h += "<tr><td>来源</td><td>" + escaped(sector.fundFlowSource) + "</td></tr>";
    h += "</table>";
    return h;
}

QString renderBacktests(const SectorSnapshot &sector)
{
    QString h = "<div class='section-title'>策略回测</div>";
    h += "<table class='overview'><tr><th>策略</th><th>交易数</th><th>胜率</th><th>平均收益</th></tr>";
    if (sector.backtestResults.isEmpty()) {
        h += "<tr><td colspan='4'>暂无回测结果</td></tr>";
    } else {
        for (const StrategyBacktest &bt : sector.backtestResults) {
            h += "<tr><td>" + escaped(bt.name) + "</td><td>" + QString::number(bt.totalTrades)
                + "</td><td>" + num(bt.winRate, 0) + "%</td><td>" + num(bt.avgReturn, 2) + "%</td></tr>";
        }
    }
    h += "</table>";
    return h;
}

QString renderNews(const SectorSnapshot &sector, const ThemeColors &theme)
{
    QString h = "<div class='section-title'>新闻证据</div>";
    if (sector.newsEntries.isEmpty() && sector.newsHeadlines.isEmpty()) {
        return h + "<div class='narrative'>暂无新闻证据。</div>";
    }
    for (const NewsEntry &news : sector.newsEntries) {
        h += "<div style='padding:8px 10px;border-bottom:1px solid " + theme.newsItemBorder
            + ";font-size:12px;'><b>" + linkHtml(news.title, news.url) + "</b>"
            + "<div class='meta'>" + escaped(news.date) + " · " + escaped(news.source) + "</div></div>";
    }
    for (const QString &headline : sector.newsHeadlines) {
        h += "<div style='padding:8px 10px;border-bottom:1px solid " + theme.newsItemBorder
            + ";font-size:12px;'>" + escaped(headline) + "</div>";
    }
    return h;
}

} // namespace

QString SectorDetailRenderer::render(const SectorSnapshot &sector,
                                     const ThemeColors &theme,
                                     const SectorDetailRenderOptions &options)
{
    const QPixmap chart = ChartRenderer::buildTrendChart(
        sector, theme, options.chartWidth, options.chartHeight);

    QString h = "<html><head><style>" + buildHtmlCss(theme) + "</style></head><body>";
    h += "<h2>" + escaped(sector.industry) + " <span style='color:"
        + colorFor(sector.todayChangePct, theme) + ";font-size:16px;'>"
        + (sector.todayChangePctValid ? pct(sector.todayChangePct) : QString::fromUtf8("涨跌缺失"))
        + "</span></h2>";
    h += "<div class='meta'>" + QString::fromUtf8("成分股 ")
        + num(sector.sectorStockCount > 0 ? sector.sectorStockCount : sector.stockCount, 0)
        + QString::fromUtf8(" 只 · 数据日期 ") + escaped(sector.lastDataDate) + "</div>";
    h += renderConclusion(sector, theme);
    h += renderCoreScores(sector, theme);
    h += renderAiReadableInsight(sector, theme);
    h += renderSignalExplanation(sector, theme);
    h += renderViews(sector, theme);
    h += renderEventImpacts(sector, theme);
    h += renderImpactPath(sector, theme);
    if (!options.simpleMode) {
        h += "<div class='section-title'>趋势图表</div>";
        h += "<img style='max-width:100%;border:1px solid " + theme.cardBorder
            + ";border-radius:8px;' src='data:image/png;base64," + pixmapToBase64(chart) + "'/>";
        h += renderTechnical(sector, theme);
        h += renderStageReturnsAndBacktests(sector, theme);
        h += renderFundFlowRelations(sector, theme);
        h += renderFundFlow(sector);
        h += renderBacktests(sector);
        h += renderDataQuality(sector, theme);
    }
    if (options.aiAvailable && !sector.aiAnalysis.isEmpty()) {
        h += "<div class='section-title'>深度行业分析 <span class='ai-badge'>AI</span></div>"
            "<div class='narrative'>" + escaped(sector.aiAnalysis).replace("\n", "<br/>") + "</div>";
    } else if (!sector.analysisNarrative.isEmpty()) {
        h += "<div class='section-title'>综合分析</div><div class='narrative'>"
            + escaped(sector.analysisNarrative).replace("\n", "<br/>") + "</div>";
    }
    h += renderNews(sector, theme);
    h += "</body></html>";
    return h;
}

} // namespace InvestInsight::Ui
