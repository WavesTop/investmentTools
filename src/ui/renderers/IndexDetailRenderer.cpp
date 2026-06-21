#include "ui/renderers/IndexDetailRenderer.h"

#include "core/MarketContext.h"
#include "core/TechIndicators.h"
#include "domain/AnalysisResult.h"
#include "ui/renderers/ChartRenderer.h"

#include <QBuffer>
#include <QDate>
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

QString actionText(AdviceAction action)
{
    switch (action) {
    case AdviceAction::Increase: return QString::fromUtf8("指数偏强");
    case AdviceAction::Decrease: return QString::fromUtf8("指数偏弱");
    default: return QString::fromUtf8("指数震荡");
    }
}

SectorSnapshot makeSyntheticSector(const IndexSnapshot &index)
{
    SectorSnapshot sector;
    sector.industry = index.name;
    sector.action = index.changePctValid ? inferAction(index.changePct) : AdviceAction::Hold;
    sector.todayChangePct = index.changePct;
    sector.todayChangePctValid = index.changePctValid;
    sector.forecastScore = index.changePctValid ? qBound(-1.0, index.changePct / 3.0, 1.0) : 0.0;
    sector.confidence = index.changePctValid ? 90.0 : 30.0;
    sector.dataQualityScore = (index.volume > 0.0 || index.amount > 0.0) ? 95.0 : 82.0;
    sector.dataQualityNote = QString::fromUtf8("指数快照与历史 K 线聚合数据");
    sector.sourceConsistencyScore = index.klineSeries.isEmpty() ? 80.0 : 95.0;
    sector.trendSummary = index.changePctValid ? inferTrend(index.changePct) : QString::fromUtf8("数据不足");
    sector.strategy.actionLabel = actionText(sector.action);
    sector.strategy.shortTermView = sector.trendSummary;
    sector.strategy.mediumTermView = QString::fromUtf8("结合 5 日动量观察指数延续性");
    sector.strategy.longTermView = QString::fromUtf8("作为板块仓位的上层风控锚");
    sector.strategy.operationAdvice = QString::fromUtf8("当指数与板块方向冲突时，优先降低总仓位风险。");
    sector.strategy.stopLossPct = 3.0;
    sector.strategy.takeProfitPct = 5.0;
    sector.klineSource = QString::fromUtf8("指数历史 K 线");
    sector.fundFlowSource = QString::fromUtf8("指数成交量额");
    sector.lastDataDate = QDate::currentDate().toString("yyyy-MM-dd");
    sector.sectorTierLabel = QString::fromUtf8("指数");
    sector.newsHeadlines << (QString::fromUtf8("指数代码：") + (index.code.isEmpty() ? QString("-") : index.code))
                         << (QString::fromUtf8("最新点位：") + num(index.lastClose))
                         << (QString::fromUtf8("成交量(亿)：") + num(index.volume, 0))
                         << (QString::fromUtf8("成交额(亿)：") + num(index.amount, 0));
    sector.dailyBars = index.dailyBars;
    sector.trendSeries = index.klineSeries;
    sector.weekSeries = index.weekSeries;
    sector.monthSeries = index.monthSeries;
    if (!index.dailyBars.isEmpty()) {
        sector.lastDataDate = index.dailyBars.last().date;
        const QVector<KBar> &bars = index.dailyBars;
        if (bars.size() >= 6 && bars[bars.size() - 6].close > 0.0) {
            sector.fiveDayMomentum = (bars.last().close - bars[bars.size() - 6].close)
                / bars[bars.size() - 6].close * 100.0;
        }
        if (bars.size() >= 21 && bars[bars.size() - 21].close > 0.0) {
            sector.twentyDayMomentum = (bars.last().close - bars[bars.size() - 21].close)
                / bars[bars.size() - 21].close * 100.0;
        }
        const int span = qMin(bars.size(), 60);
        for (int i = bars.size() - span; i < bars.size(); ++i) {
            sector.fundFlowSeries << bars[i].volume / 1e8;
        }
        if (bars.size() >= 30) {
            const auto macd = TechIndicators::calcMACD(bars);
            const auto rsi = TechIndicators::calcRSI(bars);
            const auto kdj = TechIndicators::calcKDJ(bars);
            const auto ma = TechIndicators::calcMA(bars);
            if (!macd.dif.isEmpty()) sector.tech.macdDIF = macd.dif.last();
            if (!macd.dea.isEmpty()) sector.tech.macdDEA = macd.dea.last();
            if (!macd.hist.isEmpty()) sector.tech.macdHist = macd.hist.last();
            if (macd.dif.size() >= 2 && macd.dea.size() >= 2) {
                sector.tech.macdGoldenCross = macd.dif[macd.dif.size() - 2] < macd.dea[macd.dea.size() - 2]
                    && sector.tech.macdDIF > sector.tech.macdDEA;
                sector.tech.macdDeadCross = macd.dif[macd.dif.size() - 2] > macd.dea[macd.dea.size() - 2]
                    && sector.tech.macdDIF < sector.tech.macdDEA;
            }
            if (!rsi.rsi6.isEmpty()) sector.tech.rsi6 = rsi.rsi6.last();
            if (!rsi.rsi12.isEmpty()) sector.tech.rsi12 = rsi.rsi12.last();
            if (!kdj.k.isEmpty()) sector.tech.kdjK = kdj.k.last();
            if (!kdj.d.isEmpty()) sector.tech.kdjD = kdj.d.last();
            if (!ma.ma5.isEmpty()) sector.tech.ma5 = ma.ma5.last();
            if (!ma.ma10.isEmpty()) sector.tech.ma10 = ma.ma10.last();
            if (!ma.ma20.isEmpty()) sector.tech.ma20 = ma.ma20.last();
            sector.tech.maLongArrange = sector.tech.ma5 > sector.tech.ma10 && sector.tech.ma10 > sector.tech.ma20;
            sector.tech.maShortArrange = sector.tech.ma5 < sector.tech.ma10 && sector.tech.ma10 < sector.tech.ma20;
            sector.tech.techScore = qBound(0.0, 50.0 + sector.forecastScore * 45.0, 100.0);
        }
    } else {
        sector.missingDataItems << QString::fromUtf8("指数历史 K 线缺失");
    }
    return sector;
}

