#pragma once
#include "../wm/wm.h"
#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/Xfixes.h>
#include <X11/extensions/shape.h>
#include <unordered_map>

class Compositor {
public:
    Compositor(Display* dpy, Window root, int screen);
    ~Compositor();

    bool init(bool nvidia);

    void add_window(WinInfo& w);
    void remove_window(WinInfo& w);
    void mark_dirty(Window xwin, std::vector<WinInfo>& wins);

    void render(std::vector<WinInfo>& wins,
                int view_x, int view_y);
    bool needs_render() const { return _frame_dirty; }
    void request_render() { _frame_dirty = true; }

    int  damage_event   = 0;
    Atom opacity_atom   = None;
    Atom bypass_atom    = None;

    void on_property_change(Window xwin, Atom atom, std::vector<WinInfo>& wins);

private:
    GLuint make_shader(GLenum type, const char* src);
    GLuint make_program(const char* vert, const char* frag);
    void   update_texture(WinInfo& w);
    void   bind_tex(WinInfo& w);
    void   draw_quad(int win_x, int win_y, int win_w, int win_h,
                     GLuint tex, float opacity);
    void   init_blur(int w, int h);
    void   blur_region(int x, int y, int w, int h);
    void   capture_wallpaper();
    Display*   _dpy;
    Window     _root;
    int        _screen;
    Window     _overlay   = None;
    GLXContext _ctx        = nullptr;
    GLXWindow  _glxwin    = None;

    GLuint _prog_comp   = 0;
    GLuint _prog_shadow = 0;
    GLuint _vao = 0, _vbo = 0;

    int  _rw = 0, _rh = 0;

    struct PixFBC { GLXFBConfig fbc; bool rgba; bool y_inv; };
    std::unordered_map<VisualID, PixFBC> _vis_fbc;
    bool        _ok       = false;
    bool        _nvidia   = false;

    GLint _u_comp_tex = -1, _u_comp_opacity = -1, _u_comp_opaque = -1;
    GLint _u_shad_alpha = -1, _u_shad_color = -1;

    GLuint _prog_blur   = 0;
    GLint  _u_blur_dir  = -1;
    GLuint _blur_fbo[2] = {};
    GLuint _blur_tex[2] = {};
    GLuint _blur_src     = 0;
    int    _blur_w = 0, _blur_h = 0;

    GLuint _wall_tex = 0;
    bool   _wall_ok  = false;

    PFNGLXBINDTEXIMAGEEXTPROC    _bindTex    = nullptr;
    PFNGLXRELEASETEXIMAGEEXTPROC _releaseTex = nullptr;

    bool _frame_dirty = true;
};
