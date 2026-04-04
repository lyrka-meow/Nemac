#include "monitor.h"
#include <X11/extensions/Xinerama.h>
#include <cstdio>
#include <algorithm>

std::vector<MonitorInfo> Monitor::detect(Display* dpy) {
    std::vector<MonitorInfo> result;
    int screen = DefaultScreen(dpy);

    int count = 0;
    XineramaScreenInfo* screens = XineramaQueryScreens(dpy, &count);

    if (screens && count > 0) {
        for (int i = 0; i < count; i++) {
            MonitorInfo m;
            m.x       = screens[i].x_org;
            m.y       = screens[i].y_org;
            m.width   = screens[i].width;
            m.height  = screens[i].height;
            m.primary = (i == 0);
            result.push_back(m);
        }
        XFree(screens);
    }

    if (result.empty()) {
        MonitorInfo m;
        m.x       = 0;
        m.y       = 0;
        m.width   = DisplayWidth(dpy, screen);
        m.height  = DisplayHeight(dpy, screen);
        m.primary = true;
        result.push_back(m);
    }

    fprintf(stderr, "nemac: обнаружено мониторов: %d\n", (int)result.size());
    for (int i = 0; i < (int)result.size(); i++)
        fprintf(stderr, "  [%d] %dx%d+%d+%d%s\n", i,
                result[i].width, result[i].height,
                result[i].x, result[i].y,
                result[i].primary ? " (главный)" : "");

    return result;
}

int Monitor::find_primary(const std::vector<MonitorInfo>& monitors) {
    for (int i = 0; i < (int)monitors.size(); i++)
        if (monitors[i].primary) return i;
    return 0;
}

void Monitor::set_primary(std::vector<MonitorInfo>& monitors, int idx) {
    for (auto& m : monitors) m.primary = false;
    if (idx >= 0 && idx < (int)monitors.size())
        monitors[idx].primary = true;
}
