#include <cstdio>
#include <X11/Xlib.h>
#include <GL/glx.h>
#include <GL/glxext.h>

int main() {
    Display* dpy = XOpenDisplay(nullptr);
    if (!dpy) { puts("no display"); return 1; }
    int screen = DefaultScreen(dpy);

    int nfbc = 0;
    GLXFBConfig* fbcs = glXGetFBConfigs(dpy, screen, &nfbc);
    printf("Всего FBConfig: %d\n", nfbc);

    int found_rgba = 0, found_rgb = 0;
    for (int i = 0; i < nfbc; i++) {
        int draw_type = 0, bind_rgba = 0, bind_rgb = 0, bind_tgt = 0;
        int r = 0, g = 0, b = 0, a = 0, depth = 0, dbl = 0, vid = 0;

        glXGetFBConfigAttrib(dpy, fbcs[i], GLX_DRAWABLE_TYPE, &draw_type);
        glXGetFBConfigAttrib(dpy, fbcs[i], GLX_BIND_TO_TEXTURE_RGBA_EXT, &bind_rgba);
        glXGetFBConfigAttrib(dpy, fbcs[i], GLX_BIND_TO_TEXTURE_RGB_EXT, &bind_rgb);
        glXGetFBConfigAttrib(dpy, fbcs[i], GLX_BIND_TO_TEXTURE_TARGETS_EXT, &bind_tgt);
        glXGetFBConfigAttrib(dpy, fbcs[i], GLX_RED_SIZE, &r);
        glXGetFBConfigAttrib(dpy, fbcs[i], GLX_GREEN_SIZE, &g);
        glXGetFBConfigAttrib(dpy, fbcs[i], GLX_BLUE_SIZE, &b);
        glXGetFBConfigAttrib(dpy, fbcs[i], GLX_ALPHA_SIZE, &a);
        glXGetFBConfigAttrib(dpy, fbcs[i], GLX_BUFFER_SIZE, &depth);
        glXGetFBConfigAttrib(dpy, fbcs[i], GLX_DOUBLEBUFFER, &dbl);
        glXGetFBConfigAttrib(dpy, fbcs[i], GLX_VISUAL_ID, &vid);

        bool has_pix = (draw_type & GLX_PIXMAP_BIT) != 0;
        bool has_2d  = (bind_tgt & GLX_TEXTURE_2D_BIT_EXT) != 0;

        if (has_pix && has_2d && (bind_rgba || bind_rgb)) {
            printf("  #%d: vid=0x%x rgba=%d/%d/%d/%d depth=%d dbl=%d "
                   "bind_rgba=%d bind_rgb=%d pix=%d 2d=%d\n",
                   i, vid, r, g, b, a, depth, dbl,
                   bind_rgba, bind_rgb, has_pix, has_2d);
            if (bind_rgba) found_rgba++;
            if (bind_rgb) found_rgb++;
        }
    }
    printf("Итого: bind_rgba=%d, bind_rgb=%d\n", found_rgba, found_rgb);
    XFree(fbcs);
    XCloseDisplay(dpy);
    return 0;
}
