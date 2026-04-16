#define GLEW_STATIC
#include "compositor.h"
#include "shaders.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/stat.h>

static void ensure_gpu_compositing_flag(const char* filename) {
    const char* home = getenv("HOME");
    if (!home) return;
    char path[512];
    snprintf(path, sizeof(path), "%s/.config/%s", home, filename);
    const char* flag = "--disable-gpu-compositing";
    FILE* f = fopen(path, "r");
    if (f) {
        char buf[256];
        while (fgets(buf, sizeof(buf), f)) {
            if (strstr(buf, flag)) { fclose(f); return; }
        }
        fclose(f);
        f = fopen(path, "a");
        if (f) { fprintf(f, "%s\n", flag); fclose(f); }
    } else {
        f = fopen(path, "w");
        if (f) { fprintf(f, "%s\n", flag); fclose(f); }
    }
}

Compositor::Compositor(Display* dpy, Window root, int screen)
    : _dpy(dpy), _root(root), _screen(screen)
{
    opacity_atom = XInternAtom(dpy, "_NET_WM_WINDOW_OPACITY", False);
    bypass_atom  = XInternAtom(dpy, "_NET_WM_BYPASS_COMPOSITOR", False);
}

Compositor::~Compositor() {
    if (!_ok) return;
    glXMakeCurrent(_dpy, _glxwin, _ctx);
    if (_prog_comp)  glDeleteProgram(_prog_comp);
    if (_prog_shadow)glDeleteProgram(_prog_shadow);
    if (_vao) { glDeleteVertexArrays(1, &_vao); glDeleteBuffers(1, &_vbo); }
    glXMakeCurrent(_dpy, None, nullptr);
    if (_glxwin != None) glXDestroyWindow(_dpy, _glxwin);
    if (_ctx)            glXDestroyContext(_dpy, _ctx);
    if (_overlay != None) XCompositeReleaseOverlayWindow(_dpy, _root);
}

