#pragma once
#include <cstdio>
#include <string>

inline std::string exec_cmd(const char* cmd) {
    char buf[4096];
    std::string out;
    FILE* pipe = popen(cmd, "r");
    if (!pipe) return out;
    while (fgets(buf, sizeof(buf), pipe))
        out += buf;
    pclose(pipe);
    return out;
}
