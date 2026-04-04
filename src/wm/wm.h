#pragma once
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/Xdamage.h>
#include <X11/extensions/Xinerama.h>
#include <GL/glew.h>
#include <GL/gl.h>
#include <GL/glx.h>
#include <vector>
#include <string>

struct WinInfo {
    Window xwin      = None;
    int    x         = 0, y = 0;
    int    w         = 0, h = 0;
    bool   pinned    = false;
    bool   fullscreen = false;
    bool   unmanaged = false;

    Pixmap    pixmap     = None;
    GLXPixmap glx_pixmap = None;
    GLuint    texture    = 0;
    Damage    damage     = None;
    bool      dirty      = true;
    float     opacity    = 1.0f;
    int       pix_w      = 0, pix_h = 0;
    bool      bypass     = false;
};

struct Zone {
    int x = 0, y = 0, w = 0, h = 0;
    int def_x = 0, def_y = 0, def_w = 0, def_h = 0;
    int monitor_idx = 0;
};

struct DragState {
    bool active = false;
    int  idx    = -1;
    int  sx = 0, sy = 0;
    int  ox = 0, oy = 0;
    int  ow = 0, oh = 0;
    int  left = 0, top = 0;
};

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

private:
    void spawn(const char* cmd);
    void kill_win(int idx);
    void update_zones();
    void pin_focused();
    void fullscreen_focused();
    void switch_primary(int dir);
    const MonitorInfo& primary_mon() const;

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

    static constexpr int BORDER          = 2;
    static constexpr int ZONE_MARGIN     = 40;
    static constexpr int DEFAULT_WIN_W   = 800;
    static constexpr int DEFAULT_WIN_H   = 600;
    static constexpr unsigned int MOD    = Mod1Mask;
    static constexpr unsigned long FOCUSED_BORDER = 0x88aaff;
    static constexpr unsigned long NORMAL_BORDER  = 0x444444;
};
