#include "PanelBridge.h"
#include <QScreen>
#include <QGuiApplication>
#include <cstdio>

#include <X11/Xlib.h>
#include <X11/Xatom.h>

PanelBridge::PanelBridge(QObject* parent) : QObject(parent) {
    initX();
}

PanelBridge::~PanelBridge() {
    if (_xdpy) XCloseDisplay(_xdpy);
}

void PanelBridge::toggle() {
    _open = !_open;
    if (_open) reposition(420, 520);
    emit panelOpenChanged();
}

void PanelBridge::reposition(int winW, int winH) {
    (void)winH;
    auto* scr = QGuiApplication::primaryScreen();
    if (!scr) return;
    auto geo = scr->geometry();
    _px = geo.x() + (geo.width() - winW) / 2;
    _py = geo.y() + 40;
    emit geometryChanged();
}

void PanelBridge::initX() {
    _xdpy = XOpenDisplay(nullptr);
    if (!_xdpy) { fprintf(stderr, "panel: XOpenDisplay failed\n"); return; }

    _atom = XInternAtom(_xdpy, "_NEMAC_PANEL_TOGGLE", False);

    Window root = DefaultRootWindow(_xdpy);
    Atom actual; int fmt; unsigned long n, after; unsigned char* data = nullptr;
    if (XGetWindowProperty(_xdpy, root, _atom, 0, 1, False,
            XA_CARDINAL, &actual, &fmt, &n, &after, &data) == Success && data) {
        _counter = *(long*)data;
        XFree(data);
    }
    XSelectInput(_xdpy, root, PropertyChangeMask);

    _poll = new QTimer(this);
    connect(_poll, &QTimer::timeout, this, &PanelBridge::pollX);
    _poll->start(50);
}

void PanelBridge::pollX() {
    if (!_xdpy) return;
    while (XPending(_xdpy)) {
        XEvent ev;
        XNextEvent(_xdpy, &ev);
        if (ev.type == PropertyNotify &&
            (unsigned long)ev.xproperty.atom == _atom) {
            Window root = DefaultRootWindow(_xdpy);
            Atom actual; int fmt; unsigned long n, after; unsigned char* data = nullptr;
            if (XGetWindowProperty(_xdpy, root, (Atom)_atom, 0, 1, False,
                    XA_CARDINAL, &actual, &fmt, &n, &after, &data) == Success && data) {
                long val = *(long*)data;
                XFree(data);
                if (val != _counter) {
                    _counter = val;
                    emit toggleRequested();
                    toggle();
                }
            }
        }
    }
}
