#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QEvent>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTabWidget>
#include <QTextStream>
#ifdef Q_OS_WIN
#include <QPalette>
#include <QSettings>
#include <QStyleFactory>
#endif

#include "core/EventExtractionEngine.h"
#include "core/EventRuleBook.h"
#include "core/ImpactGraphEngine.h"
#include "core/SectorImpactAnalyzer.h"
#include "core/SectorFetcher.h"
#include "ui/MainWindow.h"

namespace {

bool hasArgument(int argc, char *argv[], const QString &flag)
{
    for (int i = 1; i < argc; ++i) {
        if (QString::fromLocal8Bit(argv[i]) == flag) return true;
    }
    return false;
}

QString argumentAfter(int argc, char *argv[], const QString &flag)
{
    for (int i = 1; i + 1 < argc; ++i) {
        if (QString::fromLocal8Bit(argv[i]) == flag) {
            return QString::fromLocal8Bit(argv[i + 1]);
        }
    }
    return QString();
}

QIcon applicationIcon()
{
    QIcon icon(QStringLiteral(":/icons/app-icon.png"));
#ifdef Q_OS_MACOS
    if (icon.isNull()) {
        const QDir resourcesDir(QCoreApplication::applicationDirPath()
                                + QStringLiteral("/../Resources"));
        icon = QIcon(resourcesDir.filePath(QStringLiteral("app-icon.png")));
    }
#endif
    return icon;
}

QString checkpointSummary(const MacroEvent &event)
{
    QStringList checkpoints;
    for (const QString &checkpoint : event.checkpoint.split(QStringLiteral("/"), Qt::SkipEmptyParts)) {
        if (!checkpoint.trimmed().isEmpty()) checkpoints << checkpoint.trimmed();
    }
    for (const MacroEventCheckpoint &checkpoint : event.nextCheckpoints) {
        if (!checkpoint.name.trimmed().isEmpty()) checkpoints << checkpoint.name.trimmed();
    }
    checkpoints.removeDuplicates();
    return checkpoints.join(QStringLiteral(" / "));
}

double evidenceReliability(const MacroEvent &event)
{
    double sum = 0.0;
    int count = 0;
    for (const MacroEventEvidence &evidence : event.evidence) {
        if (evidence.reliability <= 0.0) continue;
        sum += evidence.reliability;
        ++count;
    }
    return count > 0 ? sum / count : event.confidence;
}

QString isoOrDash(const QDateTime &time)
{
    return time.isValid() ? time.toString(Qt::ISODate) : QStringLiteral("-");
}

int dumpEventRules(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QTextStream out(stdout);

    EventRuleBook ruleBook;
    for (const EventRule &rule : ruleBook.rules()) {
        out << "rule"
            << "\tkey=" << rule.key
            << "\ttype=" << toString(rule.type)
            << "\tregion=" << toString(rule.region)
            << "\tconfidence=" << QString::number(rule.confidence, 'f', 2)
            << "\tcheckpoint=" << rule.checkpoint
            << "\trequired=" << rule.requiredAny.join(QStringLiteral("|"))
            << "\tcontext=" << rule.contextAny.join(QStringLiteral("|"))
            << '\n';
    }
    return 0;
}

int debugEventImpact(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QTextStream out(stdout);
    QTextStream err(stderr);

    const QString text = argumentAfter(argc, argv, QStringLiteral("--debug-event-impact")).trimmed();
    if (text.isEmpty()) {
        err << "Usage: InvestInsight.exe --debug-event-impact \"event text\"\n";
        return 1;
    }

    EventExtractionEngine extractor;
    ImpactGraphEngine graph;
    SectorImpactAnalyzer analyzer;

    const QList<MacroEvent> events = extractor.extractFromText(
        text, QStringLiteral("debug"), QDateTime::currentDateTimeUtc());
    if (events.isEmpty()) {
        out << "NO_EVENT\n";
        return 0;
    }

    for (const MacroEvent &event : events) {
        const QList<SectorEventImpact> impacts = graph.analyze(event);
        const QDateTime now = QDateTime::currentDateTimeUtc();

        out << "event"
            << "\ttype=" << toString(event.type)
            << "\tstate=" << toString(event.state)
            << "\tregion=" << toString(event.region)
            << "\tcheckpoint=" << checkpointSummary(event)
            << "\tdetectedAt=" << isoOrDash(event.detectedAt)
            << "\tpublishedAt=" << isoOrDash(event.publishedAt)
            << "\texpectedAt=" << isoOrDash(event.expectedAt)
            << "\tconfirmedAt=" << isoOrDash(event.confirmedAt)
            << "\tevidenceReliability=" << QString::number(evidenceReliability(event), 'f', 2)
            << "\tnovelty=" << QString::number(event.novelty, 'f', 2)
            << "\timportance=" << QString::number(event.importance, 'f', 2)
            << "\ttitle=" << event.title
            << '\n';

        for (const SectorEventImpact &impact : impacts) {
            out << "sector=" << impact.sector
                << "\tdirection=" << toString(impact.direction)
                << "\trelation=" << toString(impact.relation)
                << "\thorizon=" << toString(impact.horizon)
                << "\tstrength=" << QString::number(impact.strength, 'f', 2)
                << "\tconfidence=" << QString::number(impact.confidence, 'f', 2)
                << "\tsourceReliability=" << QString::number(impact.sourceReliability, 'f', 2)
                << "\tnoveltyWeight=" << QString::number(impact.noveltyWeight, 'f', 2)
                << "\ttimeDecay=" << QString::number(impact.timeDecay, 'f', 2)
                << "\tlatestEvidenceAt=" << isoOrDash(impact.latestEvidenceAt)
                << "\tscore=" << QString::number(analyzer.scoreImpact(impact, now), 'f', 3)
                << "\tcondition=" << impact.condition
                << "\tpath=" << impact.path
                << '\n';
        }
    }

    return 0;
}

int dumpSectorChanges(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QTextStream out(stdout);

    const QStringList targets = {
        QString::fromUtf8("有色金属"),
        QString::fromUtf8("半导体"),
        QString::fromUtf8("锂电池")
    };

    SectorFetcher fetcher;
    const QList<SectorInfo> sectors = fetcher.fetchAll();
    for (const QString &target : targets) {
        const SectorInfo *matched = nullptr;
        for (const SectorInfo &sector : sectors) {
            if (sector.name == target || sector.name.contains(target) || target.contains(sector.name)) {
                matched = &sector;
                break;
            }
        }

        if (!matched) {
            out << target << "\tNOT_FOUND\n";
            continue;
        }

        out << matched->name
            << "\tchangePct=" << QString::number(matched->changePct, 'f', 4)
            << "\tvalid=" << (matched->changePctValid ? "true" : "false")
            << "\tsource=" << matched->sourceTag
            << "\tkline=" << matched->klineSource
            << "\tlastDate=" << matched->lastDataDate
            << '\n';
    }

    return 0;
}

bool containsTabText(QTabWidget *tabs, const QString &text)
{
    if (!tabs) return false;
    for (int i = 0; i < tabs->count(); ++i) {
        if (tabs->tabText(i).contains(text)) return true;
    }
    return false;
}

int runUiSmoke()
{
    QTextStream out(stdout);
    QTextStream err(stderr);

    MainWindow window;
    QStringList missing;

    const QList<QTabWidget *> tabWidgets = window.findChildren<QTabWidget *>();
    const QList<QPushButton *> buttons = window.findChildren<QPushButton *>();
    const QList<QLabel *> labels = window.findChildren<QLabel *>();
    auto hasTab = [&tabWidgets](const QString &text) {
        for (QTabWidget *tabs : tabWidgets) {
            if (containsTabText(tabs, text)) return true;
        }
        return false;
    };
    auto hasLabel = [&labels](const QString &text) {
        for (QLabel *label : labels) {
            if (label->text().contains(text)) return true;
        }
        return false;
    };
    auto hasButton = [&buttons](const QString &text) {
        for (QPushButton *button : buttons) {
            if (button->text().contains(text)) return true;
        }
        return false;
    };
    auto hasButtonWithObjectName = [&buttons](const QString &text, const QString &objectName) {
        for (QPushButton *button : buttons) {
            if (button->objectName() == objectName && button->text().contains(text)) return true;
        }
        return false;
    };
    auto visibleTopLevelTabs = [&tabWidgets]() {
        int count = 0;
        for (QTabWidget *tabs : tabWidgets) {
            if (tabs->parentWidget() && tabs->parentWidget()->objectName() == QStringLiteral("workspacePane")
                && tabs->tabBar() && tabs->tabBar()->isVisible()) {
                ++count;
            }
        }
        return count;
    };
    auto clickButton = [&buttons](const QString &text) {
        for (QPushButton *button : buttons) {
            if (button->text().contains(text)) {
                button->click();
                return true;
            }
        }
        return false;
    };

    const QStringList requiredButtons = {
        QString::fromUtf8("开始分析"),
        QString::fromUtf8("AI 助手"),
        QString::fromUtf8("配置"),
        QString::fromUtf8("总览"),
        QString::fromUtf8("事件雷达"),
        QString::fromUtf8("板块机会"),
        QString::fromUtf8("策略跟踪")
    };
    for (const QString &button : requiredButtons) {
        if (!hasButton(button)) missing << QString::fromUtf8("button:") + button;
    }
    if (!hasLabel(QStringLiteral("InvestInsight"))) missing << "label:InvestInsight";
    if (!hasLabel(QString::fromUtf8("后台刷新与提醒"))) missing << QString::fromUtf8("label:后台刷新与提醒");
    if (!hasLabel(QString::fromUtf8("数据源健康"))) missing << QString::fromUtf8("label:数据源健康");
    if (hasButtonWithObjectName(QString::fromUtf8("AI 助手"), QStringLiteral("secondaryBtn"))) {
        missing << QString::fromUtf8("top-shortcut:AI 助手");
    }
    if (hasButtonWithObjectName(QString::fromUtf8("配置"), QStringLiteral("secondaryBtn"))) {
        missing << QString::fromUtf8("top-shortcut:配置");
    }
    if (visibleTopLevelTabs() > 0) {
        missing << QString::fromUtf8("visible-tabs:workspace");
    }

    if (clickButton(QString::fromUtf8("配置"))) {
        const QList<QLabel *> configLabels = window.findChildren<QLabel *>();
        auto hasConfigLabel = [&configLabels](const QString &text) {
            for (QLabel *label : configLabels) {
                if (label->text().contains(text)) return true;
            }
            return false;
        };
        if (!hasConfigLabel(QString::fromUtf8("AI 接入配置"))) missing << QString::fromUtf8("visible-label:AI 接入配置");
        if (!hasConfigLabel(QString::fromUtf8("我的持仓"))) missing << QString::fromUtf8("visible-label:我的持仓");

        window.clearDynamicResultPagesForRefresh();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        if (clickButton(QString::fromUtf8("配置"))) {
            const QList<QLabel *> preservedLabels = window.findChildren<QLabel *>();
            auto hasPreservedLabel = [&preservedLabels](const QString &text) {
                for (QLabel *label : preservedLabels) {
                    if (label->text().contains(text)) return true;
                }
                return false;
            };
            if (!hasPreservedLabel(QString::fromUtf8("AI 接入配置"))) {
                missing << QString::fromUtf8("post-refresh-label:AI 接入配置");
            }
            if (!hasPreservedLabel(QString::fromUtf8("我的持仓"))) {
                missing << QString::fromUtf8("post-refresh-label:我的持仓");
            }
        } else {
            missing << QString::fromUtf8("click:配置-after-refresh");
        }
    } else {
        missing << QString::fromUtf8("click:配置");
    }

    if (clickButton(QString::fromUtf8("AI 助手"))) {
        const QList<QLabel *> chatLabels = window.findChildren<QLabel *>();
        auto hasChatLabel = [&chatLabels](const QString &text) {
            for (QLabel *label : chatLabels) {
                if (label->text().contains(text)) return true;
            }
            return false;
        };
        if (!hasChatLabel(QString::fromUtf8("当前上下文"))) missing << QString::fromUtf8("label:当前上下文");
        if (!hasChatLabel(QString::fromUtf8("快捷问题"))) missing << QString::fromUtf8("label:快捷问题");
    } else {
        missing << QString::fromUtf8("click:AI 助手");
    }

    if (!missing.isEmpty()) {
        err << "Main window smoke failed. Missing: " << missing.join(", ") << '\n';
        return 1;
    }

    out << "Main window smoke passed.\n";
    return 0;
}

} // namespace

