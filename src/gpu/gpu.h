#pragma once
#include <string>

enum class GpuType {
    UNKNOWN,
    AMD,
    NVIDIA,
    INTEL,
    HYBRID_NVIDIA_INTEL,
    HYBRID_NVIDIA_AMD
};

struct GpuInfo {
    GpuType type = GpuType::UNKNOWN;
    std::string name;
    bool nvidia_present = false;
    bool amd_present    = false;
    bool intel_present  = false;
};

namespace Gpu {
    GpuInfo detect();
    void apply_env(const GpuInfo& info);
}
