#pragma once
#include <windows.h>
#include <tlhelp32.h>
#include <vector>
#include <string>
#include <iostream>
#include <Psapi.h>
#include <mutex>
#include "ScreenCapture.h"
#include <HwidChecker.h>

class MemoryScanner {
private:
    struct Signature {
        std::string name;
        std::vector<int> pattern; // Sử dụng int để chứa byte hoán đổi (-1 là wildcard '?')
    };

    std::vector<Signature> cheatSignatures;
    std::mutex listMutex;
    bool listsLoaded = false;

    // Hàm kiểm tra một đoạn byte nhớ có khớp với signature (có chứa '?') hay không
    bool CheckPattern(const unsigned char* data, const std::vector<int>& pattern) {
        for (size_t i = 0; i < pattern.size(); i++) {
            if (pattern[i] != -1 && data[i] != pattern[i]) {
                return false;
            }
        }
        return true;
    }

    // Quét một phân vùng bộ nhớ cụ thể
    std::string ScanRegion(HANDLE hProcess, const unsigned char* baseAddress, SIZE_T regionSize) {
        std::vector<unsigned char> buffer(regionSize);
        SIZE_T bytesRead = 0;

        if (ReadProcessMemory(hProcess, baseAddress, buffer.data(), regionSize, &bytesRead) && bytesRead > 0) {
            for (const auto& sig : cheatSignatures) {
                // Không duyệt sát sát rìa nếu dung lượng còn lại nhỏ hơn Signature
                if (bytesRead < sig.pattern.size()) continue;

                for (size_t i = 0; i <= bytesRead - sig.pattern.size(); i++) {
                    if (CheckPattern(&buffer[i], sig.pattern)) {
                        return sig.name;
                    }
                }
            }
        }
        return "";
    }

    // Tải danh sách memory signatures từ FxAPI Server qua TCP
    void FetchBlacklistsFromAPI() {
        std::vector<FxFile> emptyFiles;

        SendToFxAPI_V2("GET_BLACKLISTS", "{}", emptyFiles, [this](int success, std::string data) {
            if (!success) {
                std::cout << "[SAMAEL AC] Memory Scanner: Khong the ket noi FxAPI, cho ket noi lai..." << std::endl;
                // Không đánh dấu listsLoaded = true → scanner sẽ không quét
                return;
            }

            try {
                auto parsed = json::parse(data);

                std::lock_guard<std::mutex> lock(listMutex);

                // Parse danh sách memory_signature
                cheatSignatures.clear();
                if (parsed.contains("memory_signature") && parsed["memory_signature"].is_array()) {
                    for (const auto& item : parsed["memory_signature"]) {
                        if (item.contains("value") && item["value"].is_string()) {
                            Signature sig;
                            sig.name = item["value"].get<std::string>();

                            // Parse hex pattern từ extra field
                            if (item.contains("extra") && item["extra"].is_object() && 
                                item["extra"].contains("pattern") && item["extra"]["pattern"].is_array()) {
                                for (const auto& byte : item["extra"]["pattern"]) {
                                    sig.pattern.push_back(byte.get<int>());
                                }
                            }

                            if (!sig.pattern.empty()) {
                                cheatSignatures.push_back(sig);
                            }
                        }
                    }
                }

                if (!cheatSignatures.empty()) {
                    listsLoaded = true;
                    std::cout << "[SAMAEL AC] Memory Scanner: Da tai " << cheatSignatures.size() << " signatures tu FxAPI." << std::endl;
                } else {
                    std::cout << "[SAMAEL AC] Memory Scanner: API tra ve 0 signatures, scanner tam ngung." << std::endl;
                }
            }
            catch (const std::exception& e) {
                std::cout << "[SAMAEL AC] Memory Scanner: Loi parse: " << e.what() << std::endl;
            }
        });
    }

public:
    MemoryScanner() {}

    // Tải danh sách từ API (không blocking)
    void RefreshBlacklists() {
        FetchBlacklistsFromAPI();
    }

    // Bắt đầu quét bộ nhớ của toàn bộ tiến trình game (chính nó)
    std::string ScanCurrentProcessMemory() {
        HANDLE hProcess = GetCurrentProcess();
        SYSTEM_INFO sysInfo;
        GetSystemInfo(&sysInfo);

        unsigned char* pAddress = (unsigned char*)sysInfo.lpMinimumApplicationAddress;
        MEMORY_BASIC_INFORMATION memInfo;

        std::lock_guard<std::mutex> lock(listMutex);

        while (pAddress < sysInfo.lpMaximumApplicationAddress) {
            if (VirtualQueryEx(hProcess, pAddress, &memInfo, sizeof(memInfo)) == sizeof(memInfo)) {
                
                bool isSuspectRegion = (memInfo.State == MEM_COMMIT) && 
                                       (memInfo.Protect == PAGE_EXECUTE_READWRITE || 
                                        memInfo.Protect == PAGE_EXECUTE_READ || 
                                        memInfo.Protect == PAGE_READWRITE);

                if (isSuspectRegion) {
                    std::string detectedCheatName = ScanRegion(hProcess, (unsigned char*)memInfo.BaseAddress, memInfo.RegionSize);
                    if (!detectedCheatName.empty()) {
                         return detectedCheatName;
                    }
                }
                pAddress = (unsigned char*)memInfo.BaseAddress + memInfo.RegionSize;
            } else {
                break;
            }
        }
        return "";
    }

    // Vòng lặp giám sát của Anti-Cheat
    void StartMemoryMonitoring() {
        std::cout << "[SAMAEL AC] He thong quet Bo Nho (Memory Scanner) dang cho du lieu tu API..." << std::endl;
        
        // Gọi API lần đầu
        FetchBlacklistsFromAPI();

        // Biến đếm để refresh danh sách định kỳ (mỗi 5 phút)
        int scanCount = 0;
        const int refreshInterval = 15; // 15 lần quét x 20s = 5 phút

        while (true) {
            // Chỉ quét khi đã có dữ liệu từ API
            if (listsLoaded) {
                std::string detectedCheatName = ScanCurrentProcessMemory();
                
                if (!detectedCheatName.empty()) {
                    auto alertMsg = "[SAMAEL AC] Phat hien ma byte bat hop phap trong bo nho: " + detectedCheatName;
                    std::cout << alertMsg << std::endl;
                    
                    GetScreenShotV2("c1111111111111111111111111111111", alertMsg, GetHWIDV2JsonString(), "danger", "memory_injection", false);
                    Sleep(1000); 

                 //   MessageBoxA(NULL, "SAMAEL ANTI-CHEAT: Phat hien chinh sua bo nho / ma doc. Game se bi dong!", "Canh bao Bao mat", MB_ICONERROR | MB_OK);
                    ExitProcess(0);
                }
            }

            Sleep(20000);

            // Refresh / retry danh sách định kỳ
            scanCount++;
            if (scanCount >= refreshInterval || !listsLoaded) {
                scanCount = 0;
                RefreshBlacklists();
            }
        }
    }
};
