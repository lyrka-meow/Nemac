#pragma once
#include <X11/Xlib.h>
#include "../monitor/monitor.h"
#include <string>
#include <vector>

namespace Wallpaper {
    bool set(Display* dpy, int screen, const std::string& path,
            const std::vector<MonitorInfo>& monitors);
    std::string pick_random(const std::string& dir);
    std::vector<std::string> list(const std::string& dir);
    void init_defaults(Display* dpy, int screen, const std::string& data_dir);
}
