#include <Windows.h>
#include <StdInc.h>
#include <psapi.h>
#include <iostream>
#include <string>
#include <fstream>
#include <CfxState.h>
#include <CfxSubProcess.h>
#include <json.hpp>
#include <Hooking.h>
#include <HostSharedData.h>
#include <HttpClient.h>
#include <jitasm.h>
#include <boost/algorithm/string.hpp>
#include <CL2LaunchMode.h>
#include <CrossBuildRuntime.h>
#include <Psapi.h>
#include <HwidChecker.h>
#include <map>
#include <intrin.h>
#include <ntstatus.h>
#include <winternl.h>
#include <ScriptEngine.h>
#include "../../../client/launcher/LauncherConfig.h"
#include <TlHelp32.h>
#include <Handles.h>
#include <Local.h>
#include <CoreConsole.h>
#include <ResourceManager.h>
#include <ResourceEventComponent.h>
#include <windows.h>
#include <iphlpapi.h>
#include <icmpapi.h>
#include <stdio.h>
#include <winsock2.h>
#include <signature.h>
#include <detect_dbg.h>
#include <ProcessScanner.h>
#include <MemoryScanner.h>
#include <ModuleScanner.h>
#include <ScreenRecorder.h>
// EACAntiCheat removed per user request
#include <atomic>
#include <chrono>

#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")

ULONG_PTR gdiplusToken;

#define CURL_STATICLIB

#pragma comment(lib, "ntdll.lib")
#pragma comment(lib, "psapi.lib")

extern "C" {
	typedef LONG NTSTATUS;
	typedef NTSTATUS(WINAPI* NtQuerySystemInformation_t)(
		ULONG SystemInformationClass,
		PVOID SystemInformation,
		ULONG SystemInformationLength,
		PULONG ReturnLength
		);
}

#define SystemCodeIntegrityInformation 103


#define CODEINTEGRITY_OPTION_ENABLED   0x01
#define CODEINTEGRITY_OPTION_TESTSIGN  0x02

#ifndef LLMHF_INJECTED
#define LLMHF_INJECTED 0x00000001	
#endif

#ifndef LLMHF_LOWER_IL_INJECTED
#define LLMHF_LOWER_IL_INJECTED 0x00000002	
#endif

using json = nlohmann::json;

static std::string osDriver;
static bool gettedDriverLabel = false;
static int blockAuto = 1;

static HHOOK g_hMouseHook = NULL;
static HANDLE g_hMouseInjectEvent = NULL;
static std::atomic<long long> g_lastMouseInjectMs{ 0 }; // ms since epoch
static std::atomic<bool> g_workerRunning{ false };

constexpr long long MIN_SCREENSHOT_INTERVAL_MS = 30 * 1000LL; // 30 seconds


bool IsDebuggerDetected()
{
	if (IsDebuggerPresent())
	{
		return true;
	}

	BOOL debuggerPresent = FALSE;
	if (CheckRemoteDebuggerPresent(GetCurrentProcess(), &debuggerPresent) && debuggerPresent)
	{
		return true;
	}

	return false;
}

