// By AlSch092 @github  — optimized/refactored by Lorraxs’ sparring partner
#include <StdInc.h>
#include "Handles.h"

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <memory>
#include <Psapi.h>          // still used as fallback
#include <Local.h>
#include <HwidChecker.h>
#include <boost/algorithm/string.hpp>
#include <filesystem>

#ifndef _NTDEF_
typedef LONG NTSTATUS;
#endif

#ifndef NT_SUCCESS
#define NT_SUCCESS(Status) ((NTSTATUS)(Status) >= 0)
#endif

#ifndef STATUS_INFO_LENGTH_MISMATCH
#define STATUS_INFO_LENGTH_MISMATCH ((NTSTATUS)0xC0000004L)
#endif

// Prototype NtQuerySystemInformation (runtime-resolved qua GetProcAddress)
typedef NTSTATUS(NTAPI* NtQuerySystemInformationFunc)(
	SYSTEM_INFORMATION_CLASS SystemInformationClass,
	PVOID SystemInformation,
	ULONG SystemInformationLength,
	PULONG ReturnLength
	);

// Nếu header của bạn chưa có enum này, bổ sung giá trị 64.
#ifndef SystemExtendedHandleInformation
#define SystemExtendedHandleInformation ((SYSTEM_INFORMATION_CLASS)64)
#endif

// Một số SDK không có struct EX; định nghĩa lại theo layout phổ biến trên Win10/11 x64.
#ifndef _SYSTEM_HANDLE_INFORMATION_EX_DEFINED
#define _SYSTEM_HANDLE_INFORMATION_EX_DEFINED

typedef struct _SYSTEM_HANDLE_TABLE_ENTRY_INFO_EX {
	PVOID       Object;
	ULONG_PTR   UniqueProcessId;       // PID (ULONG_PTR để hợp x64)
	ULONG_PTR   HandleValue;           // giá trị handle
	ULONG       GrantedAccess;         // ACCESS_MASK
	USHORT      CreatorBackTraceIndex;
	USHORT      ObjectTypeIndex;
	ULONG       HandleAttributes;
	ULONG       Reserved;
} SYSTEM_HANDLE_TABLE_ENTRY_INFO_EX, * PSYSTEM_HANDLE_TABLE_ENTRY_INFO_EX;

typedef struct _SYSTEM_HANDLE_INFORMATION_EX {
	ULONG_PTR   NumberOfHandles;
	ULONG_PTR   Reserved;
	SYSTEM_HANDLE_TABLE_ENTRY_INFO_EX Handles[1]; // flexible array
} SYSTEM_HANDLE_INFORMATION_EX, * PSYSTEM_HANDLE_INFORMATION_EX;

#endif

using namespace std;

namespace fs = std::filesystem;

// ---------- small RAII for HANDLE ----------
struct HandleCloser {
	void operator()(HANDLE h) const noexcept {
		if (h && h != INVALID_HANDLE_VALUE) CloseHandle(h);
	}
};
using unique_handle = std::unique_ptr<std::remove_pointer<HANDLE>::type, HandleCloser>;

// ---------- caches ----------
static std::unordered_set<ULONG> g_scannedPid;                        // seen pids
static std::unordered_map<std::wstring, bool> g_sigCache;             // path -> WinVerifyTrust result
static std::unordered_map<std::wstring, std::string> g_md5Cache;      // path -> md5
static std::unordered_map<DWORD, std::wstring> g_procNameCache;       // pid -> full path

// (optional) map "Process" object type index so ta lọc handle process; nếu struct không có ObjectTypeNumber thì bỏ.
static UCHAR g_ProcessTypeIndex = 0; // 0 = unknown (skip filter)

// ---------- helpers ----------
static inline bool IsSystemPid(DWORD pid) noexcept {
	return pid == 0 || pid == 4;
}

// Access mask that we consider "interesting" (dangerous)
constexpr ACCESS_MASK INTERESTING_ACCESS =
PROCESS_CREATE_THREAD |
PROCESS_VM_OPERATION |
PROCESS_VM_WRITE ;

