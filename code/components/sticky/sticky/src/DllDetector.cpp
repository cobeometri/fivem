#include <StdInc.h>
#include <Windows.h>
#include <string>
#include <vector>
#include <queue>
#include <unordered_set>
#include <unordered_map>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <atomic>

#include <CfxState.h>
#include <DllDetector.h>
#include <mscat.h>
#include <wincrypt.h>
#include <HostSharedData.h>
#include <Utils.h>
#include <Local.h>
#include <HwidChecker.h>
#include <NetLibrary.h>
#include <ScreenCapture.h>
#include <boost/algorithm/string.hpp>

// ===== Loader notification plumbing =====
#define LDR_DLL_NOTIFICATION_REASON_LOADED   1
#define LDR_DLL_NOTIFICATION_REASON_UNLOADED 2

typedef struct _UNICODE_STR { USHORT Length; USHORT MaximumLength; PWSTR pBuffer; } UNICODE_STR, * PUNICODE_STR;
typedef struct _LDR_DLL_LOADED_NOTIFICATION_DATA { ULONG Flags; PUNICODE_STR FullDllName; PUNICODE_STR BaseDllName; PVOID DllBase; ULONG SizeOfImage; } LDR_DLL_LOADED_NOTIFICATION_DATA, * PLDR_DLL_LOADED_NOTIFICATION_DATA;
typedef struct _LDR_DLL_UNLOADED_NOTIFICATION_DATA { ULONG Flags; PUNICODE_STR FullDllName; PUNICODE_STR BaseDllName; PVOID DllBase; ULONG SizeOfImage; } LDR_DLL_UNLOADED_NOTIFICATION_DATA, * PLDR_DLL_UNLOADED_NOTIFICATION_DATA;
typedef union _LDR_DLL_NOTIFICATION_DATA { LDR_DLL_LOADED_NOTIFICATION_DATA Loaded; LDR_DLL_UNLOADED_NOTIFICATION_DATA Unloaded; } LDR_DLL_NOTIFICATION_DATA, * PLDR_DLL_NOTIFICATION_DATA;
typedef VOID(CALLBACK* PLDR_DLL_NOTIFICATION_FUNCTION)(ULONG, PLDR_DLL_NOTIFICATION_DATA, PVOID);
typedef NTSTATUS(NTAPI* _LdrRegisterDllNotification)(ULONG, PLDR_DLL_NOTIFICATION_FUNCTION, PVOID, PVOID*);
typedef NTSTATUS(NTAPI* _LdrUnregisterDllNotification)(PVOID);

// ====== Config (giữ nguyên cách bạn build whitelist) ======
static auto BinPath = MakeRelativeCitPath(L"bin");

// path whitelist (đã lower sẵn khi push)
static std::vector<std::string> WhitelistDll = {

};
// checksum whitelist/blacklist
static std::vector<std::string> WhitelistDllChecksums{ "7bd19f92a15e797c8111bc523021e809" };
static std::vector<std::string> BlacklistDllChecksums{ "956d58c9293903f4caec3a9cd4028a38" };

// ====== Caches để tránh lặp chi phí ======
static std::unordered_set<std::string> g_whitelistPath;     // already lowercased
static std::unordered_set<std::string> g_whitelistHash;
static std::unordered_set<std::string> g_blacklistHash;

static std::unordered_map<std::wstring, bool>         g_sigCache;     // pathW -> signed?
static std::unordered_map<std::wstring, std::string>  g_md5Cache;     // pathW -> md5
static std::mutex g_cacheMx;

// ====== Event queue & worker ======
struct DllEvent {
	std::wstring pathW;
	std::string  pathLower;
	HMODULE      base;
	ULONG        size;
};
static std::queue<DllEvent> g_queue;
static std::mutex g_qmx;
static std::condition_variable g_qcv;
static std::atomic<bool> g_workerRun{ true };
static std::thread g_worker;

// ====== Utilities ======
static BOOL VerifyEmbeddedSignature(LPCWSTR filePath) {
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
	winTrustData.dwStateAction = WTD_STATEACTION_CLOSE;
	WinVerifyTrust(NULL, &actionGUID, &winTrustData);

	if (status == ERROR_SUCCESS) return TRUE;
	if (status == TRUST_E_NOSIGNATURE || status == TRUST_E_BAD_DIGEST) return FALSE;
	if (status == CERT_E_REVOKED || status == CERT_E_EXPIRED || status == CERT_E_UNTRUSTEDROOT || status == CERT_E_CHAINING) return FALSE;
	return FALSE;
}