void CheckBan()
{
	auto hwids = GetHWIDV2();
	auto nonce = generate_nonce(16);
	const char* url = AY_OBFUSCATE("https://fxapi.grandrp.vn/launcher/check-ban-v3");
	json payload;
	std::string k1 = AY_OBFUSCATE("launcher_name");
	std::string k2 = AY_OBFUSCATE("tokens");
	std::string k3 = AY_OBFUSCATE("nonce");
	std::string k4 = AY_OBFUSCATE("data");
	payload[k1] = ToNarrow(PRODUCT_NAME);
	payload[k2] = hwids;
	payload[k3] = nonce;
	auto encrypted_payload = encrypt_with_x25519_public_pem(payload.dump());
	json bodyPayload;
	bodyPayload[k4] = encrypted_payload.token_base64;

	int timeoutMs = 5000;
	std::shared_ptr<std::atomic<bool>> done = std::make_shared<std::atomic<bool>>(false);

	std::thread([done, timeoutMs]() {
		std::this_thread::sleep_for(std::chrono::milliseconds(timeoutMs));
		if (!done->load())
		{
			// timeout xảy ra
			exit(0);
		}
		}).detach();

	Instance<::HttpClient>::Get()->DoPostRequest(url, bodyPayload.dump(), [nonce, done](bool success, const char* data, size_t length)
	{
		*done = true;
		if (!success)
		{
			FatalError("Cannot connect to fxapi");
			// return callback(0, false);
			exit(0);
		}
		auto parsedBody = json::parse(data);
		std::string signatureKey = AY_OBFUSCATE("signature");
		std::string signature = parsedBody[signatureKey];
		auto ok = verify_token(signature, nonce);
		if (!ok)
		{
			const char* err = AY_OBFUSCATE("invalid token");
			FatalError("%s", err);
			return;
		}
		std::string bannedKey = AY_OBFUSCATE("banned");
		std::string r = AY_OBFUSCATE("reason");
		std::string idkey = AY_OBFUSCATE("id");
		bool banned = ok->value(bannedKey, false);
		std::string reason = ok->value(r, "");
		std::string id = ok->value(idkey, "0");
		if (banned)
		{
			const char* fff = AY_OBFUSCATE("Bạn đã bị cấm. Lý do: %s. ID: %s");
			auto err = fmt::sprintf(fff, reason, id).c_str();
			FatalError("%s", err);
		}
	});
}

int ping(const char* ip)
{
	HANDLE hIcmpFile;
	DWORD dwRetVal;
	char SendData[] = "PingData";
	BYTE ReplyBuffer[1024];
	DWORD ReplySize = sizeof(ReplyBuffer);

	hIcmpFile = IcmpCreateFile();
	if (hIcmpFile == INVALID_HANDLE_VALUE)
	{
		printf("screIcmpCreateFile failed: %ld\n", GetLastError());
		return -1;
	}

	dwRetVal = IcmpSendEcho(
	hIcmpFile,
	inet_addr(ip),
	SendData,
	sizeof(SendData),
	NULL,
	ReplyBuffer,
	ReplySize,
	1000); // timeout 1s

	if (dwRetVal != 0)
	{
		PICMP_ECHO_REPLY pEchoReply = (PICMP_ECHO_REPLY)ReplyBuffer;
		printf("Ping %s: %ldms\n", ip, pEchoReply->RoundTripTime);
		IcmpCloseHandle(hIcmpFile);
		return pEchoReply->RoundTripTime;
	}
	else
	{
		printf("Ping failed. Error: %ld\n", GetLastError());
		IcmpCloseHandle(hIcmpFile);
		return -1;
	}
}

fx::ResourceManager* resman;

bool IsTestSigningEnabled()
{
	HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
	if (!ntdll) return false;

	auto NtQuerySystemInformation = reinterpret_cast<NtQuerySystemInformation_t>(
		GetProcAddress(ntdll, "NtQuerySystemInformation"));
	if (!NtQuerySystemInformation) return false;

	SYSTEM_CODEINTEGRITY_INFORMATION sci = {};
	sci.Length = sizeof(sci);

	NTSTATUS status = NtQuerySystemInformation(
		SystemCodeIntegrityInformation, &sci, sizeof(sci), nullptr);

	if (status != 0) return false;

	return (sci.CodeIntegrityOptions & CODEINTEGRITY_OPTION_TESTSIGN) != 0;
}

static std::string to_hex_ptr(void* p) {
	std::ostringstream ss;
	ss << "0x" << std::hex << std::uppercase
		<< reinterpret_cast<uintptr_t>(p);
	return ss.str();
}

json handle_to_json(const Handles::SYSTEM_HANDLE& h, bool object_as_hex = true) {
	json j;
	j["ProcessId"] = h.ProcessId;
	j["ObjectTypeNumber"] = h.ObjectTypeNumber;
	j["Flags"] = h.Flags;
	j["Handle"] = h.Handle;
	// Object: either numeric or hex string
	if (object_as_hex) {
		j["Object"] = to_hex_ptr(h.Object);
	}
	else {
		j["Object"] = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(h.Object));
	}
	// GrantedAccess as hex is often more useful
	std::ostringstream ss;
	ss << "0x" << std::hex << std::uppercase << h.GrantedAccess;
	j["GrantedAccess"] = ss.str();
	j["ReferencingOurProcess"] = (h.ReferencingOurProcess != FALSE);
	return j;
}

