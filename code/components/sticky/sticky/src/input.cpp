// rawinput_hid_registry.cpp
// Build (MSVC): cl /EHsc rawinput_hid_registry.cpp setupapi.lib hid.lib user32.lib
// Build (MinGW): g++ -municode -std=c++17 rawinput_hid_registry.cpp -lsetupapi -lhid -luser32 -o rawinput_hid_registry.exe


#include <StdInc.h>
#include "input.h"
#include <InputHook.h>

static HWND g_inputHWnd = NULL;

// ========= Utils =========
static std::string W2U(const std::wstring& w)
{
	if (w.empty())
		return {};
	int sz = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
	std::string s;
	s.resize(sz);
	WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), &s[0], sz, nullptr, nullptr);
	return s;
}
static std::wstring U2W(const std::string& s)
{
	if (s.empty())
		return {};
	int sz = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
	std::wstring w;
	w.resize(sz);
	MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), &w[0], sz);
	return w;
}
static std::string Upper(std::string v)
{
	std::transform(v.begin(), v.end(), v.begin(), [](unsigned char c)
	{
		return (char)std::toupper(c);
	});
	return v;
}
static std::string NormalizeDevPath(const std::string& in)
{
	// Chuẩn hoá để so khớp: upper-case, thay "\\?\" ↔ "\\??\"
	std::string s = Upper(in);
	// Thống nhất prefix: "\\?\"
	// RawInput thường là "\\??\HID#...", SetupAPI thường "\\?\HID#..."
	size_t pos = s.find("\\\\??\\");
	if (pos != std::string::npos)
		s.replace(pos, 5, "\\\\?\\");
	return s;
}
static bool ParseVidPid(const std::string& s, std::string& outVid, std::string& outPid)
{
	static const std::regex re(R"(VID_([0-9A-F]{4})[^0-9A-F]*PID_([0-9A-F]{4}))", std::regex::icase);
	std::smatch m;
	if (std::regex_search(s, m, re) && m.size() >= 3)
	{
		outVid = m[1].str();
		outPid = m[2].str();
		for (auto& c : outVid)
			c = (char)std::toupper((unsigned char)c);
		for (auto& c : outPid)
			c = (char)std::toupper((unsigned char)c);
		return true;
	}
	return false;
}

static std::string MakeHidStableKey(const std::string& in)
{
	// Upper + normalize prefix như bạn đã có
	std::string s = NormalizeDevPath(in);
	static const std::regex re(
	R"(HID#VID_([0-9A-F]{4})&PID_([0-9A-F]{4})(?:&MI_([0-9A-F]{2}))?(?:&COL([0-9A-F]{2}))?)");
	std::smatch m;
	if (std::regex_search(s, m, re))
	{
		return m[0].str(); // ví dụ: HID#VID_046D&PID_C539&MI_02&COL02
	}
	return s; // fallback
}



// ========= SetupAPI helpers =========
static std::wstring GetDevRegPropW(HDEVINFO devs, SP_DEVINFO_DATA& di, DWORD prop)
{
	wchar_t buf[2048];
	DWORD need = 0;
	if (SetupDiGetDeviceRegistryPropertyW(devs, &di, prop, nullptr, (PBYTE)buf, sizeof(buf), &need))
	{
		return std::wstring(buf);
	}
	return {};
}
static std::wstring GetDevInstanceIdW(SP_DEVINFO_DATA& di)
{
	wchar_t id[1024];
	if (CM_Get_Device_IDW(di.DevInst, id, (ULONG)(sizeof(id) / sizeof(wchar_t)), 0) == CR_SUCCESS)
	{
		return id;
	}
	return {};
}

