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

bool Compositor::init(int sw, int sh, int mx, int my, bool nvidia) {
    _mx = mx; _my = my; _sw = sw;
    _nvidia = nvidia;
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

    XWindowAttributes ov_wa;
    XGetWindowAttributes(_dpy, _overlay, &ov_wa);
    VisualID ov_vid = XVisualIDFromVisual(ov_wa.visual);
    fprintf(stderr, "nemac: overlay visual=0x%lx depth=%d\n",
            (unsigned long)ov_vid, ov_wa.depth);
    fflush(stderr);

    int ov_attr[] = {
        GLX_DOUBLEBUFFER,  True,
        GLX_RED_SIZE,      8,
        GLX_GREEN_SIZE,    8,
        GLX_BLUE_SIZE,     8,
        GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
        None
    };
    int nfbc;
    GLXFBConfig* ov_fbcs = glXChooseFBConfig(_dpy, _screen, ov_attr, &nfbc);
    if (!ov_fbcs || nfbc == 0) {
        fprintf(stderr, "nemac: нет FBConfig для overlay\n");
        return false;
    }

    GLXFBConfig ov_fbc = nullptr;
    for (int i = 0; i < nfbc; i++) {
        XVisualInfo* vi = glXGetVisualFromFBConfig(_dpy, ov_fbcs[i]);
        if (vi && vi->visualid == ov_vid) {
            ov_fbc = ov_fbcs[i];
            XFree(vi);
            fprintf(stderr, "nemac: FBConfig #%d совпал с overlay visual\n", i);
            fflush(stderr);
            break;
        }
        if (vi) XFree(vi);
    }
    if (!ov_fbc) {
        fprintf(stderr, "nemac: точное совпадение не найдено, пробуем первый FBConfig\n");
        fflush(stderr);
        ov_fbc = ov_fbcs[0];
    }
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

    {
        int all_nfbc = 0;
        GLXFBConfig* all_fbcs = glXGetFBConfigs(_dpy, _screen, &all_nfbc);
        if (!all_fbcs || all_nfbc == 0) {
            fprintf(stderr, "nemac: нет FBConfig вообще\n");
            return false;
        }
        fprintf(stderr, "nemac: всего FBConfig: %d\n", all_nfbc);
        fprintf(stderr, "nemac: константы: BIND_RGBA=0x%x BIND_RGB=0x%x TGT=0x%x 2D_BIT=0x%x PIX_BIT=0x%x\n",
                GLX_BIND_TO_TEXTURE_RGBA_EXT, GLX_BIND_TO_TEXTURE_RGB_EXT,
                GLX_BIND_TO_TEXTURE_TARGETS_EXT, GLX_TEXTURE_2D_BIT_EXT, GLX_PIXMAP_BIT);
        fflush(stderr);

        _pix_fbc = nullptr;
        _pix_rgba = false;
        _fbc_ok = false;
        int dbg_count = 0;
        for (int i = 0; i < all_nfbc; i++) {
            int draw = 0, bind_rgba = 0, bind_rgb = 0, tgt = 0;
            int r = 0, g = 0, b = 0, a = 0, dbl = 0;
            glXGetFBConfigAttrib(_dpy, all_fbcs[i], GLX_DRAWABLE_TYPE, &draw);
            glXGetFBConfigAttrib(_dpy, all_fbcs[i], GLX_BIND_TO_TEXTURE_RGBA_EXT, &bind_rgba);
            glXGetFBConfigAttrib(_dpy, all_fbcs[i], GLX_BIND_TO_TEXTURE_RGB_EXT, &bind_rgb);
            glXGetFBConfigAttrib(_dpy, all_fbcs[i], GLX_BIND_TO_TEXTURE_TARGETS_EXT, &tgt);
            glXGetFBConfigAttrib(_dpy, all_fbcs[i], GLX_RED_SIZE, &r);
            glXGetFBConfigAttrib(_dpy, all_fbcs[i], GLX_GREEN_SIZE, &g);
            glXGetFBConfigAttrib(_dpy, all_fbcs[i], GLX_BLUE_SIZE, &b);
            glXGetFBConfigAttrib(_dpy, all_fbcs[i], GLX_ALPHA_SIZE, &a);
            glXGetFBConfigAttrib(_dpy, all_fbcs[i], GLX_DOUBLEBUFFER, &dbl);
            if (dbg_count < 3 && (draw & GLX_PIXMAP_BIT) && r >= 8) {
                fprintf(stderr, "nemac: dbg #%d draw=0x%x r=%d g=%d b=%d a=%d dbl=%d "
                        "bind_rgba=%d bind_rgb=%d tgt=0x%x\n",
                        i, draw, r, g, b, a, dbl, bind_rgba, bind_rgb, tgt);
                dbg_count++;
            }
            if (!(draw & GLX_PIXMAP_BIT)) continue;
            if (!(tgt & GLX_TEXTURE_2D_BIT_EXT)) continue;
            if (!bind_rgba && !bind_rgb) continue;
            if (r < 8 || g < 8 || b < 8) continue;
            _pix_fbc = all_fbcs[i];
            _pix_rgba = bind_rgba ? true : false;
            _fbc_ok = true;
            fprintf(stderr, "nemac: texture-from-pixmap: FBConfig #%d rgba=%d/%d/%d/%d bind=%s\n",
                    i, r, g, b, a, _pix_rgba ? "RGBA" : "RGB");
            fflush(stderr);
            break;
        }
        XFree(all_fbcs);
        if (!_fbc_ok) {
            fprintf(stderr, "nemac: нет FBConfig для texture-from-pixmap (проверено %d)\n", all_nfbc);
            fflush(stderr);
            return false;
        }
    }
    fflush(stderr);

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

void Compositor::draw_quad(int win_x, int win_y, int win_w, int win_h,
                           GLuint tex, float opacity)
{
    float x0 = (float)win_x           / _rw * 2.0f - 1.0f;
    float x1 = (float)(win_x + win_w) / _rw * 2.0f - 1.0f;
    float y0 = -(float)win_y           / _rh * 2.0f + 1.0f;
    float y1 = -(float)(win_y + win_h) / _rh * 2.0f + 1.0f;

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
