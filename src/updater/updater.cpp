#include "updater.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <fstream>
#include <sstream>

#define NEMAC_REPO "lyrka-meow/Nemac"
#define CHECK_INTERVAL_SEC 3600

static std::string exec_cmd(const char* cmd) {
    char buf[4096];
    std::string out;
    FILE* pipe = popen(cmd, "r");
    if (!pipe) return out;
    while (fgets(buf, sizeof(buf), pipe))
        out += buf;
    pclose(pipe);
    return out;
}

Updater::ReleaseInfo Updater::check_update(const std::string& current_version) {
    ReleaseInfo info;

    std::string cmd = "curl -fsSL --connect-timeout 10 "
                      "https://api.github.com/repos/" NEMAC_REPO "/releases/latest "
                      "2>/dev/null";
    std::string json = exec_cmd(cmd.c_str());
    if (json.empty()) return info;

    auto extract = [&](const std::string& key) -> std::string {
        std::string needle = "\"" + key + "\":";
        size_t pos = json.find(needle);
        if (pos == std::string::npos) return "";
        pos += needle.size();
        while (pos < json.size() && (json[pos] == ' ' || json[pos] == '"')) pos++;
        size_t end = json.find('"', pos);
        if (end == std::string::npos) return "";
        return json.substr(pos, end - pos);
    };

    info.tag = extract("tag_name");
    info.download_url = extract("browser_download_url");

    if (!info.tag.empty() && info.tag != current_version) {
        info.available = true;
        fprintf(stderr, "nemac: доступно обновление %s -> %s\n",
                current_version.c_str(), info.tag.c_str());
    }

    return info;
}

bool Updater::download_and_apply(const ReleaseInfo& release) {
    if (release.download_url.empty()) return false;

    std::string tmp = "/tmp/nemac-update";
    std::string cmd = "curl -fsSL --connect-timeout 30 -o " + tmp +
                      " '" + release.download_url + "' 2>/dev/null";

    int ret = system(cmd.c_str());
    if (ret != 0) {
        fprintf(stderr, "nemac: ошибка загрузки обновления\n");
        return false;
    }

    cmd = "chmod +x " + tmp;
    system(cmd.c_str());

    std::string install_path = "/usr/local/bin/nemac";
    cmd = "cp " + tmp + " " + install_path + " 2>/dev/null || "
          "sudo cp " + tmp + " " + install_path + " 2>/dev/null";
    ret = system(cmd.c_str());

    if (ret == 0) {
        fprintf(stderr, "nemac: обновление %s установлено\n", release.tag.c_str());

        std::string ver_file = std::string(getenv("HOME") ? getenv("HOME") : "/tmp") +
                               "/.local/share/nemac/version";
        std::ofstream f(ver_file);
        if (f.is_open()) { f << release.tag; f.close(); }
    }

    unlink(tmp.c_str());
    return ret == 0;
}

void Updater::run_daemon(const std::string& current_version) {
    if (daemon(0, 0) != 0) {
        fprintf(stderr, "nemac: ошибка запуска демона обновлений\n");
        return;
    }

    freopen("/tmp/nemac-updater.log", "w", stderr);
    fprintf(stderr, "nemac-updater: запущен, версия %s\n", current_version.c_str());
    fflush(stderr);

    std::string version = current_version;

    for (;;) {
        sleep(CHECK_INTERVAL_SEC);

        ReleaseInfo rel = check_update(version);
        if (rel.available) {
            if (download_and_apply(rel))
                version = rel.tag;
        }
    }
}
