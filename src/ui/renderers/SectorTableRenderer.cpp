#include "ui/renderers/SectorTableRenderer.h"

#include "domain/AnalysisResult.h"

#include <algorithm>
#include <cmath>

namespace InvestInsight::Ui {
namespace {

struct MixedRow {
    bool isIndex = false;
    int sectorJumpIndex = 0;
    QString indexKey;
    const SectorSnapshot *sector = nullptr;
    IndexSnapshot index;
    QString trend;
    AdviceAction action = AdviceAction::Hold;
    double todayChange = 0.0;
    double forecast = 0.0;
    double confidence = 0.0;
    double dataQuality = 0.0;
    double consistency = 0.0;
    double fiveDay = 0.0;
    double eventCatalyst = 0.0;
    QString riskHint;
    QString macdState;
    QString rsiState;
    QString kdjState;
    QString maState;
    QString volumeState;
    QString fundFlowState;
    QString keyPoint;
};

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

QString scoreBadge(double value, const ThemeColors &theme)
{
    return "<span style='display:inline-block;min-width:48px;text-align:center;font-weight:700;color:"
        + changeColor(value, theme) + ";'>" + num(value, 2) + "</span>";
}

QString firstNonEmpty(const QStringList &items, const QString &fallback)
{
    for (const QString &item : items) {
        if (!item.trimmed().isEmpty()) return item.trimmed();
    }
    return fallback;
}

QString macdState(const TechSignals &tech)
{
    if (tech.macdGoldenCross) return QString::fromUtf8("金叉");
    if (tech.macdDeadCross) return QString::fromUtf8("死叉");
    return tech.macdHist >= 0.0 ? QString::fromUtf8("红柱") : QString::fromUtf8("绿柱");
}

QString rsiState(const TechSignals &tech)
{
    if (tech.rsiOverbought) return QString::fromUtf8("超买 ") + num(tech.rsi6, 0);
    if (tech.rsiOversold) return QString::fromUtf8("超卖 ") + num(tech.rsi6, 0);
    return num(tech.rsi6, 0);
}

QString kdjState(const TechSignals &tech)
{
    if (tech.kdjGoldenCross) return QString::fromUtf8("金叉 ") + num(tech.kdjJ, 0);
    if (tech.kdjOverbought) return QString::fromUtf8("高位 ") + num(tech.kdjJ, 0);
    if (tech.kdjOversold) return QString::fromUtf8("低位 ") + num(tech.kdjJ, 0);
    return num(tech.kdjJ, 0);
}

QString maState(const TechSignals &tech)
{
    if (tech.maLongArrange) return QString::fromUtf8("多头");
    if (tech.maShortArrange) return QString::fromUtf8("空头");
    return QString::fromUtf8("交织");
}

QString inferTrendByChange(double change)
{
    if (change >= 2.0) return QString::fromUtf8("强势看多");
    if (change >= 0.5) return QString::fromUtf8("偏多");
    if (change <= -2.0) return QString::fromUtf8("强势看空");
    if (change <= -0.5) return QString::fromUtf8("偏空");
    if (std::abs(change) <= 0.2) return QString::fromUtf8("横盘震荡");
    return QString::fromUtf8("方向不明");
}

AdviceAction inferActionByChange(double change)
{
    if (change >= 1.2) return AdviceAction::Increase;
    if (change <= -1.2) return AdviceAction::Decrease;
    return AdviceAction::Hold;
}

QStringList trendNames()
{
    return {"", QString::fromUtf8("强势看多"), QString::fromUtf8("偏多"),
            QString::fromUtf8("横盘震荡"), QString::fromUtf8("方向不明"),
            QString::fromUtf8("偏空"), QString::fromUtf8("强势看空")};
}

QStringList actionNames()
{
    return {"", QString::fromUtf8("增配"), QString::fromUtf8("持有"), QString::fromUtf8("减配")};
}

QList<MixedRow> buildRows(const AnalysisResult &analysis,
                          const SectorTableRenderOptions &options)
{
    QList<MixedRow> rows;
    const QStringList trends = trendNames();
    const QStringList actions = actionNames();
    const QString filterTrend = options.trendIndex > 0 && options.trendIndex < trends.size()
        ? trends[options.trendIndex] : QString();
    const QString filterAction = options.actionIndex > 0 && options.actionIndex < actions.size()
        ? actions[options.actionIndex] : QString();

    const auto passFilter = [&](const QString &name, const QString &trend, AdviceAction action) {
        if (!options.searchText.trimmed().isEmpty()
            && !name.contains(options.searchText.trimmed(), Qt::CaseInsensitive)) return false;
        if (!filterTrend.isEmpty() && trend != filterTrend) return false;
        if (!filterAction.isEmpty() && actionText(action) != filterAction) return false;
        return true;
    };

    for (int i = 0; i < analysis.sectors.size(); ++i) {
        const SectorSnapshot &sector = analysis.sectors[i];
        const QString trend = sector.trendSummary.isEmpty()
            ? inferTrendByChange(sector.todayChangePct) : sector.trendSummary;
        if (!passFilter(sector.industry, trend, sector.action)) continue;

        MixedRow row;
        row.sectorJumpIndex = i + 1;
        row.sector = &sector;
        row.trend = trend;
        row.action = sector.action;
        row.todayChange = sector.todayChangePct;
        row.forecast = sector.forecastScore;
        row.confidence = sector.confidence;
        row.dataQuality = sector.dataQualityScore;
        row.consistency = sector.sourceConsistencyScore;
        row.fiveDay = sector.fiveDayMomentum;
        row.eventCatalyst = sector.eventCatalystScore;
        row.riskHint = firstNonEmpty(sector.negativeFactors,
            sector.dataQualityScore < 70.0
                ? QString::fromUtf8("数据质量偏低，需谨慎参考")
                : QString::fromUtf8("暂无突出风险"));
        row.macdState = macdState(sector.tech);
        row.rsiState = rsiState(sector.tech);
        row.kdjState = kdjState(sector.tech);
        row.maState = maState(sector.tech);
        row.volumeState = sector.tech.volExpansion ? QString::fromUtf8("放量")
            : (sector.tech.volShrink ? QString::fromUtf8("缩量") : num(sector.tech.volRatio, 2));
        row.fundFlowState = num(sector.fundFlowFactor, 3);
        row.keyPoint = firstNonEmpty({sector.aiInsight.primaryReason,
                                      sector.trendSummary,
                                      sector.personalAdvice},
                                     QString::fromUtf8("等待更多信号确认"));
        rows.push_back(row);
    }

    const auto appendIndex = [&](const IndexSnapshot &index, const QString &key) {
        if (index.name.isEmpty()) return;
        const QString trend = inferTrendByChange(index.changePct);
        const AdviceAction action = inferActionByChange(index.changePct);
        if (!passFilter(index.name, trend, action)) return;

        double fiveDay = 0.0;
        if (index.klineSeries.size() >= 6) {
            const double base = index.klineSeries[index.klineSeries.size() - 6];
            if (base > 0.0) fiveDay = (index.klineSeries.last() - base) / base * 100.0;
        }

        MixedRow row;
        row.isIndex = true;
        row.indexKey = key;
        row.index = index;
        row.trend = trend;
        row.action = action;
        row.todayChange = index.changePct;
        row.forecast = qBound(-1.0, index.changePct / 3.0, 1.0);
        row.confidence = index.klineSeries.size() >= 60 ? 92.0 : 82.0;
        row.dataQuality = index.klineSeries.size() >= 20 ? 90.0 : 72.0;
        row.consistency = index.klineSeries.isEmpty() ? 80.0 : 95.0;
        row.fiveDay = fiveDay;
        row.eventCatalyst = 0.0;
        row.riskHint = QString::fromUtf8("跟踪指数方向，控制仓位波动");
        row.macdState = QStringLiteral("-");
        row.rsiState = QStringLiteral("-");
        row.kdjState = QStringLiteral("-");
        row.maState = QStringLiteral("-");
        row.volumeState = QStringLiteral("-");
        row.fundFlowState = QStringLiteral("-");
        row.keyPoint = row.trend;
        rows.push_back(row);
    };

    appendIndex(analysis.marketCtx.shanghai, "SH");
    appendIndex(analysis.marketCtx.shenzhen, "SZ");
    appendIndex(analysis.marketCtx.chinext, "CY");
    appendIndex(analysis.marketCtx.csi300, "CSI300");
    appendIndex(analysis.marketCtx.csi500, "CSI500");
    appendIndex(analysis.marketCtx.nasdaq, "NASDAQ");
    appendIndex(analysis.marketCtx.sp500, "SP500");
    appendIndex(analysis.marketCtx.dowjones, "DJI");

    switch (options.sortIndex) {
    case 1:
        std::sort(rows.begin(), rows.end(), [](const MixedRow &a, const MixedRow &b) { return a.forecast > b.forecast; });
        break;
    case 2:
        std::sort(rows.begin(), rows.end(), [](const MixedRow &a, const MixedRow &b) { return a.forecast < b.forecast; });
        break;
    case 3:
        std::sort(rows.begin(), rows.end(), [](const MixedRow &a, const MixedRow &b) { return a.todayChange > b.todayChange; });
        break;
    case 4:
        std::sort(rows.begin(), rows.end(), [](const MixedRow &a, const MixedRow &b) { return a.todayChange < b.todayChange; });
        break;
    case 5:
        std::sort(rows.begin(), rows.end(), [](const MixedRow &a, const MixedRow &b) { return a.confidence > b.confidence; });
        break;
    case 6:
        std::sort(rows.begin(), rows.end(), [](const MixedRow &a, const MixedRow &b) { return a.dataQuality > b.dataQuality; });
        break;
    case 7:
        std::sort(rows.begin(), rows.end(), [](const MixedRow &a, const MixedRow &b) { return a.consistency > b.consistency; });
        break;
    default:
        break;
    }
    return rows;
}

QString rowNameHtml(const MixedRow &row, const ThemeColors &theme)
{
    if (row.isIndex) {
        return "<a href='jumpi-" + escaped(row.indexKey) + "'>" + escaped(row.index.name)
            + "</a> <span style='font-size:10px;color:#7C3AED;'>[指数]</span>";
    }

    const SectorSnapshot &sector = *row.sector;
    QString name = "<a href='jump-" + QString::number(row.sectorJumpIndex) + "'>"
        + escaped(sector.industry) + "</a>";
    if (!sector.sectorTierLabel.isEmpty()) {
        name += " <span style='font-size:9px;color:" + theme.mutedColor + ";'>["
            + escaped(sector.sectorTierLabel) + "]</span>";
    }
    if (!sector.missingDataItems.isEmpty()) {
        name += " <span style='font-size:10px;color:#D97706;' title='数据存在缺口'>&#9888;</span>";
    }
    return name;
}

QString rowSummary(const MixedRow &row)
{
    if (row.isIndex) {
        return row.trend + QString::fromUtf8("，以指数方向管理相关仓位与风险敞口");
    }

    const SectorSnapshot &sector = *row.sector;
    QString summary = sector.trendSummary;
    if (!sector.strategy.actionLabel.isEmpty()) summary += QString::fromUtf8("，") + sector.strategy.actionLabel;
    if (sector.tech.macdGoldenCross) summary += QString::fromUtf8("，MACD 金叉");
    if (sector.tech.macdDeadCross) summary += QString::fromUtf8("，MACD 死叉");
    if (sector.newsHitCount > 3) summary += QString::fromUtf8("（") + num(sector.newsHitCount, 0) + QString::fromUtf8(" 条新闻）");
    return summary;
}

QString renderAudit(const AnalysisResult &analysis, const ThemeColors &theme)
{
    if (analysis.sectors.isEmpty()) return {};

    int klineOk = 0;
    int flowOk = 0;
    int valuationOk = 0;
    double avgQuality = 0.0;
    double avgConsistency = 0.0;
    for (const SectorSnapshot &sector : analysis.sectors) {
        avgQuality += sector.dataQualityScore;
        avgConsistency += sector.sourceConsistencyScore;
        if (sector.trendSeries.size() >= 20 || !sector.dailyBars.isEmpty()) ++klineOk;
        if (!sector.fundFlowSeries.isEmpty()) ++flowOk;
        if (sector.peRatio > 0.0 || sector.pbRatio > 0.0) ++valuationOk;
    }
    avgQuality /= analysis.sectors.size();
    avgConsistency /= analysis.sectors.size();

    return "<div style='margin:4px 0 8px 0;padding:10px 12px;border-radius:8px;border:1px solid "
        + theme.cardBorder + ";background:" + theme.narrativeBg + ";font-size:11px;line-height:1.7;'>"
        + QString::fromUtf8("数据审计：<b>全局质量 ")
        + num(avgQuality, 0) + "</b> &nbsp;|&nbsp; "
        + QString::fromUtf8("源一致性 ") + num(avgConsistency, 0)
        + QString::fromUtf8(" &nbsp;|&nbsp; K 线覆盖 ") + num(klineOk, 0) + "/" + num(analysis.sectors.size(), 0)
        + QString::fromUtf8(" &nbsp;|&nbsp; 资金流覆盖 ") + num(flowOk, 0) + "/" + num(analysis.sectors.size(), 0)
        + QString::fromUtf8(" &nbsp;|&nbsp; 估值覆盖 ") + num(valuationOk, 0) + "/" + num(analysis.sectors.size(), 0)
        + "</div>";
}

QString renderRows(const QList<MixedRow> &rows,
                   const SectorTableRenderOptions &options,
                   const ThemeColors &theme)
{
    QString h = "<table class='overview'>";
    if (options.simpleMode) {
        h += "<tr><th>#</th><th>板块&指数</th><th style='text-align:right;'>今日</th>"
             "<th style='text-align:center;'>评分</th><th style='text-align:center;'>建议</th><th>一句话看点</th></tr>";
    } else {
        h += "<tr><th>#</th><th>板块&指数</th><th style='text-align:right;'>今日</th>"
             "<th style='text-align:right;'>5日</th><th style='text-align:center;'>评分</th>"
             "<th style='text-align:center;'>事件</th><th>MACD</th><th>RSI</th><th>KDJ</th>"
             "<th>均线</th><th>量能</th><th>资金</th><th style='text-align:center;'>数据</th>"
             "<th>建议</th><th>主要看点</th></tr>";
    }

    int displayIndex = 0;
    for (const MixedRow &row : rows) {
        ++displayIndex;
        const QString rowBg = displayIndex % 2 == 0
            ? " style='background:" + theme.tableAltRowBg + ";'" : "";
        h += "<tr" + rowBg + ">";
        h += "<td style='color:" + theme.subtleColor + ";text-align:center;'>" + QString::number(displayIndex) + "</td>";
        h += "<td>" + rowNameHtml(row, theme) + "</td>";
        h += "<td style='text-align:right;color:" + changeColor(row.todayChange, theme) + ";font-weight:700;'>"
            + pct(row.todayChange) + "</td>";
        if (!options.simpleMode) {
            h += "<td style='text-align:right;color:" + changeColor(row.fiveDay, theme) + ";font-weight:700;'>"
                + pct(row.fiveDay) + "</td>";
        }
        h += "<td style='text-align:center;'>" + scoreBadge(row.forecast, theme) + "</td>";
        if (!options.simpleMode) {
            h += "<td style='text-align:center;color:" + changeColor(row.eventCatalyst, theme)
                + ";font-weight:700;'>" + num(row.eventCatalyst, 2) + "</td>";
            h += "<td style='font-size:11px;'>" + escaped(row.macdState) + "</td>";
            h += "<td style='font-size:11px;'>" + escaped(row.rsiState) + "</td>";
            h += "<td style='font-size:11px;'>" + escaped(row.kdjState) + "</td>";
            h += "<td style='font-size:11px;'>" + escaped(row.maState) + "</td>";
            h += "<td style='font-size:11px;'>" + escaped(row.volumeState) + "</td>";
            h += "<td style='font-size:11px;color:" + changeColor(row.fundFlowState.toDouble(), theme)
                + ";font-weight:700;'>" + escaped(row.fundFlowState) + "</td>";
            h += "<td style='text-align:center;font-weight:700;'>" + num(row.dataQuality, 0) + "</td>";
        }
        h += "<td style='text-align:center;'><span class='tag " + tagClass(row.action) + "'>"
            + actionText(row.action) + "</span></td>";
        if (!options.simpleMode) {
            h += "<td style='font-size:11px;color:" + theme.mutedColor + ";'>"
                + escaped(row.keyPoint) + "</td>";
        } else {
            h += "<td style='font-size:11px;'>" + escaped(rowSummary(row)) + "</td>";
        }
        h += "</tr>";
    }
    if (rows.isEmpty()) {
        h += "<tr><td colspan='15' style='text-align:center;color:" + theme.mutedColor
            + ";padding:18px;'>暂无符合筛选条件的板块或指数</td></tr>";
    }
    h += "</table>";
    return h;
}

int marketIndexCount(const AnalysisResult &analysis)
{
    int count = 0;
    const auto add = [&count](const IndexSnapshot &index) {
        if (!index.name.isEmpty()) ++count;
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

} // namespace

QString SectorTableRenderer::render(const AnalysisResult &analysis,
                                    const ThemeColors &theme,
                                    const SectorTableRenderOptions &options)
{
    const QList<MixedRow> rows = buildRows(analysis, options);
    const bool filtered = !options.searchText.trimmed().isEmpty()
        || options.trendIndex > 0 || options.actionIndex > 0 || options.sortIndex > 0;
    const int universe = analysis.sectors.size() + marketIndexCount(analysis);

    QString h = "<html><head><style>" + buildHtmlCss(theme) + "</style></head><body>";
    h += "<h1 style='font-size:18px;'>板块机会</h1>";
    h += "<div class='meta'>" + num(analysis.sectors.size(), 0)
        + QString::fromUtf8(" 个板块 · ") + num(marketIndexCount(analysis), 0)
        + QString::fromUtf8(" 个指数");
    if (filtered) h += QString::fromUtf8(" · 当前展示 ") + num(rows.size(), 0) + "/" + num(universe, 0);
    h += "</div>";

    if (!options.simpleMode) h += renderAudit(analysis, theme);
    h += "<div class='section-title'>板块&指数总览</div>";
    h += renderRows(rows, options, theme);
    h += "</body></html>";
    return h;
}

} // namespace InvestInsight::Ui
