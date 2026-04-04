#define GLEW_STATIC
#include "compositor.h"
#include "shaders.h"
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <ctime>
#include <vector>
#include <sys/time.h>
#include <Imlib2.h>

Compositor::Compositor(Display* dpy, Window root, int screen)
    : _dpy(dpy), _root(root), _screen(screen)
{
    opacity_atom = XInternAtom(dpy, "_NET_WM_WINDOW_OPACITY", False);
    bypass_atom  = XInternAtom(dpy, "_NET_WM_BYPASS_COMPOSITOR", False);
}

Compositor::~Compositor() {
    if (!_ok) return;
    glXMakeCurrent(_dpy, _glxwin, _ctx);
    if (_fbo[0])     glDeleteFramebuffers(2, _fbo);
    if (_fbo_tex[0]) glDeleteTextures(2, _fbo_tex);
    if (_prog_comp)  glDeleteProgram(_prog_comp);
    if (_prog_blur)  glDeleteProgram(_prog_blur);
    if (_prog_shadow)glDeleteProgram(_prog_shadow);
    if (_bar_time_tex) glDeleteTextures(1, &_bar_time_tex);
    if (_bar_bg_tex)   glDeleteTextures(1, &_bar_bg_tex);
    if (_bar_blur_fbo) glDeleteFramebuffers(1, &_bar_blur_fbo);
    if (_bar_blur_tex) glDeleteTextures(1, &_bar_blur_tex);
    if (_bar_xdraw)    { XftDrawDestroy(_bar_xdraw); _bar_xdraw = nullptr; }
    if (_bar_font)     { XftFontClose(_dpy, _bar_font); _bar_font = nullptr; }
    if (_bar_pm)       { XFreePixmap(_dpy, _bar_pm); _bar_pm = None; }
    if (_bar_gc)       { XFreeGC(_dpy, _bar_gc); _bar_gc = None; }
    if (_prog_text)      glDeleteProgram(_prog_text);
    if (_prog_pill)      glDeleteProgram(_prog_pill);
    if (_prog_disc)      glDeleteProgram(_prog_disc);
    if (_white_tex)      glDeleteTextures(1, &_white_tex);
    if (_pan_bg_tex)     glDeleteTextures(1, &_pan_bg_tex);
    if (_pan_blur_fbo)   glDeleteFramebuffers(1, &_pan_blur_fbo);
    if (_pan_blur_tex)   glDeleteTextures(1, &_pan_blur_tex);
    if (_pan_title_tex)  glDeleteTextures(1, &_pan_title_tex);
    if (_pan_artist_tex) glDeleteTextures(1, &_pan_artist_tex);
    if (_pan_time_tex)   glDeleteTextures(1, &_pan_time_tex);
    if (_pan_font_title)  { XftFontClose(_dpy, _pan_font_title);  _pan_font_title  = nullptr; }
    if (_pan_font_artist) { XftFontClose(_dpy, _pan_font_artist); _pan_font_artist = nullptr; }
    if (_pan_font_time)   { XftFontClose(_dpy, _pan_font_time);   _pan_font_time   = nullptr; }
    if (_art_tex)        glDeleteTextures(1, &_art_tex);
    if (_prog_solid)     glDeleteProgram(_prog_solid);
    if (_dbus)           { dbus_connection_unref(_dbus); _dbus = nullptr; }
    if (_vao) { glDeleteVertexArrays(1, &_vao); glDeleteBuffers(1, &_vbo); }
    glXMakeCurrent(_dpy, None, nullptr);
    if (_glxwin != None) glXDestroyWindow(_dpy, _glxwin);
    if (_ctx)            glXDestroyContext(_dpy, _ctx);
    if (_overlay != None) XCompositeReleaseOverlayWindow(_dpy, _root);
}

/* ======== Инициализация ======== */

