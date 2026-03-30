#pragma once
#include "StdInc.h"
#include <openssl/md5.h>
#include <boost/iostreams/device/mapped_file.hpp>
#include <iomanip>
#include <sstream>
#include <boost/filesystem/path.hpp>
#include <boost/filesystem/operations.hpp>
#include <filesystem>


inline const std::string md5_from_file(const std::wstring& path)
{
	//trace("md5_from_file %s", path);
	auto p = boost::filesystem::path(path);
	auto pStr = p.string();
	if (!boost::filesystem::exists(p))
	{
		trace("md5_from_file not exists %s\n", pStr);
		return "";
	}
	unsigned char result[MD5_DIGEST_LENGTH];
	boost::iostreams::mapped_file_source src(p);
	MD5((unsigned char*)src.data(), src.size(), result);

	std::ostringstream sout;
	sout << std::hex << std::setfill('0');
	for (auto c : result) sout << std::setw(2) << (int)c;

	return sout.str();
}