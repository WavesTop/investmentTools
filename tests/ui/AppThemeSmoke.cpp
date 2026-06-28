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
    expect(widgetCss.contains("QFrame#sideNav"), name + " widget stylesheet contains side navigation rules");
    expect(widgetCss.contains("QFrame#topStatusBar"), name + " widget stylesheet contains top status bar rules");
    expect(widgetCss.contains("QFrame#configCard"), name + " widget stylesheet contains config card rules");
    expect(widgetCss.contains("QFrame#chatContextPanel"), name + " widget stylesheet contains chat context panel rules");
    expect(widgetCss.contains("QPushButton#quickQuestionButton"), name + " widget stylesheet contains quick question button rules");
    expect(widgetCss.contains(theme.winBg), name + " widget stylesheet contains window background");
    expect(widgetCss.contains(theme.btnBg), name + " widget stylesheet contains button background");

    expect(!htmlCss.isEmpty(), name + " HTML stylesheet is not empty");
    expect(htmlCss.contains("body"), name + " HTML stylesheet contains body rules");
    expect(htmlCss.contains("table.overview"), name + " HTML stylesheet contains overview table rules");
    expect(htmlCss.contains("table.fund"), name + " HTML stylesheet contains fund table rules");
    expect(htmlCss.contains("workspace-shell"), name + " HTML stylesheet contains workspace shell rules");
    expect(htmlCss.contains("metric-grid"), name + " HTML stylesheet contains metric grid rules");
    expect(htmlCss.contains("workspace-status-band"), name + " HTML stylesheet contains v2.5 status band rules");
    expect(htmlCss.contains("insight-card"), name + " HTML stylesheet contains v2.5 insight card rules");
    expect(htmlCss.contains("scan-table"), name + " HTML stylesheet contains v2.5 scan table rules");
    expect(htmlCss.contains("sector-detail-long"), name + " HTML stylesheet contains v2.5 long detail rules");
    expect(htmlCss.contains(theme.bodyBg), name + " HTML stylesheet contains body background");
    expect(htmlCss.contains(theme.linkColor), name + " HTML stylesheet contains link color");
}

} // namespace

int runChartRendererSmoke();
int runDashboardRendererSmoke();
int runSectorTableRendererSmoke();
int runStrategyRendererSmoke();
int runSectorDetailRendererSmoke();
int runIndexDetailRendererSmoke();
int runEventRadarRendererSmoke();

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    const auto light = InvestInsight::Ui::lightTheme();
    const auto dark = InvestInsight::Ui::darkTheme();

    verifyTheme(light, "light");
    verifyTheme(dark, "dark");

    expect(light.bodyBg != dark.bodyBg, "light and dark themes use different body backgrounds");
    expect(light.btnBg != dark.btnBg, "light and dark themes use different button backgrounds");
    expect(light.winBg == QStringLiteral("#F5F7FA"), "light theme uses v2.5 app background");
    expect(light.btnBg == QStringLiteral("#4258FF"), "light theme uses v2.5 primary accent");
    expect(light.paneBorder == QStringLiteral("#DDE3EC"), "light theme uses v2.5 default border");

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

    const int indexDetailResult = runIndexDetailRendererSmoke();
    if (indexDetailResult != 0) {
        ++failures;
    }

    const int eventRadarResult = runEventRadarRendererSmoke();
    if (eventRadarResult != 0) {
        ++failures;
    }

    if (failures > 0) {
        QTextStream(stderr) << failures << " UI smoke check(s) failed.\n";
        return 1;
    }

    QTextStream(stdout) << "AppTheme smoke passed.\n";
    return 0;
}
