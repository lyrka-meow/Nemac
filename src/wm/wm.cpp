#include "wm.h"
#include "../compositor/compositor.h"
#include "../monitor/monitor.h"
#include <X11/Xutil.h>
#include <X11/XKBlib.h>
#include <X11/keysym.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <algorithm>
#include <sys/time.h>

WM::WM(Display* dpy, Window root)
    : _dpy(dpy), _root(root) {}

WM::~WM() {}

void WM::init(const std::vector<MonitorInfo>& monitors, Compositor* comp) {
    _monitors = monitors;
    _comp = comp;
    _primary = Monitor::find_primary(_monitors);

    _zones.resize(_monitors.size());
    for (int i = 0; i < (int)_monitors.size(); i++) {
        auto& m = _monitors[i];
        int bar_h = comp ? 32 : 0;
        _zones[i].x     = m.x;
        _zones[i].y     = m.y + bar_h;
        _zones[i].w     = m.width;
        _zones[i].h     = m.height - bar_h;
        _zones[i].def_x = _zones[i].x;
        _zones[i].def_y = _zones[i].y;
        _zones[i].def_w = _zones[i].w;
        _zones[i].def_h = _zones[i].h;
        _zones[i].monitor_idx = i;
    }
}

const MonitorInfo& WM::primary_mon() const {
    return _monitors[_primary];
}

int WM::find_win(Window w) const {
    for (int i = 0; i < (int)_wins.size(); i++)
        if (_wins[i].xwin == w) return i;
    return -1;
}

void WM::spawn(const char* cmd) {
    if (fork() == 0) { setsid(); execlp("sh", "sh", "-c", cmd, nullptr); _exit(1); }
}

void WM::kill_win(int idx) {
    if (idx < 0) return;
    Atom wp = XInternAtom(_dpy, "WM_PROTOCOLS", False);
    Atom wd = XInternAtom(_dpy, "WM_DELETE_WINDOW", False);
    Atom* pr; int np;
    if (XGetWMProtocols(_dpy, _wins[idx].xwin, &pr, &np)) {
        for (int i = 0; i < np; i++) {
            if (pr[i] == wd) {
                XClientMessageEvent e = {};
                e.type = ClientMessage;
                e.window = _wins[idx].xwin;
                e.message_type = wp;
                e.format = 32;
                e.data.l[0] = wd;
                e.data.l[1] = CurrentTime;
                XSendEvent(_dpy, _wins[idx].xwin, False, NoEventMask, (XEvent*)&e);
                XFree(pr);
                return;
            }
        }
        XFree(pr);
    }
    XKillClient(_dpy, _wins[idx].xwin);
}

/* ======== Зоны ======== */

void WM::update_zones() {
    for (auto& z : _zones) {
        z.x = z.def_x;
        z.y = z.def_y;
        z.w = z.def_w;
        z.h = z.def_h;
    }

    for (auto& w : _wins) {
        if (w.unmanaged || w.pinned || w.fullscreen) continue;
        for (auto& z : _zones) {
            if (w.x < z.x) {
                int diff = z.x - w.x;
                z.x -= diff;
                z.w += diff;
            }
            if (w.y < z.y) {
                int diff = z.y - w.y;
                z.y -= diff;
                z.h += diff;
            }
            if (w.x + w.w > z.x + z.w) {
                z.w = (w.x + w.w) - z.x;
            }
            if (w.y + w.h > z.y + z.h) {
                z.h = (w.y + w.h) - z.y;
            }
        }
    }
}

/* ======== Закреп и фуллскрин ======== */

void WM::pin_focused() {
    if (_focused < 0 || _focused >= (int)_wins.size()) return;
    auto& w = _wins[_focused];
    w.pinned = !w.pinned;
    if (w.pinned) {
        auto& m = primary_mon();
        w.x = m.x + (m.width - w.w) / 2;
        w.y = m.y + (m.height - w.h) / 2;
        XRaiseWindow(_dpy, w.xwin);
    }
    layout();
}

void WM::fullscreen_focused() {
    if (_focused < 0 || _focused >= (int)_wins.size()) return;
    auto& w = _wins[_focused];
    w.fullscreen = !w.fullscreen;
    if (w.fullscreen) {
        auto& m = primary_mon();
        w.x = m.x;
        w.y = m.y;
        w.w = m.width;
        w.h = m.height;
        XSetWindowBorderWidth(_dpy, w.xwin, 0);
        XRaiseWindow(_dpy, w.xwin);
    } else {
        w.w = DEFAULT_WIN_W;
        w.h = DEFAULT_WIN_H;
        auto& m = primary_mon();
        w.x = m.x + (m.width - w.w) / 2;
        w.y = m.y + (m.height - w.h) / 2;
        XSetWindowBorderWidth(_dpy, w.xwin, BORDER);
    }
    layout();
}

