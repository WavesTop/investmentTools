#include <QApplication>
#include <QCoreApplication>
#include <QIcon>
#include <QPushButton>
#include <QTabWidget>
#include <QTextStream>
#ifdef Q_OS_WIN
#include <QPalette>
#include <QSettings>
#include <QStyleFactory>
#endif

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
    auto hasTab = [&tabWidgets](const QString &text) {
        for (QTabWidget *tabs : tabWidgets) {
            if (containsTabText(tabs, text)) return true;
        }
        return false;
    };

    const QStringList requiredTabs = {
        QString::fromUtf8("总览工作台"),
        QString::fromUtf8("总览"),
        QString::fromUtf8("板块机会"),
        QString::fromUtf8("策略跟踪")
    };
    for (const QString &tab : requiredTabs) {
        if (!hasTab(tab)) missing << QString::fromUtf8("tab:") + tab;
    }

    const QList<QPushButton *> buttons = window.findChildren<QPushButton *>();
    auto hasButton = [&buttons](const QString &text) {
        for (QPushButton *button : buttons) {
            if (button->text().contains(text)) return true;
        }
        return false;
    };

    const QStringList requiredButtons = {
        QString::fromUtf8("开始分析"),
        QString::fromUtf8("AI 助手"),
        QString::fromUtf8("配置中心")
    };
    for (const QString &button : requiredButtons) {
        if (!hasButton(button)) missing << QString::fromUtf8("button:") + button;
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

    QApplication app(argc, argv);
    app.setWindowIcon(QIcon(":/icons/app-icon.png"));

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
