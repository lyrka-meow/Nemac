#pragma once
#include "types.h"
#include <X11/Xatom.h>
#include <vector>

class Compositor;
struct MonitorInfo;

class WM {
public:
    WM(Display* dpy, Window root);
    ~WM();

    void init(const std::vector<MonitorInfo>& monitors, Compositor* comp);
    void handle_map_request(XMapRequestEvent* e);
    void handle_destroy(Window w);
    void handle_key(XKeyEvent* e);
    void handle_button_press(XButtonEvent* e);
    void handle_button_release(XButtonEvent* e);
    void handle_motion(XMotionEvent* e);
    void handle_configure_request(XConfigureRequestEvent* e);
    void handle_configure_notify(XConfigureEvent* e);
    void layout();

    int find_win(Window w) const;
    void set_focus(int idx);

    std::vector<WinInfo>& windows() { return _wins; }
    const std::vector<WinInfo>& windows() const { return _wins; }
    int focused() const { return _focused; }
    int primary_monitor() const { return _primary; }
    const std::vector<Zone>& zones() const { return _zones; }

    int view_x() const { return _view_x; }
    int view_y() const { return _view_y; }
    void auto_scroll_tick();

private:
    void spawn(const char* cmd);
    void kill_win(int idx);
    void update_zones();
    void pin_focused();
    void fullscreen_focused();
    void desktop_focused();
    void scroll_view(int dx, int dy);
    void clamp_view();
    void switch_primary(int dir);
    const MonitorInfo& primary_mon() const;
    bool is_dock(Window w);
    void recalc_workarea();

    Display*    _dpy;
    Window      _root;
    Compositor* _comp = nullptr;

    std::vector<WinInfo>     _wins;
    std::vector<Zone>        _zones;
    std::vector<MonitorInfo> _monitors;
    int _focused  = -1;
    int _primary  = 0;
    int _view_x   = 0, _view_y = 0;

    DragState _move_drag;
    DragState _resize_drag;
    DragState _scroll_drag;
    int _strut_top = 0;

    static constexpr int BORDER          = 2;
    static constexpr int DEFAULT_WIN_W   = 800;
    static constexpr int DEFAULT_WIN_H   = 600;
    static constexpr unsigned int MOD    = Mod1Mask;
    static constexpr int SCROLL_STEP    = 200;
};
