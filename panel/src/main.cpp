#include <QGuiApplication>
#include "panel.h"

int main(int argc, char* argv[]) {
    QGuiApplication app(argc, argv);
    Panel panel;
    return app.exec();
}
