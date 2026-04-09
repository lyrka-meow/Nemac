#include "launcher.h"
#include "../updater/updater.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/stat.h>
#include <string>

static void setenv_default(const char* name, const char* val) {
    if (!getenv(name)) setenv(name, val, 0);
}

void ensure_session() {
    const char* home = getenv("HOME");
    if (home) {
        setenv_default("XDG_DATA_HOME",   (std::string(home) + "/.local/share").c_str());
        setenv_default("XDG_CONFIG_HOME", (std::string(home) + "/.config").c_str());
        setenv_default("XDG_CACHE_HOME",  (std::string(home) + "/.cache").c_str());
    }
    setenv_default("XDG_DATA_DIRS",   "/usr/local/share:/usr/share");
    setenv_default("XDG_SESSION_TYPE", "x11");

    if (!getenv("DBUS_SESSION_BUS_ADDRESS")) {
        FILE* fp = popen("dbus-launch --sh-syntax", "r");
        if (!fp) { fprintf(stderr, "nemac: dbus-launch не найден\n"); return; }
        char line[512];
        while (fgets(line, sizeof(line), fp)) {
            char* eq = strchr(line, '=');
            if (!eq) continue;
            *eq = '\0';
            char* val = eq + 1;
            char* end = strchr(val, ';');
            if (end) *end = '\0';
            if (val[0] == '\'') { val++; char* q = strchr(val, '\''); if (q) *q = '\0'; }
            setenv(line, val, 1);
        }
        pclose(fp);
    }
    const char* dbus = getenv("DBUS_SESSION_BUS_ADDRESS");
    fprintf(stderr, "nemac: dbus=%s\n", dbus ? dbus : "(нет)");
    fflush(stderr);

    setenv_default("XDG_CURRENT_DESKTOP", "Nemac");
    setenv_default("DESKTOP_SESSION",    "nemac");
    setenv_default("GSK_RENDERER",       "cairo");
    setenv_default("GDK_RENDERING",      "image");

    system("systemctl --user import-environment DISPLAY XAUTHORITY "
           "DBUS_SESSION_BUS_ADDRESS XDG_DATA_HOME XDG_CONFIG_HOME "
           "XDG_CACHE_HOME XDG_DATA_DIRS XDG_SESSION_TYPE "
           "XDG_CURRENT_DESKTOP 2>/dev/null");
    system("dbus-update-activation-environment --systemd DISPLAY "
           "XAUTHORITY DBUS_SESSION_BUS_ADDRESS XDG_CURRENT_DESKTOP 2>/dev/null");

    const char* polkit_agents[] = {
        "/usr/lib/polkit-gnome/polkit-gnome-authentication-agent-1",
        "/usr/lib/polkit-kde-authentication-agent-1",
        nullptr
    };
    for (int i = 0; polkit_agents[i]; i++) {
        if (access(polkit_agents[i], X_OK) == 0) {
            pid_t pid = fork();
            if (pid == 0) { setsid(); execl(polkit_agents[i], polkit_agents[i], nullptr); _exit(1); }
            if (pid > 0) fprintf(stderr, "nemac: polkit agent pid=%d\n", pid);
            break;
        }
    }
    fflush(stderr);
}

void ensure_dirs() {
    const char* home = getenv("HOME");
    if (!home) return;
    std::string base = std::string(home) + "/.local/share/nemac";
    mkdir(base.c_str(), 0755);
    mkdir((base + "/wallpapers").c_str(), 0755);
}

void launch_panel() {
    std::string path = "nemac-panel";

    char self[4096];
    ssize_t len = readlink("/proc/self/exe", self, sizeof(self) - 1);
    if (len > 0) {
        self[len] = '\0';
        std::string dir(self);
        auto pos = dir.rfind('/');
        if (pos != std::string::npos) {
            std::string local = dir.substr(0, pos) + "/panel/nemac-panel";
            if (access(local.c_str(), X_OK) == 0)
                path = local;
        }
    }

    pid_t pid = fork();
    if (pid == 0) {
        setsid();
        execl(path.c_str(), "nemac-panel", nullptr);
        _exit(1);
    }
    if (pid > 0)
        fprintf(stderr, "nemac: панель запущена pid=%d\n", pid);
}

void launch_updater(const char* version) {
    pid_t pid = fork();
    if (pid == 0) {
        Updater::run_daemon(version);
        _exit(0);
    }
    if (pid > 0)
        fprintf(stderr, "nemac: демон обновлений запущен pid=%d\n", pid);
}
