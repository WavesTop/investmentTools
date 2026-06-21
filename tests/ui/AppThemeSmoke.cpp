#include "ui/AppTheme.h"

#include <QApplication>
#include <QTextStream>

namespace {

int failures = 0;

void expect(bool condition, const QString &message)
{
    if (condition) return;
    QTextStream(stderr) << "FAIL: " << message << '\n';
    ++failures;
}

void verifyTheme(const InvestInsight::Ui::ThemeColors &theme, const QString &name)
{
    const QString widgetCss = InvestInsight::Ui::buildWidgetStyleSheet(theme);
    const QString htmlCss = InvestInsight::Ui::buildHtmlCss(theme);

    expect(!widgetCss.isEmpty(), name + " widget stylesheet is not empty");
    expect(widgetCss.contains("QMainWindow"), name + " widget stylesheet contains main window rules");
    expect(widgetCss.contains("QTextBrowser"), name + " widget stylesheet contains text browser rules");
    expect(widgetCss.contains(theme.winBg), name + " widget stylesheet contains window background");
    expect(widgetCss.contains(theme.btnBg), name + " widget stylesheet contains button background");

    expect(!htmlCss.isEmpty(), name + " HTML stylesheet is not empty");
    expect(htmlCss.contains("body"), name + " HTML stylesheet contains body rules");
    expect(htmlCss.contains("table.overview"), name + " HTML stylesheet contains overview table rules");
    expect(htmlCss.contains("table.fund"), name + " HTML stylesheet contains fund table rules");
    expect(htmlCss.contains(theme.bodyBg), name + " HTML stylesheet contains body background");
    expect(htmlCss.contains(theme.linkColor), name + " HTML stylesheet contains link color");
}

} // namespace

int runChartRendererSmoke();
int runDashboardRendererSmoke();
int runSectorTableRendererSmoke();
int runStrategyRendererSmoke();
int runSectorDetailRendererSmoke();

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    const auto light = InvestInsight::Ui::lightTheme();
    const auto dark = InvestInsight::Ui::darkTheme();

    verifyTheme(light, "light");
    verifyTheme(dark, "dark");

    expect(light.bodyBg != dark.bodyBg, "light and dark themes use different body backgrounds");
    expect(light.btnBg != dark.btnBg, "light and dark themes use different button backgrounds");

    const int chartResult = runChartRendererSmoke();
    if (chartResult != 0) {
        ++failures;
    }

    const int dashboardResult = runDashboardRendererSmoke();
    if (dashboardResult != 0) {
        ++failures;
    }

    const int sectorTableResult = runSectorTableRendererSmoke();
    if (sectorTableResult != 0) {
        ++failures;
    }

    const int strategyResult = runStrategyRendererSmoke();
    if (strategyResult != 0) {
        ++failures;
    }

    const int sectorDetailResult = runSectorDetailRendererSmoke();
    if (sectorDetailResult != 0) {
        ++failures;
    }

    if (failures > 0) {
        QTextStream(stderr) << failures << " UI smoke check(s) failed.\n";
        return 1;
    }

    QTextStream(stdout) << "AppTheme smoke passed.\n";
    return 0;
}
