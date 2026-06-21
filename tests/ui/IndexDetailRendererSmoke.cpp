#include "ui/AppTheme.h"
#include "ui/renderers/IndexDetailRenderer.h"

#include "core/MarketContext.h"

#include <QDate>
#include <QTextStream>

namespace {

int failures = 0;

void expect(bool condition, const QString &message)
{
    if (condition) return;
    QTextStream(stderr) << "FAIL: " << message << '\n';
    ++failures;
}

IndexSnapshot makeIndex()
{
    IndexSnapshot index;
    index.name = QString::fromUtf8("上证指数");
    index.code = "000001";
    index.lastClose = 3120.5;
    index.changePct = 0.46;
    index.changePctValid = true;
    index.volume = 4100;
    index.amount = 5300;

    const QDate start(2026, 1, 2);
    double close = 3000.0;
    for (int i = 0; i < 80; ++i) {
        KBar bar;
        bar.date = start.addDays(i).toString("yyyy-MM-dd");
        bar.open = close;
        close += ((i % 8) - 3) * 2.0 + 2.8;
        bar.close = close;
        bar.high = close + 8.0;
        bar.low = bar.open - 7.0;
        bar.volume = 1000000000.0 + i * 10000000.0;
        index.dailyBars << bar;
        index.klineSeries << close;
    }
    return index;
}

} // namespace

int runIndexDetailRendererSmoke()
{
    const auto theme = InvestInsight::Ui::lightTheme();
    const QString html = InvestInsight::Ui::IndexDetailRenderer::render(makeIndex(), theme);

    expect(html.contains("<html>"), "index detail html contains document root");
    expect(html.contains(QString::fromUtf8("指数详情")), "index detail contains page title");
    expect(html.contains(QString::fromUtf8("上证指数")), "index detail renders index name");
    expect(html.contains("data:image/png;base64"), "index detail embeds chart image");
    expect(html.contains(QString::fromUtf8("技术指标")), "index detail contains technical indicators");
    expect(html.contains(QString::fromUtf8("市场风控")), "index detail contains risk guidance");
    expect(html.contains(QString::fromUtf8("数据质量")), "index detail contains data quality");
    expect(html.contains(theme.bodyBg), "index detail applies theme css");

    if (failures > 0) {
        QTextStream(stderr) << failures << " index detail renderer smoke check(s) failed.\n";
        return 1;
    }

    QTextStream(stdout) << "IndexDetailRenderer smoke passed.\n";
    return 0;
}
