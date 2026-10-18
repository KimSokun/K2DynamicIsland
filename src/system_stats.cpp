#include "system_stats.h"
#include <algorithm>

namespace K2 {

SystemStats& SystemStats::Instance() {
    static SystemStats instance;
    return instance;
}

SystemStats::SystemStats() {
    FILETIME idleTime, kernelTime, userTime;
    if (GetSystemTimes(&idleTime, &kernelTime, &userTime)) {
        m_prevIdleTime = FileTimeToUInt64(idleTime);
        m_prevKernelTime = FileTimeToUInt64(kernelTime);
        m_prevUserTime = FileTimeToUInt64(userTime);
        m_initialized = true;
    }
}

ULONGLONG SystemStats::FileTimeToUInt64(const FILETIME& ft) const {
    ULARGE_INTEGER uli;
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;
    return uli.QuadPart;
}

SystemMetrics SystemStats::GetCurrentMetrics() {
    SystemMetrics metrics;

    // CPU Calculation
    FILETIME idleTime, kernelTime, userTime;
    if (GetSystemTimes(&idleTime, &kernelTime, &userTime)) {
        ULONGLONG currentIdle = FileTimeToUInt64(idleTime);
        ULONGLONG currentKernel = FileTimeToUInt64(kernelTime);
        ULONGLONG currentUser = FileTimeToUInt64(userTime);

        if (m_initialized) {
            ULONGLONG idleDiff = currentIdle - m_prevIdleTime;
            ULONGLONG kernelDiff = currentKernel - m_prevKernelTime;
            ULONGLONG userDiff = currentUser - m_prevUserTime;

            ULONGLONG totalDiff = kernelDiff + userDiff;
            if (totalDiff > 0 && totalDiff >= idleDiff) {
                ULONGLONG busyDiff = totalDiff - idleDiff;
                metrics.cpuPercent = (double)(busyDiff * 100.0) / (double)totalDiff;
                metrics.cpuPercent = std::clamp(metrics.cpuPercent, 0.0, 100.0);
            }
        }

        m_prevIdleTime = currentIdle;
        m_prevKernelTime = currentKernel;
        m_prevUserTime = currentUser;
        m_initialized = true;
    }

    // RAM Calculation
    MEMORYSTATUSEX memStatus;
    memStatus.dwLength = sizeof(memStatus);
    if (GlobalMemoryStatusEx(&memStatus)) {
        metrics.ramPercent = static_cast<int>(memStatus.dwMemoryLoad);
        metrics.ramTotalMb = memStatus.ullTotalPhys / (1024 * 1024);
        metrics.ramUsedMb = (memStatus.ullTotalPhys - memStatus.ullAvailPhys) / (1024 * 1024);
    }

    // Battery / Power Calculation
    SYSTEM_POWER_STATUS sps;
    if (GetSystemPowerStatus(&sps)) {
        if (!(sps.BatteryFlag & 128) && sps.BatteryLifePercent <= 100) {
            metrics.hasBattery = true;
            metrics.batteryPercent = static_cast<int>(sps.BatteryLifePercent);
            metrics.isCharging = (sps.ACLineStatus == 1);
        } else {
            metrics.hasBattery = false;
            metrics.batteryPercent = -1;
            metrics.isCharging = (sps.ACLineStatus == 1);
        }
    }

    return metrics;
}

} // namespace K2
