#include "mainwindow.h"
#include <QApplication>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("SFC");
    app.setApplicationVersion("1.0");
    app.setOrganizationName("SFC");

    MainWindow w;
    w.show();
    return app.exec();
}
