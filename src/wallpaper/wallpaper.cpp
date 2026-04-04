#include "wallpaper.h"
#include <Imlib2.h>
#include <X11/Xatom.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <dirent.h>
#include <algorithm>

bool Wallpaper::set(Display* dpy, int screen, const std::string& path) {
    if (path.empty()) return false;

    Imlib_Image img = imlib_load_image(path.c_str());
    if (!img) {
        fprintf(stderr, "nemac: не удалось загрузить обои: %s\n", path.c_str());
        return false;
    }

    int sw = DisplayWidth(dpy, screen);
    int sh = DisplayHeight(dpy, screen);
    Window root = RootWindow(dpy, screen);

    imlib_context_set_image(img);
    int iw = imlib_image_get_width();
    int ih = imlib_image_get_height();

    Imlib_Image scaled = imlib_create_cropped_scaled_image(0, 0, iw, ih, sw, sh);
    imlib_free_image();
    if (!scaled) return false;

    imlib_context_set_image(scaled);
    imlib_context_set_display(dpy);
    imlib_context_set_visual(DefaultVisual(dpy, screen));
    imlib_context_set_colormap(DefaultColormap(dpy, screen));

    Pixmap pm = XCreatePixmap(dpy, root, sw, sh, DefaultDepth(dpy, screen));
    imlib_context_set_drawable(pm);
    imlib_render_image_on_drawable(0, 0);
    imlib_free_image();

    XSetWindowBackgroundPixmap(dpy, root, pm);
    XClearWindow(dpy, root);

    Atom prop_root = XInternAtom(dpy, "_XROOTPMAP_ID", False);
    Atom prop_esetroot = XInternAtom(dpy, "ESETROOT_PMAP_ID", False);
    XChangeProperty(dpy, root, prop_root, XA_PIXMAP, 32,
                    PropModeReplace, (unsigned char*)&pm, 1);
    XChangeProperty(dpy, root, prop_esetroot, XA_PIXMAP, 32,
                    PropModeReplace, (unsigned char*)&pm, 1);

    XFlush(dpy);
    fprintf(stderr, "nemac: обои установлены: %s\n", path.c_str());
    return true;
}

std::string Wallpaper::pick_random(const std::string& dir) {
    auto files = list(dir);
    if (files.empty()) return "";
    srand((unsigned)time(nullptr));
    return files[rand() % files.size()];
}

std::vector<std::string> Wallpaper::list(const std::string& dir) {
    std::vector<std::string> result;
    DIR* d = opendir(dir.c_str());
    if (!d) return result;

    struct dirent* ent;
    while ((ent = readdir(d)) != nullptr) {
        std::string name = ent->d_name;
        if (name == "." || name == "..") continue;
        std::string lower = name;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        if (lower.find(".jpg") != std::string::npos ||
            lower.find(".jpeg") != std::string::npos ||
            lower.find(".png") != std::string::npos ||
            lower.find(".webp") != std::string::npos) {
            result.push_back(dir + "/" + name);
        }
    }
    closedir(d);
    std::sort(result.begin(), result.end());
    return result;
}

void Wallpaper::init_defaults(Display* dpy, int screen, const std::string& data_dir) {
    std::string wall_dir = data_dir + "/wallpapers";
    std::string user_dir = std::string(getenv("HOME") ? getenv("HOME") : "/tmp") +
                           "/.local/share/nemac/wallpapers";

    auto user_walls = list(user_dir);
    if (!user_walls.empty()) {
        std::string chosen = user_walls[rand() % user_walls.size()];
        set(dpy, screen, chosen);
        return;
    }

    auto default_walls = list(wall_dir);
    if (!default_walls.empty()) {
        std::string chosen = default_walls[rand() % default_walls.size()];
        set(dpy, screen, chosen);
    }
}
