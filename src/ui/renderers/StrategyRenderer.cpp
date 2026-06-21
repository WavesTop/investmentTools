#include "ui/renderers/StrategyRenderer.h"

#include "domain/AnalysisResult.h"

#include <algorithm>
#include <cmath>

#include <QDate>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>

namespace InvestInsight::Ui {
namespace {

struct StrategyRow {
    bool isIndex = false;
    int sectorJumpIndex = 0;
    QString indexKey;
    QString name;
    AdviceAction action = AdviceAction::Hold;
    double score = 0.0;
    double change = 0.0;
    QString trend;
    QString strategy;
    const SectorSnapshot *sector = nullptr;
};

QString num(double value, int digits = 2)
{
    return QString::number(value, 'f', digits);
}

QString escaped(const QString &text)
{
    return text.toHtmlEscaped();
}

QString colorFor(double value, const ThemeColors &theme)
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

QString inferTrend(double change)
{
    if (change >= 2.0) return QString::fromUtf8("强势看多");
    if (change >= 0.5) return QString::fromUtf8("偏多");
    if (change <= -2.0) return QString::fromUtf8("强势看空");
    if (change <= -0.5) return QString::fromUtf8("偏空");
    if (std::abs(change) <= 0.2) return QString::fromUtf8("横盘震荡");
    return QString::fromUtf8("方向不明");
}

AdviceAction inferAction(double change)
{
    if (change >= 1.2) return AdviceAction::Increase;
    if (change <= -1.2) return AdviceAction::Decrease;
    return AdviceAction::Hold;
}

QList<StrategyRow> buildRows(const AnalysisResult &analysis)
{
    QList<StrategyRow> rows;
    for (int i = 0; i < analysis.sectors.size(); ++i) {
        const SectorSnapshot &sector = analysis.sectors[i];
        rows.push_back({false, i + 1, {}, sector.industry, sector.action,
                        sector.forecastScore, sector.todayChangePct,
                        sector.trendSummary, sector.strategy.actionLabel, &sector});
    }

    const auto appendIndex = [&rows](const IndexSnapshot &index, const QString &key) {
        if (index.name.isEmpty()) return;
        rows.push_back({true, 0, key, index.name, inferAction(index.changePct),
                        qBound(-1.0, index.changePct / 3.0, 1.0), index.changePct,
                        inferTrend(index.changePct),
                        QString::fromUtf8("跟踪指数方向，管理相关仓位"), nullptr});
    };
    appendIndex(analysis.marketCtx.shanghai, "SH");
    appendIndex(analysis.marketCtx.shenzhen, "SZ");
    appendIndex(analysis.marketCtx.chinext, "CY");
    appendIndex(analysis.marketCtx.csi300, "CSI300");
    appendIndex(analysis.marketCtx.csi500, "CSI500");
    appendIndex(analysis.marketCtx.nasdaq, "NASDAQ");
    appendIndex(analysis.marketCtx.sp500, "SP500");
    appendIndex(analysis.marketCtx.dowjones, "DJI");
    return rows;
}

QString sectorLink(const StrategyRow &row)
{
    if (row.isIndex) {
        return "<a href='jumpi-" + escaped(row.indexKey) + "'>" + escaped(row.name) + "</a>";
    }
    return "<a href='jump-" + QString::number(row.sectorJumpIndex) + "'>" + escaped(row.name) + "</a>";
}

QString renderTopTable(const QString &title,
                       const QList<StrategyRow> &rows,
                       const ThemeColors &theme,
                       bool reverse)
{
    QString h = "<div style='flex:1;'><div style='font-size:12px;font-weight:700;color:"
        + QString(reverse ? "#DC2626" : "#059669") + ";margin-bottom:6px;'>" + title + "</div>";
    h += "<table class='fund'><tr><th>板块</th><th>评分</th><th>策略</th></tr>";
    const int count = qMin(5, rows.size());
    for (int i = 0; i < count; ++i) {
        const StrategyRow &row = reverse ? rows[rows.size() - 1 - i] : rows[i];
        h += "<tr><td>" + sectorLink(row) + "</td><td style='color:" + colorFor(row.score, theme)
            + ";font-weight:700;'>" + num(row.score) + "</td><td style='font-size:11px;'>"
            + escaped(row.strategy) + "</td></tr>";
    }
    h += "</table></div>";
    return h;
}

QString renderMarketAdvice(const QList<StrategyRow> &sectorRows,
                           const QList<StrategyRow> &indexRows,
                           const ThemeColors &theme)
{
    if (sectorRows.isEmpty()) return {};
    int bullCount = 0;
    int bearCount = 0;
    double avgScore = 0.0;
    for (const StrategyRow &row : sectorRows) {
        if (row.score > 0.1) ++bullCount;
        if (row.score < -0.1) ++bearCount;
        avgScore += row.score;
    }
    avgScore /= sectorRows.size();

    QString tone = QString::fromUtf8("当前市场多空交织，建议精选板块、控制仓位。");
    if (bullCount > bearCount * 2) tone = QString::fromUtf8("当前市场整体偏多，可适度积极参与。");
    else if (bearCount > bullCount * 2) tone = QString::fromUtf8("当前市场整体偏空，建议保持谨慎。");

    QString h = "<hr class='divider'/><div class='section-title'>市场操作建议</div>";
    h += "<div class='narrative'>" + tone + "</div><div style='display:flex;gap:16px;margin-top:8px;'>";
    h += renderTopTable(QString::fromUtf8("&#9650; 推荐关注板块 Top5"), sectorRows, theme, false);
    h += renderTopTable(QString::fromUtf8("&#9660; 建议回避板块 Top5"), sectorRows, theme, true);
    h += "</div>";

    if (!indexRows.isEmpty()) {
        h += "<div style='margin-top:10px;'><div style='font-size:12px;font-weight:700;color:#7C3AED;margin-bottom:6px;'>"
             "&#9679; 指数方向参考</div><table class='fund'><tr><th>指数</th><th>评分</th><th>方向</th></tr>";
        for (const StrategyRow &row : indexRows) {
            h += "<tr><td>" + sectorLink(row) + "</td><td style='color:" + colorFor(row.score, theme)
                + ";font-weight:700;'>" + num(row.score) + "</td><td style='font-size:11px;'>"
                + escaped(row.strategy) + "</td></tr>";
        }
        h += "</table></div>";
    }

    const QString level = avgScore > 0.08 ? QString::fromUtf8("积极进攻")
        : (avgScore < -0.08 ? QString::fromUtf8("防御收缩") : QString::fromUtf8("均衡配置"));
    h += "<div style='margin-top:8px;padding:10px 14px;border:1px solid " + theme.cardBorder
        + ";border-radius:8px;background:" + theme.narrativeBg + ";font-size:12px;'>"
        + QString::fromUtf8("<b>整体投资策略：</b>") + level
        + QString::fromUtf8("，全市场均值评分 ") + num(avgScore)
        + QString::fromUtf8("，增配/减配 = ") + QString::number(bullCount)
        + "/" + QString::number(bearCount) + "</div>";
    return h;
}

QMap<QString, QJsonObject> parseHoldings(const QString &json)
{
    QMap<QString, QJsonObject> holdings;
    const QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
    if (!doc.isArray()) return holdings;
    for (const QJsonValue &value : doc.array()) {
        const QJsonObject object = value.toObject();
        const QString sector = object.value("sector").toString();
        if (!sector.isEmpty() && !object.value("sold").toBool(false)) holdings.insert(sector, object);
    }
    return holdings;
}

QString trackingCard(const QString &label,
                     const QString &value,
                     const QString &note,
                     const QString &color,
                     const ThemeColors &theme)
{
    return "<div class='metric-card'>"
        "<div class='label'>" + escaped(label) + "</div>"
        "<div class='value' style='color:" + color + ";'>" + escaped(value) + "</div>"
        "<div class='meta' style='margin:4px 0 0 0;'>" + escaped(note) + "</div>"
        "</div>";
}

QString renderTrackingState(const QList<StrategyRow> &sectorRows,
                            const QList<StrategyRow> &indexRows,
                            const QString &portfolioJson,
                            const ThemeColors &theme)
{
    if (sectorRows.isEmpty()) return {};

    int attackCount = 0;
    int defendCount = 0;
    int eventCount = 0;
    double avgScore = 0.0;
    for (const StrategyRow &row : sectorRows) {
        if (row.score > 0.1) ++attackCount;
        if (row.score < -0.1) ++defendCount;
        avgScore += row.score;
        if (row.sector) eventCount += row.sector->upcomingEvents.size() + row.sector->futureEventsAI.size();
    }
    avgScore /= sectorRows.size();

    const StrategyRow &best = sectorRows.first();
    const StrategyRow &risk = sectorRows.last();
    const int holdingCount = parseHoldings(portfolioJson).size();
    const QString marketTone = avgScore > 0.08 ? QString::fromUtf8("偏进攻")
        : (avgScore < -0.08 ? QString::fromUtf8("偏防守") : QString::fromUtf8("均衡观察"));
    const QString indexNote = indexRows.isEmpty()
        ? QString::fromUtf8("暂无指数参照")
        : QString::fromUtf8("指数参考：") + indexRows.first().name + " " + num(indexRows.first().score);

    QString h = "<div class='section-title'>跟踪状态</div>";
    h += "<div class='metric-grid'>";
    h += trackingCard(QString::fromUtf8("市场姿态"), marketTone,
                      QString::fromUtf8("平均评分 ") + num(avgScore), colorFor(avgScore, theme), theme);
    h += trackingCard(QString::fromUtf8("进攻板块"), QString::number(attackCount),
                      best.name + QString::fromUtf8(" 领先"), "#EF4444", theme);
    h += trackingCard(QString::fromUtf8("防守板块"), QString::number(defendCount),
                      risk.name + QString::fromUtf8(" 风险最高"), "#3B82F6", theme);
    h += trackingCard(QString::fromUtf8("观察队列"), QString::number(eventCount),
                      QString::fromUtf8("持仓 ") + QString::number(holdingCount) + QString::fromUtf8(" 项；") + indexNote,
                      theme.headingColor, theme);
    h += "</div>";
    return h;
}

QString renderPortfolio(const AnalysisResult &analysis,
                        const QString &portfolioJson,
                        const ThemeColors &theme)
{
    const QMap<QString, QJsonObject> holdings = parseHoldings(portfolioJson);
    if (holdings.isEmpty()) return {};

    QString h = "<hr class='divider'/><div class='section-title'>&#128200; 我的持仓诊断</div>";
    h += "<table class='fund'><tr><th>板块</th><th>类型</th><th>买入批次</th><th>总投入（元）</th>"
         "<th>今日参考变动</th><th>预测评分</th><th>建议操作</th></tr>";
    double total = 0.0;
    for (auto it = holdings.constBegin(); it != holdings.constEnd(); ++it) {
        const QJsonObject object = it.value();
        const QString sectorName = it.key();
        const double amount = object.value("amount").toDouble();
        total += amount;
        const SectorSnapshot *sector = nullptr;
        for (const SectorSnapshot &candidate : analysis.sectors) {
            if (candidate.industry == sectorName) { sector = &candidate; break; }
        }
        const double score = sector ? sector->forecastScore : 0.0;
        const double change = sector ? sector->todayChangePct : 0.0;
        const QString action = sector ? sector->strategy.actionLabel : QString::fromUtf8("暂无分析数据");
        h += "<tr><td style='font-weight:700;'>" + escaped(sectorName) + "</td><td>"
            + escaped(object.value("holdType").toString(QString::fromUtf8("基金"))) + "</td><td>"
            + escaped(object.value("date").toString()) + " "
            + escaped(object.value("remark").toString()) + "</td><td style='text-align:right;'>¥"
            + num(amount, 0) + "</td><td style='color:" + colorFor(change, theme)
            + ";font-weight:700;text-align:right;'>" + num(change) + "%</td><td style='color:"
            + colorFor(score, theme) + ";font-weight:700;text-align:center;'>" + num(score)
            + "</td><td style='font-size:11px;'>" + escaped(action) + "</td></tr>";
    }
    h += "</table><div style='margin-top:8px;padding:8px 14px;border:1px solid " + theme.cardBorder
        + ";border-radius:8px;background:" + theme.narrativeBg + ";font-size:12px;'>"
        + QString::fromUtf8("总投入：<b>¥") + num(total, 0)
        + QString::fromUtf8("</b>。基金净值存在 T+1 确认延迟，ETF/股票按实时成交和 T+1 规则管理。</div>");
    return h;
}

QString renderFutureEvents(const AnalysisResult &analysis, const ThemeColors &theme)
{
    QString h;
    int count = 0;
    for (const SectorSnapshot &sector : analysis.sectors) {
        count += sector.upcomingEvents.size() + sector.futureEventsAI.size();
    }
    if (count == 0) return {};

    h += "<hr class='divider'/><div class='section-title'>&#128197; 未来事件日历 "
        "<span style='font-size:11px;font-weight:400;color:" + theme.mutedColor + ";'>"
        + QString::number(count) + QString::fromUtf8(" 条前瞻事件</span></div>");
    for (const SectorSnapshot &sector : analysis.sectors) {
        const QStringList events = sector.upcomingEvents + sector.futureEventsAI;
        if (events.isEmpty()) continue;
        h += "<div style='margin-bottom:8px;'><div style='font-size:12px;font-weight:700;color:"
            + theme.sectionTitleColor + ";margin-bottom:4px;'>" + escaped(sector.industry) + "</div>";
        for (const QString &event : events) {
            h += "<div style='padding:5px 12px;margin:2px 0;border-radius:8px;background:"
                + theme.narrativeBg + ";font-size:11px;border:1px solid " + theme.cardBorder
                + ";line-height:1.5;'>" + escaped(event) + "</div>";
        }
        h += "</div>";
    }
    return h;
}

} // namespace

QString StrategyRenderer::render(const AnalysisResult &analysis,
                                 const ThemeColors &theme,
                                 const StrategyRenderOptions &options)
{
    Q_UNUSED(options.simpleMode);
    QList<StrategyRow> sectorRows;
    QList<StrategyRow> indexRows;
    for (const StrategyRow &row : buildRows(analysis)) {
        if (row.isIndex) indexRows.push_back(row);
        else sectorRows.push_back(row);
    }
    std::sort(sectorRows.begin(), sectorRows.end(), [](const StrategyRow &a, const StrategyRow &b) {
        return a.score > b.score;
    });
    std::sort(indexRows.begin(), indexRows.end(), [](const StrategyRow &a, const StrategyRow &b) {
        return a.score > b.score;
    });

    QString h = "<html><head><style>" + buildHtmlCss(theme) + "</style></head><body>";
    h += "<h1 style='font-size:18px;'>策略跟踪</h1>";
    h += renderTrackingState(sectorRows, indexRows, options.portfolioBatchesJson, theme);
    h += renderMarketAdvice(sectorRows, indexRows, theme);
    h += renderPortfolio(analysis, options.portfolioBatchesJson, theme);
    h += renderFutureEvents(analysis, theme);
    h += "<hr class='divider'/><div style='font-size:10px;color:" + theme.disclaimerColor
        + ";margin-top:10px;line-height:1.5;'>"
        + QString::fromUtf8("免责声明：以上分析基于公开数据与规则引擎/AI 推断，不构成投资建议。")
        + "</div></body></html>";
    return h;
}

} // namespace InvestInsight::Ui
