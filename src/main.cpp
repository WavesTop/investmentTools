#include <QApplication>

#include "ui/MainWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
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
