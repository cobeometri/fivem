#include "StdInc.h"
#include <HwidChecker.h>
#include <boost/container/container_fwd.hpp>
#include <intrin.h>
#include <iphlpapi.h>
#include <sstream>
#include <iomanip>
#include <string>
#include <vector>
#include <iostream>
#include <openssl/sha.h>
#include <tchar.h>
#include <winioctl.h>
#include <curl/curl.h>
#include <d3d10.h>
#include <dxgi.h>
#include <comdef.h>
#include <Wbemidl.h>
#include <mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>
#include <rpc.h>
#include <json.hpp>
#include <shlobj.h> // For SHGetFolderPath


using json = nlohmann::json;

#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "d3d10.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "wbemuuid.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "Rpcrt4.lib")



std::string GetCPUID() {
	int cpuInfo[4] = { -1 };
	__cpuid(cpuInfo, 0);
	std::ostringstream oss;
	for (int i = 0; i < 4; ++i) {
		oss << std::hex << std::setw(8) << std::setfill('0') << cpuInfo[i];
	}
	return oss.str();
}

std::string GetVolumeSerialNumber() {
	DWORD serialNumber = 0;
	GetVolumeInformationA("C:\\", NULL, 0, &serialNumber, NULL, NULL, NULL, 0);
	std::ostringstream oss;
	oss << std::hex << std::setw(8) << std::setfill('0') << serialNumber;
	return oss.str();
}

std::string GetMACAddress() {
	IP_ADAPTER_INFO adapterInfo[16];
	DWORD bufferSize = sizeof(adapterInfo);
	DWORD status = GetAdaptersInfo(adapterInfo, &bufferSize);
	if (status != ERROR_SUCCESS) return "";

	PIP_ADAPTER_INFO pAdapterInfo = adapterInfo;
	std::ostringstream oss;
	while (pAdapterInfo) {
		for (UINT i = 0; i < pAdapterInfo->AddressLength; ++i) {
			oss << std::hex << std::setw(2) << std::setfill('0') << (int)pAdapterInfo->Address[i];
		}
		pAdapterInfo = pAdapterInfo->Next;
	}
	return oss.str();
}

std::string GetBIOSSerialNumber() {
	char buffer[128];
	DWORD size = sizeof(buffer);
	HKEY hKey;
	if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System\\BIOS", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
		if (RegQueryValueExA(hKey, "SystemSerialNumber", NULL, NULL, (LPBYTE)buffer, &size) == ERROR_SUCCESS) {
			RegCloseKey(hKey);
			return std::string(buffer, size - 1);
		}
		RegCloseKey(hKey);
	}
	return "";
}

std::string HashSHA256(std::string prefix, const std::string& input) {
	unsigned char hash[SHA256_DIGEST_LENGTH];
	SHA256_CTX sha256;
	SHA256_Init(&sha256);
	SHA256_Update(&sha256, input.c_str(), input.size());
	SHA256_Final(hash, &sha256);
	std::ostringstream oss;
	for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
		oss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
	}
	return fmt::sprintf("%s:%s", prefix, oss.str());
}

std::string GetGPUId() {
	IDXGIFactory* pFactory = nullptr;
	if (FAILED(CreateDXGIFactory(__uuidof(IDXGIFactory), (void**)&pFactory))) {
		return "";
	}

	IDXGIAdapter* pAdapter = nullptr;
	if (FAILED(pFactory->EnumAdapters(0, &pAdapter))) {
		pFactory->Release();
		return "";
	}

	DXGI_ADAPTER_DESC adapterDesc;
	if (FAILED(pAdapter->GetDesc(&adapterDesc))) {
		pAdapter->Release();
		pFactory->Release();
		return "";
	}

	pAdapter->Release();
	pFactory->Release();

	std::ostringstream oss;
	oss << adapterDesc.DeviceId;
	return oss.str();
}

std::string GetSystemUUID() {
	HKEY hKey;
	if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Cryptography", 0, KEY_READ, &hKey) != ERROR_SUCCESS) {
		return "";
	}

	char uuid[256];
	DWORD size = sizeof(uuid);
	if (RegQueryValueExA(hKey, "MachineGuid", NULL, NULL, (LPBYTE)uuid, &size) != ERROR_SUCCESS) {
		RegCloseKey(hKey);
		return "";
	}

	RegCloseKey(hKey);
	return std::string(uuid, size - 1);
}