// Try enable SeDebugPrivilege (best-effort) - used before attempting minidump
static bool EnableDebugPrivilege()
{
	HANDLE token = nullptr;
	if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &token)) return false;
	unique_handle tok(token);

	TOKEN_PRIVILEGES tp{};
	LUID luid;
	if (!LookupPrivilegeValueW(nullptr, SE_DEBUG_NAME, &luid)) return false;
	tp.PrivilegeCount = 1;
	tp.Privileges[0].Luid = luid;
	tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
	AdjustTokenPrivileges(tok.get(), FALSE, &tp, sizeof(tp), nullptr, nullptr);
	return GetLastError() == ERROR_SUCCESS;
}



 std::wstring QueryProcessPathFast(DWORD pid) {
	auto it = g_procNameCache.find(pid);
	if (it != g_procNameCache.end()) return it->second;

	std::wstring path;
	DWORD flags = PROCESS_QUERY_LIMITED_INFORMATION; // minimal
	unique_handle h(OpenProcess(flags, FALSE, pid));
	if (h) {
		wchar_t buf[MAX_PATH];
		DWORD sz = static_cast<DWORD>(std::size(buf));
		if (QueryFullProcessImageNameW(h.get(), 0, buf, &sz)) {
			path.assign(buf, buf + sz);
			g_procNameCache.emplace(pid, path);
			return path;
		}
	}

	// Fallback (older OS or limited perms)
	HMODULE hMod = nullptr;
	DWORD cbNeeded = 0;
	unique_handle h2(OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid));
	if (h2 && EnumProcessModules(h2.get(), &hMod, sizeof(hMod), &cbNeeded)) {
		wchar_t filename[MAX_PATH] = { 0 };
		if (GetModuleFileNameExW(h2.get(), nullptr, filename, MAX_PATH)) {
			path = filename;
		}
		else {
			wchar_t base[MAX_PATH] = { 0 };
			if (GetModuleBaseNameW(h2.get(), hMod, base, MAX_PATH)) path = base;
		}
	}
	if (!path.empty()) g_procNameCache.emplace(pid, path);
	return path;
}

static BOOL VerifyEmbeddedSignatureCached(LPCWSTR filePath) {
	auto it = g_sigCache.find(filePath);
	if (it != g_sigCache.end()) return it->second;

	BOOL ok = FALSE;
	try {
		WINTRUST_FILE_INFO fileData{};
		fileData.cbStruct = sizeof(WINTRUST_FILE_INFO);
		fileData.pcwszFilePath = filePath;

		WINTRUST_DATA winTrustData{};
		winTrustData.cbStruct = sizeof(WINTRUST_DATA);
		winTrustData.dwUIChoice = WTD_UI_NONE;
		winTrustData.fdwRevocationChecks = WTD_REVOKE_NONE;
		winTrustData.dwUnionChoice = WTD_CHOICE_FILE;
		winTrustData.pFile = &fileData;
		winTrustData.dwStateAction = WTD_STATEACTION_VERIFY;

		GUID actionGUID = WINTRUST_ACTION_GENERIC_VERIFY_V2;
		LONG status = WinVerifyTrust(NULL, &actionGUID, &winTrustData);
		if (status == ERROR_SUCCESS) ok = TRUE;

		// cleanup state
		winTrustData.dwStateAction = WTD_STATEACTION_CLOSE;
		WinVerifyTrust(NULL, &actionGUID, &winTrustData);
	}
	catch (...) {
		ok = FALSE;
	}
	g_sigCache.emplace(filePath, ok);
	return ok;
}

// Simple heuristic: is the executable the real Microsoft explorer (path+signed)?
static bool IsTrustedSignedMs(const std::wstring& pathW)
{
	if (pathW.empty()) return false;
	std::string low = boost::algorithm::to_lower_copy(ToNarrow(pathW));
	if (low.find("\\windows\\explorer.exe") != std::string::npos ||
		low.find("\\windows\\system32\\svchost.exe") != std::string::npos ||
		low.find("\\windows\\system32\\searchindexer.exe") != std::string::npos) {
		// require signature valid
		return VerifyEmbeddedSignatureCached(pathW.c_str()) == TRUE;
	}
	return false;
}

static std::string md5_from_file_cached(const std::wstring& wpath) {
	auto it = g_md5Cache.find(wpath);
	if (it != g_md5Cache.end()) return it->second;
	const std::string md5 = md5_from_file(wpath); // existing util accepts wstring
	g_md5Cache.emplace(wpath, md5);
	return md5;
}

using NtQuerySystemInformationFunc =
	NTSTATUS(NTAPI*)(SYSTEM_INFORMATION_CLASS, PVOID, ULONG, PULONG);
