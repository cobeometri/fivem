#pragma once
#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <wintrust.h>
#include <softpub.h>
#include <string>
#include <vector>
#include <iostream>
#include <algorithm>
#include <mutex>
#include "ScreenCapture.h"
#include <HwidChecker.h>

#pragma comment (lib, "wintrust")

class ModuleScanner {
private:
    std::vector<std::string> blacklistedModules;
    std::mutex listMutex;
    bool listsLoaded = false;

    std::string ToLower(std::string str) {
        std::transform(str.begin(), str.end(), str.begin(), ::tolower);
        return str;
    }

    // Dùng Windows API (WinTrust) để kiểm tra xem file có chữ ký số (Certificate) hợp lệ không
    bool VerifyFileSignature(const wchar_t* filePath) {
        WINTRUST_FILE_INFO fileData;
        memset(&fileData, 0, sizeof(fileData));
        fileData.cbStruct = sizeof(WINTRUST_FILE_INFO);
        fileData.pcwszFilePath = filePath;
        fileData.hFile = NULL;
        fileData.pgKnownSubject = NULL;

        WINTRUST_DATA wintrustData;
        memset(&wintrustData, 0, sizeof(wintrustData));
        wintrustData.cbStruct = sizeof(wintrustData);
        wintrustData.pPolicyCallbackData = NULL;
        wintrustData.pSIPClientData = NULL;
        wintrustData.dwUIChoice = WTD_UI_NONE;
        wintrustData.fdwRevocationChecks = WTD_REVOKE_NONE; 
        wintrustData.dwUnionChoice = WTD_CHOICE_FILE;
        wintrustData.dwStateAction = WTD_STATEACTION_VERIFY;
        wintrustData.hWVTStateData = NULL;
        wintrustData.pwszURLReference = NULL;
        wintrustData.dwUIContext = 0;
        wintrustData.pFile = &fileData;

        GUID policyGUID = WINTRUST_ACTION_GENERIC_VERIFY_V2;
        LONG lStatus = WinVerifyTrust(NULL, &policyGUID, &wintrustData);

        wintrustData.dwStateAction = WTD_STATEACTION_CLOSE;
        WinVerifyTrust(NULL, &policyGUID, &wintrustData);

        return (lStatus == ERROR_SUCCESS);
    }

    // Danh sách đường dẫn an toàn (Hệ thống Windows) -> Bỏ qua kiểm tra để tối ưu tốc độ RAM
    bool IsWhitelistPath(const std::wstring& path) {
        std::wstring lowerPath = path;
        std::transform(lowerPath.begin(), lowerPath.end(), lowerPath.begin(), ::towlower);

        if (lowerPath.find(L"\\windows\\system32\\") != std::wstring::npos) return true;
        if (lowerPath.find(L"\\windows\\syswow64\\") != std::wstring::npos) return true;
        if (lowerPath.find(L"\\windows\\winsxs\\") != std::wstring::npos) return true;
        if (lowerPath.find(L"\\fivem\\") != std::wstring::npos) return true;
        if (lowerPath.find(L"citilaunch_tlsdummy.dll") != std::wstring::npos) return true;
        
        return false;
    }

