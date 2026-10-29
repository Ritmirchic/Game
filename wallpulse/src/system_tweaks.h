#pragma once

#include "common.h"
#include "config.h"

namespace wp {

struct SystemStatsSnapshot {
    float cpuPercent = 0.0f;
    uint32_t ramPercent = 0;
    double ramUsedGb = 0.0;
    double ramTotalGb = 0.0;
};

class SystemTweaks {
public:
    SystemTweaks() = default;
    ~SystemTweaks();

    // Apply transparent / acrylic blur style to Windows taskbar (#8)
    void ApplyTaskbarMode(TaskbarMode mode);
    void RestoreTaskbarDefault();

    // Non-blocking CPU / RAM system stats (#5)
    SystemStatsSnapshot SampleSystemStats();

private:
    bool wasModified_ = false;
    TaskbarMode lastMode_ = TaskbarMode::Default;

    uint64_t prevIdle_ = 0;
    uint64_t prevKernel_ = 0;
    uint64_t prevUser_ = 0;
    float lastCpuPct_ = 0.0f;
    ULONGLONG lastSampleMs_ = 0;
};

}  // namespace wp
