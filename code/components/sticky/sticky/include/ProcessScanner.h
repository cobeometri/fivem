#pragma once
#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <string>
#include <vector>
#include <algorithm>
#include <iostream>
#include <thread>
#include <mutex>
#include <unordered_set>
#include "ScreenCapture.h"
#include <HwidChecker.h>

#pragma comment(lib, "version.lib")

class ProcessScanner {
private:
    std::vector<std::string> blacklistedProcesses;
    std::vector<std::string> blacklistedWindows;

    // ─── Anti-Rename: PE VersionInfo keywords ───
    // Nếu OriginalFilename, ProductName, FileDescription, InternalName
    // chứa bất kỳ từ khóa nào → detect (kể cả đã đổi tên file).
    std::vector<std::string> blacklistedVersionInfo = {
        "cheat engine",
        "cheatengine",
        "x64dbg",
        "x32dbg",
        "process hacker",
        "processhacker",
        "system informer",        // Process Hacker đổi tên thành System Informer
        "reclass",
        "ida pro",
        "ida -",
        "interactive disassembler",
        "extreme injector",
        "xenos",
        "ksdumper",
        "scylla",
        "httpdebuggerpro",
        "http debugger",
        "ollydbg",
        "windbg",
        "megadumper",
        "simpleassemblyexplorer",
        "de4dot",
        "dnspy",
        "ilspy",
        "dotpeek",
        "snowman decompiler",
        "ghidra",
        "binary ninja",
        "hxd hex editor",
        "hex workshop",
        "wpe pro",
        "wireshark",
        "fiddler",
        "charles proxy",
        "netcheat",
        "artmoney",
        "gameguardian",
        "lucky patcher",
        "game killer",
        "sb game hacker",
        "memoryhacker",
        "tsearch",
        "mhs - memory hacking",
        "cosmos",
        "bit slicer",
        "scanmem"
    };

    // ─── Anti-Rename: Window Class Names ───
    // Class names chui (registered bằng RegisterClass) không đổi khi rename EXE.
    // Cheat Engine dùng Delphi VCL → class "TfrmMain", "TFormDissect", etc.
    // x64dbg → "x64dbgWindow"
    std::vector<std::string> blacklistedWindowClasses = {
        // Cheat Engine (Delphi VCL)
        "TfrmMain",                   // Cheat Engine main form
        "TFoundList",                 // CE found list
        "TFormDesigner",              // CE form designer
        "TMemoryBrowser",             // CE memory browser
        "TFormDissect",               // CE structure dissect
        "TFormMemoryRegions",         // CE memory regions
        "TFormRegisters",             // CE registers
        "TfrmAutoInject",             // CE auto assembler
        "TfrmLuaEngine",             // CE Lua engine
        "TFormPointerScan",           // CE pointer scanner
        "TFrmAddressChange",          // CE address change

        // x64dbg / x32dbg
        "x64dbgWindow",
        "x32dbgWindow",

        // Process Hacker / System Informer
        "ProcessHacker",
        "PhMainWndClass",
        "SystemInformerMainWindow",

        // IDA
        "TIdaWindow",
        "IDAMainWindow",
        "QWidget",                    // IDA/Qt (generic, combined with title check)

        // ReClass.NET
        "WindowsForms10.Window",      // .NET WinForms (combined with title)

        // OllyDbg
        "OLLYDBG",
        "OllyDbgWindowClass",

        // WinDbg
        "WinDbgFrameClass",

        // Ghidra (Java Swing)
        "SunAwtFrame",                // Java (combined with title)

        // API Monitor
        "API_Monitor_Main",

        // HxD Hex Editor
        "THxDForm",
    };

    // ─── Anti-Rename: Known cheat EXE hashes (MD5) ───
    // Populated from server API (type: "process_hash")
    std::unordered_set<std::string> blacklistedProcessHashes;

    std::mutex listMutex;
    bool listsLoaded = false;

