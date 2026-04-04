#include "gpu.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>

static bool file_exists(const char* path) {
    std::ifstream f(path);
    return f.good();
}

static std::string exec_cmd(const char* cmd) {
    char buf[512];
    std::string out;
    FILE* pipe = popen(cmd, "r");
    if (!pipe) return out;
    while (fgets(buf, sizeof(buf), pipe))
        out += buf;
    pclose(pipe);
    return out;
}

GpuInfo Gpu::detect() {
    GpuInfo info;
    std::string lspci = exec_cmd("lspci 2>/dev/null");

    std::istringstream ss(lspci);
    std::string line;
    while (std::getline(ss, line)) {
        bool is_vga = line.find("VGA") != std::string::npos ||
                      line.find("3D controller") != std::string::npos ||
                      line.find("Display controller") != std::string::npos;
        if (!is_vga) continue;

        if (line.find("NVIDIA") != std::string::npos || line.find("nvidia") != std::string::npos) {
            info.nvidia_present = true;
            info.name = line;
        }
        if (line.find("AMD") != std::string::npos || line.find("ATI") != std::string::npos ||
            line.find("Radeon") != std::string::npos) {
            info.amd_present = true;
            if (info.name.empty()) info.name = line;
        }
        if (line.find("Intel") != std::string::npos) {
            info.intel_present = true;
            if (info.name.empty()) info.name = line;
        }
    }

    if (info.nvidia_present && info.intel_present)
        info.type = GpuType::HYBRID_NVIDIA_INTEL;
    else if (info.nvidia_present && info.amd_present)
        info.type = GpuType::HYBRID_NVIDIA_AMD;
    else if (info.nvidia_present)
        info.type = GpuType::NVIDIA;
    else if (info.amd_present)
        info.type = GpuType::AMD;
    else if (info.intel_present)
        info.type = GpuType::INTEL;

    return info;
}

void Gpu::apply_env(const GpuInfo& info) {
    setenv("ELECTRON_OZONE_PLATFORM_HINT", "x11", 0);

    switch (info.type) {
    case GpuType::NVIDIA:
        setenv("__GL_SYNC_TO_VBLANK", "1", 0);
        setenv("__GL_GSYNC_ALLOWED", "1", 0);
        if (file_exists("/usr/lib/xorg/modules/drivers/nvidia_drv.so"))
            setenv("GBM_BACKEND", "nvidia-drm", 0);
        break;

    case GpuType::HYBRID_NVIDIA_INTEL:
    case GpuType::HYBRID_NVIDIA_AMD:
        setenv("__NV_PRIME_RENDER_OFFLOAD", "1", 0);
        setenv("__GLX_VENDOR_LIBRARY_NAME", "nvidia", 0);
        setenv("__VK_LAYER_NV_optimus", "NVIDIA_only", 0);
        setenv("__GL_SYNC_TO_VBLANK", "1", 0);
        break;

    case GpuType::AMD:
        setenv("AMD_VULKAN_ICD", "RADV", 0);
        break;

    case GpuType::INTEL:
    case GpuType::UNKNOWN:
        break;
    }

    fprintf(stderr, "nemac: gpu type=%d nvidia=%d amd=%d intel=%d [%s]\n",
            (int)info.type, info.nvidia_present, info.amd_present,
            info.intel_present, info.name.c_str());
}
