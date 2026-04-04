#include "../gpu/gpu.h"
#include "../monitor/monitor.h"
#include "../compositor/compositor.h"
#include "../wm/wm.h"
#include "../wallpaper/wallpaper.h"
#include "../updater/updater.h"
#include <X11/Xlib.h>
#include <X11/XKBlib.h>
#include <X11/keysym.h>
#include <X11/extensions/Xdamage.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <signal.h>

#define NEMAC_VERSION "0.1.0"
#define NEMAC_DATA_DIR "/usr/share/nemac"

static int x_error_handler(Display* d, XErrorEvent* e) {
    char msg[256] = {};
    XGetErrorText(d, e->error_code, msg, sizeof(msg));
    fprintf(stderr, "nemac: X ошибка: %s (req=%d res=0x%lx)\n",
            msg, e->request_code, e->resourceid);
    return 0;
}

static void ensure_dirs() {
    const char* home = getenv("HOME");
    if (!home) return;
    std::string base = std::string(home) + "/.local/share/nemac";
    mkdir(base.c_str(), 0755);
    mkdir((base + "/wallpapers").c_str(), 0755);
}

static void launch_panel() {
    pid_t pid = fork();
    if (pid == 0) {
        setsid();
        execlp("nemac-panel", "nemac-panel", nullptr);
        fprintf(stderr, "nemac: не удалось запустить панель\n");
        _exit(1);
    }
    if (pid > 0)
        fprintf(stderr, "nemac: панель запущена pid=%d\n", pid);
}

static void launch_updater() {
    pid_t pid = fork();
    if (pid == 0) {
        Updater::run_daemon(NEMAC_VERSION);
        _exit(0);
    }
    if (pid > 0)
        fprintf(stderr, "nemac: демон обновлений запущен pid=%d\n", pid);
}