bool Compositor::init(int sw, int sh, int mx, int my) {
    _mx = mx; _my = my; _sw = sw; _sh = sh;
    _rw = DisplayWidth(_dpy, _screen);
    _rh = DisplayHeight(_dpy, _screen);
    fprintf(stderr, "nemac: композитор sw=%d sh=%d mx=%d my=%d rw=%d rh=%d\n",
            sw, sh, mx, my, _rw, _rh);
    fflush(stderr);
    int ev, er;

    if (!XCompositeQueryExtension(_dpy, &ev, &er)) {
        fprintf(stderr, "nemac: XComposite недоступен\n");
        return false;
    }
    int maj = 0, min = 0;
    XCompositeQueryVersion(_dpy, &maj, &min);
    if (maj == 0 && min < 2) {
        fprintf(stderr, "nemac: нужен XComposite >= 0.2\n");
        return false;
    }

    int dev, der;
    if (!XDamageQueryExtension(_dpy, &dev, &der)) {
        fprintf(stderr, "nemac: XDamage недоступен\n");
        return false;
    }
    damage_event = dev;

    XCompositeRedirectSubwindows(_dpy, _root, CompositeRedirectAutomatic);

    _overlay = XCompositeGetOverlayWindow(_dpy, _root);
    XserverRegion empty = XFixesCreateRegion(_dpy, nullptr, 0);
    XFixesSetWindowShapeRegion(_dpy, _overlay, ShapeInput, 0, 0, empty);
    XFixesDestroyRegion(_dpy, empty);

    int ov_attr[] = {
        GLX_DOUBLEBUFFER,  True,
        GLX_RED_SIZE,      8,
        GLX_GREEN_SIZE,    8,
        GLX_BLUE_SIZE,     8,
        GLX_ALPHA_SIZE,    8,
        GLX_RENDER_TYPE,   GLX_RGBA_BIT,
        GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
        None
    };
    int nfbc;
    GLXFBConfig* ov_fbcs = glXChooseFBConfig(_dpy, _screen, ov_attr, &nfbc);
    if (!ov_fbcs || nfbc == 0) {
        fprintf(stderr, "nemac: нет FBConfig для overlay\n");
        return false;
    }
    GLXFBConfig ov_fbc = ov_fbcs[0];
    XFree(ov_fbcs);

    _ctx = glXCreateNewContext(_dpy, ov_fbc, GLX_RGBA_TYPE, nullptr, True);
    if (!_ctx) { fprintf(stderr, "nemac: glXCreateNewContext провал\n"); return false; }

    _glxwin = glXCreateWindow(_dpy, ov_fbc, _overlay, nullptr);
    if (!glXMakeCurrent(_dpy, _glxwin, _ctx)) {
        fprintf(stderr, "nemac: glXMakeCurrent провал\n");
        return false;
    }
    glewExperimental = GL_TRUE;
    GLenum glew_err = glewInit();
    if (glew_err != GLEW_OK) {
        fprintf(stderr, "nemac: glewInit провал: %s\n", glewGetErrorString(glew_err));
        return false;
    }

    _bindTex = (PFNGLXBINDTEXIMAGEEXTPROC)
               glXGetProcAddressARB((const GLubyte*)"glXBindTexImageEXT");
    _releaseTex = (PFNGLXRELEASETEXIMAGEEXTPROC)
                  glXGetProcAddressARB((const GLubyte*)"glXReleaseTexImageEXT");
    if (!_bindTex || !_releaseTex) {
        fprintf(stderr, "nemac: GLX_EXT_texture_from_pixmap недоступен\n");
        return false;
    }

    int pix_attr[] = {
        GLX_BIND_TO_TEXTURE_RGBA_EXT,    True,
        GLX_DRAWABLE_TYPE,               GLX_PIXMAP_BIT,
        GLX_BIND_TO_TEXTURE_TARGETS_EXT, GLX_TEXTURE_2D_BIT_EXT,
        GLX_DOUBLEBUFFER,                False,
        GLX_RED_SIZE,   8,
        GLX_GREEN_SIZE, 8,
        GLX_BLUE_SIZE,  8,
        GLX_ALPHA_SIZE, 8,
        None
    };
    GLXFBConfig* pfbcs = glXChooseFBConfig(_dpy, _screen, pix_attr, &nfbc);
    if (!pfbcs || nfbc == 0) {
        fprintf(stderr, "nemac: нет FBConfig для texture-from-pixmap\n");
        return false;
    }
    _pix_fbc = pfbcs[0];
    _fbc_ok  = true;
    XFree(pfbcs);

    glEnable(GL_BLEND);
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA,
                        GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    _prog_comp   = make_program(VERT_SRC, FRAG_COMP);
    _prog_blur   = make_program(VERT_SRC, FRAG_BLUR);
    _prog_shadow = make_program(VERT_SRC, FRAG_SHADOW);
    _prog_text   = make_program(VERT_SRC, FRAG_TEXT);
    _prog_pill   = make_program(VERT_SRC, FRAG_PILL);
    _prog_disc   = make_program(VERT_SRC, FRAG_DISC);
    _prog_solid  = make_program(VERT_SRC, FRAG_SOLID);
    if (!_prog_comp || !_prog_blur || !_prog_shadow ||
        !_prog_text || !_prog_pill || !_prog_disc || !_prog_solid) return false;

    _u_comp_tex     = glGetUniformLocation(_prog_comp,   "uTex");
    _u_comp_opacity = glGetUniformLocation(_prog_comp,   "uOpacity");
    _u_blur_tex     = glGetUniformLocation(_prog_blur,   "uTex");
    _u_blur_dir     = glGetUniformLocation(_prog_blur,   "uDir");
    _u_shad_alpha   = glGetUniformLocation(_prog_shadow, "uAlpha");
    _u_text_tex     = glGetUniformLocation(_prog_text,   "uTex");
    _u_text_color   = glGetUniformLocation(_prog_text,   "uColor");
    _u_pill_tex     = glGetUniformLocation(_prog_pill,   "uTex");
    _u_pill_color   = glGetUniformLocation(_prog_pill,   "uColor");
    _u_pill_size    = glGetUniformLocation(_prog_pill,   "uSize");
    _u_disc_color   = glGetUniformLocation(_prog_disc,   "uColor");
    _u_disc_angle   = glGetUniformLocation(_prog_disc,   "uAngle");
    _u_disc_has_art = glGetUniformLocation(_prog_disc,   "uHasArt");
    _u_disc_art     = glGetUniformLocation(_prog_disc,   "uArt");
    _u_sol_color    = glGetUniformLocation(_prog_solid,  "uColor");

    uint32_t white_px = 0xFFFFFFFF;
    glGenTextures(1, &_white_tex);
    glBindTexture(GL_TEXTURE_2D, _white_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, &white_px);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    auto glXSwapIntervalEXT_fn = (PFNGLXSWAPINTERVALEXTPROC)
        glXGetProcAddressARB((const GLubyte*)"glXSwapIntervalEXT");
    if (glXSwapIntervalEXT_fn)
        glXSwapIntervalEXT_fn(_dpy, _glxwin, 0);
    else {
        auto glXSwapIntervalMESA_fn = (PFNGLXSWAPINTERVALMESAPROC)
            glXGetProcAddressARB((const GLubyte*)"glXSwapIntervalMESA");
        if (glXSwapIntervalMESA_fn) glXSwapIntervalMESA_fn(0);
    }

    float dummy[24] = {};
    glGenVertexArrays(1, &_vao);
    glGenBuffers(1, &_vbo);
    glBindVertexArray(_vao);
    glBindBuffer(GL_ARRAY_BUFFER, _vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(dummy), dummy, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);

    _fbw = _rw; _fbh = _rh;
    make_fbo(_rw, _rh, _fbo[0], _fbo_tex[0]);
    make_fbo(_rw, _rh, _fbo[1], _fbo_tex[1]);

    XSelectInput(_dpy, _overlay, ButtonPressMask);

    _ok = true;
    fprintf(stderr, "nemac: GL готов, инициализация бара\n");
    fflush(stderr);
    init_bar();
    _dbus = mpris_connect();
    init_panel();
    update_overlay_input();
    fprintf(stderr, "nemac: инициализация завершена\n");
    fflush(stderr);
    return true;
}

/* ======== GL утилиты ======== */

GLuint Compositor::make_shader(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    int ok; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512]; glGetShaderInfoLog(s, 512, nullptr, log);
        fprintf(stderr, "nemac шейдер: %s\n", log);
        glDeleteShader(s); return 0;
    }
    return s;
}

GLuint Compositor::make_program(const char* vert, const char* frag) {
    GLuint v = make_shader(GL_VERTEX_SHADER, vert);
    GLuint f = make_shader(GL_FRAGMENT_SHADER, frag);
    if (!v || !f) { glDeleteShader(v); glDeleteShader(f); return 0; }
    GLuint p = glCreateProgram();
    glAttachShader(p, v); glAttachShader(p, f);
    glLinkProgram(p);
    glDeleteShader(v); glDeleteShader(f);
    int ok; glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[512]; glGetProgramInfoLog(p, 512, nullptr, log);
        fprintf(stderr, "nemac линковка: %s\n", log);
        glDeleteProgram(p); return 0;
    }
    return p;
}

