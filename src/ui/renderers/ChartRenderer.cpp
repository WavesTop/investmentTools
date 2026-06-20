#include "ui/renderers/ChartRenderer.h"

#include "core/TechIndicators.h"
#include "domain/AnalysisResult.h"

#include <QPainter>
#include <QPainterPath>
#include <QtMath>

namespace InvestInsight::Ui {
namespace {

QVector<double> closesFromBars(const QVector<KBar> &bars, int offset, int count)
{
    QVector<double> closes;
    closes.reserve(count);
    for (int i = 0; i < count && offset + i < bars.size(); ++i)
        closes.push_back(bars[offset + i].close);
    return closes;
}

QVector<KBar> aggregateBars(const QVector<KBar> &daily, int groupDays)
{
    QVector<KBar> result;
    for (int i = 0; i < daily.size(); i += groupDays) {
        KBar bar = daily[i];
        const int end = qMin(i + groupDays, daily.size());
        for (int j = i + 1; j < end; ++j) {
            bar.high = qMax(bar.high, daily[j].high);
            bar.low = qMin(bar.low, daily[j].low);
            bar.close = daily[j].close;
            bar.volume += daily[j].volume;
            bar.date = daily[j].date;
        }
        result.push_back(bar);
    }
    return result;
}

void drawPanelTitle(QPainter &p, const QRectF &rect, const QString &title, const QColor &textColor)
{
    QFont font;
    font.setPixelSize(11);
    font.setBold(true);
    p.setFont(font);
    p.setPen(textColor);
    p.drawText(rect.adjusted(52, 0, -8, -rect.height() + 18), Qt::AlignLeft | Qt::AlignVCenter, title);
}

QRectF dataRectForPanel(const QRectF &panel, bool reserveDates)
{
    return panel.adjusted(52, 20, -12, reserveDates ? -16 : -8);
}

void drawGrid(QPainter &p, const QRectF &dataRect, const QRectF &panel,
              const QColor &gridColor, const QColor &textColor,
              double minV, double maxV)
{
    const double range = qMax(1e-9, maxV - minV);
    QFont font;
    font.setPixelSize(9);
    p.setFont(font);
    for (int i = 0; i <= 4; ++i) {
        const double yFrac = static_cast<double>(i) / 4.0;
        const double y = dataRect.top() + dataRect.height() * yFrac;
        QColor grid = gridColor;
        grid.setAlpha(100);
        p.setPen(QPen(grid, 0.5, Qt::DotLine));
        p.drawLine(QPointF(dataRect.left(), y), QPointF(dataRect.right(), y));

        const double value = maxV - range * yFrac;
        p.setPen(textColor);
        p.drawText(QRectF(panel.left(), y - 7, 48, 14),
                   Qt::AlignRight | Qt::AlignVCenter,
                   QString::number(value, 'f', 2));
    }
}

void drawDateLabels(QPainter &p, const QVector<KBar> &bars, int offset, int count,
                    const QRectF &dataRect, const QColor &textColor)
{
    if (count < 2) return;
    QFont font;
    font.setPixelSize(8);
    p.setFont(font);
    p.setPen(textColor);
    const double dx = dataRect.width() / static_cast<double>(count - 1);
    for (int i = 0; i < 5; ++i) {
        int idx = qBound(0, i * (count - 1) / 4, count - 1);
        const QString label = bars[qMin(offset + idx, bars.size() - 1)].date.mid(2, 8);
        const int width = QFontMetrics(font).horizontalAdvance(label);
        const double x = qBound(dataRect.left(), dataRect.left() + dx * idx - width / 2.0, dataRect.right() - width);
        p.drawText(QPointF(x, dataRect.bottom() + 11), label);
    }
}

void drawLinePanel(QPainter &p, const QVector<double> &series, const QRectF &panel,
                   const QString &title, const QColor &lineColor,
                   const QColor &gridColor, const QColor &textColor,
                   const QColor &bgColor)
{
    p.fillRect(panel, bgColor);
    drawPanelTitle(p, panel, title, textColor);
    const QRectF dataRect = dataRectForPanel(panel, false);
    if (series.size() < 2) {
        p.setPen(textColor);
        p.drawText(dataRect, Qt::AlignCenter, QString::fromUtf8("暂无数据"));
        return;
    }

    double minV = series.first();
    double maxV = series.first();
    for (double value : series) {
        minV = qMin(minV, value);
        maxV = qMax(maxV, value);
    }
    if (qFuzzyCompare(minV, maxV)) maxV = minV + 1.0;
    drawGrid(p, dataRect, panel, gridColor, textColor, minV, maxV);

    const double range = maxV - minV;
    const double dx = dataRect.width() / static_cast<double>(series.size() - 1);
    QPainterPath linePath;
    QPainterPath fillPath;
    for (int i = 0; i < series.size(); ++i) {
        const double x = dataRect.left() + dx * i;
        const double y = dataRect.bottom() - dataRect.height() * (series[i] - minV) / range;
        if (i == 0) {
            linePath.moveTo(x, y);
            fillPath.moveTo(x, dataRect.bottom());
            fillPath.lineTo(x, y);
        } else {
            linePath.lineTo(x, y);
            fillPath.lineTo(x, y);
        }
    }
    fillPath.lineTo(dataRect.right(), dataRect.bottom());
    fillPath.closeSubpath();

    QColor fill = lineColor;
    fill.setAlpha(30);
    p.fillPath(fillPath, fill);
    p.setPen(QPen(lineColor, 1.6));
    p.drawPath(linePath);
}

void drawKlinePanel(QPainter &p, const QVector<KBar> &bars, int offset, int count,
                    const QRectF &panel, const QString &title,
                    const QColor &gridColor, const QColor &textColor,
                    const QColor &bgColor)
{
    p.fillRect(panel, bgColor);
    drawPanelTitle(p, panel, title, textColor);
    const QRectF dataRect = dataRectForPanel(panel, true);
    count = qMin(count, bars.size() - offset);
    if (count < 2) {
        p.setPen(textColor);
        p.drawText(dataRect, Qt::AlignCenter, QString::fromUtf8("暂无数据"));
        return;
    }

    double minP = bars[offset].low;
    double maxP = bars[offset].high;
    for (int i = offset; i < offset + count; ++i) {
        minP = qMin(minP, bars[i].low);
        maxP = qMax(maxP, bars[i].high);
    }
    if (qFuzzyCompare(minP, maxP)) maxP = minP + 1.0;
    drawGrid(p, dataRect, panel, gridColor, textColor, minP, maxP);

    const double candleW = dataRect.width() / static_cast<double>(count);
    const double bodyW = qMax(1.0, candleW * 0.58);
    const double range = maxP - minP;
    for (int i = 0; i < count; ++i) {
        const KBar &bar = bars[offset + i];
        const bool up = bar.close >= bar.open;
        const QColor color = up ? QColor("#EF4444") : QColor("#3B82F6");
        const double centerX = dataRect.left() + candleW * (i + 0.5);
        const double yHigh = dataRect.bottom() - dataRect.height() * (bar.high - minP) / range;
        const double yLow = dataRect.bottom() - dataRect.height() * (bar.low - minP) / range;
        const double yOpen = dataRect.bottom() - dataRect.height() * (bar.open - minP) / range;
        const double yClose = dataRect.bottom() - dataRect.height() * (bar.close - minP) / range;

        p.setPen(QPen(color, 1));
        p.drawLine(QPointF(centerX, yHigh), QPointF(centerX, yLow));
        const QRectF body(centerX - bodyW / 2.0, qMin(yOpen, yClose), bodyW, qMax(1.0, qAbs(yOpen - yClose)));
        if (up) {
            p.setBrush(Qt::NoBrush);
            p.drawRect(body);
        } else {
            p.fillRect(body, color);
        }
    }
    drawDateLabels(p, bars, offset, count, dataRect, textColor);
}

void drawBarPanel(QPainter &p, const QVector<KBar> &bars, int offset, int count,
                  const QRectF &panel, const QString &title,
                  const QColor &gridColor, const QColor &textColor,
                  const QColor &bgColor)
{
    p.fillRect(panel, bgColor);
    drawPanelTitle(p, panel, title, textColor);
    const QRectF dataRect = dataRectForPanel(panel, true);
    count = qMin(count, bars.size() - offset);
    if (count < 2) return;

    double maxV = 1.0;
    for (int i = offset; i < offset + count; ++i) maxV = qMax(maxV, bars[i].volume);
    drawGrid(p, dataRect, panel, gridColor, textColor, 0, maxV);

    const double dx = dataRect.width() / static_cast<double>(count);
    for (int i = 0; i < count; ++i) {
        const KBar &bar = bars[offset + i];
        const QColor color = bar.close >= bar.open ? QColor("#EF4444") : QColor("#3B82F6");
        const double h = dataRect.height() * bar.volume / maxV;
        p.fillRect(QRectF(dataRect.left() + dx * i, dataRect.bottom() - h, qMax(1.0, dx * 0.65), h), color);
    }
    drawDateLabels(p, bars, offset, count, dataRect, textColor);
}

void drawFundFlowPanel(QPainter &p, const QVector<double> &series, const QRectF &panel,
                       const QColor &gridColor, const QColor &textColor, const QColor &bgColor)
{
    p.fillRect(panel, bgColor);
    drawPanelTitle(p, panel, QString::fromUtf8("主力资金流（亿元）"), textColor);
    const QRectF dataRect = dataRectForPanel(panel, false);
    if (series.size() < 2) {
        p.setPen(textColor);
        p.drawText(dataRect, Qt::AlignCenter, QString::fromUtf8("暂无资金流数据"));
        return;
    }

    double absMax = 1.0;
    for (double value : series) absMax = qMax(absMax, qAbs(value));
    drawGrid(p, dataRect, panel, gridColor, textColor, -absMax, absMax);

    const double zeroY = dataRect.center().y();
    p.setPen(QPen(gridColor, 1, Qt::DashLine));
    p.drawLine(QPointF(dataRect.left(), zeroY), QPointF(dataRect.right(), zeroY));

    const double dx = dataRect.width() / static_cast<double>(series.size());
    for (int i = 0; i < series.size(); ++i) {
        const double value = series[i];
        const double h = qAbs(value) / (absMax * 2.0) * dataRect.height();
        const QColor color = value >= 0 ? QColor("#EF4444") : QColor("#3B82F6");
        const double y = value >= 0 ? zeroY - h : zeroY;
        p.fillRect(QRectF(dataRect.left() + dx * i, y, qMax(1.0, dx * 0.65), h), color);
    }
}

} // namespace

QPixmap ChartRenderer::buildTrendChart(const SectorSnapshot &snap,
                                       const ThemeColors &theme,
                                       int width,
                                       int minHeight)
{
    width = qMax(360, width);
    minHeight = qMax(420, minHeight);

    const QVector<KBar> &bars = snap.dailyBars;
    const int count = qMin(60, bars.size());
    const int offset = qMax(0, bars.size() - count);

    const int gap = 6;
    const int trendH = 150;
    const int klineH = 220;
    const int volumeH = 110;
    const int macdH = 110;
    const int flowH = 120;
    const int weekMonthH = 160;
    const int totalH = qMax(minHeight, trendH + klineH + volumeH + macdH + flowH + weekMonthH + gap * 6);

    QPixmap pixmap(width, totalH);
    pixmap.fill(QColor(theme.chartBg));
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QColor gridColor(theme.chartGrid);
    const QColor textColor(theme.chartAxisText);
    const QColor bgColor(theme.chartBg);

    int y = 0;
    const int panelW = width / 3 - 2;
    drawLinePanel(painter, closesFromBars(bars, qMax(0, bars.size() - qMin(65, bars.size())), qMin(65, bars.size())),
                  QRectF(0, y, panelW, trendH), QString::fromUtf8("近3月趋势"), QColor("#EF4444"), gridColor, textColor, bgColor);
    drawLinePanel(painter, closesFromBars(bars, qMax(0, bars.size() - qMin(130, bars.size())), qMin(130, bars.size())),
                  QRectF(panelW + 3, y, panelW, trendH), QString::fromUtf8("近6月趋势"), QColor("#F59E0B"), gridColor, textColor, bgColor);
    drawLinePanel(painter, closesFromBars(bars, qMax(0, bars.size() - qMin(250, bars.size())), qMin(250, bars.size())),
                  QRectF((panelW + 3) * 2, y, panelW, trendH), QString::fromUtf8("近1年趋势"), QColor("#8B5CF6"), gridColor, textColor, bgColor);
    y += trendH + gap;

    drawKlinePanel(painter, bars, offset, count, QRectF(0, y, width, klineH),
                   QString::fromUtf8("日K线"), gridColor, textColor, bgColor);
    y += klineH + gap;

    drawBarPanel(painter, bars, offset, count, QRectF(0, y, width, volumeH),
                 QString::fromUtf8("成交量"), gridColor, textColor, bgColor);
    y += volumeH + gap;

    if (bars.size() >= 26) {
        const auto macd = TechIndicators::calcMACD(bars);
        QVector<KBar> synthetic;
        synthetic.reserve(qMax(0, macd.hist.size() - offset));
        for (int i = offset; i < macd.hist.size() && i < bars.size(); ++i) {
            KBar bar = bars[i];
            bar.open = 0.0;
            bar.close = macd.hist[i];
            bar.high = qMax(macd.dif[i], qMax(macd.dea[i], macd.hist[i]));
            bar.low = qMin(macd.dif[i], qMin(macd.dea[i], macd.hist[i]));
            bar.volume = qAbs(macd.hist[i]);
            synthetic.push_back(bar);
        }
        drawKlinePanel(painter, synthetic, 0, synthetic.size(), QRectF(0, y, width, macdH),
                       "MACD(12,26,9)", gridColor, textColor, bgColor);
    } else {
        drawLinePanel(painter, {}, QRectF(0, y, width, macdH), "MACD(12,26,9)", QColor("#94A3B8"), gridColor, textColor, bgColor);
    }
    y += macdH + gap;

    const int flowCount = qMin(count, snap.fundFlowSeries.size());
    drawFundFlowPanel(painter, snap.fundFlowSeries.mid(qMax(0, snap.fundFlowSeries.size() - flowCount)),
                      QRectF(0, y, width, flowH), gridColor, textColor, bgColor);
    y += flowH + gap;

    const int halfW = width / 2 - 2;
    const QVector<KBar> weekBars = aggregateBars(bars, 5);
    const QVector<KBar> monthBars = aggregateBars(bars, 22);
    drawKlinePanel(painter, weekBars, qMax(0, weekBars.size() - qMin(52, weekBars.size())), qMin(52, weekBars.size()),
                   QRectF(0, y, halfW, weekMonthH), QString::fromUtf8("周K线"), gridColor, textColor, bgColor);
    drawKlinePanel(painter, monthBars, qMax(0, monthBars.size() - qMin(12, monthBars.size())), qMin(12, monthBars.size()),
                   QRectF(halfW + 4, y, halfW, weekMonthH), QString::fromUtf8("月K线"), gridColor, textColor, bgColor);
    y += weekMonthH;

    return pixmap.copy(0, 0, width, qMax(minHeight, y));
}

} // namespace InvestInsight::Ui