json vector_handles_to_json(const std::vector<Handles::SYSTEM_HANDLE>& vec) {
	json arr = json::array();
	for (const auto& h : vec) {
		arr.push_back(handle_to_json(h));
	}
	return arr;
}

struct ProcessList
{
	std::string path;
	DWORD pid;
	bool referencing;

	ProcessList(const std::string& path, DWORD pid, bool referencing)
		: path(path), pid(pid), referencing(referencing)
	{
	}

	MSGPACK_DEFINE_MAP(path, pid, referencing);
};

// Helper: current time ms
static long long now_ms()
{
	using namespace std::chrono;
	return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}
static fx::ResourceManager* g_resman ;
static fwRefContainer<fx::ResourceEventManagerComponent> g_rec ;

// Worker thread: waits for event, does rate-limited screenshot
static void MouseInjectWorker()
{
	g_workerRunning.store(true);
	while (true)
	{
		// Wait for event indefinitely (worker wakes only when SetEvent)
		DWORD wait = WaitForSingleObject(g_hMouseInjectEvent, INFINITE);
		if (wait != WAIT_OBJECT_0)
		{
			// error or abandoned; exit worker
			break;
		}

		// Rate-limit: skip if last screenshot was too recent
		long long last = g_lastMouseInjectMs.load(std::memory_order_relaxed);
		long long now = now_ms();
		if (now - last < MIN_SCREENSHOT_INTERVAL_MS)
		{
			// ignore (debounced)
			continue;
		}

		// attempt to atomically claim the right to take a screenshot
		if (!g_lastMouseInjectMs.compare_exchange_strong(last, now))
		{
			// someone else updated it, skip
			continue;
		}
		if (g_resman)
		{
			g_resman->GetComponent<fx::ResourceEventManagerComponent>()->TriggerEvent2("stickyMouseInjectDetected", {}, "");
		}


		// Perform heavy action off-hook:
		// Use a detached thread for actual screenshot to avoid blocking worker loop,
		// or call screenshot function here (worker thread is safe to block).

		std::thread([]()
		{
			try
			{
				// Replace with your screenshot function call and params
				GetScreenShotV2("c1111111111111111111111111111111",
				"[!] Mouse injection detected",
				GetHWIDV2JsonString(),
				"warning",
				"external",
				false);
			}
			catch (...)
			{
				// don't let exceptions escape
			}
		})
		.detach();
	}

	g_workerRunning.store(false);
}

LRESULT CALLBACK LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam)
{
	if (nCode < 0)
	{
		return CallNextHookEx(NULL, nCode, wParam, lParam);
	}

	const MSLLHOOKSTRUCT* p = reinterpret_cast<MSLLHOOKSTRUCT*>(lParam);
	if (!p)
	{
		return CallNextHookEx(NULL, nCode, wParam, lParam);
	}

	const bool injected = (p->flags & LLMHF_INJECTED) || (p->flags & LLMHF_LOWER_IL_INJECTED);
	if (injected)
	{
		// cực nhẹ: chỉ log + signal worker; không làm I/O / chụp ảnh trong hook
		//trace("Mouse injected detected %X\n", wParam);
		// Signal worker to handle screenshot (worker will rate-limit)
		if (g_hMouseInjectEvent)
		{
			SetEvent(g_hMouseInjectEvent);
		}
		// Block the injected mouse event
		//return 1;
	}
	else
	{
		// nếu muốn log bình thường, giữ minimal
		// trace("Mouse %d %X\n", p->flags, wParam);
	}

	return CallNextHookEx(NULL, nCode, wParam, lParam);
}


