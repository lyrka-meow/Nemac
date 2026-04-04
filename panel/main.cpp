#include "PanelWindow.h"
#include <QApplication>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("nemac-panel");

    PanelWindow panel;
    return app.exec();
}
