#pragma once
#include <botan/hex.h>
#include <botan/base64.h>
#include <botan/x509_key.h>
#include <botan/pubkey.h>
#include <botan/ed25519.h>
#include <botan/auto_rng.h>
#include <botan/data_src.h>
#include <botan/aead.h>  
#include <botan/curve25519.h> 
#include <botan/kdf.h>
#include <botan/pk_keys.h>
#include <optional>

#include <ctime>

Botan::SecureVector<uint8_t> b64url_decode(const std::string& s);
using json = nlohmann::json;

std::optional<json> verify_token(const std::string& token,
	const std::string& expected_nonce);

std::string sign(const json& payload);

static Botan::SecureVector<uint8_t> sig_pubkey = Botan::base64_decode(
	"BKx0SXcFxisRdYSDowHoMMBy+0mkiPpIqoLDJXjiM0I="
);

static const char* PUB_PEM =
"-----BEGIN PUBLIC KEY-----\n"
"MCowBQYDK2VwAyEApx8NXHE3GvAbNvX8XneDwhjN9zswrmVe9KK6tqC5eko=\n"
"-----END PUBLIC KEY-----\n";

static const char* x25519_pub_pem =
"-----BEGIN PUBLIC KEY-----\n"
"MCowBQYDK2VuAyEAa5VZoHjP3hWcBx/7EWhNtStw95ASdVAvTgX+W4y7qWY=\n"
"-----END PUBLIC KEY-----\n";

static const char* PUB32_B64 = AY_OBFUSCATE("-----BEGIN PUBLIC KEY-----\n"
	"MCowBQYDK2VuAyEAa5VZoHjP3hWcBx/7EWhNtStw95ASdVAvTgX+W4y7qWY=\n"
"-----END PUBLIC KEY-----\n");

struct TokenMsg {
	int v;
	std::string nonce;
	long long ts;
	long long exp;
};

struct EncResult {
	// Packed = eph_pub(32) | salt(32) | nonce(12) | ct_and_tag
	std::string token_base64;
};


std::string generate_nonce(size_t bytes = 16);

EncResult encrypt_with_x25519_public_pem(
	const std::string& plaintext,
	const std::string& aad = "");