void Compositor::make_fbo(int w, int h, GLuint& fbo, GLuint& tex) {
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

/* ======== Управление окнами (текстуры) ======== */

void Compositor::add_window(WinInfo& w) {
    Atom actual; int fmt; unsigned long n, after; unsigned char* data = nullptr;
    if (XGetWindowProperty(_dpy, w.xwin, opacity_atom, 0, 1, False,
            XA_CARDINAL, &actual, &fmt, &n, &after, &data) == Success && data) {
        w.opacity = *(unsigned long*)data / (float)0xFFFFFFFFu;
        XFree(data);
    }
    if (w.bypass) return;
    w.damage = XDamageCreate(_dpy, w.xwin, XDamageReportNonEmpty);
    w.dirty  = true;
}

void Compositor::on_property_change(Window xwin, Atom atom,
                                    std::vector<WinInfo>& wins) {
    if (atom != bypass_atom) return;
    int idx = -1;
    for (int i = 0; i < (int)wins.size(); i++)
        if (wins[i].xwin == xwin) { idx = i; break; }
    if (idx < 0) return;
    WinInfo& w = wins[idx];
    Atom actual; int fmt; unsigned long n, after;
    unsigned char* bdata = nullptr;
    unsigned long val = 0;
    if (XGetWindowProperty(_dpy, xwin, bypass_atom, 0, 1, False,
            XA_CARDINAL, &actual, &fmt, &n, &after, &bdata) == Success && bdata) {
        val = *(unsigned long*)bdata;
        XFree(bdata);
    }
    bool want_bypass = (val == 1);
    if (want_bypass == w.bypass) return;
    w.bypass = want_bypass;
    if (want_bypass) {
        if (w.damage)     { XDamageDestroy(_dpy, w.damage); w.damage = None; }
        if (w.glx_pixmap) {
            _releaseTex(_dpy, w.glx_pixmap, GLX_FRONT_LEFT_EXT);
            glXDestroyPixmap(_dpy, w.glx_pixmap); w.glx_pixmap = None;
        }
        if (w.pixmap)  { XFreePixmap(_dpy, w.pixmap); w.pixmap = None; }
        if (w.texture) { glDeleteTextures(1, &w.texture); w.texture = 0; }
        XCompositeUnredirectWindow(_dpy, xwin, CompositeRedirectAutomatic);
    } else {
        XCompositeRedirectWindow(_dpy, xwin, CompositeRedirectAutomatic);
    }
}

void Compositor::remove_window(WinInfo& w) {
    if (!_ok) return;
    glXMakeCurrent(_dpy, _glxwin, _ctx);
    if (w.damage)     { XDamageDestroy(_dpy, w.damage); w.damage = None; }
    if (w.glx_pixmap) {
        _releaseTex(_dpy, w.glx_pixmap, GLX_FRONT_LEFT_EXT);
        glXDestroyPixmap(_dpy, w.glx_pixmap);
        w.glx_pixmap = None;
    }
    if (w.texture) { glDeleteTextures(1, &w.texture); w.texture = 0; }
    if (w.pixmap)  { XFreePixmap(_dpy, w.pixmap); w.pixmap = None; }
}

void Compositor::mark_dirty(Window xwin, std::vector<WinInfo>& wins) {
    for (auto& w : wins)
        if (w.xwin == xwin) { w.dirty = true; break; }
}

void Compositor::update_texture(WinInfo& w) {
    if (!w.dirty || !_fbc_ok) return;
    w.dirty = false;

    bool need_recreate = !w.glx_pixmap || (w.w != w.pix_w || w.h != w.pix_h);

    if (need_recreate) {
        if (w.glx_pixmap) {
            _releaseTex(_dpy, w.glx_pixmap, GLX_FRONT_LEFT_EXT);
            glXDestroyPixmap(_dpy, w.glx_pixmap);
            w.glx_pixmap = None;
        }
        if (w.pixmap) { XFreePixmap(_dpy, w.pixmap); w.pixmap = None; }

        w.pixmap = XCompositeNameWindowPixmap(_dpy, w.xwin);
        if (!w.pixmap) return;

        int pix_attribs[] = {
            GLX_TEXTURE_TARGET_EXT, GLX_TEXTURE_2D_EXT,
            GLX_TEXTURE_FORMAT_EXT, GLX_TEXTURE_FORMAT_RGBA_EXT,
            None
        };
        w.glx_pixmap = glXCreatePixmap(_dpy, _pix_fbc, w.pixmap, pix_attribs);
        w.pix_w = w.w;
        w.pix_h = w.h;

        if (!w.texture) {
            glGenTextures(1, &w.texture);
            glBindTexture(GL_TEXTURE_2D, w.texture);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        } else {
            glBindTexture(GL_TEXTURE_2D, w.texture);
        }
    } else {
        _releaseTex(_dpy, w.glx_pixmap, GLX_FRONT_LEFT_EXT);
        glBindTexture(GL_TEXTURE_2D, w.texture);
    }

    _bindTex(_dpy, w.glx_pixmap, GLX_FRONT_LEFT_EXT, nullptr);
}

void Compositor::draw_quad(int, int,
                           int mx, int my,
                           int win_x, int win_y, int win_w, int win_h,
                           GLuint tex, float opacity)
{
    float x0 = (float)(mx + win_x)         / _rw * 2.0f - 1.0f;
    float x1 = (float)(mx + win_x + win_w) / _rw * 2.0f - 1.0f;
    float y0 = -(float)(my + win_y)          / _rh * 2.0f + 1.0f;
    float y1 = -(float)(my + win_y + win_h)  / _rh * 2.0f + 1.0f;

    float verts[24] = {
        x0, y0, 0.0f, 0.0f,
        x0, y1, 0.0f, 1.0f,
        x1, y1, 1.0f, 1.0f,
        x0, y0, 0.0f, 0.0f,
        x1, y1, 1.0f, 1.0f,
        x1, y0, 1.0f, 0.0f,
    };
    glBindBuffer(GL_ARRAY_BUFFER, _vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);
    glUniform1i(_u_comp_tex, 0);
    glUniform1f(_u_comp_opacity, opacity);
    glBindVertexArray(_vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

void Compositor::blur_fbo(int sw, int sh) {
    float q[24] = {
        -1, 1, 0, 1,  -1, -1, 0, 0,  1, -1, 1, 0,
        -1, 1, 0, 1,   1, -1, 1, 0,   1,  1, 1, 1,
    };
    glBindBuffer(GL_ARRAY_BUFFER, _vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(q), q);
    glBindVertexArray(_vao);
    glUseProgram(_prog_blur);
    glUniform1i(_u_blur_tex, 0);

    glBindFramebuffer(GL_FRAMEBUFFER, _fbo[1]);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, _fbo_tex[0]);
    glUniform2f(_u_blur_dir, 1.0f / sw, 0.0f);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    glBindFramebuffer(GL_FRAMEBUFFER, _fbo[0]);
    glBindTexture(GL_TEXTURE_2D, _fbo_tex[1]);
    glUniform2f(_u_blur_dir, 0.0f, 1.0f / sh);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

/* ======== Бар (часы в пилюле) ======== */

void Compositor::init_bar() {
    fprintf(stderr, "nemac: инициализация бара sw=%d\n", _sw);
    fflush(stderr);
    _bar_font = XftFontOpenName(_dpy, _screen, "sans:bold:size=13");
    if (!_bar_font) _bar_font = XftFontOpenName(_dpy, _screen, "monospace:size=12");
    if (!_bar_font) { fprintf(stderr, "nemac: бар: нет шрифта\n"); return; }

    XGlyphInfo ext;
    XftTextExtents8(_dpy, _bar_font, (FcChar8*)"00:00", 5, &ext);
    _bar_pw = ext.width + _bar_ph + 24;
    _bar_tw = _bar_pw;

    _bar_pm  = XCreatePixmap(_dpy, _root, _bar_tw, _bar_ph,
                             DefaultDepth(_dpy, _screen));
    _bar_gc  = XCreateGC(_dpy, _bar_pm, 0, nullptr);
    _bar_xdraw = XftDrawCreate(_dpy, _bar_pm,
                               DefaultVisual(_dpy, _screen),
                               DefaultColormap(_dpy, _screen));

    glGenTextures(1, &_bar_time_tex);
    glBindTexture(GL_TEXTURE_2D, _bar_time_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, _bar_tw, _bar_ph,
                 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glGenTextures(1, &_bar_bg_tex);
    glBindTexture(GL_TEXTURE_2D, _bar_bg_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, _bar_pw, _bar_ph,
                 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    make_fbo(_bar_pw, _bar_ph, _bar_blur_fbo, _bar_blur_tex);
}

void Compositor::update_bar_time() {
    if (!_bar_font || !_bar_pm) return;
    time_t t = time(nullptr);
    char str[16];
    strftime(str, sizeof(str), "%H:%M", localtime(&t));
    if (strcmp(str, _bar_last_time) == 0) return;
    memcpy(_bar_last_time, str, sizeof(_bar_last_time));

    XSetForeground(_dpy, _bar_gc, 0x000000);
    XFillRectangle(_dpy, _bar_pm, _bar_gc, 0, 0, _bar_tw, _bar_ph);

    XftColor white;
    white.pixel = WhitePixel(_dpy, _screen);
    white.color = {0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF};
    XGlyphInfo ext;
    XftTextExtents8(_dpy, _bar_font, (FcChar8*)str, (int)strlen(str), &ext);
    int tx = (_bar_tw - ext.width) / 2;
    int ty = (_bar_ph + _bar_font->ascent - _bar_font->descent) / 2;
    XftDrawString8(_bar_xdraw, &white, _bar_font, tx, ty,
                   (FcChar8*)str, (int)strlen(str));

    XImage* img = XGetImage(_dpy, _bar_pm, 0, 0, _bar_tw, _bar_ph,
                            AllPlanes, ZPixmap);
    if (!img) return;
    std::vector<uint8_t> rbuf(_bar_tw * _bar_ph);
    for (int y = 0; y < _bar_ph; y++)
        for (int x = 0; x < _bar_tw; x++)
            rbuf[y * _bar_tw + x] = (uint8_t)((XGetPixel(img, x, y) >> 16) & 0xFF);
    XDestroyImage(img);

    glBindTexture(GL_TEXTURE_2D, _bar_time_tex);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, _bar_tw, _bar_ph,
                    GL_RED, GL_UNSIGNED_BYTE, rbuf.data());
}

void Compositor::render_bar(int mx, int sw) {
    if (!_bar_font || !_bar_time_tex || _bar_pw == 0) return;
    update_bar_time();

    const int margin_top = 4;
    int pill_x = _mx + (sw - _bar_pw) / 2;
    int pill_y = margin_top;

    float px0 = (float)(pill_x)           / _rw * 2.0f - 1.0f;
    float px1 = (float)(pill_x + _bar_pw) / _rw * 2.0f - 1.0f;
    float py0 = 1.0f - 2.0f * (_my + pill_y)            / (float)_rh;
    float py1 = 1.0f - 2.0f * (_my + pill_y + _bar_ph)  / (float)_rh;

    {
        XImage* ximg = XGetImage(_dpy, _root, pill_x, _my + pill_y,
                                 _bar_pw, _bar_ph, AllPlanes, ZPixmap);
        if (ximg) {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, _bar_bg_tex);
            int bpp = ximg->bits_per_pixel / 8;
            glPixelStorei(GL_UNPACK_ROW_LENGTH, ximg->bytes_per_line / bpp);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, _bar_pw, _bar_ph,
                            GL_BGRA, GL_UNSIGNED_BYTE, ximg->data);
            glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
            XDestroyImage(ximg);
            glBindFramebuffer(GL_FRAMEBUFFER, _bar_blur_fbo);
            glViewport(0, 0, _bar_pw, _bar_ph);
            glUseProgram(_prog_blur);
            glBindTexture(GL_TEXTURE_2D, _bar_bg_tex);
            glUniform1i(_u_blur_tex, 0);
            glUniform2f(_u_blur_dir, 4.0f / _bar_pw, 0.0f);
            float fq[24] = {-1, 1, 0, 1, -1, -1, 0, 0, 1, -1, 1, 0,
                            -1, 1, 0, 1,  1, -1, 1, 0, 1,  1, 1, 1};
            glBindBuffer(GL_ARRAY_BUFFER, _vbo);
            glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(fq), fq);
            glBindVertexArray(_vao);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glViewport(0, 0, _rw, _rh);
        }
    }

    auto pill_quad = [&](float v[24]) {
        glBindBuffer(GL_ARRAY_BUFFER, _vbo);
        glBufferSubData(GL_ARRAY_BUFFER, 0, 24 * sizeof(float), v);
        glBindVertexArray(_vao);
        glDrawArrays(GL_TRIANGLES, 0, 6);
    };
    auto make_quad = [&](float* v, float u0, float u1) {
        v[0]=px0;v[1]=py0;v[2]=u0;v[3]=0.f;
        v[4]=px0;v[5]=py1;v[6]=u0;v[7]=1.f;
        v[8]=px1;v[9]=py1;v[10]=u1;v[11]=1.f;
        v[12]=px0;v[13]=py0;v[14]=u0;v[15]=0.f;
        v[16]=px1;v[17]=py1;v[18]=u1;v[19]=1.f;
        v[20]=px1;v[21]=py0;v[22]=u1;v[23]=0.f;
    };

    glUseProgram(_prog_pill);
    glUniform2f(_u_pill_size, (float)_bar_pw, (float)_bar_ph);
    glUniform4f(_u_pill_color, 1.f, 1.f, 1.f, 1.f);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, _bar_blur_tex);
    glUniform1i(_u_pill_tex, 0);
    float bv[24]; make_quad(bv, 0.f, 1.f); pill_quad(bv);

    glBindTexture(GL_TEXTURE_2D, _white_tex);
    glUniform4f(_u_pill_color, 0.05f, 0.05f, 0.08f, 0.50f);
    float tv[24]; make_quad(tv, 0.f, 1.f); pill_quad(tv);

    int tx_left = pill_x + (_bar_pw - _bar_tw) / 2;
    float tx0 = (float)tx_left           / _rw * 2.0f - 1.0f;
    float tx1 = (float)(tx_left + _bar_tw) / _rw * 2.0f - 1.0f;
    float ttv[24] = {
        tx0, py0, 0.f, 0.f, tx0, py1, 0.f, 1.f, tx1, py1, 1.f, 1.f,
        tx0, py0, 0.f, 0.f, tx1, py1, 1.f, 1.f, tx1, py0, 1.f, 0.f,
    };
    glUseProgram(_prog_text);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, _bar_time_tex);
    glUniform1i(_u_text_tex, 0);
    glUniform4f(_u_text_color, 1.f, 1.f, 1.f, 1.f);
    glBindBuffer(GL_ARRAY_BUFFER, _vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(ttv), ttv);
    glBindVertexArray(_vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    (void)mx; (void)sw;
}

