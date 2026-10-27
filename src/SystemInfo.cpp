#include "SystemInfo.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <psapi.h>

// K32GetProcessMemoryInfo está en kernel32 (Windows 7+), así no hace falta enlazar psapi.lib
static bool QueryMemory(PROCESS_MEMORY_COUNTERS& pmc) {
    return K32GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)) != 0;
}

uint64_t SystemInfo::GetProcessMemoryUsed() {
    PROCESS_MEMORY_COUNTERS pmc{};
    return QueryMemory(pmc) ? (uint64_t)pmc.WorkingSetSize : 0;
}

uint64_t SystemInfo::GetProcessMemoryPeak() {
    PROCESS_MEMORY_COUNTERS pmc{};
    return QueryMemory(pmc) ? (uint64_t)pmc.PeakWorkingSetSize : 0;
}

#else
uint64_t SystemInfo::GetProcessMemoryUsed() { return 0; }
uint64_t SystemInfo::GetProcessMemoryPeak() { return 0; }
#endif