void WM::switch_primary(int dir) {
    if (_monitors.size() <= 1) return;
    _primary = (_primary + dir + (int)_monitors.size()) % (int)_monitors.size();
    for (auto& m : _monitors) m.primary = false;
    _monitors[_primary].primary = true;
    fprintf(stderr, "nemac: главный монитор -> %d\n", _primary);
    layout();
}

/* ======== Layout ======== */

void WM::layout() {
    update_zones();

    for (auto& w : _wins) {
        if (w.unmanaged) continue;
        if (w.fullscreen) {
            XMoveResizeWindow(_dpy, w.xwin, w.x, w.y, w.w, w.h);
            continue;
        }
        if (w.pinned) {
            auto& m = primary_mon();
            w.x = m.x + (m.width - w.w) / 2;
            w.y = m.y + (m.height - w.h) / 2;
        }
        XMoveResizeWindow(_dpy, w.xwin, w.x, w.y, w.w, w.h);
    }

    if (_comp) {
        auto& m = primary_mon();
        _comp->render(_wins, _view_x, _view_y, m.x, m.y, m.width, m.height);
    }
    XFlush(_dpy);
}

/* ======== Фокус ======== */

void WM::set_focus(int idx) {
    if (_focused >= 0 && _focused < (int)_wins.size())
        XSetWindowBorder(_dpy, _wins[_focused].xwin, NORMAL_BORDER);
    _focused = idx;
    if (_focused < 0 || _focused >= (int)_wins.size()) return;
    XSetWindowBorder(_dpy, _wins[_focused].xwin, FOCUSED_BORDER);
    XRaiseWindow(_dpy, _wins[_focused].xwin);
    XSetInputFocus(_dpy, _wins[_focused].xwin, RevertToPointerRoot, CurrentTime);
    layout();
}

/* ======== Обработчики событий ======== */

void WM::handle_map_request(XMapRequestEvent* e) {
    if (find_win(e->window) >= 0) return;
    XWindowAttributes wa;
    if (!XGetWindowAttributes(_dpy, e->window, &wa) || wa.override_redirect) return;

    WinInfo nw;
    nw.xwin = e->window;
    nw.w    = std::min(wa.width  > 1 ? wa.width  : DEFAULT_WIN_W, primary_mon().width - BORDER * 2);
    nw.h    = std::min(wa.height > 1 ? wa.height : DEFAULT_WIN_H, primary_mon().height - BORDER * 2);

    auto& m = primary_mon();
    nw.x = m.x + (m.width - nw.w) / 2;
    nw.y = m.y + (m.height - nw.h) / 2;

    _wins.push_back(nw);

    XSetWindowBorderWidth(_dpy, e->window, BORDER);
    XSelectInput(_dpy, e->window, FocusChangeMask | PropertyChangeMask);
    XGrabButton(_dpy, Button1, AnyModifier, e->window, False,
                ButtonPressMask, GrabModeSync, GrabModeAsync, None, None);
    XGrabButton(_dpy, Button1, MOD, e->window, False,
                ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
                GrabModeAsync, GrabModeAsync, None, None);
    XGrabButton(_dpy, Button3, MOD, e->window, False,
                ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
                GrabModeAsync, GrabModeAsync, None, None);
    XMapWindow(_dpy, e->window);

    if (_comp) _comp->add_window(_wins.back());
    set_focus((int)_wins.size() - 1);
}

void WM::handle_destroy(Window w) {
    int idx = find_win(w);
    if (idx < 0) return;
    bool was_unmanaged = _wins[idx].unmanaged;
    if (_comp) _comp->remove_window(_wins[idx]);
    _wins.erase(_wins.begin() + idx);
    if (was_unmanaged) {
        if (_comp) {
            auto& m = primary_mon();
            _comp->render(_wins, _view_x, _view_y, m.x, m.y, m.width, m.height);
        }
        return;
    }
    if (_focused > idx) _focused--;
    if (_focused >= (int)_wins.size()) _focused = (int)_wins.size() - 1;
    if (!_wins.empty()) set_focus(_focused);
    else { _focused = -1; layout(); }
}

void WM::handle_key(XKeyEvent* e) {
    KeySym sym = XkbKeycodeToKeysym(_dpy, e->keycode, 0, 0);
    unsigned int mod = e->state & ~(LockMask | Mod2Mask | Mod3Mask | Mod5Mask);

    if (mod == MOD) {
        if (sym == XK_Return) { spawn("xterm"); return; }
        if (sym == XK_q)      { kill_win(_focused); return; }
        if (sym == XK_p)      { pin_focused(); return; }
        if (sym == XK_f)      { fullscreen_focused(); return; }
    }
    if (mod == (MOD | ShiftMask)) {
        if (sym == XK_q)      { XCloseDisplay(_dpy); exit(0); }
        if (sym == XK_Left)   { switch_primary(-1); return; }
        if (sym == XK_Right)  { switch_primary(1); return; }
    }
}

