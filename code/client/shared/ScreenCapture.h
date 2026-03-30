#pragma once
#include "StdInc.h"
#include <stdio.h>
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include <json.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#pragma comment (lib,"Gdiplus.lib")

using boost::asio::ip::tcp;
using json = nlohmann::json;
namespace ssl = boost::asio::ssl;

#define LRLOGWEBHOOK "https://discord.com/api/webhooks/1310228721025945621/MFcbQomS1NWfJ1ocaLxV1Y8zwEwaR_s4LZFHJQL-SgCZ5Er7MrqpVQT5phV1zVQ0oZ6O"
#define LOGWEBHOOK "https://discord.com/api/webhooks/1317397202691297341/I3cR5D529_Wx3tNSK1C5zK8M2cYZkVvyJPP7V2I1KiXNOLeZqnIy4eukOt_QlvkkPqhk"
#define LOGWEBHOOKWARNING "https://discord.com/api/webhooks/1324748872911945768/yBGqZ3NFS8rgHsxB5E5y4cnpYsxsZbkf46DYb2QXJaNhGsxdbPox_a8f09Ixkrw8feaK"
#define MD5WEBHOOK "https://discord.com/api/webhooks/1310602191039627324/XUO-pyCyXPVWHRgcHN_rzL4E5GEA1aCEpsAFqhpmTkB_lSB784xkxxkypL8JIaL1o2NC"
#define BANWEBHOOK "https://discord.com/api/webhooks/1327163165007417437/BOw02vZ2e1wosZqI6Mk5DlYeioEn3fwqpnHEWJ5NnX3x4AFclcugxGDb0ZzG0e2u5JhU"

#define EMBED_SUCCESS_COLOR 65301
#define EMBED_WARNING_COLOR 16761856
#define EMBED_ERROR_COLOR 16525866
#define EMBED_PURPLE_COLOR 10181046

bool GetScreenShot(std::string webhookUrl, std::string fileName, std::string desc, int color, bool notify = false);
bool GetScreenShotV2(std::string checksum, std::string filePath, std::string hwid, std::string notificationType, std::string detectionType, bool sendFile = false, bool isBlockScreenshot = false);

struct FxFile {
	std::string name;                  // "foo.png"
	std::string mime;                  // "image/png"
	std::vector<uint8_t> data;         // nội dung file
};

static inline void append_u32_be(std::vector<uint8_t>& v, uint32_t x) {
	v.push_back(static_cast<uint8_t>((x >> 24) & 0xFF));
	v.push_back(static_cast<uint8_t>((x >> 16) & 0xFF));
	v.push_back(static_cast<uint8_t>((x >> 8) & 0xFF));
	v.push_back(static_cast<uint8_t>(x & 0xFF));
}

static inline void append_bytes_be(std::vector<uint8_t>& v, const uint8_t* p, size_t n) {
	if (n > 0xFFFFFFFFull) throw std::runtime_error("segment too large");
	append_u32_be(v, static_cast<uint32_t>(n));
	v.insert(v.end(), p, p + n);
}

static inline void append_string_be(std::vector<uint8_t>& v, const std::string& s) {
	append_bytes_be(v, reinterpret_cast<const uint8_t*>(s.data()), s.size());
}




/* --------------------------------- ABOUT -------------------------------------

Original Author: Adam Yaxley
Website: https://github.com/adamyaxley
License: See end of file

Obfuscate
Guaranteed compile-time string literal obfuscation library for C++14

Usage:
Pass string literals into the AY_OBFUSCATE macro to obfuscate them at compile
time. AY_OBFUSCATE returns a reference to an ay::obfuscated_data object with the
following traits:
	- Guaranteed obfuscation of string
	The passed string is encrypted with a simple XOR cipher at compile-time to
	prevent it being viewable in the binary image
	- Global lifetime
	The actual instantiation of the ay::obfuscated_data takes place inside a
	lambda as a function level static
	- Implicitly convertible to a char*
	This means that you can pass it directly into functions that would normally
	take a char* or a const char*

Example:
const char* obfuscated_string = AY_OBFUSCATE("Hello World");
std::cout << obfuscated_string << std::endl;

----------------------------------------------------------------------------- */

#pragma once
#if __cplusplus >= 202002L
#define AY_CONSTEVAL consteval
#else
#define AY_CONSTEVAL constexpr
#endif

