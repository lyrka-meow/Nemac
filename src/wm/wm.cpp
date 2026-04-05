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
#include <climits>
#include <sys/time.h>

WM::WM(Display* dpy, Window root)
    : _dpy(dpy), _root(root) {}

WM::~WM() {}

void WM::init(const std::vector<MonitorInfo>& monitors, Compositor* comp) {
    _monitors = monitors;
    _comp = comp;
    _primary = Monitor::find_primary(_monitors);

    int bar_h = comp ? 32 : 0;
    int bx0 = INT_MAX, by0 = INT_MAX, bx1 = INT_MIN, by1 = INT_MIN;
    for (auto& m : _monitors) {
        if (m.x < bx0) bx0 = m.x;
        if (m.y + bar_h < by0) by0 = m.y + bar_h;
        if (m.x + m.width > bx1) bx1 = m.x + m.width;
        if (m.y + m.height > by1) by1 = m.y + m.height;
    }
    _zones.resize(_monitors.size());
    for (int i = 0; i < (int)_monitors.size(); i++) {
        _zones[i].x     = bx0;
        _zones[i].y     = by0;
        _zones[i].w     = bx1 - bx0;
        _zones[i].h     = by1 - by0;
        _zones[i].def_x = bx0;
        _zones[i].def_y = by0;
        _zones[i].def_w = bx1 - bx0;
        _zones[i].def_h = by1 - by0;
        _zones[i].monitor_idx = i;
    }
    unsigned int mods[] = {0, LockMask, Mod2Mask, LockMask | Mod2Mask};
    for (auto x : mods)
        XGrabButton(_dpy, Button3, ShiftMask | x, _root, False,
                    ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
                    GrabModeAsync, GrabModeAsync, None, None);
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

void WM::update_zones() {
    for (auto& z : _zones) {
        z.x = z.def_x;
        z.y = z.def_y;
        z.w = z.def_w;
        z.h = z.def_h;
    }

    for (auto& w : _wins) {
        if (w.unmanaged || w.pinned || w.fullscreen || w.desktop) continue;
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
        w.x = m.x + _view_x;
        w.y = m.y + _view_y;
        w.w = m.width;
        w.h = m.height;
        XSetWindowBorderWidth(_dpy, w.xwin, 0);
        XRaiseWindow(_dpy, w.xwin);
    } else {
        w.w = DEFAULT_WIN_W;
        w.h = DEFAULT_WIN_H;
        auto& m = primary_mon();
        w.x = m.x + _view_x + (m.width - w.w) / 2;
        w.y = m.y + _view_y + (m.height - w.h) / 2;
        XSetWindowBorderWidth(_dpy, w.xwin, BORDER);
    }
    layout();
}

void WM::desktop_focused() {
    if (_focused < 0 || _focused >= (int)_wins.size()) return;
    auto& w = _wins[_focused];
    w.desktop = !w.desktop;
    if (w.desktop) {
        w.pre_desk_x = w.x; w.pre_desk_y = w.y;
        w.pre_desk_w = w.w; w.pre_desk_h = w.h;
        XSetWindowBorderWidth(_dpy, w.xwin, 0);
        XLowerWindow(_dpy, w.xwin);
    } else {
        w.x = w.pre_desk_x; w.y = w.pre_desk_y;
        w.w = w.pre_desk_w; w.h = w.pre_desk_h;
        XSetWindowBorderWidth(_dpy, w.xwin, BORDER);
        XRaiseWindow(_dpy, w.xwin);
    }
    layout();
}

void WM::clamp_view() {
    int zx0 = INT_MAX, zy0 = INT_MAX, zx1 = INT_MIN, zy1 = INT_MIN;
    for (auto& z : _zones) {
        if (z.x < zx0) zx0 = z.x;
        if (z.y < zy0) zy0 = z.y;
        if (z.x + z.w > zx1) zx1 = z.x + z.w;
        if (z.y + z.h > zy1) zy1 = z.y + z.h;
    }
    int mx0 = INT_MAX, my0 = INT_MAX, mx1 = INT_MIN, my1 = INT_MIN;
    for (auto& m : _monitors) {
        if (m.x < mx0) mx0 = m.x;
        if (m.y < my0) my0 = m.y;
        if (m.x + m.width > mx1) mx1 = m.x + m.width;
        if (m.y + m.height > my1) my1 = m.y + m.height;
    }
    int total_mw = mx1 - mx0;
    int total_mh = my1 - my0;
    int min_vx = zx0;
    int max_vx = zx1 - total_mw;
    int min_vy = zy0;
    int max_vy = zy1 - total_mh;
    if (min_vx > max_vx) min_vx = max_vx = 0;
    if (min_vy > max_vy) min_vy = max_vy = 0;
    if (_view_x < min_vx) _view_x = min_vx;
    if (_view_x > max_vx) _view_x = max_vx;
    if (_view_y < min_vy) _view_y = min_vy;
    if (_view_y > max_vy) _view_y = max_vy;
}

void WM::scroll_view(int dx, int dy) {
    _view_x += dx;
    _view_y += dy;
    clamp_view();
    layout();
}

void WM::auto_scroll_tick() {
    if (!_move_drag.active || _move_drag.idx < 0) return;

    static struct timeval last_t = {0, 0};
    struct timeval now;
    gettimeofday(&now, nullptr);
    double dt = (now.tv_sec - last_t.tv_sec) + (now.tv_usec - last_t.tv_usec) * 1e-6;
    if (dt < 0.001 || dt > 0.5) { last_t = now; return; }
    last_t = now;

    Window wr, wc;
    int rx, ry, wx, wy;
    unsigned int mask;
    if (!XQueryPointer(_dpy, _root, &wr, &wc, &rx, &ry, &wx, &wy, &mask)) return;

    int mx0 = INT_MAX, my0 = INT_MAX, mx1 = INT_MIN, my1 = INT_MIN;
    for (auto& m : _monitors) {
        if (m.x < mx0) mx0 = m.x;
        if (m.y < my0) my0 = m.y;
        if (m.x + m.width > mx1) mx1 = m.x + m.width;
        if (m.y + m.height > my1) my1 = m.y + m.height;
    }

    const int zone = 30;
    const float max_speed = 600.0f;
    auto calc = [&](int pos, int lo, int hi) -> float {
        if (pos < lo + zone) return -max_speed * (1.0f - (float)(pos - lo) / zone);
        if (pos > hi - zone) return  max_speed * (1.0f - (float)(hi - pos) / zone);
        return 0.0f;
    };
    float fx = calc(rx, mx0, mx1) * (float)dt;
    float fy = calc(ry, my0, my1) * (float)dt;
    int dx = (fx > 0) ? (int)(fx + 0.5f) : (int)(fx - 0.5f);
    int dy = (fy > 0) ? (int)(fy + 0.5f) : (int)(fy - 0.5f);
    if (dx == 0 && dy == 0) return;

    int old_vx = _view_x, old_vy = _view_y;
    _view_x += dx;
    _view_y += dy;
    clamp_view();
    int real_dx = _view_x - old_vx;
    int real_dy = _view_y - old_vy;
    if (real_dx == 0 && real_dy == 0) return;

    _wins[_move_drag.idx].x += real_dx;
    _wins[_move_drag.idx].y += real_dy;
    update_zones();
    for (auto& wi : _wins) {
        if (wi.unmanaged) continue;
        XMoveWindow(_dpy, wi.xwin, wi.x - _view_x, wi.y - _view_y);
    }
}

void WM::switch_primary(int dir) {
    if (_monitors.size() <= 1) return;
    _primary = (_primary + dir + (int)_monitors.size()) % (int)_monitors.size();
    for (auto& m : _monitors) m.primary = false;
    _monitors[_primary].primary = true;
    fprintf(stderr, "nemac: главный монитор -> %d\n", _primary);
    layout();
}

void WM::layout() {
    update_zones();

    for (auto& w : _wins) {
        if (w.unmanaged) continue;
        if (w.desktop) {
            XMoveResizeWindow(_dpy, w.xwin, w.x - _view_x, w.y - _view_y, w.w, w.h);
            XLowerWindow(_dpy, w.xwin);
            continue;
        }
        if (w.fullscreen) {
            XMoveResizeWindow(_dpy, w.xwin, w.x - _view_x, w.y - _view_y, w.w, w.h);
            continue;
        }
        if (w.pinned) {
            auto& m = primary_mon();
            w.x = m.x + _view_x + (m.width - w.w) / 2;
            w.y = m.y + _view_y + (m.height - w.h) / 2;
        }
        XMoveResizeWindow(_dpy, w.xwin, w.x - _view_x, w.y - _view_y, w.w, w.h);
    }

    if (_comp) {
        auto& m = primary_mon();
        _comp->render(_wins, _view_x, _view_y, m.width);
    }
    XFlush(_dpy);
}

void WM::set_focus(int idx) {
    if (_focused >= 0 && _focused < (int)_wins.size())
        XSetWindowBorder(_dpy, _wins[_focused].xwin, NORMAL_BORDER);
    _focused = idx;
    if (_focused < 0 || _focused >= (int)_wins.size()) return;
    auto& w = _wins[_focused];
    if (w.desktop) return;
    XSetWindowBorder(_dpy, w.xwin, FOCUSED_BORDER);
    XRaiseWindow(_dpy, w.xwin);
    XSetInputFocus(_dpy, w.xwin, RevertToPointerRoot, CurrentTime);
    layout();
}

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
    unsigned int smods[] = {0, LockMask, Mod2Mask, LockMask | Mod2Mask};
    for (auto x : smods)
        XGrabButton(_dpy, Button3, ShiftMask | x, e->window, False,
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
            _comp->render(_wins, _view_x, _view_y, m.width);
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
        if (sym == XK_Return) { spawn("alacritty"); return; }
        if (sym == XK_q)      { kill_win(_focused); return; }
        if (sym == XK_p)      { pin_focused(); return; }
        if (sym == XK_f)      { fullscreen_focused(); return; }
        if (sym == XK_d)      { desktop_focused(); return; }
        if (sym == XK_Left)   { scroll_view(-SCROLL_STEP, 0); return; }
        if (sym == XK_Right)  { scroll_view(SCROLL_STEP, 0); return; }
        if (sym == XK_Up)     { scroll_view(0, -SCROLL_STEP); return; }
        if (sym == XK_Down)   { scroll_view(0, SCROLL_STEP); return; }
    }
    if (mod == (MOD | ShiftMask)) {
        if (sym == XK_q)      { XCloseDisplay(_dpy); exit(0); }
    }
}