std::string GetRAMSerialNumber() {
	HRESULT hres;

	// Initialize COM.
	hres = CoInitializeEx(0, COINIT_MULTITHREADED);
	if (FAILED(hres)) {
		return "";
	}

	// Initialize security.
	hres = CoInitializeSecurity(
		NULL,
		-1,
		NULL,
		NULL,
		RPC_C_AUTHN_LEVEL_DEFAULT,
		RPC_C_IMP_LEVEL_IMPERSONATE,
		NULL,
		EOAC_NONE,
		NULL
	);

	if (FAILED(hres)) {
		CoUninitialize();
		return "";
	}

	// Obtain the initial locator to WMI.
	IWbemLocator* pLoc = NULL;
	hres = CoCreateInstance(
		CLSID_WbemLocator,
		0,
		CLSCTX_INPROC_SERVER,
		IID_IWbemLocator, (LPVOID*)&pLoc);

	if (FAILED(hres)) {
		CoUninitialize();
		return "";
	}

	// Connect to WMI through the IWbemLocator::ConnectServer method.
	IWbemServices* pSvc = NULL;
	hres = pLoc->ConnectServer(
		_bstr_t(L"ROOT\\CIMV2"),
		NULL,
		NULL,
		0,
		NULL,
		0,
		0,
		&pSvc
	);

	if (FAILED(hres)) {
		pLoc->Release();
		CoUninitialize();
		return "";
	}

	// Set security levels on the proxy.
	hres = CoSetProxyBlanket(
		pSvc,
		RPC_C_AUTHN_WINNT,
		RPC_C_AUTHZ_NONE,
		NULL,
		RPC_C_AUTHN_LEVEL_CALL,
		RPC_C_IMP_LEVEL_IMPERSONATE,
		NULL,
		EOAC_NONE
	);

	if (FAILED(hres)) {
		pSvc->Release();
		pLoc->Release();
		CoUninitialize();
		return "";
	}

	// Use the IWbemServices pointer to make requests of WMI.
	IEnumWbemClassObject* pEnumerator = NULL;
	hres = pSvc->ExecQuery(
		bstr_t("WQL"),
		bstr_t("SELECT SerialNumber FROM Win32_PhysicalMemory"),
		WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
		NULL,
		&pEnumerator);

	if (FAILED(hres)) {
		pSvc->Release();
		pLoc->Release();
		CoUninitialize();
		return "";
	}

	// Get the data from the query.
	IWbemClassObject* pclsObj = NULL;
	ULONG uReturn = 0;
	std::string ramSerialNumber;

	while (pEnumerator) {
		HRESULT hr = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);

		if (0 == uReturn) {
			break;
		}

		VARIANT vtProp;
		hr = pclsObj->Get(L"SerialNumber", 0, &vtProp, 0, 0);
		if (SUCCEEDED(hr)) {
			ramSerialNumber = _bstr_t(vtProp.bstrVal);
		}
		VariantClear(&vtProp);
		pclsObj->Release();
	}

	// Cleanup.
	pSvc->Release();
	pLoc->Release();
	pEnumerator->Release();
	CoUninitialize();

	return ramSerialNumber;
}

std::string GetAudioDeviceUUID() {
	HRESULT hr;
	IMMDeviceEnumerator* pEnumerator = NULL;
	IMMDeviceCollection* pCollection = NULL;
	IMMDevice* pDevice = NULL;
	IPropertyStore* pProps = NULL;
	LPWSTR pwszID = NULL;
	std::string audioDeviceUUID;

	// Initialize COM library
	hr = CoInitialize(NULL);
	if (FAILED(hr)) {
		std::cerr << "Failed to initialize COM library: " << std::hex << hr << std::endl;
		return "";
	}

	// Create device enumerator
	hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&pEnumerator);
	if (FAILED(hr)) {
		std::cerr << "Failed to create device enumerator: " << std::hex << hr << std::endl;
		CoUninitialize();
		return "";
	}

	// Enumerate audio endpoints
	hr = pEnumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &pCollection);
	if (FAILED(hr)) {
		std::cerr << "Failed to enumerate audio endpoints: " << std::hex << hr << std::endl;
		pEnumerator->Release();
		CoUninitialize();
		return "";
	}

	UINT count;
	hr = pCollection->GetCount(&count);
	if (FAILED(hr)) {
		std::cerr << "Failed to get device count: " << std::hex << hr << std::endl;
		pCollection->Release();
		pEnumerator->Release();
		CoUninitialize();
		return "";
	}

	for (UINT i = 0; i < count; i++) {
		hr = pCollection->Item(i, &pDevice);
		if (FAILED(hr)) {
			std::cerr << "Failed to get device: " << std::hex << hr << std::endl;
			continue;
		}

		// Get device ID
		hr = pDevice->GetId(&pwszID);
		if (FAILED(hr)) {
			std::cerr << "Failed to get device ID: " << std::hex << hr << std::endl;
			pDevice->Release();
			continue;
		}

		// Open property store
		hr = pDevice->OpenPropertyStore(STGM_READ, &pProps);
		if (FAILED(hr)) {
			std::cerr << "Failed to open property store: " << std::hex << hr << std::endl;
			CoTaskMemFree(pwszID);
			pDevice->Release();
			continue;
		}

		// Get device friendly name
		PROPVARIANT varName;
		PropVariantInit(&varName);
		hr = pProps->GetValue(PKEY_Device_FriendlyName, &varName);
		if (SUCCEEDED(hr)) {
			std::wcout << L"Device " << i << L": " << varName.pwszVal << std::endl;
		}
		PropVariantClear(&varName);

		// Get device instance ID (UUID)
		PROPVARIANT varInstanceId;
		PropVariantInit(&varInstanceId);
		hr = pProps->GetValue(PKEY_Device_InstanceId, &varInstanceId);
		if (SUCCEEDED(hr)) {
			audioDeviceUUID = _bstr_t(varInstanceId.pwszVal);
			std::wcout << L"Instance ID (UUID): " << varInstanceId.pwszVal << std::endl;
		}
		PropVariantClear(&varInstanceId);

		pProps->Release();
		CoTaskMemFree(pwszID);
		pDevice->Release();
	}

	pCollection->Release();
	pEnumerator->Release();
	CoUninitialize();

	return audioDeviceUUID;
}