int main() {
    freopen("/tmp/nemac.log", "w", stderr);
    fprintf(stderr, "nemac: запуск v%s\n", NEMAC_VERSION);
    fflush(stderr);

    ensure_dirs();

    /* ======== GPU ======== */
    GpuInfo gpu = Gpu::detect();
    Gpu::apply_env(gpu);

    /* ======== X11 ======== */
    Display* dpy = XOpenDisplay(nullptr);
    if (!dpy) {
        fputs("nemac: не удалось открыть дисплей\n", stderr);
        return 1;
    }

    Window root = DefaultRootWindow(dpy);
    int screen  = DefaultScreen(dpy);

    /* ======== Мониторы ======== */
    auto monitors = Monitor::detect(dpy);
    int primary_idx = Monitor::find_primary(monitors);
    auto& pm = monitors[primary_idx];

    /* ======== Обои ======== */
    Wallpaper::init_defaults(dpy, screen, NEMAC_DATA_DIR);

    /* ======== Композитор ======== */
    Compositor* comp = new Compositor(dpy, root, screen);
    if (!comp->init(pm.width, pm.height, pm.x, pm.y)) {
        fprintf(stderr, "nemac: композитор не инициализирован, работа без эффектов\n");
        delete comp;
        comp = nullptr;
    }

    /* ======== WM ======== */
    WM wm(dpy, root);
    wm.init(monitors, comp);

    XSetErrorHandler(x_error_handler);
    XSelectInput(dpy, root,
                 SubstructureRedirectMask | SubstructureNotifyMask |
                 KeyPressMask);

    /* ======== Привязки клавиш ======== */
    unsigned int MOD = Mod1Mask;
    XUngrabKey(dpy, AnyKey, AnyModifier, root);
    struct { unsigned int mod; KeySym sym; } binds[] = {
        {MOD,             XK_Return},
        {MOD,             XK_q},
        {MOD,             XK_p},
        {MOD,             XK_f},
        {MOD | ShiftMask, XK_q},
        {MOD | ShiftMask, XK_Left},
        {MOD | ShiftMask, XK_Right},
    };
    unsigned int extra[] = {0, LockMask, Mod2Mask, LockMask | Mod2Mask};
    for (auto& b : binds) {
        KeyCode kc = XKeysymToKeycode(dpy, b.sym);
        for (auto x : extra)
            XGrabKey(dpy, kc, b.mod | x, root, True, GrabModeAsync, GrabModeAsync);
    }

    /* ======== Запуск параллельных процессов ======== */
    launch_panel();
    launch_updater();

    fprintf(stderr, "nemac: все компоненты запущены\n");
    fflush(stderr);

    /* ======== Главный цикл ======== */
    struct timeval tv_last;
    gettimeofday(&tv_last, nullptr);

    XEvent ev;
    for (;;) {
        int xfd = ConnectionNumber(dpy);
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(xfd, &fds);
        struct timeval timeout = {0, 50000};
        select(xfd + 1, &fds, nullptr, nullptr, &timeout);

        struct timeval now;
        gettimeofday(&now, nullptr);
        double dt = (now.tv_sec - tv_last.tv_sec) +
                    (now.tv_usec - tv_last.tv_usec) * 1e-6;
        tv_last = now;

        bool dirty = comp && comp->tick(dt);

        while (XPending(dpy)) {
            XNextEvent(dpy, &ev);

            if (comp && ev.type == comp->damage_event + XDamageNotify) {
                XDamageNotifyEvent* de = (XDamageNotifyEvent*)&ev;
                comp->mark_dirty(de->drawable, wm.windows());
                XDamageSubtract(dpy, de->damage, None, None);
                dirty = true;
                continue;
            }

            switch (ev.type) {
            case MapRequest:
                wm.handle_map_request(&ev.xmaprequest);
                break;

            case MapNotify: {
                Window w = ev.xmap.window;
                if (!comp || wm.find_win(w) >= 0) break;
                XWindowAttributes wa;
                if (!XGetWindowAttributes(dpy, w, &wa)) break;
                if (!wa.override_redirect || wa.c_class == InputOnly) break;
                WinInfo nw;
                nw.xwin      = w;
                nw.x         = wa.x;
                nw.y         = wa.y;
                nw.w         = wa.width;
                nw.h         = wa.height;
                nw.unmanaged = true;
                wm.windows().push_back(nw);
                comp->add_window(wm.windows().back());
                dirty = true;
                break;
            }

            case ConfigureNotify:
                wm.handle_configure_notify(&ev.xconfigure);
                break;

            case DestroyNotify:
                wm.handle_destroy(ev.xdestroywindow.window);
                break;

            case UnmapNotify:
                wm.handle_destroy(ev.xunmap.window);
                break;

            case ConfigureRequest:
                wm.handle_configure_request(&ev.xconfigurerequest);
                break;

            case PropertyNotify:
                if (comp) {
                    if (ev.xproperty.atom == comp->opacity_atom) {
                        int idx = wm.find_win(ev.xproperty.window);
                        if (idx >= 0) comp->add_window(wm.windows()[idx]);
                    } else if (ev.xproperty.atom == comp->bypass_atom) {
                        comp->on_property_change(ev.xproperty.window,
                                                 ev.xproperty.atom, wm.windows());
                        dirty = true;
                    }
                }
                break;

            case ButtonPress:
                if (comp && ev.xbutton.window == comp->overlay()) {
                    comp->handle_click(ev.xbutton.x_root, ev.xbutton.y_root);
                    auto& m = monitors[wm.primary_monitor()];
                    comp->render(wm.windows(), wm.view_x(), wm.view_y(),
                                 m.x, m.y, m.width, m.height);
                } else {
                    wm.handle_button_press(&ev.xbutton);
                }
                break;

            case ButtonRelease:
                wm.handle_button_release(&ev.xbutton);
                break;

            case MotionNotify:
                wm.handle_motion(&ev.xmotion);
                break;

            case KeyPress:
                wm.handle_key(&ev.xkey);
                break;
            }
        }

        if (comp && dirty) {
            auto& m = monitors[wm.primary_monitor()];
            comp->render(wm.windows(), wm.view_x(), wm.view_y(),
                         m.x, m.y, m.width, m.height);
        }
    }

    delete comp;
    XCloseDisplay(dpy);
    return 0;
}
