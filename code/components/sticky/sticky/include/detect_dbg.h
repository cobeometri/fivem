// detect_dbg_x64dbg.cpp
// Build: cl /EHsc /O2 detect_dbg_x64dbg.cpp /link user32.lib advapi32.lib
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <winternl.h>
#include <cstdio>
#include <string>
#include <vector>
#include <array>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "psapi.lib")

// ------------------ Orchestrator
struct DetectResult {
    bool isDebugger = false;
    bool isLikelyX64dbg = false;
    std::vector<std::string> hits;
};

DetectResult RunAllChecks();
