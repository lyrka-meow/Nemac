#pragma once
#include <X11/Xlib.h>
#include <X11/extensions/Xdamage.h>
#include <GL/glew.h>
#include <GL/glx.h>

struct WinInfo {
    Window xwin      = None;
    int    x         = 0, y = 0;
    int    w         = 0, h = 0;
    bool   pinned    = false;
    bool   fullscreen = false;
    bool   unmanaged = false;
    bool   desktop   = false;
    bool   dock      = false;
    bool   focused   = false;
    int    pre_desk_x = 0, pre_desk_y = 0;
    int    pre_desk_w = 0, pre_desk_h = 0;

    Pixmap    pixmap     = None;
    GLXPixmap glx_pixmap = None;
    GLuint    texture    = 0;
    Damage    damage     = None;
    bool      dirty      = true;
    float     opacity    = 1.0f;
    int       pix_w      = 0, pix_h = 0;
    bool      bypass     = false;
    int       black_frames = 0;
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
