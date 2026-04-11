#include "panel.h"
#include <QGuiApplication>
#include <QScreen>
#include <QSurfaceFormat>
#include <QQmlContext>
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
    _screenX = screen->geometry().x();
    _screenW = screen->geometry().width();
    setGeometry(_screenX, screen->geometry().y(), _screenW, BAR_H);

    setResizeMode(SizeRootObjectToView);
}

void Panel::load() {
    setSource(QUrl("qrc:/qml/main.qml"));
    create();

    auto* x11 = qApp->nativeInterface<QNativeInterface::QX11Application>();
    if (x11)
        applyDockHints(x11->connection(), static_cast<xcb_window_t>(winId()), true);

    show();
}

void Panel::applyDockHints(xcb_connection_t* c, xcb_window_t w, bool withStrut) {
    xcb_atom_t type = intern(c, "_NET_WM_WINDOW_TYPE");
    xcb_atom_t dock = intern(c, "_NET_WM_WINDOW_TYPE_DOCK");
    xcb_change_property(c, XCB_PROP_MODE_REPLACE, w, type,
                        XCB_ATOM_ATOM, 32, 1, &dock);

    if (withStrut) {
        xcb_atom_t strut_partial = intern(c, "_NET_WM_STRUT_PARTIAL");
        uint32_t struts[12] = {};
        struts[2] = BAR_H;
        struts[8] = _screenX;
        struts[9] = _screenX + _screenW - 1;
        xcb_change_property(c, XCB_PROP_MODE_REPLACE, w, strut_partial,
                            XCB_ATOM_CARDINAL, 32, 12, struts);

        xcb_atom_t strut_atom = intern(c, "_NET_WM_STRUT");
        uint32_t simple[4] = {0, 0, (uint32_t)BAR_H, 0};
        xcb_change_property(c, XCB_PROP_MODE_REPLACE, w, strut_atom,
                            XCB_ATOM_CARDINAL, 32, 4, simple);
    }

    xcb_flush(c);
}

void Panel::setPopupOpen(bool open) {
    int cx = geometry().x();
    int cw = geometry().width();

    if (open && !_popup) {
        _popup = new QQuickView;
        QSurfaceFormat fmt;
        fmt.setAlphaBufferSize(8);
        _popup->setFormat(fmt);
        _popup->setFlags(Qt::FramelessWindowHint | Qt::WindowDoesNotAcceptFocus);
        _popup->setColor(Qt::transparent);
        _popup->setGeometry(cx + cw - POPUP_W - 8, BAR_H + 4,
                            POPUP_W, POPUP_H);
        _popup->setResizeMode(QQuickView::SizeRootObjectToView);
        _popup->rootContext()->setContextProperty("mpris",
            rootContext()->contextProperty("mpris"));
        _popup->rootContext()->setContextProperty("panel",
            rootContext()->contextProperty("panel"));
        _popup->setSource(QUrl("qrc:/qml/PlayerPopup.qml"));
        _popup->create();
        auto* x11 = qApp->nativeInterface<QNativeInterface::QX11Application>();
        if (x11)
            applyDockHints(x11->connection(),
                           static_cast<xcb_window_t>(_popup->winId()), false);
        _popup->show();
    } else if (open && _popup) {
        _popup->setGeometry(cx + cw - POPUP_W - 8, BAR_H + 4,
                            POPUP_W, POPUP_H);
        _popup->show();
    } else if (!open && _popup) {
        _popup->hide();
    }
}
