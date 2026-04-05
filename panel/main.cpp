#include "PanelBridge.h"
#include "MprisBackend.h"
#include "FileModel.h"
#include "WallpaperModel.h"
#include "PackageBackend.h"
#include "TermBackend.h"
#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("nemac-panel");
    QQuickStyle::setStyle("Basic");

    PanelBridge    bridge;
    MprisBackend   mpris;
    FileModel      fileModel;
    WallpaperModel wallpapers;
    PackageBackend packages;
    TermBackend    term;

    QQmlApplicationEngine engine;
    auto* ctx = engine.rootContext();
    ctx->setContextProperty("bridge",    &bridge);
    ctx->setContextProperty("mpris",     &mpris);
    ctx->setContextProperty("fileModel", &fileModel);
    ctx->setContextProperty("wallpapers",&wallpapers);
    ctx->setContextProperty("packages",  &packages);
    ctx->setContextProperty("term",      &term);

    engine.load(QUrl("qrc:/qml/Main.qml"));
    if (engine.rootObjects().isEmpty()) return 1;

    return app.exec();
}