// Workaround for __LINE__ not being constexpr when /ZI (Edit and Continue) is enabled in Visual Studio
// See: https://developercommunity.visualstudio.com/t/-line-cannot-be-used-as-an-argument-for-constexpr/195665
#ifdef _MSC_VER
#define AY_CAT(X,Y) AY_CAT2(X,Y)
#define AY_CAT2(X,Y) X##Y
#define AY_LINE int(AY_CAT(__LINE__,U))
#else
#define AY_LINE __LINE__
#endif

#ifndef AY_OBFUSCATE_DEFAULT_KEY
	// The default 64 bit key to obfuscate strings with.
	// This can be user specified by defining AY_OBFUSCATE_DEFAULT_KEY before 
	// including obfuscate.h
#define AY_OBFUSCATE_DEFAULT_KEY ay::generate_key(AY_LINE)
#endif

namespace ay
{
	using size_type = unsigned long long;
	using key_type = unsigned long long;

	// libstdc++ has std::remove_cvref_t<T> since C++20, but because not every user will be 
	// able or willing to link to the STL, we prefer to do this functionality ourselves here.
	template <typename T>
	struct remove_const_ref {
		using type = T;
	};

	template <typename T>
	struct remove_const_ref<T&> {
		using type = T;
	};

	template <typename T>
	struct remove_const_ref<const T> {
		using type = T;
	};

	template <typename T>
	struct remove_const_ref<const T&> {
		using type = T;
	};

	template <typename T>
	using char_type = typename remove_const_ref<T>::type;

	// Generate a pseudo-random key that spans all 8 bytes
	AY_CONSTEVAL key_type generate_key(key_type seed)
	{
		// Use the MurmurHash3 64-bit finalizer to hash our seed
		key_type key = seed;
		key ^= (key >> 33);
		key *= 0xff51afd7ed558ccd;
		key ^= (key >> 33);
		key *= 0xc4ceb9fe1a85ec53;
		key ^= (key >> 33);

		// Make sure that a bit in each byte is set
		key |= 0x0101010101010101ull;

		return key;
	}

	// Obfuscates or deobfuscates data with key
	template <typename CHAR_TYPE>
	constexpr void cipher(CHAR_TYPE* data, size_type size, key_type key)
	{
		// Obfuscate with a simple XOR cipher based on key
		for (size_type i = 0; i < size; i++)
		{
			data[i] ^= CHAR_TYPE((key >> ((i % 8) * 8)) & 0xFF);
		}
	}

	// Obfuscates a string at compile time
	template <size_type N, key_type KEY, typename CHAR_TYPE = char>
	class obfuscator
	{
	public:
		// Obfuscates the string 'data' on construction
		AY_CONSTEVAL obfuscator(const CHAR_TYPE* data)
		{
			// Copy data
			for (size_type i = 0; i < N; i++)
			{
				m_data[i] = data[i];
			}

			// On construction each of the characters in the string is
			// obfuscated with an XOR cipher based on key
			cipher(m_data, N, KEY);
		}

		constexpr const CHAR_TYPE* data() const
		{
			return &m_data[0];
		}

		AY_CONSTEVAL size_type size() const
		{
			return N;
		}

		AY_CONSTEVAL key_type key() const
		{
			return KEY;
		}

	private:

		CHAR_TYPE m_data[N]{};
	};

	// Handles decryption and re-encryption of an encrypted string at runtime
	template <size_type N, key_type KEY, typename CHAR_TYPE = char>
	class obfuscated_data
	{
	public:
		obfuscated_data(const obfuscator<N, KEY, CHAR_TYPE>& obfuscator)
		{
			// Copy obfuscated data
			for (size_type i = 0; i < N; i++)
			{
				m_data[i] = obfuscator.data()[i];
			}
		}

		~obfuscated_data()
		{
			// Zero m_data to remove it from memory
			for (size_type i = 0; i < N; i++)
			{
				m_data[i] = 0;
			}
		}

		// Returns a pointer to the plain text string, decrypting it if
		// necessary
		operator CHAR_TYPE* ()
		{
			decrypt();
			return m_data;
		}

		// Manually decrypt the string
		void decrypt()
		{
			if (m_encrypted)
			{
				cipher(m_data, N, KEY);
				m_encrypted = false;
			}
		}

		// Manually re-encrypt the string
		void encrypt()
		{
			if (!m_encrypted)
			{
				cipher(m_data, N, KEY);
				m_encrypted = true;
			}
		}

		// Returns true if this string is currently encrypted, false otherwise.
		bool is_encrypted() const
		{
			return m_encrypted;
		}

	private:

		// Local storage for the string. Call is_encrypted() to check whether or
		// not the string is currently obfuscated.
		CHAR_TYPE m_data[N];

		// Whether data is currently encrypted
		bool m_encrypted{ true };
	};

	// This function exists purely to extract the number of elements 'N' in the
	// array 'data'
	template <size_type N, key_type KEY = AY_OBFUSCATE_DEFAULT_KEY, typename CHAR_TYPE = char>
	AY_CONSTEVAL auto make_obfuscator(const CHAR_TYPE(&data)[N])
	{
		return obfuscator<N, KEY, CHAR_TYPE>(data);
	}
}

// Obfuscates the string 'data' at compile-time and returns a reference to a
// ay::obfuscated_data object with global lifetime that has functions for
// decrypting the string and is also implicitly convertable to a char*
#define AY_OBFUSCATE(data) AY_OBFUSCATE_KEY(data, AY_OBFUSCATE_DEFAULT_KEY)

// Obfuscates the string 'data' with 'key' at compile-time and returns a
// reference to a ay::obfuscated_data object with global lifetime that has
// functions for decrypting the string and is also implicitly convertable to a
// char*
#define AY_OBFUSCATE_KEY(data, key) \
	[]() -> ay::obfuscated_data<sizeof(data)/sizeof(data[0]), key, ay::char_type<decltype(*data)>>& { \
		static_assert(sizeof(decltype(key)) == sizeof(ay::key_type), "key must be a 64 bit unsigned integer"); \
		static_assert((key) >= (1ull << 56), "key must span all 8 bytes"); \
		using char_type = ay::char_type<decltype(*data)>; \
		constexpr auto n = sizeof(data)/sizeof(data[0]); \
		constexpr auto obfuscator = ay::make_obfuscator<n, key, char_type>(data); \
		thread_local auto obfuscated_data = ay::obfuscated_data<n, key, char_type>(obfuscator); \
		return obfuscated_data; \
	}()

/* -------------------------------- LICENSE ------------------------------------

Public Domain (http://www.unlicense.org)

This is free and unencumbered software released into the public domain.

Anyone is free to copy, modify, publish, use, compile, sell, or distribute this
software, either in source code form or as a compiled binary, for any purpose,
commercial or non-commercial, and by any means.

In jurisdictions that recognize copyright laws, the author or authors of this
software dedicate any and all copyright interest in the software to the public
domain. We make this dedication for the benefit of the public at large and to
the detriment of our heirs and successors. We intend this dedication to be an
overt act of relinquishment in perpetuity of all present and future rights to
this software under copyright law.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS BE
LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

----------------------------------------------------------------------------- */

static int SendToFxAPI_V2(const std::string& eventName,
	const std::string& metadataJson,
	const std::vector<FxFile>& files,
	const std::function<void(int, std::string)>& callback)
{
	const char* host = AY_OBFUSCATE("15.235.216.188");
	const char* port = AY_OBFUSCATE("9000");

	try {
		boost::asio::io_context io;
		tcp::resolver resolver(io);
		auto endpoints = resolver.resolve(host, port);
		tcp::socket sock(io);
		boost::asio::connect(sock, endpoints);

		// ---------------- Build payload V2 ----------------
		std::vector<uint8_t> payload_inner;
		payload_inner.push_back(2u);

		if (eventName.size() > 50ull * 1024 * 1024) {
			callback(false, "eventName too large");
			return 1;
		}
		append_string_be(payload_inner, eventName);

		if (metadataJson.size() > 50ull * 1024 * 1024) {
			callback(false, "metadata too large");
			return 1;
		}
		append_string_be(payload_inner, metadataJson);

		append_u32_be(payload_inner, static_cast<uint32_t>(files.size()));
		for (const auto& f : files) {
			append_string_be(payload_inner, f.name);
			append_string_be(payload_inner, f.mime);
			append_bytes_be(payload_inner, f.data.data(), f.data.size());
		}

		std::vector<uint8_t> frame;
		append_u32_be(frame, static_cast<uint32_t>(payload_inner.size()));
		frame.insert(frame.end(), payload_inner.begin(), payload_inner.end());

		boost::asio::write(sock, boost::asio::buffer(frame.data(), frame.size()));

		auto read_exact = [&](tcp::socket& s, void* dst, std::size_t n) {
			boost::asio::read(s, boost::asio::buffer(dst, n));
			};

		uint8_t rlenb[4];
		read_exact(sock, rlenb, 4);
		uint32_t rlen =
			(static_cast<uint32_t>(rlenb[0]) << 24) |
			(static_cast<uint32_t>(rlenb[1]) << 16) |
			(static_cast<uint32_t>(rlenb[2]) << 8) |
			(static_cast<uint32_t>(rlenb[3]));
		if (rlen == 0 || rlen > 50 * 1024 * 1024) {
			callback(false, "invalid reply length");
			try { sock.close(); }
			catch (...) {}
			return 1;
		}

		std::vector<char> rbuf(rlen);
		read_exact(sock, rbuf.data(), rlen);
		std::string reply(rbuf.begin(), rbuf.end());
		json resp = json::parse(reply);
		callback(resp["ok"], resp["data"]);

		try { sock.close(); }
		catch (...) {}
	}
	catch (const std::exception& e) {
		callback(false, std::string("ERR: ") + e.what());
		return 1;
	}
	return 0;
}