void WM::handle_button_press(XButtonEvent* e) {
    if (e->button == Button1 && (e->state & MOD)) {
        int idx = find_win(e->window);
        if (idx >= 0) {
            set_focus(idx);
            XWindowAttributes wa;
            XGetWindowAttributes(_dpy, e->window, &wa);
            _wins[idx].x = wa.x;
            _wins[idx].y = wa.y;
            _move_drag = {true, idx, e->x_root, e->y_root,
                          _wins[idx].x, _wins[idx].y, 0, 0, 0, 0};
            XRaiseWindow(_dpy, e->window);
            if (_comp) _comp->set_dragging(true);
        }
        return;
    }

    if (e->button == Button3 && (e->state & MOD)) {
        int idx = find_win(e->window);
        if (idx >= 0) {
            set_focus(idx);
            XWindowAttributes wa;
            XGetWindowAttributes(_dpy, e->window, &wa);
            _wins[idx].x = wa.x;
            _wins[idx].y = wa.y;
            _wins[idx].w = wa.width;
            _wins[idx].h = wa.height;
            _resize_drag = {
                true, idx,
                e->x_root, e->y_root,
                _wins[idx].x, _wins[idx].y,
                wa.width, wa.height,
                (e->x_root - wa.x) < (wa.width / 2) ? 1 : 0,
                (e->y_root - wa.y) < (wa.height / 2) ? 1 : 0
            };
            XRaiseWindow(_dpy, e->window);
            if (_comp) _comp->set_dragging(true);
        }
        return;
    }

    int idx = find_win(e->window);
    if (idx >= 0) set_focus(idx);
    XAllowEvents(_dpy, ReplayPointer, CurrentTime);
}

void WM::handle_button_release(XButtonEvent* e) {
    (void)e;
    if (_move_drag.active) {
        _move_drag = {};
        if (_comp) _comp->set_dragging(false);
        update_zones();
    }
    if (_resize_drag.active) {
        _resize_drag = {};
        if (_comp) _comp->set_dragging(false);
        update_zones();
    }
}

void WM::handle_motion(XMotionEvent* e) {
    if (_move_drag.active && _move_drag.idx >= 0) {
        auto& w = _wins[_move_drag.idx];
        w.x = _move_drag.ox + (e->x_root - _move_drag.sx);
        w.y = _move_drag.oy + (e->y_root - _move_drag.sy);
        XMoveWindow(_dpy, w.xwin, w.x, w.y);
        if (_comp) {
            static struct timeval lm = {0, 0};
            struct timeval now;
            gettimeofday(&now, nullptr);
            double el = (now.tv_sec - lm.tv_sec) + (now.tv_usec - lm.tv_usec) * 1e-6;
            if (el >= 0.016) {
                auto& m = primary_mon();
                _comp->render(_wins, _view_x, _view_y, m.x, m.y, m.width, m.height);
                lm = now;
            }
        }
        return;
    }

    if (_resize_drag.active && _resize_drag.idx >= 0) {
        auto& w = _wins[_resize_drag.idx];
        int dx = e->x_root - _resize_drag.sx;
        int dy = e->y_root - _resize_drag.sy;
        int nx = _resize_drag.ox, ny = _resize_drag.oy;
        int nw = _resize_drag.ow, nh = _resize_drag.oh;
        if (_resize_drag.left) { nx = _resize_drag.ox + dx; nw = _resize_drag.ow - dx; }
        else                   { nw = _resize_drag.ow + dx; }
        if (_resize_drag.top)  { ny = _resize_drag.oy + dy; nh = _resize_drag.oh - dy; }
        else                   { nh = _resize_drag.oh + dy; }
        if (nw < 80)  { nw = 80;  if (_resize_drag.left) nx = _resize_drag.ox + _resize_drag.ow - 80; }
        if (nh < 80)  { nh = 80;  if (_resize_drag.top)  ny = _resize_drag.oy + _resize_drag.oh - 80; }
        w.x = nx; w.y = ny; w.w = nw; w.h = nh;
        XMoveResizeWindow(_dpy, w.xwin, nx, ny, nw, nh);
        if (_comp) {
            static struct timeval lr = {0, 0};
            struct timeval now;
            gettimeofday(&now, nullptr);
            double el = (now.tv_sec - lr.tv_sec) + (now.tv_usec - lr.tv_usec) * 1e-6;
            if (el >= 0.016) {
                auto& m = primary_mon();
                _comp->render(_wins, _view_x, _view_y, m.x, m.y, m.width, m.height);
                lr = now;
            }
        }
    }
}

void WM::handle_configure_request(XConfigureRequestEvent* cr) {
    XWindowChanges wc = {cr->x, cr->y, cr->width, cr->height,
                         cr->border_width, cr->above, cr->detail};
    XConfigureWindow(_dpy, cr->window, cr->value_mask, &wc);
}

void WM::handle_configure_notify(XConfigureEvent* e) {
    int idx = find_win(e->window);
    if (idx >= 0 && _wins[idx].unmanaged) {
        _wins[idx].x = e->x;
        _wins[idx].y = e->y;
        _wins[idx].w = e->width;
        _wins[idx].h = e->height;
    }
}