std::string GetOrCreateUUID()
{
	HKEY hKey;
	char uuidBuffer[37] = { 0 };
	DWORD bufferSize = sizeof(uuidBuffer);
	LONG result = RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\LR", 0, KEY_READ, &hKey);

	if (result == ERROR_SUCCESS)
	{
		result = RegQueryValueExA(hKey, "UUID", NULL, NULL, (LPBYTE)uuidBuffer, &bufferSize);
		RegCloseKey(hKey);

		if (result == ERROR_SUCCESS)
		{
			return std::string(uuidBuffer);
		}
	}

	UUID uuid;
	UuidCreate(&uuid);

	RPC_CSTR uuidString;
	UuidToStringA(&uuid, &uuidString);

	result = RegCreateKeyExA(HKEY_CURRENT_USER, "Software\\LR", 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL);
	if (result == ERROR_SUCCESS)
	{
		RegSetValueExA(hKey, "UUID", 0, REG_SZ, (const BYTE*)uuidString, strlen((const char*)uuidString) + 1);
		RegCloseKey(hKey);
	}

	std::string newUUID((char*)uuidString);
	RpcStringFreeA(&uuidString);

	return newUUID;
}

std::string GetOrCreateUUID2()
{
	HKEY hKey;
	char uuidBuffer[37] = { 0 };
	DWORD bufferSize = sizeof(uuidBuffer);
	LONG result = RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\SyncEngine", 0, KEY_READ, &hKey);

	if (result == ERROR_SUCCESS)
	{
		result = RegQueryValueExA(hKey, "UUID", NULL, NULL, (LPBYTE)uuidBuffer, &bufferSize);
		RegCloseKey(hKey);

		if (result == ERROR_SUCCESS)
		{
			return std::string(uuidBuffer);
		}
	}

	UUID uuid;
	UuidCreate(&uuid);

	RPC_CSTR uuidString;
	UuidToStringA(&uuid, &uuidString);

	result = RegCreateKeyExA(HKEY_CURRENT_USER, "Software\\SyncEngine", 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL);
	if (result == ERROR_SUCCESS)
	{
		RegSetValueExA(hKey, "UUID", 0, REG_SZ, (const BYTE*)uuidString, strlen((const char*)uuidString) + 1);
		RegCloseKey(hKey);
	}

	std::string newUUID((char*)uuidString);
	RpcStringFreeA(&uuidString);

	return newUUID;
}

std::string GetDiscordId()
{
	static std::string discordId;
	HKEY hKey;
	DWORD bufferSize = 0;

	// Mở registry key
	if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\LR", 0, KEY_READ, &hKey) == ERROR_SUCCESS)
	{
		// Lấy kích thước giá trị
		if (RegQueryValueExA(hKey, "DiscordId", NULL, NULL, NULL, &bufferSize) == ERROR_SUCCESS)
		{
			std::vector<char> buffer(bufferSize);
			// Lấy giá trị
			if (RegQueryValueExA(hKey, "DiscordId", NULL, NULL, (LPBYTE)buffer.data(), &bufferSize) == ERROR_SUCCESS)
			{
				discordId.assign(buffer.begin(), buffer.end() - 1); // Loại bỏ ký tự null ở cuối
				RegCloseKey(hKey);
				trace("%s\n", discordId.c_str());
				return discordId.c_str();
			}
		}
		RegCloseKey(hKey);
	}

	return "unk";
}


std::vector<std::string> GetHWIDV2() {

	std::vector<std::string> hwids = {
		HashSHA256("0", GetOrCreateUUID()),
		HashSHA256("1", GetVolumeSerialNumber()),
		HashSHA256("2", GetMACAddress()),
		HashSHA256("3", GetSystemUUID()),
		HashSHA256("5", GetOrCreateUUID2())
	};
	auto discordId = GetDiscordId();
	if (discordId != "unk") {
		hwids.push_back(HashSHA256("4", discordId));
	}


	return hwids;
}


std::string GetHWIDV2JsonString() {
	auto hwids = GetHWIDV2();
	json jsonArray = json::array();
	for (const auto& str : hwids) {
		jsonArray.push_back(str);
	}
	return jsonArray.dump();
}