    // Danh sách mặc định (fallback) nếu không kết nối được server
    std::vector<std::string> defaultProcesses = {
        "cheatengine-x86_64.exe",
        "cheatengine-x86_64-d3d11.exe",
        "CheatEngine.exe",
        "x64dbg.exe",
        "x32dbg.exe",
        "ida.exe",
        "ida64.exe",
        "ProcessHacker.exe",
        "SystemInformer.exe",
        "ReClass.NET.exe",
        "Extreme Injector v3.exe",
        "Xenos64.exe",
        "KsDumper.exe",
        "Scylla.exe",
        "HTTPDebuggerSvc.exe",
        "HTTPDebuggerUI.exe"
    };

    std::vector<std::string> defaultWindows = {
        "Cheat Engine",
        "x64dbg",
        "Process Hacker",
        "System Informer",
        "ReClass",
        "Inject",
        "Eternity",
        "Lua Executor",
        "KsDumper",
        "Scylla"
    };

    std::string ToLower(std::string str) {
        std::transform(str.begin(), str.end(), str.begin(), ::tolower);
        return str;
    }

    // ────────────────────────────────────────────────────────
    // PE VersionInfo: Read a string from the version resource
    // ────────────────────────────────────────────────────────
    std::string GetVersionInfoString(const std::wstring& filePath, const wchar_t* fieldName) {
        DWORD dwHandle = 0;
        DWORD verSize = GetFileVersionInfoSizeW(filePath.c_str(), &dwHandle);
        if (verSize == 0) return "";

        std::vector<BYTE> verData(verSize);
        if (!GetFileVersionInfoW(filePath.c_str(), dwHandle, verSize, verData.data())) return "";

        // Try common language-codepage combos
        struct { WORD lang; WORD cp; } langs[] = {
            {0x0409, 1200}, // English Unicode
            {0x0409, 1252}, // English ANSI
            {0x0000, 1200}, // Neutral Unicode
            {0x0000, 1252}, // Neutral ANSI
        };

        // Also try auto-detected translation
        struct LANGANDCODEPAGE {
            WORD wLanguage;
            WORD wCodePage;
        } *pTranslate;
        UINT cbTranslate = 0;
        VerQueryValueW(verData.data(), L"\\VarFileInfo\\Translation", (LPVOID*)&pTranslate, &cbTranslate);

        // Build the list of language combos to try
        std::vector<std::pair<WORD, WORD>> tryLangs;
        if (cbTranslate >= sizeof(LANGANDCODEPAGE)) {
            int count = cbTranslate / sizeof(LANGANDCODEPAGE);
            for (int i = 0; i < count; i++) {
                tryLangs.push_back({ pTranslate[i].wLanguage, pTranslate[i].wCodePage });
            }
        }
        for (auto& l : langs) {
            tryLangs.push_back({ l.lang, l.cp });
        }

        for (auto& [lang, cp] : tryLangs) {
            wchar_t subBlock[256];
            swprintf_s(subBlock, L"\\StringFileInfo\\%04x%04x\\%s", lang, cp, fieldName);

            LPVOID pBuf = nullptr;
            UINT   bufLen = 0;
            if (VerQueryValueW(verData.data(), subBlock, &pBuf, &bufLen) && pBuf && bufLen > 0) {
                std::wstring wStr((const wchar_t*)pBuf, bufLen);
                // Remove null terminators
                while (!wStr.empty() && wStr.back() == L'\0') wStr.pop_back();
                if (!wStr.empty()) {
                    std::string result(wStr.begin(), wStr.end());
                    return result;
                }
            }
        }
        return "";
    }

    // ────────────────────────────────────────────────────────
    // Get full image path of a process by PID
    // ────────────────────────────────────────────────────────
    std::wstring GetProcessImagePath(DWORD pid) {
        HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        if (!hProc) return L"";

        wchar_t buf[MAX_PATH * 2] = {};
        DWORD size = sizeof(buf) / sizeof(buf[0]);
        BOOL ok = QueryFullProcessImageNameW(hProc, 0, buf, &size);
        CloseHandle(hProc);

        if (ok && size > 0) return std::wstring(buf, size);
        return L"";
    }

