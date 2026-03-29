//By AlSch092 @github
#pragma once
#include <windows.h>
#include <winternl.h>
#include <iostream>
#include <vector>
#include <SharedFunction.h>
#include <msgpack.hpp>

#pragma comment(lib, "ntdll.lib")

#ifndef STATUS_INFO_LENGTH_MISMATCH
#define STATUS_INFO_LENGTH_MISMATCH ((NTSTATUS)0xC0000004L)
#endif

namespace fx
{
	struct scrObject
	{
		const char* data;
		uintptr_t length;
	};

	template<typename T>
	inline scrObject SerializeObject(const T& object)
	{
		static msgpack::sbuffer buf;
		buf.clear();

		msgpack::packer<msgpack::sbuffer> packer(buf);
		packer.pack(object);

		scrObject packed;
		packed.data = buf.data();
		packed.length = buf.size();

		return packed;
	}

	template<typename T>
	inline T DeserializeObject(const scrObject& obj, bool* outSuccess = nullptr)
	{
		T result{};
		try
		{
			msgpack::unpacked unpacked;
			msgpack::unpack(unpacked, obj.data, obj.length);
			result = unpacked.get().as<T>();
			if (outSuccess)
				*outSuccess = true;
		}
		catch (...)
		{
			if (outSuccess)
				*outSuccess = false;
		}
		return result;
	}

}

namespace Handles
{
    typedef NTSTATUS(NTAPI* NtQuerySystemInformationFunc)(SYSTEM_INFORMATION_CLASS SystemInformationClass, PVOID SystemInformation, ULONG SystemInformationLength, PULONG ReturnLength);

    typedef struct _SYSTEM_HANDLE
    {
        ULONG ProcessId;
        BYTE ObjectTypeNumber;
        BYTE Flags;
        USHORT Handle;
        PVOID Object;
        ACCESS_MASK GrantedAccess;
        BOOL ReferencingOurProcess; //my own addition to the structure, we fill this member in ::DetectOpenHandlesToProcess

    } SYSTEM_HANDLE, * PSYSTEM_HANDLE;

    typedef struct _SYSTEM_HANDLE_INFORMATION
    {
        ULONG HandleCount;
        SYSTEM_HANDLE Handles[1];
    } SYSTEM_HANDLE_INFORMATION, * PSYSTEM_HANDLE_INFORMATION;

    std::vector<SYSTEM_HANDLE> GetHandles();
    std::vector<SYSTEM_HANDLE>  DetectOpenHandlesToProcess();

	bool DoesProcessHaveOpenHandleTous(DWORD pid, const std::vector<Handles::SYSTEM_HANDLE>& handles);
	bool IsHandleReferencingCurrentProcess(const Handles::SYSTEM_HANDLE& h);
}
std::wstring QueryProcessPathFast(DWORD pid);
void CheckOpenHandles();
