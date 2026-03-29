#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <Local.h>
#include <CfxState.h>

// ---- Winsock must come first ----
#include <winsock2.h>
#include <ws2tcpip.h>
#include <mswsock.h>

// ---- Then Windows and the rest ----
#include <windows.h>
#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <setupapi.h>
#include <hidsdi.h>
#include <hidclass.h>
#include <cfgmgr32.h>
#include <regex>
#include <algorithm>

// ---- Link libs ----
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "mswsock.lib")
#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "hid.lib")
#pragma comment(lib, "user32.lib")


// ========= Data structures =========
struct HidDeviceInfo
{
	std::string devicePath; // SetupAPI DevicePath (normalized)
	std::string devInstanceId; // e.g. "HID\VID_046D&PID_C534\7&2B...."
	std::string friendlyName; // SPDRP_FRIENDLYNAME / DEVICEDESC
	std::string hardwareIds; // SPDRP_HARDWAREID (multi-sz, line1 captured)
	uint16_t vendorId = 0;
	uint16_t productId = 0;
	uint16_t version = 0;
	std::string vidStr; // parsed "046D"
	std::string pidStr; // parsed "C534"
	std::wstring manufacturer; // HidD_GetManufacturerString
	std::wstring product; // HidD_GetProductString
	std::wstring serial; // HidD_GetSerialNumberString
	bool parsedOk = false;
};

struct RawDeviceInfo
{
	HANDLE hDevice = nullptr;
	std::string rawName; // RIDI_DEVICENAME (normalized)
};

struct HidRegistry
{
	// Danh sách HID từ SetupAPI
	std::vector<HidDeviceInfo> hidList;
	// Tất cả Raw devices
	std::vector<RawDeviceInfo> rawList;

	// Map nhanh: rawName -> index HID (nếu bắt cặp được)
	std::map<std::string, int> rawToHidIndex;
	// Map: HANDLE -> raw name normalized
	std::map<HANDLE, std::string> handleToRawName;

	// Whitelist VID/PID (upper hex)
	std::vector<std::pair<std::string, std::string>> whitelist = {
		{ "046D", "C534" }, // ví dụ Logitech Unifying
		// Thêm VID/PID hợp lệ của bạn tại đây
	};
};

static HidRegistry g_hidRegistry;
static std::wstring GetDevRegPropW(HDEVINFO devs, SP_DEVINFO_DATA& di, DWORD prop);
static std::wstring GetDevInstanceIdW(SP_DEVINFO_DATA& di);
static void EnumerateHidDevices(std::vector<HidDeviceInfo>& out);
static void EnumerateRawDevices(std::vector<RawDeviceInfo>& out, std::map<HANDLE, std::string>& handleToName);
static void Correlate(HidRegistry& reg);
static void BuildHidRegistry(HidRegistry& reg);
static bool IsRawDeviceAllowed(HANDLE hDevice, const HidRegistry& reg, std::string* reasonOut = nullptr);