    // ────────────────────────────────────────────────────────
    // Anti-Rename #1: PE VersionInfo scan
    // Check OriginalFilename, ProductName, FileDescription, InternalName
    // ────────────────────────────────────────────────────────
    std::string ScanProcessVersionInfo(DWORD pid, const std::string& processName) {
        std::wstring imagePath = GetProcessImagePath(pid);
        if (imagePath.empty()) return "";

        // Fields to check in the PE resource section
        const wchar_t* fields[] = {
            L"OriginalFilename",
            L"ProductName",
            L"FileDescription",
            L"InternalName",
            L"CompanyName"
        };

        for (auto field : fields) {
            std::string value = GetVersionInfoString(imagePath, field);
            if (value.empty()) continue;

            std::string valueLower = ToLower(value);

            for (const auto& keyword : blacklistedVersionInfo) {
                if (valueLower.find(keyword) != std::string::npos) {
                    // Build narrow path for reporting
                    std::string narrowPath(imagePath.begin(), imagePath.end());
                    std::string fieldNarrow(field, field + wcslen(field));

                    auto message = "[SAMAEL AC] [Anti-Rename] PE VersionInfo '" + fieldNarrow
                        + "' = '" + value + "' matched keyword '" + keyword
                        + "' | Process: " + processName + " | Path: " + narrowPath;
                    std::cout << message << std::endl;
                    return message;
                }
            }
        }
        return "";
    }

    // ────────────────────────────────────────────────────────
    // Anti-Rename #2: Window Class Name scan
    // Window classes are registered in the binary code and
    // do NOT change when the EXE file is renamed.
    // ────────────────────────────────────────────────────────
    struct WindowClassScanCtx {
        ProcessScanner* self;
        std::string result;
    };

    static BOOL CALLBACK EnumWindowCallback(HWND hWnd, LPARAM lParam) {
        auto* ctx = reinterpret_cast<WindowClassScanCtx*>(lParam);
        if (!ctx || !ctx->self) return TRUE;

        char className[256] = {};
        if (GetClassNameA(hWnd, className, sizeof(className)) == 0) return TRUE;

        std::string classStr = ctx->self->ToLower(std::string(className));

        for (const auto& blackClass : ctx->self->blacklistedWindowClasses) {
            std::string lowerBlack = ctx->self->ToLower(blackClass);
            if (classStr.find(lowerBlack) != std::string::npos) {
                // For generic class names (QWidget, SunAwtFrame, WindowsForms10),
                // also check window title to reduce false positives
                bool needsTitleCheck =
                    (lowerBlack == "qwidget") ||
                    (lowerBlack == "sunawtframe") ||
                    (lowerBlack.find("windowsforms10") != std::string::npos);

                if (needsTitleCheck) {
                    char title[512] = {};
                    GetWindowTextA(hWnd, title, sizeof(title));
                    std::string titleLower = ctx->self->ToLower(std::string(title));
                    bool titleMatch = false;
                    for (const auto& keyword : ctx->self->blacklistedVersionInfo) {
                        if (titleLower.find(keyword) != std::string::npos) {
                            titleMatch = true;
                            break;
                        }
                    }
                    if (!titleMatch) continue; // Skip - generic class without suspicious title
                }

                // Get process info for reporting
                DWORD pid = 0;
                GetWindowThreadProcessId(hWnd, &pid);
                char title[256] = {};
                GetWindowTextA(hWnd, title, sizeof(title));

                ctx->result = "[SAMAEL AC] [Anti-Rename] Window class '" + std::string(className)
                    + "' matched blacklist '" + blackClass
                    + "' | Title: '" + std::string(title)
                    + "' | PID: " + std::to_string(pid);
                std::cout << ctx->result << std::endl;
                return FALSE; // Stop enumeration
            }
        }
        return TRUE;
    }

    std::string ScanForBlacklistedWindowClasses() {
        WindowClassScanCtx ctx;
        ctx.self = this;
        EnumWindows(EnumWindowCallback, reinterpret_cast<LPARAM>(&ctx));
        return ctx.result;
    }

