#pragma once

#include <windows.h>
#include <string>

namespace K2 {

struct SystemMetrics {
    double cpuPercent = 0.0;
    int ramPercent = 0;
    unsigned long long ramUsedMb = 0;
    unsigned long long ramTotalMb = 0;
    int batteryPercent = -1; // -1 if no battery
    bool isCharging = false;
    bool hasBattery = false;
};

class SystemStats {
public:
    static SystemStats& Instance();

    SystemMetrics GetCurrentMetrics();

private:
    SystemStats();

    ULONGLONG FileTimeToUInt64(const FILETIME& ft) const;

    ULONGLONG m_prevIdleTime = 0;
    ULONGLONG m_prevKernelTime = 0;
    ULONGLONG m_prevUserTime = 0;
    bool m_initialized = false;
};

} // namespace K2