void WM::handle_button_press(XButtonEvent* e) {
    if (e->button == Button1 && (e->state & MOD)) {
        int idx = find_win(e->window);
        if (idx >= 0) {
            set_focus(idx);
            XWindowAttributes wa;
            XGetWindowAttributes(_dpy, e->window, &wa);
            _wins[idx].x = wa.x + _view_x;
            _wins[idx].y = wa.y + _view_y;
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
            _wins[idx].x = wa.x + _view_x;
            _wins[idx].y = wa.y + _view_y;
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

    unsigned int bmask = e->state & ~(LockMask | Mod2Mask | Mod3Mask | Mod5Mask);
    if (e->button == Button3 && bmask == ShiftMask) {
        _scroll_drag = {true, -1, e->x_root, e->y_root, _view_x, _view_y, 0, 0, 0, 0};
        return;
    }

    int idx = find_win(e->window);
    if (idx >= 0 && !_wins[idx].desktop) set_focus(idx);
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
    if (_scroll_drag.active) {
        _scroll_drag = {};
    }
}

void WM::handle_motion(XMotionEvent* e) {
    if (_scroll_drag.active) {
        int dx = e->x_root - _scroll_drag.sx;
        int dy = e->y_root - _scroll_drag.sy;
        _view_x = _scroll_drag.ox - dx;
        _view_y = _scroll_drag.oy - dy;
        clamp_view();
        static struct timeval ls = {0, 0};
        struct timeval now;
        gettimeofday(&now, nullptr);
        double el = (now.tv_sec - ls.tv_sec) + (now.tv_usec - ls.tv_usec) * 1e-6;
        if (el >= 0.016) {
            layout();
            ls = now;
        }
        return;
    }
    if (_move_drag.active && _move_drag.idx >= 0) {
        auto& w = _wins[_move_drag.idx];
        w.x = _move_drag.ox + (e->x_root - _move_drag.sx);
        w.y = _move_drag.oy + (e->y_root - _move_drag.sy);
        update_zones();
        int old_vx = _view_x, old_vy = _view_y;
        clamp_view();
        if (_view_x != old_vx || _view_y != old_vy) {
            for (auto& wi : _wins) {
                if (wi.unmanaged) continue;
                XMoveWindow(_dpy, wi.xwin, wi.x - _view_x, wi.y - _view_y);
            }
        } else {
            XMoveWindow(_dpy, w.xwin, w.x - _view_x, w.y - _view_y);
        }
        if (_comp) {
            static struct timeval lm = {0, 0};
            struct timeval now;
            gettimeofday(&now, nullptr);
            double el = (now.tv_sec - lm.tv_sec) + (now.tv_usec - lm.tv_usec) * 1e-6;
            if (el >= 0.016) {
                auto& m = primary_mon();
                _comp->render(_wins, _view_x, _view_y, m.width);
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
        XMoveResizeWindow(_dpy, w.xwin, nx - _view_x, ny - _view_y, nw, nh);
        update_zones();
        if (_comp) {
            static struct timeval lr = {0, 0};
            struct timeval now;
            gettimeofday(&now, nullptr);
            double el = (now.tv_sec - lr.tv_sec) + (now.tv_usec - lr.tv_usec) * 1e-6;
            if (el >= 0.016) {
                auto& m = primary_mon();
                _comp->render(_wins, _view_x, _view_y, m.width);
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
