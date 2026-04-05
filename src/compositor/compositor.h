#pragma once
#include "../wm/wm.h"
#include "../mpris/mpris.h"
#include <X11/Xft/Xft.h>
#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/Xfixes.h>
#include <X11/extensions/shape.h>
#include <ctime>

class Compositor {
public:
    Compositor(Display* dpy, Window root, int screen);
    ~Compositor();

    bool init(int sw, int sh, int mx, int my, bool nvidia = false);

    void add_window(WinInfo& w);
    void remove_window(WinInfo& w);
    void mark_dirty(Window xwin, std::vector<WinInfo>& wins);

    void render(std::vector<WinInfo>& wins,
                int view_x, int view_y,
                int sw);

    int  damage_event   = 0;
    Atom opacity_atom   = None;
    Atom bypass_atom    = None;

    void set_dragging(bool d) { _dragging = d; }
    void on_property_change(Window xwin, Atom atom, std::vector<WinInfo>& wins);
    void handle_click(int root_x, int root_y);
    bool tick(double dt);
    Window overlay() const { return _overlay; }

    static constexpr int BAR_H   = 32;
    static constexpr int PAN_W   = 340;
    static constexpr int PAN_H   = 152;
    static constexpr int DISC_R  = 50;

private:
    void init_bar();
    void update_bar_time();
    void render_bar(int sw);

    void init_panel();
    void render_panel(int sw);
    void update_panel_texts();
    void draw_panel_text(GLuint tex, int tw, int th,
                         int px, int py, int pan_x, int pan_y);

    void update_overlay_input();

    GLuint make_shader(GLenum type, const char* src);
    GLuint make_program(const char* vert, const char* frag);
    void   make_fbo(int w, int h, GLuint& fbo, GLuint& tex);
    void   update_texture(WinInfo& w);
    void   draw_quad(int win_x, int win_y, int win_w, int win_h,
                     GLuint tex, float opacity);
    void   blur_fbo(int sw, int sh);

    void   xft_to_tex(const std::string& text, XftFont* font,
                      GLuint& tex, int& tw, int& th);

    void   load_art(const std::string& url);

    void   draw_solid_tri(float x0, float y0, float x1, float y1, float x2, float y2,
                          float r, float g, float b, float a);
    void   draw_solid_rect(float x0, float y0, float x1, float y1,
                           float r, float g, float b, float a);

    Display*   _dpy;
    Window     _root;
    int        _screen;
    Window     _overlay   = None;
    GLXContext _ctx        = nullptr;
    GLXWindow  _glxwin    = None;

    GLuint _prog_comp   = 0;
    GLuint _prog_blur   = 0;
    GLuint _prog_shadow = 0;
    GLuint _vao = 0, _vbo = 0;

    GLuint _fbo[2]     = {};
    GLuint _fbo_tex[2] = {};
    int    _fbw = 0, _fbh = 0;

    int  _rw = 0, _rh = 0;
    int  _mx = 0, _my = 0;
    int  _sw = 0;

    GLXFBConfig _pix_fbc  = nullptr;
    bool        _fbc_ok   = false;
    bool        _pix_rgba = true;
    bool        _ok       = false;
    bool        _dragging = false;
    bool        _nvidia   = false;

    GLint _u_comp_tex = -1, _u_comp_opacity = -1;
    GLint _u_blur_tex = -1, _u_blur_dir     = -1;
    GLint _u_shad_alpha = -1;
    GLuint _prog_text   = 0;
    GLint  _u_text_tex  = -1, _u_text_color = -1;
    GLuint _prog_pill   = 0;
    GLint  _u_pill_tex  = -1, _u_pill_color = -1, _u_pill_size = -1;
    GLuint _white_tex   = 0;

    PFNGLXBINDTEXIMAGEEXTPROC    _bindTex    = nullptr;
    PFNGLXRELEASETEXIMAGEEXTPROC _releaseTex = nullptr;

    XftFont*  _bar_font    = nullptr;
    XftDraw*  _bar_xdraw   = nullptr;
    Pixmap    _bar_pm      = None;
    GC        _bar_gc      = None;
    GLuint    _bar_time_tex = 0;
    GLuint    _bar_bg_tex   = 0;
    GLuint    _bar_blur_fbo = 0;
    GLuint    _bar_blur_tex = 0;
    int       _bar_pw      = 0;
    int       _bar_ph      = 26;
    int       _bar_tw      = 256;
    char      _bar_last_time[16] = {};

    GLuint _prog_disc     = 0;
    GLint  _u_disc_color  = -1, _u_disc_angle = -1;
    GLint  _u_disc_has_art= -1, _u_disc_art   = -1;
    GLuint _prog_solid    = 0;
    GLint  _u_sol_color   = -1;
    GLuint      _art_tex      = 0;
    std::string _art_last_url;
    GLuint _pan_bg_tex   = 0, _pan_blur_fbo = 0, _pan_blur_tex = 0;
    GLuint _pan_title_tex = 0;  int _pan_title_tw = 0, _pan_title_th = 0;
    GLuint _pan_artist_tex = 0; int _pan_artist_tw = 0, _pan_artist_th = 0;
    GLuint _pan_time_tex  = 0;  int _pan_time_tw  = 0, _pan_time_th  = 0;
    bool   _pan_open    = false;
    float  _pan_anim    = 0.0f;
    float  _disc_angle  = 0.0f;
    std::string _pan_last_title, _pan_last_artist, _pan_last_time;
    MprisInfo   _mpris;
    DBusConnection* _dbus = nullptr;
    double _mpris_poll_t  = 0.0;
    XftFont* _pan_font_title  = nullptr;
    XftFont* _pan_font_artist = nullptr;
    XftFont* _pan_font_time   = nullptr;
    double _last_render_t = 0.0;
    float _disc_r = 0.45f, _disc_g = 0.25f, _disc_b = 0.70f;
};
