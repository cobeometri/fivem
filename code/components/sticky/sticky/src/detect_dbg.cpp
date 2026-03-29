#include <StdInc.h>
#include "detect_dbg.h"
// ------------------ NtQueryInformationProcess (undocumented in Win32 headers)
typedef NTSTATUS (NTAPI* NtQueryInformationProcess_t)(
    HANDLE, PROCESSINFOCLASS, PVOID, ULONG, PULONG
);

// ------------------ Helpers
static bool loadNt(NtQueryInformationProcess_t& fn) {
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    if (!ntdll) return false;
    fn = reinterpret_cast<NtQueryInformationProcess_t>(
        GetProcAddress(ntdll, "NtQueryInformationProcess"));
    return fn != nullptr;
}

static std::wstring ToW(const std::string& s) {
    if (s.empty()) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    std::wstring ws(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), ws.data(), len);
    return ws;
}

// ------------------ 1) Classic WinAPI checks
bool Check_IsDebuggerPresent() {
    return IsDebuggerPresent() ? true : false;
}

bool Check_CheckRemoteDebuggerPresent() {
    BOOL present = FALSE;
    if (CheckRemoteDebuggerPresent(GetCurrentProcess(), &present)) return !!present;
    return false;
}

bool Check_NtQueryInformationProcess_DebugFlags() {
    NtQueryInformationProcess_t NtQueryInformationProcess = nullptr;
    if (!loadNt(NtQueryInformationProcess)) return false;

    ULONG DebugFlags = 0;
    NTSTATUS st = NtQueryInformationProcess(GetCurrentProcess(),
        (PROCESSINFOCLASS)0x1f /*ProcessDebugFlags*/, &DebugFlags, sizeof(DebugFlags), nullptr);
    // When not being debugged, DebugFlags typically has the PROCESS_DEBUG_FLAGS_NO_DEBUG_INHERIT bit set (1)
    if (st == 0) {
        // If a debugger is present, some sources report this can flip/clear
        return (DebugFlags == 0); // heuristics: 0 often means debugged
    }
    return false;
}

bool Check_NtQueryInformationProcess_DebugPort() {
    NtQueryInformationProcess_t NtQueryInformationProcess = nullptr;
    if (!loadNt(NtQueryInformationProcess)) return false;

    HANDLE DebugPort = 0;
    NTSTATUS st = NtQueryInformationProcess(GetCurrentProcess(),
        (PROCESSINFOCLASS)7 /*ProcessDebugPort*/, &DebugPort, sizeof(DebugPort), nullptr);
    if (st == 0) {
        return DebugPort != 0 && DebugPort != (HANDLE)-1;
    }
    return false;
}

bool Check_NtQueryInformationProcess_DebugObject() {
    NtQueryInformationProcess_t NtQueryInformationProcess = nullptr;
    if (!loadNt(NtQueryInformationProcess)) return false;

    HANDLE DebugObject = nullptr;
    NTSTATUS st = NtQueryInformationProcess(GetCurrentProcess(),
        (PROCESSINFOCLASS)0x1e /*ProcessDebugObjectHandle*/, &DebugObject, sizeof(DebugObject), nullptr);
    if (st == 0) {
        return DebugObject != nullptr && DebugObject != (HANDLE)-1;
    }
    return false;
}

// ------------------ 2) PEB-based checks (no-API)
#ifdef _M_X64
#define PEB_PTR __readgsqword(0x60)
#else
#define PEB_PTR __readfsdword(0x30)
#endif