/* ======== Медиа-панель ======== */

void Compositor::xft_to_tex(const std::string& text, XftFont* font,
                             GLuint& tex, int& tw, int& th) {
    if (!font || text.empty()) return;
    XGlyphInfo ext;
    XftTextExtentsUtf8(_dpy, font, (FcChar8*)text.c_str(), (int)text.size(), &ext);
    tw = std::max(1, (int)ext.width + 4);
    th = std::max(1, font->ascent + font->descent + 2);

    Pixmap pm = XCreatePixmap(_dpy, _root, tw, th, DefaultDepth(_dpy, _screen));
    GC gc = XCreateGC(_dpy, pm, 0, nullptr);
    XSetForeground(_dpy, gc, 0x000000);
    XFillRectangle(_dpy, pm, gc, 0, 0, tw, th);
    XFreeGC(_dpy, gc);

    XftDraw* xd = XftDrawCreate(_dpy, pm,
        DefaultVisual(_dpy, _screen), DefaultColormap(_dpy, _screen));
    XftColor white; white.pixel = WhitePixel(_dpy, _screen);
    white.color = {0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF};
    XftDrawStringUtf8(xd, &white, font, 2, font->ascent + 1,
                      (FcChar8*)text.c_str(), (int)text.size());
    XftDrawDestroy(xd);

    XImage* img = XGetImage(_dpy, pm, 0, 0, tw, th, AllPlanes, ZPixmap);
    XFreePixmap(_dpy, pm);
    if (!img) return;
    std::vector<uint8_t> buf(tw * th);
    for (int y = 0; y < th; y++) {
        for (int x = 0; x < tw; x++) {
            unsigned long px = XGetPixel(img, x, y);
            uint8_t r = (px >> 16) & 0xFF;
            uint8_t g = (px >> 8)  & 0xFF;
            uint8_t b = (px)       & 0xFF;
            buf[y * tw + x] = std::max({r, g, b});
        }
    }
    XDestroyImage(img);

    if (!tex) {
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    } else {
        glBindTexture(GL_TEXTURE_2D, tex);
    }
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, tw, th,
                 0, GL_RED, GL_UNSIGNED_BYTE, buf.data());
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
}