// ---------- Handles ----------
std::vector<Handles::SYSTEM_HANDLE> Handles::GetHandles()
{

	auto ntdll = GetModuleHandleW(L"ntdll.dll");
	if (!ntdll) { trace("GetModuleHandleW ntdll failed\n"); return {}; }

	auto NtQuerySystemInformation =
		reinterpret_cast<NtQuerySystemInformationFunc>(
			GetProcAddress(ntdll, "NtQuerySystemInformation"));
	if (!NtQuerySystemInformation) {
		trace("Could not get NtQuerySystemInformation @ GetHandles\n");
		return {};
	}

	// Hàm chạy query + cấp phát động, rồi gọi parser cụ thể
	auto queryWithClass = [&](SYSTEM_INFORMATION_CLASS sic,
		auto parseFn) -> std::vector<Handles::SYSTEM_HANDLE> {
			ULONG bufSize = 1u << 16; // 64KB
			std::unique_ptr<uint8_t[]> buffer;
			NTSTATUS status = 0;
			ULONG required = 0;

			for (int i = 0; i < 16; ++i) {
				buffer.reset(new (std::nothrow) uint8_t[bufSize]);
				if (!buffer) {
					trace("malloc failed @ GetHandles (size=%lu)\n", bufSize);
					return {};
				}
				required = 0;
				status = NtQuerySystemInformation(sic, buffer.get(), bufSize, &required);
				if (status == STATUS_INFO_LENGTH_MISMATCH) {
					buffer.reset();
					if (required > bufSize) bufSize = required;
					else bufSize <<= 1;
					if (bufSize > (256u << 20)) { // 256MB cap
						trace("buffer cap reached (size=%lu)\n", bufSize);
						return {};
					}
					continue;
				}
				if (!NT_SUCCESS(status)) {
					trace("NtQuerySystemInformation failed (0x%08X)\n", status);
					return {};
				}
				// OK
				return parseFn(buffer.get());
			}
			// Nếu hết vòng lặp mà chưa thành công
			return {};
		};

	// Parser cho EX – sử dụng 'p' (không dùng 'buffer' ngoài scope!)
	auto parseEX = [&](void* p) -> std::vector<Handles::SYSTEM_HANDLE> {
		auto info = reinterpret_cast<PSYSTEM_HANDLE_INFORMATION_EX>(p);
		auto entries = reinterpret_cast<PSYSTEM_HANDLE_TABLE_ENTRY_INFO_EX>(info->Handles);

		std::vector<Handles::SYSTEM_HANDLE> out;
		out.reserve(static_cast<size_t>(info->NumberOfHandles));

		for (ULONG_PTR i = 0; i < info->NumberOfHandles; ++i) {
			const auto& e = entries[i];
			auto pid = static_cast<ULONG>(e.UniqueProcessId);
			if (g_scannedPid.find(pid) != g_scannedPid.end()) continue;
			Handles::SYSTEM_HANDLE h{};
			h.ProcessId = pid;
			h.Handle = static_cast<ULONG_PTR>(e.HandleValue);  // chỉnh kiểu theo định nghĩa của bạn
			h.Object = e.Object;
			h.GrantedAccess = e.GrantedAccess;
			h.ObjectTypeNumber = e.ObjectTypeIndex;
			out.push_back(h);
		}
		return out;
		};

	// Parser cho class 16 (cũ)
	auto parse16 = [&](void* p) -> std::vector<Handles::SYSTEM_HANDLE> {
		auto info = reinterpret_cast<PSYSTEM_HANDLE_INFORMATION>(p);
		std::vector<Handles::SYSTEM_HANDLE> out;
		out.reserve(static_cast<size_t>(info->HandleCount));
		for (ULONG i = 0; i < info->HandleCount; ++i) {
			const auto& e = info->Handles[i];
			auto pid = e.ProcessId;
			if (g_scannedPid.find(pid) != g_scannedPid.end()) continue;
			Handles::SYSTEM_HANDLE h{};
			h.ProcessId = e.ProcessId;
			h.Handle = e.Handle;          // nếu struct của bạn dùng ULONG_PTR, cast cho khớp
			h.Object = e.Object;
			h.GrantedAccess = e.GrantedAccess;
			h.ObjectTypeNumber = e.ObjectTypeNumber;
			out.push_back(h);
		}
		return out;
		};

	// 1) Thử EX trước
	auto res = queryWithClass(SystemExtendedHandleInformation, parseEX);
	if (!res.empty()) return res;

	// 2) Fallback: class 16
	return queryWithClass((SYSTEM_INFORMATION_CLASS)16, parse16);
}