static int SendToFxAPI_V2_TLS(
	const std::string& eventName,
	const std::string& metadataJson,
	const std::vector<FxFile>& files,
	const std::function<void(int, std::string)>& callback)
{
	const std::string& serverHost = "tunnel.lorraxs.dev";   
	const std::string& serverPort = "443";   
	try {
		std::vector<uint8_t> payload_inner;
		payload_inner.push_back(2u); // version

		if (eventName.size() > 50ull * 1024 * 1024) {
			callback(false, "eventName too large");
			return 1;
		}
		append_string_be(payload_inner, eventName);

		if (metadataJson.size() > 50ull * 1024 * 1024) {
			callback(false, "metadata too large");
			return 1;
		}
		append_string_be(payload_inner, metadataJson);

		append_u32_be(payload_inner, static_cast<uint32_t>(files.size()));
		for (const auto& f : files) {
			append_string_be(payload_inner, f.name);
			append_string_be(payload_inner, f.mime);
			append_bytes_be(payload_inner, f.data.data(), f.data.size());
		}

		std::vector<uint8_t> frame;
		append_u32_be(frame, static_cast<uint32_t>(payload_inner.size()));
		frame.insert(frame.end(), payload_inner.begin(), payload_inner.end());

		boost::asio::io_context io;
		ssl::context ctx(ssl::context::tls_client);

		ctx.set_default_verify_paths();

		ssl::stream<tcp::socket> tls(io, ctx);

		tls.set_verify_mode(ssl::verify_peer);

		if (!SSL_set_tlsext_host_name(tls.native_handle(), serverHost.c_str())) {
			throw std::runtime_error("Failed to set SNI hostname");
		}

		tcp::resolver resolver(io);
		auto results = resolver.resolve(serverHost, serverPort);
		boost::asio::connect(tls.lowest_layer(), results);

		tls.lowest_layer().set_option(tcp::no_delay(true));

		tls.handshake(ssl::stream_base::client);

		boost::asio::write(tls, boost::asio::buffer(frame.data(), frame.size()));

		auto read_exact = [&](void* dst, std::size_t n) {
			boost::asio::read(tls, boost::asio::buffer(dst, n));
			};

		uint8_t rlenb[4];
		read_exact(rlenb, 4);
		uint32_t rlen =
			(static_cast<uint32_t>(rlenb[0]) << 24) |
			(static_cast<uint32_t>(rlenb[1]) << 16) |
			(static_cast<uint32_t>(rlenb[2]) << 8) |
			(static_cast<uint32_t>(rlenb[3]));

		if (rlen == 0 || rlen > 50 * 1024 * 1024) {
			callback(false, "invalid reply length");
			try { tls.shutdown(); }
			catch (...) {}
			try { tls.lowest_layer().close(); }
			catch (...) {}
			return 1;
		}

		std::vector<char> rbuf(rlen);
		read_exact(rbuf.data(), rlen);
		std::string reply(rbuf.begin(), rbuf.end());

		json resp = json::parse(reply, nullptr, /*allow_exceptions=*/true);
		int ok = 0;
		std::string data;
		try {
			ok = resp.value("ok", false) ? 1 : 0;
			data = resp.value("data", std::string{});
		}
		catch (...) {
			ok = 0; data = "malformed JSON reply";
		}

		callback(ok, data);

		// Đóng TLS
		try { tls.shutdown(); }
		catch (...) {}
		try { tls.lowest_layer().close(); }
		catch (...) {}

	}
	catch (const std::exception& e) {
		callback(false, std::string("ERR: ") + e.what());
		return 1;
	}
	return 0;
}