void Compositor::init_panel() {
    _pan_font_title  = XftFontOpenName(_dpy, _screen, "sans:bold:size=14");
    _pan_font_artist = XftFontOpenName(_dpy, _screen, "sans:size=12");
    _pan_font_time   = XftFontOpenName(_dpy, _screen, "monospace:size=12");
    if (!_pan_font_title) _pan_font_title = XftFontOpenName(_dpy, _screen, "fixed:size=14");

    glGenTextures(1, &_pan_bg_tex);
    glBindTexture(GL_TEXTURE_2D, _pan_bg_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, PAN_W, PAN_H,
                 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    make_fbo(PAN_W, PAN_H, _pan_blur_fbo, _pan_blur_tex);
}

void Compositor::load_art(const std::string& url) {
    if (url == _art_last_url) return;
    _art_last_url = url;
    if (_art_tex) { glDeleteTextures(1, &_art_tex); _art_tex = 0; }
    if (url.empty()) return;
    std::string path = url;
    if (path.rfind("file://", 0) == 0) path = path.substr(7);
    Imlib_Image img = imlib_load_image(path.c_str());
    if (!img) return;
    imlib_context_set_image(img);
    int w = imlib_image_get_width();
    int h = imlib_image_get_height();
    const uint32_t* data = (const uint32_t*)imlib_image_get_data_for_reading_only();
    glGenTextures(1, &_art_tex);
    glBindTexture(GL_TEXTURE_2D, _art_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_BGRA, GL_UNSIGNED_BYTE, data);
    imlib_free_image();
}

void Compositor::draw_solid_tri(float x0, float y0, float x1, float y1,
                                 float x2, float y2,
                                 float r, float g, float b, float a) {
    float v[12] = {
        x0 / (float)_rw * 2.f - 1.f, 1.f - y0 / (float)_rh * 2.f, 0, 0,
        x1 / (float)_rw * 2.f - 1.f, 1.f - y1 / (float)_rh * 2.f, 0, 0,
        x2 / (float)_rw * 2.f - 1.f, 1.f - y2 / (float)_rh * 2.f, 0, 0,
    };
    glUseProgram(_prog_solid);
    glUniform4f(_u_sol_color, r, g, b, a);
    glBindBuffer(GL_ARRAY_BUFFER, _vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(v), v);
    glBindVertexArray(_vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
}

void Compositor::draw_solid_rect(float x0, float y0, float x1, float y1,
                                  float r, float g, float b, float a) {
    draw_solid_tri(x0, y0, x0, y1, x1, y1, r, g, b, a);
    draw_solid_tri(x0, y0, x1, y1, x1, y0, r, g, b, a);
}

static std::string fmt_time(int64_t us) {
    if (us < 0) us = 0;
    int total_s = (int)(us / 1000000);
    int m = total_s / 60, s = total_s % 60;
    char buf[16]; snprintf(buf, sizeof(buf), "%d:%02d", m, s);
    return buf;
}

static std::string fit_text(Display* dpy, XftFont* font,
                             const std::string& text, int max_px) {
    XGlyphInfo ext;
    XftTextExtentsUtf8(dpy, font, (FcChar8*)text.c_str(), (int)text.size(), &ext);
    if ((int)ext.width <= max_px) return text;
    std::string t = text;
    while (t.size() > 1) {
        t.pop_back();
        std::string ts = t + "\xe2\x80\xa6";
        XftTextExtentsUtf8(dpy, font, (FcChar8*)ts.c_str(), (int)ts.size(), &ext);
        if ((int)ext.width <= max_px) return ts;
    }
    return t;
}

void Compositor::update_panel_texts() {
    if (!_pan_font_title) return;
    const int TW = PAN_W - 128 - 14;

    std::string title  = _mpris.valid && !_mpris.title.empty()
                         ? _mpris.title : "\xd0\x9d\xd0\xb8\xd1\x87\xd0\xb5\xd0\xb3\xd0\xbe \xd0\xbd\xd0\xb5 \xd0\xb8\xd0\xb3\xd1\x80\xd0\xb0\xd0\xb5\xd1\x82";
    std::string artist = _mpris.valid && !_mpris.artist.empty()
                         ? _mpris.artist : "";
    std::string tpos   = _mpris.valid
                         ? fmt_time(_mpris.position_us) + " / " + fmt_time(_mpris.length_us)
                         : "";

    if (title != _pan_last_title) {
        _pan_last_title = title;
        std::string t = fit_text(_dpy, _pan_font_title, title, TW);
        xft_to_tex(t, _pan_font_title, _pan_title_tex, _pan_title_tw, _pan_title_th);
        size_t h = std::hash<std::string>{}(title);
        _disc_r = 0.3f + 0.5f * ((h & 0xFF) / 255.0f);
        _disc_g = 0.2f + 0.5f * (((h >> 8) & 0xFF) / 255.0f);
        _disc_b = 0.4f + 0.5f * (((h >> 16) & 0xFF) / 255.0f);
    }
    if (artist != _pan_last_artist) {
        _pan_last_artist = artist;
        if (!artist.empty()) {
            std::string a = fit_text(_dpy, _pan_font_artist, artist, TW);
            xft_to_tex(a, _pan_font_artist, _pan_artist_tex, _pan_artist_tw, _pan_artist_th);
        }
    }
    if (tpos != _pan_last_time && _pan_font_time) {
        _pan_last_time = tpos;
        if (!tpos.empty())
            xft_to_tex(tpos, _pan_font_time, _pan_time_tex, _pan_time_tw, _pan_time_th);
    }
}

void Compositor::draw_panel_text(GLuint tex, int tw, int th,
                                 int px, int py, int pan_x, int pan_y) {
    if (!tex || tw <= 0 || th <= 0) return;
    int ax = pan_x + px, ay = pan_y + py;
    float x0 = ax        / (float)_rw * 2.0f - 1.0f;
    float x1 = (ax + tw) / (float)_rw * 2.0f - 1.0f;
    float y0 = 1.0f - 2.0f * ay        / (float)_rh;
    float y1 = 1.0f - 2.0f * (ay + th) / (float)_rh;
    float v[24] = {
        x0, y0, 0, 0,  x0, y1, 0, 1,  x1, y1, 1, 1,
        x0, y0, 0, 0,  x1, y1, 1, 1,  x1, y0, 1, 0,
    };
    glBindBuffer(GL_ARRAY_BUFFER, _vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(v), v);
    glBindVertexArray(_vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

static void draw_rrect(GLuint prog, GLint u_tex, GLint u_col, GLint u_size,
                       GLuint white, GLuint vao, GLuint vbo,
                       int rw, int rh,
                       int ax, int ay, int aw, int ah,
                       float r, float g, float b, float a) {
    float x0 = ax      / (float)rw * 2.0f - 1.0f;
    float x1 = (ax+aw) / (float)rw * 2.0f - 1.0f;
    float y0 = 1.0f - 2.0f * ay       / (float)rh;
    float y1 = 1.0f - 2.0f * (ay+ah)  / (float)rh;
    float v[24] = {
        x0, y0, 0, 0,  x0, y1, 0, 1,  x1, y1, 1, 1,
        x0, y0, 0, 0,  x1, y1, 1, 1,  x1, y0, 1, 0,
    };
    glUseProgram(prog);
    glUniform2f(u_size, (float)aw, (float)ah);
    glUniform4f(u_col, r, g, b, a);
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, white);
    glUniform1i(u_tex, 0);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(v), v);
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

void Compositor::render_panel(int mx, int sw) {
    if (_pan_anim <= 0.01f) return;
    if (!_pan_font_title) return;

    int pan_x = _mx + (sw - PAN_W) / 2;
    int pan_y = _my + BAR_H + 6;
    int vis_h = (int)(PAN_H * _pan_anim);
    if (vis_h < 4) return;

    update_panel_texts();

    {
        int cap_h = std::min(vis_h, PAN_H);
        XImage* ximg = XGetImage(_dpy, _root, pan_x, pan_y + vis_h - cap_h,
                                 PAN_W, cap_h, AllPlanes, ZPixmap);
        if (ximg) {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, _pan_bg_tex);
            int bpp = ximg->bits_per_pixel / 8;
            glPixelStorei(GL_UNPACK_ROW_LENGTH, ximg->bytes_per_line / bpp);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, PAN_H - cap_h, PAN_W, cap_h,
                            GL_BGRA, GL_UNSIGNED_BYTE, ximg->data);
            glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
            XDestroyImage(ximg);
            glBindFramebuffer(GL_FRAMEBUFFER, _pan_blur_fbo);
            glViewport(0, 0, PAN_W, PAN_H);
            glUseProgram(_prog_blur);
            glBindTexture(GL_TEXTURE_2D, _pan_bg_tex);
            glUniform1i(_u_blur_tex, 0);
            glUniform2f(_u_blur_dir, 4.0f / PAN_W, 0.0f);
            float fq[24] = {-1, 1, 0, 1, -1, -1, 0, 0, 1, -1, 1, 0,
                            -1, 1, 0, 1,  1, -1, 1, 0, 1,  1, 1, 1};
            glBindBuffer(GL_ARRAY_BUFFER, _vbo);
            glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(fq), fq);
            glBindVertexArray(_vao); glDrawArrays(GL_TRIANGLES, 0, 6);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glViewport(0, 0, _rw, _rh);
        }
    }

    float px0 = pan_x         / (float)_rw * 2.0f - 1.0f;
    float px1 = (pan_x+PAN_W) / (float)_rw * 2.0f - 1.0f;
    float py0 = 1.0f - 2.0f * pan_y          / (float)_rh;
    float py1 = 1.0f - 2.0f * (pan_y+vis_h)  / (float)_rh;

    float uv_y0 = (float)(PAN_H - vis_h) / PAN_H;
    float uv_y1 = 1.0f;

    float bv[24] = {
        px0, py0, 0, uv_y0,  px0, py1, 0, uv_y1,  px1, py1, 1, uv_y1,
        px0, py0, 0, uv_y0,  px1, py1, 1, uv_y1,  px1, py0, 1, uv_y0,
    };
    glUseProgram(_prog_pill);
    glUniform2f(_u_pill_size, (float)PAN_W, (float)vis_h);
    glUniform4f(_u_pill_color, 1.f, 1.f, 1.f, 1.f);
    glBindTexture(GL_TEXTURE_2D, _pan_blur_tex);
    glUniform1i(_u_pill_tex, 0);
    glBindBuffer(GL_ARRAY_BUFFER, _vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(bv), bv);
    glBindVertexArray(_vao); glDrawArrays(GL_TRIANGLES, 0, 6);

    glBindTexture(GL_TEXTURE_2D, _white_tex);
    glUniform4f(_u_pill_color, 0.05f, 0.05f, 0.08f, 0.78f);
    glBindBuffer(GL_ARRAY_BUFFER, _vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(bv), bv);
    glBindVertexArray(_vao); glDrawArrays(GL_TRIANGLES, 0, 6);

    if (_pan_anim < 0.95f) return;

    const bool has_track = _mpris.valid;
    if (has_track) {
        const int disc_cx = pan_x + 14 + DISC_R;
        const int disc_cy = pan_y + PAN_H / 2;
        float dx0 = (disc_cx - DISC_R) / (float)_rw * 2.f - 1.f;
        float dx1 = (disc_cx + DISC_R) / (float)_rw * 2.f - 1.f;
        float dy0 = 1.f - 2.f * (disc_cy - DISC_R) / (float)_rh;
        float dy1 = 1.f - 2.f * (disc_cy + DISC_R) / (float)_rh;
        float dv[24] = {
            dx0, dy0, 0, 0,  dx0, dy1, 0, 1,  dx1, dy1, 1, 1,
            dx0, dy0, 0, 0,  dx1, dy1, 1, 1,  dx1, dy0, 1, 0,
        };
        glUseProgram(_prog_disc);
        glUniform3f(_u_disc_color, _disc_r, _disc_g, _disc_b);
        glUniform1f(_u_disc_angle, _disc_angle);
        glUniform1i(_u_disc_has_art, _art_tex ? 1 : 0);
        glUniform1i(_u_disc_art, 1);
        if (_art_tex) {
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, _art_tex);
            glActiveTexture(GL_TEXTURE0);
        }
        glBindBuffer(GL_ARRAY_BUFFER, _vbo);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(dv), dv);
        glBindVertexArray(_vao); glDrawArrays(GL_TRIANGLES, 0, 6);
    }

    const int tx0_px   = has_track ? 128 : (PAN_W - _pan_title_tw) / 2;
    const int ty_title  = has_track ? 22 : (PAN_H - _pan_title_th) / 2;
    const int ty_artist = 48;
    const int ty_bar    = 74, ty_time = 88, ty_ctrl = 116;

    glUseProgram(_prog_text);
    glActiveTexture(GL_TEXTURE0);
    glUniform1i(_u_text_tex, 0);

    glUniform4f(_u_text_color, 1.f, 1.f, 1.f, 1.f);
    glBindTexture(GL_TEXTURE_2D, _pan_title_tex);
    draw_panel_text(_pan_title_tex, _pan_title_tw, _pan_title_th,
                    tx0_px, ty_title, pan_x, pan_y);

    if (_pan_artist_tex && !_pan_last_artist.empty()) {
        glUniform4f(_u_text_color, 0.7f, 0.7f, 0.75f, 1.f);
        glBindTexture(GL_TEXTURE_2D, _pan_artist_tex);
        draw_panel_text(_pan_artist_tex, _pan_artist_tw, _pan_artist_th,
                        tx0_px, ty_artist, pan_x, pan_y);
    }

    if (_pan_time_tex && !_pan_last_time.empty()) {
        glUniform4f(_u_text_color, 0.55f, 0.55f, 0.60f, 1.f);
        glBindTexture(GL_TEXTURE_2D, _pan_time_tex);
        draw_panel_text(_pan_time_tex, _pan_time_tw, _pan_time_th,
                        tx0_px, ty_time, pan_x, pan_y);
    }

    if (!has_track) goto done_panel;

    if (_mpris.length_us > 0) {
        const int bar_w = PAN_W - tx0_px - 14;
        const int bar_h = 4;
        draw_rrect(_prog_pill, _u_pill_tex, _u_pill_color, _u_pill_size,
                   _white_tex, _vao, _vbo, _rw, _rh,
                   pan_x + tx0_px, pan_y + ty_bar, bar_w, bar_h,
                   0.4f, 0.4f, 0.44f, 0.6f);
        float frac = (float)std::min(_mpris.position_us, _mpris.length_us) /
                     _mpris.length_us;
        int fill_w = std::max(bar_h, (int)(bar_w * frac));
        draw_rrect(_prog_pill, _u_pill_tex, _u_pill_color, _u_pill_size,
                   _white_tex, _vao, _vbo, _rw, _rh,
                   pan_x + tx0_px, pan_y + ty_bar, fill_w, bar_h,
                   1.f, 1.f, 1.f, 0.9f);
    }

    {
    const float cy  = (float)(pan_y + ty_ctrl);
    const float btn_r = 14.f;
    const float cx  = (float)(pan_x + tx0_px) + (float)(PAN_W - tx0_px - 14) * 0.5f;
    const float gap = btn_r * 2.f + 10.f;
    const float pcx = cx - gap;
    const float ncx = cx + gap;

    draw_rrect(_prog_pill, _u_pill_tex, _u_pill_color, _u_pill_size,
               _white_tex, _vao, _vbo, _rw, _rh,
               (int)(cx - btn_r), (int)(cy - btn_r), (int)(btn_r * 2), (int)(btn_r * 2),
               1.f, 1.f, 1.f, 0.18f);
    draw_rrect(_prog_pill, _u_pill_tex, _u_pill_color, _u_pill_size,
               _white_tex, _vao, _vbo, _rw, _rh,
               (int)(pcx - 12.f), (int)(cy - 12.f), 24, 24,
               1.f, 1.f, 1.f, 0.13f);
    draw_rrect(_prog_pill, _u_pill_tex, _u_pill_color, _u_pill_size,
               _white_tex, _vao, _vbo, _rw, _rh,
               (int)(ncx - 12.f), (int)(cy - 12.f), 24, 24,
               1.f, 1.f, 1.f, 0.13f);

    if (_mpris.playing) {
        draw_solid_rect(cx - 6.f, cy - 9.f, cx - 2.f, cy + 9.f, 1, 1, 1, 1);
        draw_solid_rect(cx + 2.f, cy - 9.f, cx + 6.f, cy + 9.f, 1, 1, 1, 1);
    } else {
        draw_solid_tri(cx - 6.f, cy - 9.f, cx - 6.f, cy + 9.f, cx + 9.f, cy, 1, 1, 1, 1);
    }

    draw_solid_rect(pcx - 8.f, cy - 8.f, pcx - 5.f, cy + 8.f, .85f, .85f, .85f, 1);
    draw_solid_tri(pcx + 7.f, cy - 8.f, pcx + 7.f, cy + 8.f, pcx - 4.f, cy, .85f, .85f, .85f, 1);

    draw_solid_tri(ncx - 7.f, cy - 8.f, ncx - 7.f, cy + 8.f, ncx + 4.f, cy, .85f, .85f, .85f, 1);
    draw_solid_rect(ncx + 5.f, cy - 8.f, ncx + 8.f, cy + 8.f, .85f, .85f, .85f, 1);
    }

