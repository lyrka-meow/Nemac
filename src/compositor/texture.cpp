#define GLEW_STATIC
#include "compositor.h"
#include <cstdio>

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
            GLX_TEXTURE_FORMAT_EXT, _pix_rgba ? GLX_TEXTURE_FORMAT_RGBA_EXT
                                              : GLX_TEXTURE_FORMAT_RGB_EXT,
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