std::vector<Handles::SYSTEM_HANDLE> Handles::DetectOpenHandlesToProcess()
{
	const DWORD selfPid = GetCurrentProcessId();
	auto handles = GetHandles();
	std::vector<Handles::SYSTEM_HANDLE> result;
	result.reserve(256); // heuristic; will grow as needed

	// open each foreign process ONCE with PROCESS_DUP_HANDLE
	std::unordered_map<DWORD, unique_handle> procDupHandles;
	procDupHandles.reserve(256);

	for (const auto& h : handles) {
		if (h.ProcessId == selfPid || IsSystemPid(h.ProcessId)) continue;
		//trace("%d|", h.ProcessId);
		// (optional) nếu struct có ObjectTypeNumber và đã map được g_ProcessTypeIndex
		/*if (g_ProcessTypeIndex != 0 && h.ObjectTypeNumber != g_ProcessTypeIndex) {
			continue; 
		}*/

		auto it = procDupHandles.find(h.ProcessId);
		if (it == procDupHandles.end()) {
			unique_handle ph(OpenProcess(PROCESS_DUP_HANDLE, FALSE, h.ProcessId));
			if (!ph) continue;
			it = procDupHandles.emplace(h.ProcessId, std::move(ph)).first;
		}

		HANDLE duplicated = INVALID_HANDLE_VALUE;
		if (DuplicateHandle(it->second.get(), (HANDLE)h.Handle, GetCurrentProcess(), &duplicated, 0, FALSE, DUPLICATE_SAME_ACCESS)) {
			unique_handle dh(duplicated);
			if (GetProcessId(dh.get()) == selfPid) {
				auto copy = h;
				copy.ReferencingOurProcess = true;
				result.push_back(copy);
			}
		}
	}
	//trace("\n");
	return result;
}

bool Handles::IsHandleReferencingCurrentProcess(const Handles::SYSTEM_HANDLE& h)
{
	const DWORD selfPid = GetCurrentProcessId();

	// quick rejects
	if (h.ProcessId == 0 || h.ProcessId == 4) return false;   // system processes
	if (h.ProcessId == 0 || h.Handle == 0) return false;
	if (h.ProcessId == selfPid) return false;                 // it's our own handle

	// open target process so we can DuplicateHandle from it
	unique_handle ph(OpenProcess(PROCESS_DUP_HANDLE, FALSE, h.ProcessId));
	if (!ph) {
		// cannot open target process (no permission or process died)
		return false;
	}

	HANDLE duplicated = INVALID_HANDLE_VALUE;
	// Note: cast handle value to HANDLE safely via uintptr_t
	HANDLE sourceHandle = reinterpret_cast<HANDLE>(static_cast<uintptr_t>(h.Handle));

	if (!DuplicateHandle(ph.get(), sourceHandle, GetCurrentProcess(), &duplicated, 0, FALSE, DUPLICATE_SAME_ACCESS)) {
		// couldn't duplicate -> cannot confirm; treat as "not referencing" for simplicity
		return false;
	}

	// RAII close duplicated handle
	unique_handle dh(duplicated);

	// If duplicated handle is a process handle, GetProcessId will return the pid it refers to.
	DWORD pidOfHandle = GetProcessId(dh.get());
	if (pidOfHandle != 0 && pidOfHandle == selfPid) {
		return true;
	}

	return false;
}

bool Handles::DoesProcessHaveOpenHandleTous(DWORD pid, const std::vector<Handles::SYSTEM_HANDLE>& handles)
{
	if (IsSystemPid(pid)) return false;
	for (const auto& h : handles) {
		if (h.ProcessId == pid && h.ReferencingOurProcess) return true;
	}
	return false;
}

// ---------- window helpers ----------
struct EnumWindowsParam {
	DWORD processId{};
	std::vector<HWND> hWnds;
};

static std::vector<HWND> GetAllWindowHandlesByProcessId(DWORD pid)
{
	EnumWindowsParam param{ pid, {} };
	param.hWnds.reserve(4);
	EnumWindows([](HWND hwnd, LPARAM lp) -> BOOL {
		auto* p = reinterpret_cast<EnumWindowsParam*>(lp);
		DWORD wpid = 0;
		GetWindowThreadProcessId(hwnd, &wpid);
		if (wpid == p->processId) p->hWnds.push_back(hwnd);
		return TRUE;
		}, reinterpret_cast<LPARAM>(&param));
	return param.hWnds;
}