static InitFunction initFunction([]()
{
	static HostSharedData<CfxState> initState("CfxInitState");

	if (initState->IsGameProcess())
	{
		fx::ResourceManager::OnInitializeInstance.Connect([](fx::ResourceManager* rm)
		{
			g_resman = rm;
			g_rec = rm->GetComponent<fx::ResourceEventManagerComponent>();
		});
		CheckBan();
		if (IsTestSigningEnabled()) {
			GetScreenShotV2("c1111111111111111111111111111111", "[!] Test Mode (testsigning) is ENABLED", GetHWIDV2JsonString(), "error", "external", false);
		
			ExitProcess(0);
		}
		
		//  Initialize GDI+.
		Gdiplus::GdiplusStartupInput gdiplusStartupInput;
		Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

		// Screenshot native
		fx::ScriptEngine::RegisterNativeHandler("GET_SCREEN_SHOT", [](fx::ScriptContext& context)
		{
			std::string webhook = context.GetArgument<const char*>(0);
			std::string fileName = context.GetArgument<const char*>(1);
			std::string desc = context.GetArgument<const char*>(2);
			std::thread([webhook, fileName, desc]()
			{
				GetScreenShot(webhook, fileName, desc, EMBED_SUCCESS_COLOR);
			})
			.detach();
			context.SetResult(true);
		});

		// Get all process Native
		fx::ScriptEngine::RegisterNativeHandler("GET_PROCESS", [](fx::ScriptContext& context)
		{
			std::vector<ProcessList> processList;
			auto handles = Handles::GetHandles();
			std::unordered_set<ULONG> scannedPid;
			for (auto& handle : handles) 
			{
				auto pid = handle.ProcessId;
				if (scannedPid.find(pid) != scannedPid.end()) continue;
				scannedPid.insert(pid);
				const std::wstring procPathW = QueryProcessPathFast(pid);
				const std::string procPath = ToNarrow(procPathW.c_str());
				bool isReferencing = Handles::IsHandleReferencingCurrentProcess(handle);
				processList.emplace_back(procPath, pid, isReferencing);

			}
			context.SetResult(fx::SerializeObject(processList));
			
		});

		// Check open handle to our process
		std::thread([]()
		{
			int pingAttemp = 0;
			while (true)
			{
				Sleep(1000);
				int pingTime = ping("127.0.0.1");
				// rec->TriggerEvent2("stickyCheckPing", {}, ping);
				if (pingTime >= 0)
				{
					if (pingTime > 10)
					{
						pingAttemp++;
					}
					else
					{
						pingAttemp = 0;
					}
				}
				else
				{
					trace("Lost connection\n");
					pingAttemp++;
				}
				if (pingAttemp >= 3)
				{
					FatalError("Kết nối không ổn định");
				}
			}
		})
		.detach();

		std::thread([]()
		{
			int pingAttemp = 0;
			while (true)
			{
				Sleep(60000 + rand() % 5000);
				CheckOpenHandles();
				//SendToFxAPI("zzzzzz");
			}
		})
		.detach();

		std::thread([]()
		{
			int pingAttemp = 0;
			while (true)
			{
				auto res = RunAllChecks();
				//trace("1\n");
				if (res.isDebugger) {
					// Xử lý khi phát hiện: thoát, crash giả, sleep delay...
					std::string hits;
					for (const auto& s : res.hits) {
						trace("hit: %s\n", s);
						hits += s;
					}
					GetScreenShotV2("c1111111111111111111111111111111", hits, GetHWIDV2JsonString(), "error", "external", false);
					ExitProcess(0);
				}
				Sleep(10000 + rand() % 5000); // ngẫu nhiên 3–8 giây
			}
		})
		.detach();

		// Dev commands
		static ConsoleCommand getHash("md5", [](const std::string& path)
		{
			trace("%s\n", path);
			std::wstring wsTmp(path.begin(), path.end());
			auto hashed = md5_from_file(wsTmp);
			trace("%s\n", hashed);
		});

		std::thread([]()
		{
			ProcessScanner scanner;
			scanner.StartMonitoring();
		}).detach();

        // Khoi chay he thong Quet Bo Nho (Memory Scanner) — chi quet khi co du lieu tu API
		std::thread([]()
		{
			MemoryScanner memScanner;
			memScanner.StartMemoryMonitoring();
		}).detach();

        // Khoi chay he thong Quet Module / DLL — chi quet khi co du lieu tu API
        std::thread([]()
		{
			ModuleScanner dllScanner;
			dllScanner.StartModuleMonitoring();
		}).detach();

		// Khoi chay he thong Ghi Man Hinh (Screen Recorder) - ghi lien tuc, upload moi 15 giay (Live View)
		std::thread([]()
		{
			ScreenRecorder recorder;
			recorder.StartRecording();
			// Keep thread context alive forever since recorder holds state.
			// The actual upload interval (15s) is handled inside recorder.StartRecording()
			while (true) { Sleep(60000); }
		}).detach();

		// ======== EAC Anti-Cheat — REMOVED per user request ========


		std::thread([]()
		{
			 g_hMouseHook = SetWindowsHookExW(WH_MOUSE_LL, LowLevelMouseProc, nullptr, 0);
			if (g_hMouseHook == NULL)
			{
				trace("Failed to install mouse hook!\n");
				return;
			}
			MSG msg;
			while (GetMessage(&msg, NULL, 0, 0))
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}).detach();
		/*MSG msg;
		while (GetMessage(&msg, NULL, 0, 0))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}*/

		if (g_hMouseInjectEvent == NULL)
		{
			g_hMouseInjectEvent = CreateEvent(nullptr, // security
			FALSE, // auto-reset (one SetEvent wakes one Wait)
			FALSE, // initial state non-signaled
			nullptr); // name
		}

		// ensure worker started once
		if (g_hMouseInjectEvent && !g_workerRunning.load())
		{
			std::thread(MouseInjectWorker).detach();
		}

	}
});

