#include "panel.h"
#include <QGuiApplication>
#include <QScreen>
#include <QSurfaceFormat>
#include <cstring>
#include <cstdlib>
#include <xcb/xcb.h>

static xcb_atom_t intern(xcb_connection_t* c, const char* name) {
    auto cookie = xcb_intern_atom(c, 0, strlen(name), name);
    auto* reply = xcb_intern_atom_reply(c, cookie, nullptr);
    xcb_atom_t atom = reply ? reply->atom : XCB_NONE;
    free(reply);
    return atom;
}

Panel::Panel(QWindow* parent) : QQuickView(parent) {
    QSurfaceFormat fmt;
    fmt.setAlphaBufferSize(8);
    setFormat(fmt);

    setFlags(Qt::FramelessWindowHint | Qt::WindowDoesNotAcceptFocus);
    setColor(Qt::transparent);

    auto* screen = QGuiApplication::primaryScreen();
    setGeometry(screen->geometry().x(), screen->geometry().y(),
                screen->geometry().width(), HEIGHT);

    setResizeMode(SizeRootObjectToView);
    setSource(QUrl("qrc:/qml/main.qml"));

    create();
    applyDockHints();
    show();
}

void Panel::applyDockHints() {
    auto* x11 = qApp->nativeInterface<QNativeInterface::QX11Application>();
    if (!x11) return;

    xcb_connection_t* c = x11->connection();
    xcb_window_t w = static_cast<xcb_window_t>(winId());

    xcb_atom_t type = intern(c, "_NET_WM_WINDOW_TYPE");
    xcb_atom_t dock = intern(c, "_NET_WM_WINDOW_TYPE_DOCK");
    xcb_change_property(c, XCB_PROP_MODE_REPLACE, w, type,
                        XCB_ATOM_ATOM, 32, 1, &dock);

    xcb_atom_t strut_partial = intern(c, "_NET_WM_STRUT_PARTIAL");
    uint32_t struts[12] = {};
    struts[2] = HEIGHT;
    struts[8] = geometry().x();
    struts[9] = geometry().x() + geometry().width() - 1;
    xcb_change_property(c, XCB_PROP_MODE_REPLACE, w, strut_partial,
                        XCB_ATOM_CARDINAL, 32, 12, struts);

    xcb_atom_t strut_atom = intern(c, "_NET_WM_STRUT");
    uint32_t simple[4] = {0, 0, (uint32_t)HEIGHT, 0};
    xcb_change_property(c, XCB_PROP_MODE_REPLACE, w, strut_atom,
                        XCB_ATOM_CARDINAL, 32, 4, simple);

    xcb_flush(c);
}