done_panel:
    (void)mx; (void)sw;
}

/* ======== Тик (анимации + MPRIS) ======== */

bool Compositor::tick(double dt) {
    if (!_ok) return false;

    bool needs_render = false;

    const float SPD = 4.0f;
    if (_pan_open) {
        if (_pan_anim < 1.0f) {
            _pan_anim = std::min(1.0f, _pan_anim + (float)dt * SPD);
            needs_render = true;
        }
    } else {
        if (_pan_anim > 0.0f) {
            _pan_anim = std::max(0.0f, _pan_anim - (float)dt * SPD);
            needs_render = true;
        }
    }

    if (_pan_anim > 0.0f && _mpris.playing) {
        _disc_angle += (float)(dt * 2.0 * 3.14159265 / 8.0);
        needs_render = true;
    }

    if (_pan_anim > 0.0f && _pan_anim < 1.0f)
        _pan_bg_dirty = true;

    if (_pan_anim > 0.0f || _pan_open) {
        _mpris_poll_t += dt;
        if (_mpris_poll_t >= 3.0) {
            _mpris_poll_t = 0.0;
            if (_dbus) {
                MprisInfo info;
                if (mpris_poll(_dbus, info)) {
                    _mpris = info;
                    load_art(_mpris.art_url);
                } else {
                    _mpris = {};
                    load_art("");
                }
                needs_render = true;
            }
        }
    } else {
        _mpris_poll_t = 0.0;
    }

    return needs_render;
}

