#pragma once
#include <X11/Xlib.h>
#include <vector>

struct MonitorInfo {
    int x      = 0;
    int y      = 0;
    int width  = 0;
    int height = 0;
    bool primary = false;
};

namespace Monitor {
    std::vector<MonitorInfo> detect(Display* dpy);
    int find_primary(const std::vector<MonitorInfo>& monitors);
    void set_primary(std::vector<MonitorInfo>& monitors, int idx);
}
