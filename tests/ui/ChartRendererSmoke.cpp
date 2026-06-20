#include "ui/AppTheme.h"
#include "ui/renderers/ChartRenderer.h"

#include "domain/AnalysisResult.h"

#include <QColor>
#include <QDate>
#include <QPixmap>
#include <QTextStream>

namespace {

int failures = 0;

void expect(bool condition, const QString &message)
{
    if (condition) return;
    QTextStream(stderr) << "FAIL: " << message << '\n';
    ++failures;
}

SectorSnapshot makeSampleSector()
{
    SectorSnapshot snap;
    snap.industry = QString::fromUtf8("半导体");

    const QDate start(2026, 1, 2);
    double close = 100.0;
    for (int i = 0; i < 90; ++i) {
        const double open = close;
        close = open + ((i % 9) - 3) * 0.35 + 0.18;

        KBar bar;
        bar.date = start.addDays(i).toString("yyyy-MM-dd");
        bar.open = open;
        bar.close = close;
        bar.high = qMax(open, close) + 1.2;
        bar.low = qMin(open, close) - 1.1;
        bar.volume = 100000000.0 + i * 1200000.0;
        snap.dailyBars.push_back(bar);
        snap.trendSeries.push_back(close);
        snap.fundFlowSeries.push_back((i % 7 - 3) * 0.8);
    }

    return snap;
}

bool containsNonBackgroundPixels(const QPixmap &pixmap, const QColor &background)
{
    const QImage image = pixmap.toImage();
    const int stepX = qMax(1, image.width() / 40);
    const int stepY = qMax(1, image.height() / 40);
    for (int y = 0; y < image.height(); y += stepY) {
        for (int x = 0; x < image.width(); x += stepX) {
            if (image.pixelColor(x, y) != background) return true;
        }
    }
    return false;
}

} // namespace

int runChartRendererSmoke()
{
    const auto theme = InvestInsight::Ui::lightTheme();
    const SectorSnapshot snap = makeSampleSector();
    const QPixmap chart = InvestInsight::Ui::ChartRenderer::buildTrendChart(snap, theme, 640, 520);

    expect(!chart.isNull(), "trend chart pixmap is not null");
    expect(chart.width() == 640, "trend chart keeps requested width");
    expect(chart.height() >= 520, "trend chart height keeps requested minimum");
    expect(containsNonBackgroundPixels(chart, QColor(theme.chartBg)), "trend chart draws non-background pixels");

    if (failures > 0) {
        QTextStream(stderr) << failures << " chart renderer smoke check(s) failed.\n";
        return 1;
    }

    QTextStream(stdout) << "ChartRenderer smoke passed.\n";
    return 0;
}