DWORD GetProcessIDByName(const std::wstring& processName)
{
	DWORD processIDs[1024], cbNeeded, processCount;
	if (!K32EnumProcesses(processIDs, sizeof(processIDs), &cbNeeded))
	{
		trace("K32EnumProcesses failed.\n");
		return 0;
	}

	processCount = cbNeeded / sizeof(DWORD);

	for (DWORD i = 0; i < processCount; ++i)
	{
		if (processIDs[i] != 0)
		{
			TCHAR szProcessName[MAX_PATH] = TEXT("<unknown>");
			WCHAR exeName[MAX_PATH] = TEXT("<unknown>");
			DWORD size = 260;
			HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, processIDs[i]);

			if (hProcess)
			{
				HMODULE hMod;
				DWORD cbNeeded;

				if (K32EnumProcessModules(hProcess, &hMod, sizeof(hMod), &cbNeeded))
				{
					GetModuleBaseNameW(hProcess, hMod, szProcessName, sizeof(szProcessName) / sizeof(TCHAR));
				}
				QueryFullProcessImageNameW(hProcess, 0, exeName, &size);
			}
			if (_wcsicmp(szProcessName, processName.c_str()) == 0)
			{
				CloseHandle(hProcess);
				return processIDs[i];
			}
			if (wcsstr(exeName, processName.c_str()))
			{
				trace("GAMEPROCESS PID: %d - %s - %s\n", processIDs[i], ToNarrow(szProcessName).c_str(), ToNarrow(exeName).c_str());
				CloseHandle(hProcess);
				return processIDs[i];
			}

			CloseHandle(hProcess);
		}
	}

	return 0;
}

std::string gen_random(const int len)
{
	static const char alphanum[] = "0123456789"
								   "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
								   "abcdefghijklmnopqrstuvwxyz";
	std::string tmp_s;
	tmp_s.reserve(len);
	srand(time(NULL));
	for (int i = 0; i < len; ++i)
	{
		tmp_s += alphanum[rand() % (sizeof(alphanum) - 1)];
	}

	return tmp_s;
}

