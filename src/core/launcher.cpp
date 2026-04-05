#include "launcher.h"
#include "../updater/updater.h"
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <sys/stat.h>
#include <string>

void ensure_dirs() {
    const char* home = getenv("HOME");
    if (!home) return;
    std::string base = std::string(home) + "/.local/share/nemac";
    mkdir(base.c_str(), 0755);
    mkdir((base + "/wallpapers").c_str(), 0755);
}

void launch_panel() {
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

void launch_updater(const char* version) {
    pid_t pid = fork();
    if (pid == 0) {
        Updater::run_daemon(version);
        _exit(0);
    }
    if (pid > 0)
        fprintf(stderr, "nemac: демон обновлений запущен pid=%d\n", pid);
}