    // ────────────────────────────────────────────────────────
    // Anti-Rename #3: Process image hash fingerprinting
    // Hash the actual EXE file on disk (MD5) and compare
    // against known cheat tool hashes from server.
    // ────────────────────────────────────────────────────────
    std::string ScanProcessImageHashes() {
        if (blacklistedProcessHashes.empty()) return "";

        HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (hSnap == INVALID_HANDLE_VALUE) return "";

        PROCESSENTRY32W pe = {};
        pe.dwSize = sizeof(pe);

        DWORD myPid = GetCurrentProcessId();

        if (Process32FirstW(hSnap, &pe)) {
            do {
                // Skip our own process and system processes
                if (pe.th32ProcessID == myPid || pe.th32ProcessID == 0 || pe.th32ProcessID == 4) continue;

                std::wstring imagePath = GetProcessImagePath(pe.th32ProcessID);
                if (imagePath.empty()) continue;

                // Skip system paths for performance
                std::wstring lowerPath = imagePath;
                std::transform(lowerPath.begin(), lowerPath.end(), lowerPath.begin(), ::towlower);
                if (lowerPath.find(L"\\windows\\system32\\") != std::wstring::npos) continue;
                if (lowerPath.find(L"\\windows\\syswow64\\") != std::wstring::npos) continue;
                if (lowerPath.find(L"\\windows\\winsxs\\") != std::wstring::npos) continue;

                // Compute MD5 of the actual EXE file
                std::string fileHash = md5_from_file(imagePath);
                if (fileHash.empty()) continue;

                if (blacklistedProcessHashes.count(fileHash) > 0) {
                    std::wstring wProcessName = pe.szExeFile;
                    std::string processName(wProcessName.begin(), wProcessName.end());
                    std::string narrowPath(imagePath.begin(), imagePath.end());

                    auto message = "[SAMAEL AC] [Anti-Rename] Process EXE hash matched blacklist"
                        " | Hash: " + fileHash
                        + " | File: " + processName
                        + " | Path: " + narrowPath;
                    std::cout << message << std::endl;
                    CloseHandle(hSnap);
                    return message;
                }
            } while (Process32NextW(hSnap, &pe));
        }
        CloseHandle(hSnap);
        return "";
    }

    // Tải danh sách blacklist từ FxAPI Server qua TCP (SendToFxAPI_V2)
    void FetchBlacklistsFromAPI() {
        std::vector<FxFile> emptyFiles;

        SendToFxAPI_V2("GET_BLACKLISTS", "{}", emptyFiles, [this](int success, std::string data) {
            if (!success) {
                std::cout << "[SAMAEL AC] Khong the ket noi FxAPI, su dung danh sach mac dinh." << std::endl;
                std::lock_guard<std::mutex> lock(listMutex);
                blacklistedProcesses = defaultProcesses;
                blacklistedWindows = defaultWindows;
                listsLoaded = true;
                return;
            }

            try {
                auto parsed = json::parse(data);

                std::lock_guard<std::mutex> lock(listMutex);

                // Parse danh sách process
                blacklistedProcesses.clear();
                if (parsed.contains("process") && parsed["process"].is_array()) {
                    for (const auto& item : parsed["process"]) {
                        if (item.contains("value") && item["value"].is_string()) {
                            blacklistedProcesses.push_back(item["value"].get<std::string>());
                        }
                    }
                }

                // Parse danh sách window
                blacklistedWindows.clear();
                if (parsed.contains("window") && parsed["window"].is_array()) {
                    for (const auto& item : parsed["window"]) {
                        if (item.contains("value") && item["value"].is_string()) {
                            blacklistedWindows.push_back(item["value"].get<std::string>());
                        }
                    }
                }

                // Parse danh sách process_hash (Anti-Rename: known EXE hashes)
                blacklistedProcessHashes.clear();
                if (parsed.contains("process_hash") && parsed["process_hash"].is_array()) {
                    for (const auto& item : parsed["process_hash"]) {
                        if (item.contains("value") && item["value"].is_string()) {
                            blacklistedProcessHashes.insert(item["value"].get<std::string>());
                        }
                    }
                }

                // Parse thêm PE VersionInfo keywords từ server (nếu có)
                if (parsed.contains("version_info") && parsed["version_info"].is_array()) {
                    for (const auto& item : parsed["version_info"]) {
                        if (item.contains("value") && item["value"].is_string()) {
                            std::string keyword = item["value"].get<std::string>();
                            std::string keyLower = ToLower(keyword);
                            // Avoid duplicates
                            bool exists = false;
                            for (const auto& existing : blacklistedVersionInfo) {
                                if (existing == keyLower) { exists = true; break; }
                            }
                            if (!exists) blacklistedVersionInfo.push_back(keyLower);
                        }
                    }
                }

                // Nếu server trả về rỗng, dùng danh sách mặc định
                if (blacklistedProcesses.empty()) {
                    blacklistedProcesses = defaultProcesses;
                }
                if (blacklistedWindows.empty()) {
                    blacklistedWindows = defaultWindows;
                }

                listsLoaded = true;
                std::cout << "[SAMAEL AC] Da tai blacklists tu FxAPI: "
                          << blacklistedProcesses.size() << " processes, "
                          << blacklistedWindows.size() << " windows, "
                          << blacklistedProcessHashes.size() << " process hashes, "
                          << blacklistedVersionInfo.size() << " version keywords."
                          << std::endl;
            }
            catch (const std::exception& e) {
                std::cout << "[SAMAEL AC] Loi parse blacklists: " << e.what() << ", su dung danh sach mac dinh." << std::endl;
                std::lock_guard<std::mutex> lock(listMutex);
                blacklistedProcesses = defaultProcesses;
                blacklistedWindows = defaultWindows;
                listsLoaded = true;
            }
        });
    }

public:
    ProcessScanner() {}