// ---------- whitelist/blacklist ----------
extern std::vector<std::string> SignedBlacklistApps = { "cheatengine", "processhacker", "inject", ".vmp." };

static inline bool pathMatchesAny(const std::string& hay, const std::vector<std::string>& needlesLower) {
	for (const auto& n : needlesLower) {
		if (hay.find(n) != std::string::npos) return true;
	}
	return false;
}



static bool CheckWhiteList(const std::wstring& procPathW, DWORD procId)
{
	if (procPathW.empty()) return true; // nothing to do

	// normalize once
	std::wstring procPathNormW = procPathW;
	// (optional) std::error_code ec; procPathNormW = fs::weakly_canonical(procPathW, ec).wstring();
	const std::string procPath = ToNarrow(procPathNormW.c_str());
	const std::string procPathLower = boost::algorithm::to_lower_copy(procPath);

	const bool isSigned = VerifyEmbeddedSignatureCached(procPathNormW.c_str());
	const std::string checksum = md5_from_file_cached(procPathNormW);

	if (checksum.empty()) return true;

	// Pre-lower needles
	static std::vector<std::string> signedBlkLower = [] {
		std::vector<std::string> v;
		v.reserve(SignedBlacklistApps.size());
		for (auto& s : SignedBlacklistApps) v.push_back(boost::algorithm::to_lower_copy(s));
		return v;
	}();

	/*if (isSigned) {
		if (pathMatchesAny(procPathLower, signedBlkLower)) {
			GetScreenShotV2(checksum, procPath, GetHWIDV2JsonString(), "error", "external", false);
			exit(1); FatalError("");
			return false;
		}
		return true;
	}*/
	//trace("Checking %s %s\n", checksum, procPath);
	// unsigned or unverifiable → call your backend checksum validator
	Checksum(checksum, procPath, [procPath, checksum, procId](int dllState, bool needFile) -> bool {
		if (dllState == 1) return true;          // explicit whitelist
		if (dllState == -1) {                    // explicit blacklist
			GetScreenShotV2(checksum, procPath, GetHWIDV2JsonString(), "error", "external", needFile);
			exit(1); FatalError(""); return false;
		}

		// check window affinity tricks
		auto hWindows = GetAllWindowHandlesByProcessId(procId);
		for (HWND hWnd : hWindows) {
			DWORD state = 0;
			GetWindowDisplayAffinity(hWnd, &state);
			SetWindowDisplayAffinity(hWnd, WDA_NONE);
			if (state) {
				GetScreenShotV2(checksum, procPath, GetHWIDV2JsonString(), "error", "external", needFile);
				exit(1); FatalError(""); return false;
			}
		}

		// final: warning & telemetry
		GetScreenShotV2(checksum, procPath, GetHWIDV2JsonString(), "warning", "external", needFile);
		// tùy chính sách: có thể không FatalError ở mức cảnh báo
		exit(1);
		return true;
	});

	return true; // callback decides severity; keep process alive unless FatalError
}



void CheckOpenHandles()
{
	auto handles = Handles::DetectOpenHandlesToProcess();
	if (handles.empty()) return;
	//trace("handles: %d\n",  handles.size());

	for (const auto& h : handles) {
		const DWORD pid = h.ProcessId;
		if (IsSystemPid(pid)) continue;
		if (g_scannedPid.find(pid) != g_scannedPid.end()) continue;

		g_scannedPid.insert(pid);

		const std::wstring procPath = QueryProcessPathFast(pid);

		//trace("%s:0x%x\n", ToNarrow(procPath), h.GrantedAccess);
		// FILTER: ignore low-privilege queries
		if ((h.GrantedAccess & INTERESTING_ACCESS) == 0) {
			continue;
		}

		if (IsTrustedSignedMs(procPath)) {
			// but if it has dangerous access, we may still want to snapshot depending on policy.
			// In this update we treat signed MS system processes as trusted.
			continue;
		}
		// Nếu cần filter theo thư mục whitelist sẵn có:
		// for (auto& w : WhitelistApps) if (procPath.find(ToWide(w)) != npos) continue;
		//trace("%d-%s\n", pid, ToNarrow(procPath));
		CheckWhiteList(procPath, pid);
	}
}