// ========= Enumerate HID devices via SetupAPI =========
static void EnumerateHidDevices(std::vector<HidDeviceInfo>& out)
{
	GUID hidGuid;
	HidD_GetHidGuid(&hidGuid);

	HDEVINFO devs = SetupDiGetClassDevsW(&hidGuid, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
	if (devs == INVALID_HANDLE_VALUE)
		return;

	SP_DEVICE_INTERFACE_DATA ifData;
	ifData.cbSize = sizeof(ifData);
	for (DWORD idx = 0; SetupDiEnumDeviceInterfaces(devs, nullptr, &hidGuid, idx, &ifData); ++idx)
	{
		DWORD req = 0;
		SetupDiGetDeviceInterfaceDetailW(devs, &ifData, nullptr, 0, &req, nullptr);
		if (!req)
			continue;
		std::vector<BYTE> buf(req);
		auto detail = (PSP_DEVICE_INTERFACE_DETAIL_DATA_W)buf.data();
		detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);
		SP_DEVINFO_DATA di;
		di.cbSize = sizeof(di);
		if (!SetupDiGetDeviceInterfaceDetailW(devs, &ifData, detail, req, nullptr, &di))
			continue;

		HidDeviceInfo hi;
		hi.devicePath = NormalizeDevPath(W2U(detail->DevicePath));
		hi.friendlyName = W2U(GetDevRegPropW(devs, di, SPDRP_FRIENDLYNAME));
		if (hi.friendlyName.empty())
			hi.friendlyName = W2U(GetDevRegPropW(devs, di, SPDRP_DEVICEDESC));
		hi.devInstanceId = W2U(GetDevInstanceIdW(di));

		// HardwareIDs (lấy dòng đầu)
		{
			WCHAR buffer[4096];
			DWORD need = 0;
			if (SetupDiGetDeviceRegistryPropertyW(devs, &di, SPDRP_HARDWAREID, nullptr, (PBYTE)buffer, sizeof(buffer), &need))
			{
				hi.hardwareIds = W2U(buffer);
			}
		}

		// Open to query HID attributes & strings
		HANDLE h = CreateFileW(detail->DevicePath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (h != INVALID_HANDLE_VALUE)
		{
			HIDD_ATTRIBUTES a = {};
			a.Size = sizeof(a);
			if (HidD_GetAttributes(h, &a))
			{
				hi.vendorId = a.VendorID;
				hi.productId = a.ProductID;
				hi.version = a.VersionNumber;
			}
			wchar_t wb[512];
			if (HidD_GetManufacturerString(h, wb, sizeof(wb)))
				hi.manufacturer = wb;
			if (HidD_GetProductString(h, wb, sizeof(wb)))
				hi.product = wb;
			if (HidD_GetSerialNumberString(h, wb, sizeof(wb)))
				hi.serial = wb;
			CloseHandle(h);
		}

		std::string vid, pid;
		if (ParseVidPid(hi.devicePath, vid, pid) || ParseVidPid(hi.devInstanceId, vid, pid) || ParseVidPid(hi.hardwareIds, vid, pid))
		{
			hi.vidStr = vid;
			hi.pidStr = pid;
			hi.parsedOk = true;
		}

		out.push_back(std::move(hi));
	}

	SetupDiDestroyDeviceInfoList(devs);
}

// ========= Enumerate RAW devices (all) =========
static void EnumerateRawDevices(std::vector<RawDeviceInfo>& out, std::map<HANDLE, std::string>& handleToName)
{
	UINT n = 0;
	if (GetRawInputDeviceList(nullptr, &n, sizeof(RAWINPUTDEVICELIST)) != 0 || n == 0)
		return;
	std::vector<RAWINPUTDEVICELIST> list(n);
	if (GetRawInputDeviceList(list.data(), &n, sizeof(RAWINPUTDEVICELIST)) == (UINT)-1)
		return;

	for (UINT i = 0; i < n; i++)
	{
		UINT sz = 0;
		GetRawInputDeviceInfoA(list[i].hDevice, RIDI_DEVICENAME, nullptr, &sz);
		std::string name(sz, '\0'); // ✅ đúng kích thước
		if (GetRawInputDeviceInfoA(list[i].hDevice, RIDI_DEVICENAME, name.data(), &sz) == (UINT)-1)
			continue;
		name.resize(sz);
		RawDeviceInfo ri;
		ri.hDevice = list[i].hDevice;
		ri.rawName = NormalizeDevPath(name);
		handleToName[ri.hDevice] = ri.rawName;
		out.push_back(std::move(ri));
	}
}

// ========= Correlate RAW <-> HID =========
// Chiến lược:
// 1) so khớp trực tiếp chuỗi đường dẫn đã chuẩn hoá (nhiều khi trùng 1-1).
// 2) nếu không trùng, so khớp theo VID/PID và substring token (sau dấu '#').
// 3) fallback: nếu chỉ có 1 HID có cùng VID/PID, nhận nó.
static void Correlate(HidRegistry& reg)
{
	// Index HID theo path & theo VID/PID
	std::map<std::string, int> pathToIdx;
	std::multimap<std::string, int> vidpidToIdx;
	for (int i = 0; i < (int)reg.hidList.size(); ++i)
	{
		pathToIdx[reg.hidList[i].devicePath] = i;
		if (reg.hidList[i].parsedOk)
		{
			vidpidToIdx.emplace(reg.hidList[i].vidStr + ":" + reg.hidList[i].pidStr, i);
		}
	}

	for (auto& r : reg.rawList)
	{
		// cách 1: so khớp trực tiếp
		auto it = pathToIdx.find(r.rawName);
		if (it != pathToIdx.end())
		{
			reg.rawToHidIndex[r.rawName] = it->second;
			trace("[Correlate] Direct match: RAW '%s' -> HID index %d\n", r.rawName.c_str(), it->second);
			continue;
		}
		// cách 2 (ổn định theo key)
		const std::string rawKey = MakeHidStableKey(r.rawName);
		int chosen = -1;
		for (int i = 0; i < (int)reg.hidList.size(); ++i)
		{
			if (rawKey == MakeHidStableKey(reg.hidList[i].devicePath))
			{
				chosen = i;
				break;
			}
		}
		if (chosen >= 0)
		{
			reg.rawToHidIndex[r.rawName] = chosen;
			trace("[Correlate] Key match: RAW %s -> HID index %d\n", r.rawName.c_str(), chosen);
			continue;
		}

		// cách 3: dựa vào VID/PID
		std::string vid, pid;
		if (ParseVidPid(r.rawName, vid, pid))
		{
			trace("[Correlate] VID=%s PID=%s\n",  vid, pid);
			auto range = vidpidToIdx.equal_range(vid + ":" + pid);
			if (std::distance(range.first, range.second) == 1)
			{
				reg.rawToHidIndex[r.rawName] = range.first->second;
				trace("[Correlate] VID/PID match: RAW '%s' -> HID index %d\n", r.rawName.c_str(), range.first->second);
				continue;
			}
		}
		// nếu vẫn không match, để trống (not found)
		trace("[Correlate] No match for RAW '%s'\n", r.rawName.c_str());
	}
}

// ========= Public API =========
static void BuildHidRegistry(HidRegistry& reg)
{
	reg.hidList.clear();
	reg.rawList.clear();
	reg.rawToHidIndex.clear();
	reg.handleToRawName.clear();

	EnumerateHidDevices(reg.hidList);
	EnumerateRawDevices(reg.rawList, reg.handleToRawName);
	Correlate(reg);


	trace("[Build] HID devices: %zu\n", reg.hidList.size());
	trace("[Build] RAW devices: %zu\n", reg.rawList.size());
	trace("[Build] Correlated:  %zu\n", reg.rawToHidIndex.size());
}

// Kiểm tra một RAW hDevice có hợp lệ theo whitelist & không nghi vấn spoof/clone
static bool IsRawDeviceAllowed(HANDLE hDevice, const HidRegistry& reg, std::string* reasonOut)
{
	auto itName = reg.handleToRawName.find(hDevice);
	if (itName == reg.handleToRawName.end())
	{
		if (reasonOut)
			*reasonOut = "raw handle not registered";
		return false;
	}
	auto itIdx = reg.rawToHidIndex.find(itName->second);
	if (itIdx == reg.rawToHidIndex.end())
	{
		if (reasonOut)
			*reasonOut = fmt::sprintf("raw device not correlated to HID: %s", itName->second);
		return false; // thận trọng: không match ⇒ từ chối
	}
	const HidDeviceInfo& hi = reg.hidList[itIdx->second];

	// Whitelist VID/PID
	bool onWhite = false;
	if (hi.parsedOk)
	{
		for (auto& p : reg.whitelist)
		{
			if (hi.vidStr == p.first && hi.pidStr == p.second)
			{
				onWhite = true;
				break;
			}
		}
	}
	if (!onWhite)
	{
		if (reasonOut)
			*reasonOut = "VID/PID not in whitelist";
		return false;
	}

	// Heuristic: thiết bị whitelist nhưng không có serial ⇒ nghi vấn (USB clone)
	if (hi.serial.empty())
	{
		if (reasonOut)
			*reasonOut = "whitelisted but missing serial";
		// tuỳ chính sách: có thể cho pass nhưng flagged. Ở đây từ chối.
		return false;
	}

	// (Bạn có thể thêm kiểm tra chữ ký driver/nhà sản xuất ở đây)
	if (reasonOut)
		*reasonOut = "ok";
	return true;
}

// ========= Demo integration =========
// - Tạo một window nhỏ để nhận WM_INPUT.
// - Dùng IsRawDeviceAllowed() trong WndProc để lọc thiết bị.


//LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
//{
//	switch (msg)
//	{
//		case WM_CREATE:
//		{
//			g_inputHWnd = hwnd;
//			// 3) Đăng ký Raw Input (Mouse)
//			RAWINPUTDEVICE rid{};
//			rid.usUsagePage = HID_USAGE_PAGE_GENERIC; // Generic Desktop
//			rid.usUsage = HID_USAGE_GENERIC_MOUSE; // Mouse
//			rid.dwFlags = RIDEV_INPUTSINK | RIDEV_DEVNOTIFY; // nhận cả khi ko focus
//			rid.hwndTarget = hwnd;
//			if (!RegisterRawInputDevices(&rid, 1, sizeof(rid)))
//			{
//				trace("[ERR] RegisterRawInputDevices failed: %u\n", GetLastError());
//				break;
//			}
//			trace("[INFO] Ready. Move/click mouse to see filtering logs.\n");
//			break;
//		}
//		case WM_INPUT:
//		{
//			trace("[WM_INPUT] received\n");
//			UINT sz = 0;
//			GetRawInputData((HRAWINPUT)lParam, RID_INPUT, nullptr, &sz, sizeof(RAWINPUTHEADER));
//			if (!sz)
//				break;
//			std::vector<BYTE> buf(sz);
//			if (GetRawInputData((HRAWINPUT)lParam, RID_INPUT, buf.data(), &sz, sizeof(RAWINPUTHEADER)) != sz)
//				break;
//			RAWINPUT* raw = (RAWINPUT*)buf.data();
//			if (raw->header.dwType == RIM_TYPEMOUSE)
//			{
//				std::string why;
//				bool ok = IsRawDeviceAllowed(raw->header.hDevice, g_hidRegistry, &why);
//				LONG dx = raw->data.mouse.lLastX;
//				LONG dy = raw->data.mouse.lLastY;
//				// std::cout << "[WM_INPUT] dx=" << dx << " dy=" << dy << " allow=" << (ok ? "YES" : "NO") << " reason=" << why << "\n";
//				trace("[WM_INPUT] dx=%d dy=%d allow=%s reason=%s\n", dx, dy, ok ? "YES" : "NO", why.c_str());
//				// Nếu ok==true => xử lý aim/move trong game; nếu không => bỏ qua
//			}
//			break;
//		}
//		case WM_DESTROY:
//			g_inputHWnd = NULL;
//			PostQuitMessage(0);
//			return 0;
//	}
//	return DefWindowProc(hwnd, msg, wParam, lParam);
//}

//int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int)
//{
//	AllocConsole();
//	FILE* f;
//	freopen_s(&f, "CONOUT$", "w", stdout);
//
//	// 1) Xây registry ban đầu
//	BuildHidRegistry(g_hidRegistry);
//
//	// 2) Tạo window để nhận WM_INPUT
//	const char* WC = "RAW_HID_REGISTRY_DEMO";
//	WNDCLASSA wc{};
//	wc.lpfnWndProc = WndProc;
//	wc.hInstance = hInst;
//	wc.lpszClassName = WC;
//	RegisterClassA(&wc);
//	HWND hwnd = CreateWindowExA(0, WC, "Raw HID Registry Demo", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 640, 120, nullptr, nullptr, hInst, nullptr);
//	ShowWindow(hwnd, SW_HIDE);
//
//	// 3) Đăng ký Raw Input (Mouse)
//	RAWINPUTDEVICE rid{};
//	rid.usUsagePage = 0x01; // Generic Desktop
//	rid.usUsage = 0x02; // Mouse
//	rid.dwFlags = RIDEV_INPUTSINK; // nhận cả khi ko focus
//	rid.hwndTarget = hwnd;
//	if (!RegisterRawInputDevices(&rid, 1, sizeof(rid)))
//	{
//		std::cout << "[ERR] RegisterRawInputDevices failed: " << GetLastError() << "\n";
//		return 1;
//	}
//
//	std::cout << "[INFO] Ready. Move/click mouse to see filtering logs.\n";
//
//	// 4) Message loop
//	MSG msg;
//	while (GetMessage(&msg, nullptr, 0, 0))
//	{
//		TranslateMessage(&msg);
//		DispatchMessage(&msg);
//	}
//	return 0;
//}

static HookFunction initFunction([]()
{
	static HostSharedData<CfxState> initState("CfxInitState");
	if (initState->IsGameProcess())
	{
		BuildHidRegistry(g_hidRegistry);
		/*AllocConsole();
		FILE* f;
		freopen_s(&f, "CONOUT$", "w", stdout);*/

		// 1) Xây registry ban đầu

		// 2) Tạo window để nhận WM_INPUT
		/*WNDCLASS wc{};
		wc.lpfnWndProc = WndProc;
		wc.hInstance = GetModuleHandle(NULL);
		wc.lpszClassName = L"RAW_HID_REGISTRY_DEMO";
		RegisterClass(&wc);
		CreateWindow(L"RAW_HID_REGISTRY_DEMO", L"CitizenFX Global Input Window", 0, 0, 0, 1, 1, NULL, NULL, GetModuleHandle(NULL), nullptr);*/
		

		InputHook::DeprecatedOnWndProc.Connect([](HWND, UINT wMsg, WPARAM wParam, LPARAM lParam, bool&, LRESULT& result)
		{
			switch (wMsg)
			{
				case WM_INPUT:
				{
					UINT sz = 0;
					GetRawInputData((HRAWINPUT)lParam, RID_INPUT, nullptr, &sz, sizeof(RAWINPUTHEADER));
					if (!sz)
						break;
					std::vector<BYTE> buf(sz);
					if (GetRawInputData((HRAWINPUT)lParam, RID_INPUT, buf.data(), &sz, sizeof(RAWINPUTHEADER)) != sz)
						break;
					RAWINPUT* raw = (RAWINPUT*)buf.data();
					if (raw->header.dwType == RIM_TYPEMOUSE)
					{
						std::string why;
						bool ok = IsRawDeviceAllowed(raw->header.hDevice, g_hidRegistry, &why);
						LONG dx = raw->data.mouse.lLastX;
						LONG dy = raw->data.mouse.lLastY;
						trace("[WM_INPUT] dx=%d dy=%d allow=%s reason=%s\n", dx, dy, ok ? "YES" : "NO", why.c_str());
						// Nếu ok==true => xử lý aim/move trong game; nếu không => bỏ qua
					}
					break;
				}
			}
		});
		

		//// 4) Message loop
		//MSG msg;
		//while (GetMessage(&msg, nullptr, 0, 0))
		//{
		//	TranslateMessage(&msg);
		//	DispatchMessage(&msg);
		//}
		//std::thread([]()
		//{
		//	Sleep(1000); // chờ window tạo xong
		//	MSG msg = {0};
		//	while (g_inputHWnd)
		//	{
		//		trace("// Waiting for WM_INPUT...\n");
		//		if (GetMessage(&msg, NULL, 0, 0))
		//		{
		//			TranslateMessage(&msg);
		//			DispatchMessage(&msg);
		//		}
		//	}
		//}).detach();
	}
});