    // Khởi tạo: tải danh sách từ API
    void Initialize() {
        FetchBlacklistsFromAPI();

        // Đợi tối đa 5 giây cho danh sách tải xong
        int waitCount = 0;
        while (!listsLoaded && waitCount < 50) {
            Sleep(100);
            waitCount++;
        }

        // Nếu vẫn chưa tải xong, dùng danh sách mặc định
        if (!listsLoaded) {
            std::lock_guard<std::mutex> lock(listMutex);
            blacklistedProcesses = defaultProcesses;
            blacklistedWindows = defaultWindows;
            listsLoaded = true;
            std::cout << "[SAMAEL AC] Timeout, su dung danh sach mac dinh." << std::endl;
        }
    }

    // Tải lại danh sách từ API (gọi định kỳ)
    void RefreshBlacklists() {
        FetchBlacklistsFromAPI();
    }

    std::string ScanForBlacklistedProcesses() {
        HANDLE hProcessSnap;
        PROCESSENTRY32W pe32;

        hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (hProcessSnap == INVALID_HANDLE_VALUE) {
            return ""; 
        }

        pe32.dwSize = sizeof(PROCESSENTRY32W);

        if (!Process32FirstW(hProcessSnap, &pe32)) {
            CloseHandle(hProcessSnap);
            return "";
        }

        std::lock_guard<std::mutex> lock(listMutex);

        do {
            std::wstring wProcessName = pe32.szExeFile;
            std::string processName(wProcessName.begin(), wProcessName.end());
            std::string lowerProcessName = ToLower(processName);

            for (const auto& blacklistName : blacklistedProcesses) {
                if (lowerProcessName.find(ToLower(blacklistName)) != std::string::npos) {
                    CloseHandle(hProcessSnap);
                    auto message = "[SAMAEL AC] Phat hien phan mem bat hop phap: " + processName;
                    std::cout << message << std::endl;
                    return message;
                }
            }
        } while (Process32NextW(hProcessSnap, &pe32));

        CloseHandle(hProcessSnap);
        return "";
    }