bool Check_PEB_BeingDebugged() {
    // PEB->BeingDebugged is a BYTE at offset 0x2 in the PEB
    // Safer: read via TEB segment register
    struct PEB_LOCAL {
        BYTE _pad0[2];
        BYTE BeingDebugged;
        BYTE _pad1;
    };
    auto peb = reinterpret_cast<PEB_LOCAL*>(PEB_PTR);
    __try {
        return peb->BeingDebugged != 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool Check_PEB_NtGlobalFlag() {
    // PEB->NtGlobalFlag (offset depends on arch). For 64-bit often at 0xBC.
    // We’ll read conservatively: cast to byte array and sample known offset.
    BYTE* peb = reinterpret_cast<BYTE*>(PEB_PTR);
#ifdef _M_X64
    const SIZE_T ofs = 0xBC;
#else
    const SIZE_T ofs = 0x68;
#endif
    __try {
        DWORD flags = *reinterpret_cast<DWORD*>(peb + ofs);
        // Typical debug flags: FLG_HEAP_ENABLE_TAIL_CHECK (0x10),
        // FLG_HEAP_ENABLE_FREE_CHECK (0x20), FLG_HEAP_VALIDATE_PARAMETERS (0x40)
        return (flags & 0x70) != 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

// ------------------ 3) Hardware breakpoint checks (DR0-DR3)
bool Check_HardwareBreakpoints() {
    CONTEXT ctx = {};
    ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;
    if (!GetThreadContext(GetCurrentThread(), &ctx)) return false;

    // If any local/global enable bits in DR7 are set, or DR0-DR3 nonzero => suspicious
    bool drUsed = (ctx.Dr0 || ctx.Dr1 || ctx.Dr2 || ctx.Dr3);
    bool dr7Enabled = (ctx.Dr7 & 0xFF) != 0; // simplistic heuristic
    return drUsed || dr7Enabled;
}

// ------------------ 4) Software breakpoint scan (0xCC) in .text of current module
bool GetModuleTextRange(HMODULE hMod, BYTE*& base, DWORD& size) {
    if (!hMod) hMod = GetModuleHandleW(nullptr);
    if (!hMod) return false;

    auto dos = (PIMAGE_DOS_HEADER)hMod;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return false;
    auto nt = (PIMAGE_NT_HEADERS)((BYTE*)hMod + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return false;

#ifdef _M_X64
    auto& opt = nt->OptionalHeader;
#else
    auto& opt = nt->OptionalHeader;
#endif
    auto sec = IMAGE_FIRST_SECTION(nt);
    for (WORD i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++sec) {
        // heuristic: look for .text (executable)
        if (sec->Characteristics & IMAGE_SCN_MEM_EXECUTE) {
            base = (BYTE*)hMod + sec->VirtualAddress;
            size = sec->Misc.VirtualSize ? sec->Misc.VirtualSize : sec->SizeOfRawData;
            return true;
        }
    }
    return false;
}

bool Check_SoftwareBreakpoints_INT3() {
    BYTE* textBase = nullptr;
    DWORD textSize = 0;
    if (!GetModuleTextRange(nullptr, textBase, textSize)) return false;

    // NOTE: false positives nếu app của bạn thực sự dùng INT3
    const BYTE INT3 = 0xCC;
    SIZE_T hits = 0;
    for (DWORD i = 0; i < textSize; ++i) {
        if (textBase[i] == INT3) {
            ++hits;
            if (hits >= 1) return true; // thấy 1 là đủ nghi ngờ
        }
    }
    return false;
}

// ------------------ 5) Tên tiến trình / cửa sổ / module đặc trưng của x64dbg
bool Check_ProcessList_Names(const std::vector<std::wstring>& suspects) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return false;

    PROCESSENTRY32W pe = { sizeof(pe) };
    if (!Process32FirstW(snap, &pe)) { CloseHandle(snap); return false; }

    bool found = false;
    do {
        for (auto& s : suspects) {
            if (_wcsicmp(pe.szExeFile, s.c_str()) == 0) {
                found = true;
                break;
            }
        }
        if (found) break;
    } while (Process32NextW(snap, &pe));

    CloseHandle(snap);
    return found;
}

bool Check_FindWindow_X64DBG() {
    // x64dbg là Qt app; có khi title thay đổi. Ta thử nhiều class/title phổ biến.
    std::array<LPCWSTR, 4> classes = { L"x64dbg", L"x32dbg", L"Qt5QWindowIcon", L"Qt6QWindowIcon" };
    for (auto cls : classes) {
        if (FindWindowW(cls, nullptr)) return true;
    }
    // Title heuristics
    std::array<LPCWSTR, 4> titles = { L"x64dbg", L"x32dbg", L"CPU - x64dbg", L"Graph - x64dbg" };
    for (auto t : titles) {
        if (FindWindowW(nullptr, t)) return true;
    }
    return false;
}

bool Check_LoadedModules_Signatures() {
    // Kiểm tra DLL của x64dbg được inject/loaded trong tiến trình hiện tại (hiếm, nhưng check cho chắc)
    HMODULE mods[512];
    DWORD needed = 0;
    if (!EnumProcessModules(GetCurrentProcess(), mods, sizeof(mods), &needed)) return false;
    DWORD count = needed / sizeof(HMODULE);

    for (DWORD i = 0; i < count; ++i) {
        wchar_t path[MAX_PATH] = {0};
        if (GetModuleFileNameW(mods[i], path, MAX_PATH)) {
            std::wstring p(path);
            for (auto& token : { L"x64dbg", L"x32dbg", L"olly", L"scylla", L"ida" }) {
                if (wcsstr(_wcsupr(&p[0]), token)) return true;
            }
        }
    }
    return false;
}

// ------------------ 6) Timing anomaly (đơn giản)
bool Check_TimingSkew() {
    // Debugger/Breakpoints/Step có thể kéo dài latency bất thường
    LARGE_INTEGER f = {}, t0 = {}, t1 = {};
    QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&t0);
    // workload nhỏ
    volatile int sum = 0;
    for (int i = 0; i < 1000000; ++i) sum += i;
    QueryPerformanceCounter(&t1);
    double ms = 1000.0 * double(t1.QuadPart - t0.QuadPart) / double(f.QuadPart);
    // Ngưỡng này tùy hệ thống; đây chỉ là heuristic
    return ms > 30.0; // nếu >30ms cho vòng lặp nhỏ -> nghi có can thiệp
}


DetectResult RunAllChecks() {
    DetectResult r;

    auto hit = [&](bool v, const char* tag, bool x64=false) {
        if (v) {
            r.hits.push_back(tag);
            r.isDebugger = true;
            if (x64) r.isLikelyX64dbg = true;
        }
    };

    // API/PEB
    hit(Check_IsDebuggerPresent(),                "IsDebuggerPresent");
    hit(Check_CheckRemoteDebuggerPresent(),       "CheckRemoteDebuggerPresent");
    hit(Check_NtQueryInformationProcess_DebugFlags(), "NtQIP:DebugFlags");
    hit(Check_NtQueryInformationProcess_DebugPort(),  "NtQIP:DebugPort");
    hit(Check_NtQueryInformationProcess_DebugObject(),"NtQIP:DebugObject");
    hit(Check_PEB_BeingDebugged(),                "PEB.BeingDebugged");
    hit(Check_PEB_NtGlobalFlag(),                 "PEB.NtGlobalFlag");

    // Breakpoints
    hit(Check_HardwareBreakpoints(),              "HW Breakpoints");
    //hit(Check_SoftwareBreakpoints_INT3(),         "SW Breakpoint INT3");

    // x64dbg dấu hiệu đặc thù
    const std::vector<std::wstring> suspects = {
        L"x64dbg.exe", L"x32dbg.exe", L"ida64.exe", L"ida.exe", L"ollydbg.exe"
    };
    hit(Check_ProcessList_Names(suspects),        "ProcList:x64dbg/x32dbg/IDA/Olly", true);
    //hit(Check_FindWindow_X64DBG(),                "FindWindow:x64dbg/Qt", true);
    hit(Check_LoadedModules_Signatures(),         "LoadedModules:x64dbg/IDA/Olly", true);

    // Timing
    //hit(Check_TimingSkew(),                       "TimingSkew");

    return r;
}