void Compositor::handle_click(int root_x, int root_y) {
    if (!_ok) return;
    int lx = root_x - _mx, ly = root_y - _my;

    int pill_x_l = (_sw - _bar_pw) / 2;
    int pill_x_r = pill_x_l + _bar_pw;
    int pill_y_t = 4;
    int pill_y_b = 4 + _bar_ph;
    if (lx >= pill_x_l && lx <= pill_x_r && ly >= pill_y_t && ly <= pill_y_b) {
        _pan_open = !_pan_open;
        update_overlay_input();
        return;
    }

    if (!_pan_open || _pan_anim < 0.9f) return;

    int pan_x = _mx + (_sw - PAN_W) / 2;
    int pan_y = _my + BAR_H + 6;

    const int tx0_px = 128;
    const int ctrl_cx_l = pan_x + tx0_px + (PAN_W - tx0_px - 14) / 2;
    const int ctrl_cy   = pan_y + 116;
    const int gap_px    = 14 * 2 + 10;

    int bar_x0 = pan_x + tx0_px;
    int bar_x1 = pan_x + PAN_W - 14;
    int bar_y0 = pan_y + 70;
    int bar_y1 = pan_y + 82;
    if (root_x >= bar_x0 && root_x <= bar_x1 &&
        root_y >= bar_y0 && root_y <= bar_y1 &&
        _mpris.valid && _mpris.length_us > 0 && _dbus) {
        float frac = (float)(root_x - bar_x0) / (bar_x1 - bar_x0);
        int64_t pos = (int64_t)(frac * _mpris.length_us);
        mpris_seek_abs(_dbus, _mpris.player_bus, pos);
        _mpris.position_us = pos;
        return;
    }

    int dx = root_x - ctrl_cx_l, dy = root_y - ctrl_cy;
    if (dx * dx + dy * dy <= 14 * 14 && _mpris.valid && _dbus) {
        mpris_play_pause(_dbus, _mpris.player_bus);
        _mpris.playing = !_mpris.playing;
        return;
    }
    dx = root_x - (ctrl_cx_l - gap_px); dy = root_y - ctrl_cy;
    if (dx * dx + dy * dy <= 10 * 10 && _mpris.valid && _dbus) {
        mpris_prev(_dbus, _mpris.player_bus);
        return;
    }
    dx = root_x - (ctrl_cx_l + gap_px); dy = root_y - ctrl_cy;
    if (dx * dx + dy * dy <= 10 * 10 && _mpris.valid && _dbus) {
        mpris_next(_dbus, _mpris.player_bus);
        return;
    }

    if (root_x < pan_x || root_x > pan_x + PAN_W ||
        root_y < pan_y || root_y > pan_y + PAN_H) {
        _pan_open = false;
        update_overlay_input();
    }
}