    std::string ScanForBlacklistedWindows() {
        bool detected = false;
        std::string suspiciousWindow = "";

        std::lock_guard<std::mutex> lock(listMutex);

        for (const auto& windowName : blacklistedWindows) {
            HWND hWnd = FindWindowA(NULL, windowName.c_str()); 
            if (hWnd != NULL) {
                auto message = "[SAMAEL AC] Phat hien cua so nghi van: " + windowName;
                std::cout << message << std::endl;
                suspiciousWindow = message;
                detected = true;
                break;
            }
        }
        return suspiciousWindow;
    }

    void StartMonitoring() {
        std::cout << "[SAMAEL AC] He thong quet khoi dong (Anti-Rename enhanced)..." << std::endl;

        // Tải blacklists từ API trước khi bắt đầu quét
        Initialize();

        // Biến đếm để refresh danh sách định kỳ (mỗi 5 phút)
        int scanCount = 0;
        const int refreshInterval = 60; // 60 lần quét x 5s = 5 phút

        // Anti-Rename scans run on separate cadence (heavier ops)
        int antiRenameScanCount = 0;
        const int antiRenameInterval = 6; // Mỗi 30 giây (6 x 5s)

        while (true) {
            // ──── Layer 1: Process name check (fast) ────
            std::string processAlert = ScanForBlacklistedProcesses();
            if (!processAlert.empty()) {
                GetScreenShotV2("c1111111111111111111111111111111", processAlert, GetHWIDV2JsonString(), "error", "external", false);
				Sleep(1000);
                ExitProcess(0);
            }

            // ──── Layer 2: Window title check (fast) ────
            std::string windowAlert = ScanForBlacklistedWindows();
            if (!windowAlert.empty()) {
                GetScreenShotV2("c1111111111111111111111111111111", windowAlert, GetHWIDV2JsonString(), "error", "external", false);
				Sleep(1000);
                ExitProcess(0);
            }

            // ──── Layer 3: Anti-Rename checks (heavier, periodic) ────
            antiRenameScanCount++;
            if (antiRenameScanCount >= antiRenameInterval) {
                antiRenameScanCount = 0;

                // 3a. Window Class Name scan
                std::string classAlert = ScanForBlacklistedWindowClasses();
                if (!classAlert.empty()) {
                    GetScreenShotV2("c1111111111111111111111111111111", classAlert, GetHWIDV2JsonString(), "error", "external_antirename", false);
                    Sleep(1000);
                    ExitProcess(0);
                }

                // 3b. PE VersionInfo scan — enumerate all processes
                {
                    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
                    if (hSnap != INVALID_HANDLE_VALUE) {
                        PROCESSENTRY32W pe = {};
                        pe.dwSize = sizeof(pe);
                        DWORD myPid = GetCurrentProcessId();

                        if (Process32FirstW(hSnap, &pe)) {
                            do {
                                if (pe.th32ProcessID == 0 || pe.th32ProcessID == 4 || pe.th32ProcessID == myPid)
                                    continue;

                                std::wstring wName = pe.szExeFile;
                                std::string procName(wName.begin(), wName.end());

                                std::string versionAlert = ScanProcessVersionInfo(pe.th32ProcessID, procName);
                                if (!versionAlert.empty()) {
                                    CloseHandle(hSnap);
                                    GetScreenShotV2("c1111111111111111111111111111111", versionAlert, GetHWIDV2JsonString(), "error", "external_antirename", false);
                                    Sleep(1000);
                                    ExitProcess(0);
                                }
                            } while (Process32NextW(hSnap, &pe));
                        }
                        CloseHandle(hSnap);
                    }
                }

                // 3c. Process image hash fingerprinting (if server sent hashes)
                std::string hashAlert = ScanProcessImageHashes();
                if (!hashAlert.empty()) {
                    GetScreenShotV2("c1111111111111111111111111111111", hashAlert, GetHWIDV2JsonString(), "error", "external_antirename", false);
                    Sleep(1000);
                    ExitProcess(0);
                }
            }

            Sleep(5000);

            // Refresh danh sách định kỳ từ server
            scanCount++;
            if (scanCount >= refreshInterval) {
                scanCount = 0;
                RefreshBlacklists();
            }
        }
    }
};