QString renderMarketRisk(const SectorSnapshot &sector, const ThemeColors &theme)
{
    QString tone = QString::fromUtf8("指数震荡，建议以板块个体信号为主。");
    if (sector.action == AdviceAction::Increase) {
        tone = QString::fromUtf8("指数方向偏强，可提高同方向板块的观察优先级。");
    } else if (sector.action == AdviceAction::Decrease) {
        tone = QString::fromUtf8("指数方向偏弱，应优先控制总仓位和回撤风险。");
    }
    return "<div class='section-title'>市场风控</div><div class='narrative'>"
        + escaped(tone) + "<br/>" + escaped(sector.strategy.operationAdvice)
        + "</div><table class='overview'><tr><th>风控锚</th><th>数值</th></tr>"
        "<tr><td>5 日动量</td><td style='color:" + colorFor(sector.fiveDayMomentum, theme) + ";'>"
        + pct(sector.fiveDayMomentum) + "</td></tr><tr><td>20 日动量</td><td style='color:"
        + colorFor(sector.twentyDayMomentum, theme) + ";'>" + pct(sector.twentyDayMomentum)
        + "</td></tr></table>";
}

QString renderTechnical(const SectorSnapshot &sector, const ThemeColors &theme)
{
    const TechSignals &tech = sector.tech;
    QString h = "<div class='section-title'>技术指标</div><table class='overview'>"
        "<tr><th>指标</th><th>数值</th><th>状态</th></tr>";
    h += "<tr><td>MACD</td><td>DIF " + num(tech.macdDIF, 3) + " / DEA " + num(tech.macdDEA, 3)
        + "</td><td>" + (tech.macdGoldenCross ? QString::fromUtf8("金叉") : (tech.macdDeadCross ? QString::fromUtf8("死叉") : QString::fromUtf8("观察"))) + "</td></tr>";
    h += "<tr><td>RSI</td><td>RSI6 " + num(tech.rsi6, 1) + " / RSI12 " + num(tech.rsi12, 1)
        + "</td><td>" + QString::fromUtf8("风险中性") + "</td></tr>";
    h += "<tr><td>均线</td><td>MA5 " + num(tech.ma5, 2) + " / MA20 " + num(tech.ma20, 2)
        + "</td><td>" + (tech.maLongArrange ? QString::fromUtf8("多头排列") : (tech.maShortArrange ? QString::fromUtf8("空头排列") : QString::fromUtf8("震荡"))) + "</td></tr>";
    h += "<tr><td>技术综合</td><td colspan='2' style='color:" + colorFor(tech.techScore - 50.0, theme)
        + ";font-weight:700;'>" + num(tech.techScore, 0) + "/100</td></tr></table>";
    return h;
}

QString renderDataQuality(const SectorSnapshot &sector)
{
    return "<div class='section-title'>数据质量</div><table class='overview'>"
        "<tr><th>项目</th><th>结果</th></tr><tr><td>质量分</td><td><b>"
        + num(sector.dataQualityScore, 0) + "</b>/100</td></tr><tr><td>一致性</td><td><b>"
        + num(sector.sourceConsistencyScore, 0) + "</b>/100</td></tr><tr><td>说明</td><td>"
        + escaped(sector.dataQualityNote) + "</td></tr><tr><td>日期</td><td>"
        + escaped(sector.lastDataDate) + "</td></tr></table>";
}

} // namespace

QString IndexDetailRenderer::render(const IndexSnapshot &index,
                                    const ThemeColors &theme,
                                    const IndexDetailRenderOptions &options)
{
    const SectorSnapshot sector = makeSyntheticSector(index);
    const QPixmap chart = ChartRenderer::buildTrendChart(
        sector, theme, options.chartWidth, options.chartHeight);

    QString h = "<html><head><style>" + buildHtmlCss(theme) + "</style></head><body>";
    h += "<h2>指数详情：" + escaped(index.name) + " <span style='color:"
        + colorFor(index.changePct, theme) + ";font-size:16px;'>"
        + (index.changePctValid ? pct(index.changePct) : QString::fromUtf8("涨跌缺失"))
        + "</span></h2>";
    h += "<div class='meta'>" + escaped(index.code) + QString::fromUtf8(" · 最新点位 ")
        + num(index.lastClose, 2) + QString::fromUtf8(" · 成交额 ")
        + num(index.amount, 0) + QString::fromUtf8(" 亿</div>");
    h += "<div class='section-title'>指数方向</div><div class='narrative'>"
        + escaped(sector.trendSummary) + QString::fromUtf8("，当前动作：")
        + escaped(sector.strategy.actionLabel) + "</div>";
    if (!options.simpleMode) {
        h += "<div class='section-title'>趋势图表</div><img style='max-width:100%;border:1px solid "
            + theme.cardBorder + ";border-radius:8px;' src='data:image/png;base64,"
            + pixmapToBase64(chart) + "'/>";
        h += renderTechnical(sector, theme);
    }
    h += renderMarketRisk(sector, theme);
    h += renderDataQuality(sector);
    h += "</body></html>";
    return h;
}

} // namespace InvestInsight::Ui
