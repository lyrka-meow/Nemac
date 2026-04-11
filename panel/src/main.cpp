#include <QGuiApplication>
#include <QQmlContext>
#include <QTimer>
#include "panel.h"
#include "mpris.h"

int main(int argc, char* argv[]) {
    QGuiApplication app(argc, argv);
    MprisClient mpris;
    Panel panel;
    panel.rootContext()->setContextProperty("mpris", &mpris);
    panel.rootContext()->setContextProperty("panel", &panel);
    panel.load();
    QTimer::singleShot(2000, &mpris, &MprisClient::init);
    return app.exec();
}