void Compositor::update_overlay_input() {
    if (_overlay == None) return;

    int pill_x = _mx + (_sw - _bar_pw) / 2;
    int pill_y_root = _my + 4;

    std::vector<XRectangle> rects;
    { XRectangle r; r.x = (short)pill_x; r.y = (short)pill_y_root;
      r.width = (unsigned short)_bar_pw; r.height = (unsigned short)_bar_ph;
      rects.push_back(r); }

    if (_pan_open) {
        int pan_x = _mx + (_sw - PAN_W) / 2;
        int pan_y = _my + BAR_H + 6;
        XRectangle r; r.x = (short)pan_x; r.y = (short)pan_y;
        r.width = (unsigned short)PAN_W; r.height = (unsigned short)PAN_H;
        rects.push_back(r);
    }

    XserverRegion rgn = XFixesCreateRegion(_dpy, rects.data(), (int)rects.size());
    XFixesSetWindowShapeRegion(_dpy, _overlay, ShapeInput, 0, 0, rgn);
    XFixesDestroyRegion(_dpy, rgn);
}

/* ======== Главный рендер ======== */

void Compositor::render(std::vector<WinInfo>& wins,
                        int view_x, int view_y,
                        int mx, int my, int sw, int sh)
{
    if (!_ok) return;

    struct timeval _rnow; gettimeofday(&_rnow, nullptr);
    double _rt = _rnow.tv_sec + _rnow.tv_usec * 1e-6;
    if (_rt - _last_render_t < 0.033) return;
    _last_render_t = _rt;

    glXMakeCurrent(_dpy, _glxwin, _ctx);
    glViewport(0, 0, _rw, _rh);

    auto draw_shadow = [&](int sx, int sy, int ww, int wh) {
        const int off = 5;
        float bx0 = (float)(sx + off)      / _rw * 2.f - 1.f;
        float bx1 = (float)(sx + off + ww) / _rw * 2.f - 1.f;
        float by0 = -(float)(sy + off)      / _rh * 2.f + 1.f;
        float by1 = -(float)(sy + off + wh) / _rh * 2.f + 1.f;
        float sv[24] = {bx0, by0, 0, 0, bx0, by1, 0, 1, bx1, by1, 1, 1,
                        bx0, by0, 0, 0, bx1, by1, 1, 1, bx1, by0, 1, 0};
        glBindBuffer(GL_ARRAY_BUFFER, _vbo);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(sv), sv);
        glBindVertexArray(_vao);
        glDrawArrays(GL_TRIANGLES, 0, 6);
    };

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glClearColor(0.07f, 0.07f, 0.07f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    for (auto& w : wins)
        if (w.dirty && !w.bypass) update_texture(w);

    for (auto& w : wins) {
        if (!w.texture || w.bypass) continue;
        int sx = w.x;
        int sy = w.y;
        if (!w.unmanaged) {
            glUseProgram(_prog_shadow);
            glUniform1f(_u_shad_alpha, 0.30f);
            draw_shadow(sx, sy, w.w, w.h);
        }
        glUseProgram(_prog_comp);
        draw_quad(sw, sh, 0, 0, sx, sy, w.w, w.h, w.texture, w.opacity);
    }

    render_bar(mx, sw);
    render_panel(mx, sw);
    glXSwapBuffers(_dpy, _glxwin);
    (void)view_x; (void)view_y; (void)my; (void)sh;
}
