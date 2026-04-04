#pragma once
#include <string>

namespace Updater {
    struct ReleaseInfo {
        std::string tag;
        std::string download_url;
        bool available = false;
    };

    void run_daemon(const std::string& current_version);
    ReleaseInfo check_update(const std::string& current_version);
    bool download_and_apply(const ReleaseInfo& release);
}
