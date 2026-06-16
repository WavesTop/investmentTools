#include <QApplication>
#include <QCoreApplication>
#ifdef Q_OS_WIN
#include <QPalette>
#include <QSettings>
#include <QStyleFactory>
#endif

#include "ui/MainWindow.h"

int main(int argc, char *argv[])
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif

    QApplication app(argc, argv);

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