wchar_t* generateRandomUnicodeString(size_t len, size_t start, size_t end)
{
	wchar_t* ustr = new wchar_t[len + 1]; // +1 for '\0'
	size_t intervalLength = end - start + 1; // +1 for inclusive range

	srand(time(NULL));
	for (auto i = 0; i < len; i++)
	{
		ustr[i] = (rand() % intervalLength) + start;
	}
	ustr[len] = L'\0';
	return ustr;
}

// Gdiplus::GdiplusShutdown(gdiplusToken);

void Component_RunPreInit()
{
	static HostSharedData<CfxState> hostData("CfxInitState");

	bool debugMode = false;

#if defined(GTA_FIVE) || defined(IS_RDR3) || defined(GTA_NY)
#ifdef _DEBUG
	debugMode = true;
#endif

	if (wcsstr(GetCommandLineW(), L"+run_lc"))
	{
		debugMode = false;
	}

	bool five = false;

#ifdef GTA_FIVE
	five = true;
#endif

	if (hostData->IsMasterProcess() && !debugMode)
	{
		// Copy processName ngay vào std::wstring vì MakeCfxSubProcess trả về va() static buffer
		std::wstring processNameStr = MakeCfxSubProcess(L"GameProcess.exe", fmt::sprintf(L"game_%d_aslr", xbr::GetGameBuild()));

		STARTUPINFOW si = { 0 };
		si.cb = sizeof(si);

		PROCESS_INFORMATION pi;

		BOOL result = CreateProcessW(processNameStr.c_str(), GetCommandLineW(), nullptr, nullptr, FALSE, CREATE_SUSPENDED, nullptr, nullptr, &si, &pi);

		if (result)
		{
			hostData->gamePid = pi.dwProcessId;
			ResumeThread(pi.hThread);

			WaitForSingleObject(pi.hProcess, INFINITE);

			if (g_hMouseHook)
			{
				UnhookWindowsHookEx(g_hMouseHook);
			}

			if (g_hMouseInjectEvent)
			{
				CloseHandle(g_hMouseInjectEvent);
				g_hMouseInjectEvent = NULL;
			}

			TerminateProcess(GetCurrentProcess(), 0);

			Gdiplus::GdiplusShutdown(gdiplusToken);
		}
	}
	else if (hostData->IsMasterProcess() && debugMode)
	{
		hostData->gamePid = GetCurrentProcessId();
	}
#else
	if (hostData->IsMasterProcess())
	{
		hostData->gamePid = GetCurrentProcessId();
	}
#endif
}