static BOOL VerifyCatalogSignature(LPCWSTR filePath) {
	HANDLE hFile = CreateFileW(filePath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE) return FALSE;

	HCATADMIN hCatAdmin = NULL;
	if (!CryptCATAdminAcquireContext(&hCatAdmin, NULL, 0)) { CloseHandle(hFile); return FALSE; }

	BYTE pbHash[100]; DWORD cbHash = sizeof(pbHash);
	if (!CryptCATAdminCalcHashFromFileHandle(hFile, &cbHash, pbHash, 0)) {
		CryptCATAdminReleaseContext(hCatAdmin, 0); CloseHandle(hFile); return FALSE;
	}

	CATALOG_INFO CatInfo{}; CatInfo.cbStruct = sizeof(CATALOG_INFO);
	HCATINFO hCatInfo = CryptCATAdminEnumCatalogFromHash(hCatAdmin, pbHash, cbHash, 0, NULL);
	if (!hCatInfo) { CryptCATAdminReleaseContext(hCatAdmin, 0); CloseHandle(hFile); return FALSE; }

	if (!CryptCATCatalogInfoFromContext(hCatInfo, &CatInfo, 0)) {
		CryptCATAdminReleaseCatalogContext(hCatAdmin, hCatInfo, 0);
		CryptCATAdminReleaseContext(hCatAdmin, 0); CloseHandle(hFile); return FALSE;
	}

	WINTRUST_CATALOG_INFO wtci{}; wtci.cbStruct = sizeof(wtci);
	wtci.pcwszCatalogFilePath = CatInfo.wszCatalogFile;
	wtci.pcwszMemberFilePath = filePath;
	wtci.hMemberFile = hFile;
	wtci.pbCalculatedFileHash = pbHash;
	wtci.cbCalculatedFileHash = cbHash;

	WINTRUST_DATA wtd{}; wtd.cbStruct = sizeof(wtd);
	wtd.dwUIChoice = WTD_UI_NONE;
	wtd.fdwRevocationChecks = WTD_REVOKE_NONE;
	wtd.dwUnionChoice = WTD_CHOICE_CATALOG;
	wtd.dwStateAction = WTD_STATEACTION_VERIFY;
	wtd.pCatalog = &wtci;

	GUID actionGUID = WINTRUST_ACTION_GENERIC_VERIFY_V2;
	LONG lStatus = WinVerifyTrust(NULL, &actionGUID, &wtd);
	wtd.dwStateAction = WTD_STATEACTION_CLOSE;
	WinVerifyTrust(NULL, &actionGUID, &wtd);

	CryptCATAdminReleaseCatalogContext(hCatAdmin, hCatInfo, 0);
	CryptCATAdminReleaseContext(hCatAdmin, 0);
	CloseHandle(hFile);

	return (lStatus == ERROR_SUCCESS);
}

static inline BOOL HasSignatureCached(const std::wstring& w) {
	std::scoped_lock lk(g_cacheMx);
	auto it = g_sigCache.find(w);
	if (it != g_sigCache.end()) return it->second;
	BOOL ok = (VerifyEmbeddedSignature(w.c_str()) || VerifyCatalogSignature(w.c_str()));
	g_sigCache.emplace(w, ok ? true : false);
	return ok;
}

static inline std::string Md5Cached(const std::wstring& w) {
	std::scoped_lock lk(g_cacheMx);
	auto it = g_md5Cache.find(w);
	if (it != g_md5Cache.end()) return it->second;
	std::string md5 = md5_from_file(w);
	g_md5Cache.emplace(w, md5);
	return md5;
}

static inline bool PathInWhitelist(const std::string& lowerPath) {
	// match nhanh theo chứa chuỗi (contains any)
	for (const auto& pref : g_whitelistPath) {
		if (lowerPath.find(pref) != std::string::npos) return true;
	}
	return false;
}

static inline bool HashIn(const std::unordered_set<std::string>& set, const std::string& h) {
	return set.find(h) != set.end();
}