int main(int argc, char *argv[])
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif

    if (hasArgument(argc, argv, "--dump-sector-changes")) {
        return dumpSectorChanges(argc, argv);
    }
    if (hasArgument(argc, argv, "--dump-event-rules")) {
        return dumpEventRules(argc, argv);
    }
    if (hasArgument(argc, argv, "--debug-event-impact")) {
        return debugEventImpact(argc, argv);
    }

    QApplication app(argc, argv);
    const QIcon icon = applicationIcon();
    if (!icon.isNull()) {
        app.setWindowIcon(icon);
    }

#ifdef Q_OS_WIN
    app.setStyle(QStyleFactory::create("Fusion"));

    QSettings reg("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows"
                  "\\CurrentVersion\\Themes\\Personalize",
                  QSettings::NativeFormat);
    if (reg.value("AppsUseLightTheme", 1).toInt() == 0) {
        QPalette p;
        p.setColor(QPalette::Window,          QColor(30, 41, 59));
        p.setColor(QPalette::WindowText,      QColor(226, 232, 240));
        p.setColor(QPalette::Base,            QColor(15, 23, 42));
        p.setColor(QPalette::AlternateBase,   QColor(30, 41, 59));
        p.setColor(QPalette::Text,            QColor(226, 232, 240));
        p.setColor(QPalette::Button,          QColor(30, 41, 59));
        p.setColor(QPalette::ButtonText,      QColor(226, 232, 240));
        p.setColor(QPalette::BrightText,      Qt::white);
        p.setColor(QPalette::Highlight,       QColor(99, 102, 241));
        p.setColor(QPalette::HighlightedText, Qt::white);
        p.setColor(QPalette::ToolTipBase,     QColor(30, 41, 59));
        p.setColor(QPalette::ToolTipText,     QColor(226, 232, 240));
        app.setPalette(p);
    }
#endif

    if (hasArgument(argc, argv, "--ui-smoke")) {
        return runUiSmoke();
    }

    MainWindow window;
    window.show();

    for (int i = 1; i < argc; ++i) {
        if (QString(argv[i]) == "--auto-analyze") {
            window.autoAnalyze();
            break;
        }
    }

    return app.exec();
}