    // Tải danh sách module blacklist từ FxAPI Server qua TCP
    void FetchBlacklistsFromAPI() {
        std::vector<FxFile> emptyFiles;

        SendToFxAPI_V2("GET_BLACKLISTS", "{}", emptyFiles, [this](int success, std::string data) {
            if (!success) {
                std::cout << "[SAMAEL AC] Module Scanner: Khong the ket noi FxAPI, cho ket noi lai..." << std::endl;
                // Không đánh dấu listsLoaded = true → scanner sẽ không quét
                return;
            }

            try {
                auto parsed = json::parse(data);

                std::lock_guard<std::mutex> lock(listMutex);

                // Parse danh sách module
                blacklistedModules.clear();
                if (parsed.contains("module") && parsed["module"].is_array()) {
                    for (const auto& item : parsed["module"]) {
                        if (item.contains("value") && item["value"].is_string()) {
                            blacklistedModules.push_back(item["value"].get<std::string>());
                        }
                    }
                }

                if (!blacklistedModules.empty()) {
                    listsLoaded = true;
                    std::cout << "[SAMAEL AC] Module Scanner: Da tai " << blacklistedModules.size() << " modules tu FxAPI." << std::endl;
                } else {
                    std::cout << "[SAMAEL AC] Module Scanner: API tra ve 0 modules, scanner tam ngung." << std::endl;
                }
            }
            catch (const std::exception& e) {
                std::cout << "[SAMAEL AC] Module Scanner: Loi parse: " << e.what() << std::endl;
            }
        });
    }

public:
    ModuleScanner() {}

    void RefreshBlacklists() {
        FetchBlacklistsFromAPI();
    }

    std::string ScanLoadedModules() {
        HANDLE hProcess = GetCurrentProcess();
        HMODULE hMods[1024];
        DWORD cbNeeded;

        std::lock_guard<std::mutex> lock(listMutex);

        if (EnumProcessModules(hProcess, hMods, sizeof(hMods), &cbNeeded)) {
            for (size_t i = 0; i < (cbNeeded / sizeof(HMODULE)); i++) {
                wchar_t szModName[MAX_PATH];
                if (GetModuleFileNameExW(hProcess, hMods[i], szModName, sizeof(szModName) / sizeof(wchar_t))) {
                    
                    std::wstring wModName(szModName);
                    std::string modNameStr(wModName.begin(), wModName.end());
                    std::string lowerModName = ToLower(modNameStr);

                    // 1. Kiểm tra xem tên DLL có trùng Blacklist không
                    for (const auto& black : blacklistedModules) {
                        if (lowerModName.find(ToLower(black)) != std::string::npos) {
                            return "Blacklisted DLL: " + modNameStr;
                        }
                    }

                    // 2. Chống DLL Trắng (Không chữ ký) - Ngăn chặn Bypass bằng cách đổi tên file
                    if (!IsWhitelistPath(wModName)) {
                        if (lowerModName.find(".exe") == std::string::npos && lowerModName.find(".asi") == std::string::npos) {
                            if (!VerifyFileSignature(szModName)) {
                                return "Unsigned/Untrusted DLL Injected: " + modNameStr;
                            }
                        }
                    }
                }
            }
        }
        return "";
    }

    void StartModuleMonitoring() {
        std::cout << "[SAMAEL AC] He thong quet DLL (Module Scanner) dang cho du lieu tu API..." << std::endl;
        
        // Gọi API lần đầu
        FetchBlacklistsFromAPI();

        // Biến đếm để refresh danh sách định kỳ (mỗi 5 phút)
        int scanCount = 0;
        const int refreshInterval = 20; // 20 lần quét x 15s = 5 phút

        while (true) {
            // Chỉ quét khi đã có dữ liệu từ API
            if (listsLoaded) {
                std::string alertMsg = ScanLoadedModules();
                if (!alertMsg.empty()) {
                    std::cout << "[SAMAEL AC] " << alertMsg << std::endl;
                    
                    GetScreenShotV2("c1111111111111111111111111111111", alertMsg, GetHWIDV2JsonString(), "danger", "dll_injection", false);
                    Sleep(1000); 

                    // MessageBoxA(NULL, "SAMAEL ANTI-CHEAT: Phat hien DLL khong ro nguon goc (Injection). Game se bi dong!", "Canh bao Bao mat", MB_ICONERROR | MB_OK);
                    ExitProcess(0);
                }
            }

            Sleep(15000);

            // Refresh / retry danh sách định kỳ
            scanCount++;
            if (scanCount >= refreshInterval || !listsLoaded) {
                scanCount = 0;
                RefreshBlacklists();
            }
        }
    }
};