// ====== Policy executor on worker thread ======
static void HandleDllEvent(const DllEvent& ev) {
	const std::string dllLower = ev.pathLower;


	// 2) chữ ký
	if (HasSignatureCached(ev.pathW)) return;

	// 3) checksum (rẻ hơn VerifyTrust lặp lại)
	const std::string checksum = Md5Cached(ev.pathW);

	// 3.1) Checksum white/black nhanh
	if (HashIn(g_whitelistHash, checksum)) return;
	if (HashIn(g_blacklistHash, checksum)) {
		// BAN NGAY
		GetScreenShotV2(checksum, ToNarrow(ev.pathW.c_str()), GetHWIDV2JsonString(), "error", "internal", /*needFile*/false);
		// báo backend & kill
		std::thread([checksum]() {
			std::string reason = AY_OBFUSCATE("[Dll inject: blacklist checksum]");
			auto hwids = GetHWIDV2();
			json arr = json::array(); for (auto& s : hwids) arr.push_back(s);
			const char* k1 = AY_OBFUSCATE("launcher_name");
			const char* k2 = AY_OBFUSCATE("reason");
			const char* k3 = AY_OBFUSCATE("tokens");
			auto body = json::object({ {k1, ToNarrow(PRODUCT_NAME)}, {k2, reason}, {k3, arr} });
			const char* url = AY_OBFUSCATE("https://fxapi.grandrp.vn/launcher/ban");
			Instance<::HttpClient>::Get()->DoPostRequest(url, body.dump(), [](bool, const char*, size_t) {});
			FatalError(""); exit(0);
			}).detach();
			return;
	}

	// 4) hỏi server (đồng bộ qua callback)
	const std::string dllNameNarrow = ToNarrow(ev.pathW.c_str());
	std::atomic<int> verdict{ 0 }; // 1=whitelist, -1=blacklist, 0=unknown
	std::atomic<bool> needFile{ false };
	std::mutex m; std::condition_variable cv; bool done = false;

	Checksum(checksum, dllNameNarrow, [&](int dllState, bool nf) {
		verdict.store(dllState, std::memory_order_relaxed);
		needFile.store(nf, std::memory_order_relaxed);
		std::unique_lock lk(m); done = true; cv.notify_one();
		return; // giữ nguyên kiểu callback của bạn
		});

	// chờ nhanh  (tối đa 1s để không treo vô hạn)
	{
		std::unique_lock lk(m);
		cv.wait_for(lk, std::chrono::seconds(1), [&] {return done; });
	}

	const int v = verdict.load(std::memory_order_relaxed);

	if (v == 1) return; // server whitelist ok

	if (v == -1) {
		// BAN
		GetScreenShotV2(checksum, dllNameNarrow, GetHWIDV2JsonString(), "error", "internal", needFile.load());
		std::thread([=]() {
			std::string reason = AY_OBFUSCATE("[Dll inject]");
			auto hwids = GetHWIDV2();
			json arr = json::array(); for (auto& s : hwids) arr.push_back(s);
			const char* k1 = AY_OBFUSCATE("launcher_name");
			const char* k2 = AY_OBFUSCATE("reason");
			const char* k3 = AY_OBFUSCATE("tokens");
			auto body = json::object({ {k1, ToNarrow(PRODUCT_NAME)}, {k2, reason}, {k3, arr} });
			const char* url = AY_OBFUSCATE("https://fxapi.grandrp.vn/launcher/ban");
			Instance<::HttpClient>::Get()->DoPostRequest(url, body.dump(), [](bool, const char*, size_t) {});
			FatalError(""); exit(0);
			}).detach();
			return;
	}

	// v == 0 → unknown: cảnh báo & theo policy có thể kill hoặc không
	GetScreenShotV2(checksum, dllNameNarrow, GetHWIDV2JsonString(), "warning", "internal", needFile.load());
	// Nếu bạn muốn dừng hẳn khi unknown, bật 2 dòng sau:
	FatalError(""); exit(0);
}

// ====== Worker loop ======
static void WorkerLoop() {
	while (g_workerRun.load()) {
		DllEvent ev;
		{
			std::unique_lock lk(g_qmx);
			g_qcv.wait(lk, [] { return !g_queue.empty() || !g_workerRun.load(); });
			if (!g_workerRun.load()) break;
			ev = std::move(g_queue.front());
			g_queue.pop();
		}
		// xử lý ngoài lock
		HandleDllEvent(ev);
	}
}

// ====== Loader callback — nhẹ, chỉ enqueue ======
static VOID CALLBACK OnDllNotification(ULONG reason, const PLDR_DLL_NOTIFICATION_DATA data, PVOID /*ctx*/) {
	if (reason != LDR_DLL_NOTIFICATION_REASON_LOADED) return;
	if (!data || !data->Loaded.FullDllName || !data->Loaded.FullDllName->pBuffer) return;

	const LPCWSTR full = data->Loaded.FullDllName->pBuffer;
	DllEvent ev;
	ev.pathW = full;
	ev.base = (HMODULE)data->Loaded.DllBase;
	ev.size = data->Loaded.SizeOfImage;
	ev.pathLower = boost::algorithm::to_lower_copy(ToNarrow(full));

	{
		std::lock_guard lk(g_qmx);
		g_queue.push(std::move(ev));
	}
	g_qcv.notify_one();
}

// ====== Init: build caches, start worker, register callback ======
static HookFunction initFunction([]() {
	// Chuẩn hoá whitelist containers
	{
		std::unordered_set<std::string> tmpPath; tmpPath.reserve(WhitelistDll.size());
		for (auto& p : WhitelistDll) tmpPath.insert(p); // đã lower sẵn
		g_whitelistPath.swap(tmpPath);

		std::unordered_set<std::string> tmpW, tmpB;
		tmpW.reserve(WhitelistDllChecksums.size());
		tmpB.reserve(BlacklistDllChecksums.size());
		for (auto& h : WhitelistDllChecksums) tmpW.insert(h);
		for (auto& h : BlacklistDllChecksums) tmpB.insert(h);
		g_whitelistHash.swap(tmpW);
		g_blacklistHash.swap(tmpB);
	}

	// Worker
	g_workerRun.store(true);
	g_worker = std::thread(WorkerLoop);

	// Register DLL notifications
	if (HMODULE ntdll = GetModuleHandleW(L"ntdll.dll")) {
		auto pLdrRegisterDllNotification = reinterpret_cast<_LdrRegisterDllNotification>(
			GetProcAddress(ntdll, AY_OBFUSCATE("LdrRegisterDllNotification")));
		if (pLdrRegisterDllNotification) {
			PVOID cookie = nullptr;
			pLdrRegisterDllNotification(0, (PLDR_DLL_NOTIFICATION_FUNCTION)OnDllNotification, nullptr, &cookie);
		}
	}
});