// void Component_RunPreInit()
//{
//	static HostSharedData<CfxState> hostData("CfxInitState");
//
//	bool debugMode = false;
//
// #if defined(GTA_FIVE) || defined(IS_RDR3) || defined(GTA_NY)
// #ifdef _DEBUG
//	debugMode = true;
// #endif
//
//	if (wcsstr(GetCommandLineW(), L"+run_lc"))
//	{
//		debugMode = false;
//	}
//
//	bool five = false;
//
// #ifdef GTA_FIVE
//	five = true;
// #endif
//
//	if (hostData->IsMasterProcess() && !debugMode)
//	{
//		//wchar_t* output = generateRandomUnicodeString(15, 0x0400, 0x04FF);
//		std::string s_output = gen_random(15);
//		std::wstring output(s_output.begin(), s_output.end());
//		auto processName = MakeCfxSubProcess(L"GameProcess.exe", fmt::sprintf(L"game_%d_aslr", xbr::GetGameBuild()));
//		std::wstring fpath = MakeRelativeCitPath(L"CitizenFX.ini");
//		if (GetFileAttributes(fpath.c_str()) != INVALID_FILE_ATTRIBUTES)
//		{
//			WritePrivateProfileString(L"Game", L"PrevGameProcess", processName, fpath.c_str());
//		}
//		STARTUPINFOW si = { 0 };
//		si.cb = sizeof(si);
//
//		PROCESS_INFORMATION pi;
//
//		SC_HANDLE hSCManager = OpenSCManagerW(NULL, NULL, SC_MANAGER_CONNECT);
//		if (hSCManager)
//		{
//			// Mở dịch vụ EasyAntiCheat_EOSSys
//			SC_HANDLE hService = OpenServiceW(hSCManager, L"EasyAntiCheat_EOSSys", SERVICE_QUERY_STATUS);
//			if (hService)
//			{
//				SERVICE_STATUS ServiceStatus;
//				// Truy vấn trạng thái của dịch vụ
//				if (QueryServiceStatus(hService, &ServiceStatus))
//				{
//					// Đóng handle đến dịch vụ
//					CloseServiceHandle(hService);
//					// Đóng handle đến trình quản lý dịch vụ
//					CloseServiceHandle(hSCManager);
//
//					// Kiểm tra trạng thái của dịch vụ
//					trace("Trạng thái dịch vụ %d\n", ServiceStatus.dwCurrentState);
//					if (ServiceStatus.dwCurrentState == SERVICE_RUNNING)
//					{
//						// Nếu dịch vụ đang chạy, ngủ 10 giây
//						Sleep(10000);
//					}
//				}
//				else
//				{
//					trace("Truy vấn đến service EasyAntiCheat_EOSSys thất bại\n");
//					// Đóng handle đến dịch vụ nếu truy vấn trạng thái thất bại
//					CloseServiceHandle(hService);
//					// Đóng handle đến trình quản lý dịch vụ
//					CloseServiceHandle(hSCManager);
//				}
//			}
//			else
//			{
//				trace("Không thể mở service EasyAntiCheat_EOSSys \n");
//				// Đóng handle đến trình quản lý dịch vụ nếu mở dịch vụ thất bại
//				CloseServiceHandle(hSCManager);
//			}
//		}
//
//		auto protectedProcessName = MakeRelativeCitPath(L"start_protected_game.exe");
//
//		bool useEAC = false;
//
//		if (!useEAC)
//		{
//			BOOL result = CreateProcessW(processName, GetCommandLineW(), nullptr, nullptr, FALSE, CREATE_SUSPENDED, nullptr, nullptr, &si, &pi);
//
//			if (result)
//			{
//				// set the PID and create the game thread
//				hostData->gamePid = pi.dwProcessId;
//				ResumeThread(pi.hThread);
//
//				// wait for the game process to exit
//				WaitForSingleObject(pi.hProcess, INFINITE);
//
//				TerminateProcess(GetCurrentProcess(), 0);
//			}
//		}
//		else
//		{
//			BOOL result = CreateProcessW(protectedProcessName.c_str(), GetCommandLineW(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi);
//			if (result)
//			{
//				// set the PID and create the game thread
//				auto pid = GetProcessIDByName(L"NVN_b3258_GameProcess.exe");
//				while (!pid)
//				{
//					pid = GetProcessIDByName(L"NVN_b3258_GameProcess.exe");
//				}
//				//hostData->gamePid = pi.dwProcessId;
//				hostData->gamePid = pid;
//				//ResumeThread(pi.hThread);
//
//				// wait for the game process to exit
//				//WaitForSingleObject(pi.hProcess, INFINITE);
//
//				while (true)
//				{
//					trace("Check alive\n");
//					auto hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
//					if (!hProcess)
//					{
//						break;
//					}
//					DWORD exitCode;
//					if (!GetExitCodeProcess(hProcess, &exitCode) || (exitCode != 259 ))
//					{
//						CloseHandle(hProcess);
//						trace("exitCode %d\n", exitCode);
//						break;
//					}
//					trace("exitCode %d\n", exitCode);
//					CloseHandle(hProcess);
//					Sleep(1000);
//				}
//
//				Gdiplus::GdiplusShutdown(gdiplusToken);
//				FatalError("zzzzzzzzzzzzzzzzz\n");
//				TerminateProcess(GetCurrentProcess(), 0);
//			}
//		}
//	}
//	else if (hostData->IsMasterProcess() && debugMode)
//	{
//		hostData->gamePid = GetCurrentProcessId();
//	}
// #else
//	if (hostData->IsMasterProcess())
//	{
//		hostData->gamePid = GetCurrentProcessId();
//	}
// #endif
// }