bool Compositor::init(bool nvidia) {
    _nvidia = nvidia;
    _rw = DisplayWidth(_dpy, _screen);
    _rh = DisplayHeight(_dpy, _screen);
    fprintf(stderr, "nemac: композитор %dx%d nvidia=%d\n", _rw, _rh, _nvidia);
    fflush(stderr);

    if (_nvidia) {
        ensure_gpu_compositing_flag("chromium-flags.conf");
        ensure_gpu_compositing_flag("chrome-flags.conf");
        ensure_gpu_compositing_flag("brave-flags.conf");
    }
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
            break;
        }
        if (vi) XFree(vi);
    }
    if (!ov_fbc) ov_fbc = ov_fbcs[0];
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
        fprintf(stderr, "nemac: total FBConfigs: %d\n", all_nfbc);
        if (all_fbcs && all_nfbc > 0) {
            for (int pass = 0; pass < 2; pass++) {
                for (int i = 0; i < all_nfbc; i++) {
                    int draw = 0, r = 0, g = 0, b = 0, a = 0, dbl = 0;
                    glXGetFBConfigAttrib(_dpy, all_fbcs[i], GLX_DRAWABLE_TYPE, &draw);
                    if (!(draw & GLX_PIXMAP_BIT)) continue;
                    glXGetFBConfigAttrib(_dpy, all_fbcs[i], GLX_DOUBLEBUFFER, &dbl);
                    if (pass == 0 && dbl) continue;
                    if (pass == 1 && !dbl) continue;
                    glXGetFBConfigAttrib(_dpy, all_fbcs[i], GLX_RED_SIZE, &r);
                    glXGetFBConfigAttrib(_dpy, all_fbcs[i], GLX_GREEN_SIZE, &g);
                    glXGetFBConfigAttrib(_dpy, all_fbcs[i], GLX_BLUE_SIZE, &b);
                    glXGetFBConfigAttrib(_dpy, all_fbcs[i], GLX_ALPHA_SIZE, &a);
                    if (r < 8 || g < 8 || b < 8) continue;
                    XVisualInfo* vi = glXGetVisualFromFBConfig(_dpy, all_fbcs[i]);
                    if (vi) {
                        if (_vis_fbc.find(vi->visualid) == _vis_fbc.end()) {
                            bool has_alpha = (a > 0);
                            _vis_fbc[vi->visualid] = {all_fbcs[i], has_alpha, false};
                            fprintf(stderr, "nemac: tfp visual 0x%lx depth=%d alpha=%d dbl=%d\n",
                                    vi->visualid, vi->depth, a, dbl);
                        }
                        XFree(vi);
                    }
                }
            }
            XFree(all_fbcs);
        }
        fprintf(stderr, "nemac: texture-from-pixmap: %d visuals mapped\n", (int)_vis_fbc.size());
        fflush(stderr);
    }

    glEnable(GL_BLEND);
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA,
                        GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    _prog_comp   = make_program(VERT_SRC, FRAG_COMP);
    _prog_shadow = make_program(VERT_SRC, FRAG_SHADOW);
    if (!_prog_comp || !_prog_shadow) return false;

    _u_comp_tex     = glGetUniformLocation(_prog_comp,   "uTex");
    _u_comp_opacity = glGetUniformLocation(_prog_comp,   "uOpacity");
    _u_comp_opaque  = glGetUniformLocation(_prog_comp,   "uOpaqueAlpha");
    _u_shad_alpha   = glGetUniformLocation(_prog_shadow, "uAlpha");
    _u_shad_color   = glGetUniformLocation(_prog_shadow, "uColor");

    _prog_blur = make_program(VERT_SRC, FRAG_BLUR);
    if (!_prog_blur) return false;
    glUseProgram(_prog_blur);
    glUniform1i(glGetUniformLocation(_prog_blur, "uTex"), 0);
    _u_blur_dir = glGetUniformLocation(_prog_blur, "uDir");

    auto glXSwapIntervalEXT_fn = (PFNGLXSWAPINTERVALEXTPROC)
        glXGetProcAddressARB((const GLubyte*)"glXSwapIntervalEXT");
    if (glXSwapIntervalEXT_fn)
        glXSwapIntervalEXT_fn(_dpy, _glxwin, 1);
    else {
        auto glXSwapIntervalMESA_fn = (PFNGLXSWAPINTERVALMESAPROC)
            glXGetProcAddressARB((const GLubyte*)"glXSwapIntervalMESA");
        if (glXSwapIntervalMESA_fn) glXSwapIntervalMESA_fn(1);
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

    _ok = true;
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

void Compositor::capture_wallpaper() {
    if (_wall_ok && _wall_tex) return;

    Atom prop = XInternAtom(_dpy, "_XROOTPMAP_ID", True);
    if (prop == None) return;

    Atom actual; int fmt;
    unsigned long n, left;
    unsigned char* data = nullptr;
    if (XGetWindowProperty(_dpy, _root, prop, 0, 1, False, XA_PIXMAP,
                           &actual, &fmt, &n, &left, &data) != Success || !data)
        return;

    Pixmap pm = *(Pixmap*)data;
    XFree(data);
    if (!pm) return;

    glXWaitX();
    XImage* img = XGetImage(_dpy, pm, 0, 0, _rw, _rh, AllPlanes, ZPixmap);
    if (!img) return;

    if (!_wall_tex) glGenTextures(1, &_wall_tex);
    glBindTexture(GL_TEXTURE_2D, _wall_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, _rw, _rh, 0,
                 GL_BGRA, GL_UNSIGNED_BYTE, img->data);
    XDestroyImage(img);
    _wall_ok = true;
}

void Compositor::init_blur(int w, int h) {
    if (_blur_w == w && _blur_h == h) return;
    _blur_w = w; _blur_h = h;

    if (_blur_fbo[0]) glDeleteFramebuffers(2, _blur_fbo);
    if (_blur_tex[0]) glDeleteTextures(2, _blur_tex);
    if (_blur_src)    glDeleteTextures(1, &_blur_src);

    auto mk = [](GLuint tex, int tw, int th) {
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, tw, th, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    };

    glGenTextures(1, &_blur_src);
    mk(_blur_src, w, h);

    glGenTextures(2, _blur_tex);
    glGenFramebuffers(2, _blur_fbo);
    for (int i = 0; i < 2; i++) {
        mk(_blur_tex[i], w, h);
        glBindFramebuffer(GL_FRAMEBUFFER, _blur_fbo[i]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                               GL_TEXTURE_2D, _blur_tex[i], 0);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Compositor::blur_region(int x, int y, int w, int h) {
    init_blur(w, h);

    glBindTexture(GL_TEXTURE_2D, _blur_src);
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, x, _rh - y - h, w, h);

    static const float fq[24] = {
        -1, -1, 0, 0,  -1, 1, 0, 1,  1, 1, 1, 1,
        -1, -1, 0, 0,   1, 1, 1, 1,  1, -1, 1, 0,
    };

    glUseProgram(_prog_blur);
    glDisable(GL_BLEND);

    glBindBuffer(GL_ARRAY_BUFFER, _vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(fq), fq);
    glBindVertexArray(_vao);

    GLuint src = _blur_src;
    for (int p = 0; p < 3; p++) {
        glBindFramebuffer(GL_FRAMEBUFFER, _blur_fbo[0]);
        glViewport(0, 0, w, h);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, src);
        glUniform2f(_u_blur_dir, 1.0f / w, 0.0f);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        glBindFramebuffer(GL_FRAMEBUFFER, _blur_fbo[1]);
        glBindTexture(GL_TEXTURE_2D, _blur_tex[0]);
        glUniform2f(_u_blur_dir, 0.0f, 1.0f / h);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        src = _blur_tex[1];
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, _rw, _rh);
    glEnable(GL_BLEND);

    float bx0 = (float)x       / _rw * 2.0f - 1.0f;
    float bx1 = (float)(x + w) / _rw * 2.0f - 1.0f;
    float by0 = -(float)y       / _rh * 2.0f + 1.0f;
    float by1 = -(float)(y + h) / _rh * 2.0f + 1.0f;

    float bv[24] = {
        bx0, by0, 0.0f, 1.0f,
        bx0, by1, 0.0f, 0.0f,
        bx1, by1, 1.0f, 0.0f,
        bx0, by0, 0.0f, 1.0f,
        bx1, by1, 1.0f, 0.0f,
        bx1, by0, 1.0f, 1.0f,
    };
    glBindBuffer(GL_ARRAY_BUFFER, _vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(bv), bv);

    glUseProgram(_prog_comp);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, _blur_tex[1]);
    glUniform1i(_u_comp_tex, 0);
    glUniform1f(_u_comp_opacity, 1.0f);
    glUniform1i(_u_comp_opaque, 1);
    glBindVertexArray(_vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

