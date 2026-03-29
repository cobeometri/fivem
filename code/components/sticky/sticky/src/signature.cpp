#include <StdInc.h>
#include "signature.h"


Botan::SecureVector<uint8_t> b64url_decode(const std::string& s) {
	std::string b64 = s;
	for (char& c : b64) {
		if (c == '-') c = '+';
		else if (c == '_') c = '/';
	}
	while (b64.size() % 4 != 0) b64.push_back('=');
	return Botan::base64_decode(b64);
}



TokenMsg parse_msg(const std::string& msg) {
	TokenMsg m{};
	std::istringstream ss(msg);
	std::string kv;
	std::string obf_v = AY_OBFUSCATE("v");
	std::string obf_nonce = AY_OBFUSCATE("nonce");
	std::string obf_ts = AY_OBFUSCATE("ts");
	std::string obf_exp = AY_OBFUSCATE("exp");
	while (std::getline(ss, kv, ';')) {
		auto pos = kv.find('=');
		if (pos == std::string::npos) continue;
		auto k = kv.substr(0, pos);
		auto v = kv.substr(pos + 1);
		if (k == obf_v) m.v = std::stoi(v);
		else if (k == obf_nonce) m.nonce = v;
		else if (k == obf_ts) m.ts = std::stoll(v);
		else if (k == obf_exp) m.exp = std::stoll(v);
	}
	return m;
}

std::optional<json> verify_token(const std::string& token,
	const std::string& expected_nonce)
{
	Botan::DataSource_Memory ds(
		reinterpret_cast<const uint8_t*>(PUB_PEM),
		std::strlen(PUB_PEM)
	);
	auto pub = Botan::X509::load_key(ds);

	auto dot = token.find('.');
	if (dot == std::string::npos) return std::nullopt;
	auto p64 = token.substr(0, dot);
	auto s64 = token.substr(dot + 1);

	auto payload = Botan::base64_decode(p64); // server nên dùng base64 chuẩn (có '=')
	auto sig = Botan::base64_decode(s64);

	if (sig.size() != 64) return std::nullopt; // Ed25519 chữ ký = 64 bytes

	try {
		const char* obf_Pure = AY_OBFUSCATE("Pure");
		Botan::PK_Verifier verifier(*pub, obf_Pure);
		verifier.update(payload);
		const char* obf_check_signature = AY_OBFUSCATE("check_signature failed\n");
		if (!verifier.check_signature(sig)) {
			trace(obf_check_signature);
			delete pub;
			return std::nullopt;
		}
		delete pub;

		// parse payload nội dung
		std::string payloadStr(payload.begin(), payload.end());
		auto m = json::parse(payloadStr);

		long long now = std::time(nullptr);
		std::string obf_nonce = AY_OBFUSCATE("nonce");
		if (!expected_nonce.empty() && m[obf_nonce] != expected_nonce) {
			const char* err = AY_OBFUSCATE("invalid nonce %s - %s | payload: %s\n");
			trace(err, m[obf_nonce], expected_nonce, p64);
			return std::nullopt;
		};
		if (m["exp"] < now) {
			const char* err = AY_OBFUSCATE("expired %d:%d\n");
			trace(err, m["exp"], now);
			return std::nullopt;
		};
		if (m["v"] != 2) {
			const char* err = AY_OBFUSCATE("invalid version\n");
			trace(err);
			return std::nullopt;
		}
		std::string obf_data = AY_OBFUSCATE("data");
		return m[obf_data];

	}
	catch (std::exception& e) {
		const char* err = AY_OBFUSCATE("verify error %s\n");
		trace(err, e.what());
		return std::nullopt;
	}
}

std::string generate_nonce(size_t bytes) {
	Botan::AutoSeeded_RNG rng;
	std::vector<uint8_t> buf(bytes);
	rng.randomize(buf.data(), buf.size());
	return Botan::hex_encode(buf); // chuỗi hex, độ dài = bytes*2
}

static std::string b64(const std::vector<uint8_t>& v) {
	return Botan::base64_encode(v);
}


EncResult encrypt_with_x25519_public_pem(
	const std::string& plaintext,
	const std::string& aad )
{
	Botan::AutoSeeded_RNG rng;

	// 1) Load receiver's X25519 public key
	Botan::DataSource_Memory ds(PUB32_B64);
	std::unique_ptr<Botan::Public_Key> pub(Botan::X509::load_key(ds));
	if (!pub) throw std::runtime_error(AY_OBFUSCATE("Invalid public key PEM"));

	// Ensure it's X25519 / Curve25519 type
	const auto* rpub = dynamic_cast<const Botan::Curve25519_PublicKey*>(pub.get());
	if (!rpub) throw std::runtime_error(AY_OBFUSCATE("Public key is not X25519/Curve25519"));

	// 2) Generate ephemeral X25519 keypair
	Botan::Curve25519_PrivateKey eph_priv(rng);
	const std::vector<uint8_t> eph_pub = eph_priv.public_value(); // 32 bytes

	// 3) ECDH (Key Agreement): raw shared secret
	const char* obf_Raw = AY_OBFUSCATE("Raw");
	Botan::PK_Key_Agreement ka(eph_priv, rng, obf_Raw);
	const std::vector<uint8_t> peer_pub = rpub->public_value();   // 32 bytes
	Botan::SymmetricKey shared = ka.derive_key(32, peer_pub);

	// 4) KDF (HKDF-SHA256) → key(32) + nonce(12)
	std::string k1 = AY_OBFUSCATE("HKDF(SHA-256)");
	std::unique_ptr<Botan::KDF> hkdf(Botan::KDF::create(k1));
	if (!hkdf) throw std::runtime_error(AY_OBFUSCATE("HKDF(SHA-256) not available"));

	Botan::SecureVector<uint8_t> salt = rng.random_vec(32);
	auto secret = shared.bits_of();
	// Derive 44 bytes: 32 (key) + 12 (nonce)
	Botan::SecureVector<uint8_t> okm = hkdf->derive_key(32+12,
		secret.data(), secret.size(),
		salt.data(), salt.size(),
		nullptr, 0);
	std::vector<uint8_t> key(okm.begin(), okm.begin() + 32);
	std::vector<uint8_t> nonce(okm.begin() + 32, okm.end()); // 12 bytes

	// 5) AEAD: ChaCha20-Poly1305
	std::string k2 = AY_OBFUSCATE("ChaCha20Poly1305");
	std::unique_ptr<Botan::AEAD_Mode> aead(Botan::AEAD_Mode::create(k2, Botan::ENCRYPTION));
	if (!aead) throw std::runtime_error(AY_OBFUSCATE("ChaCha20Poly1305 not available"));
	aead->set_key(Botan::SymmetricKey(key));
	aead->start(nonce.data(), nonce.size());

	if (!aad.empty()) {
		aead->set_associated_data(reinterpret_cast<const uint8_t*>(aad.data()), aad.size());
	}

	Botan::SecureVector<uint8_t> buf(plaintext.begin(), plaintext.end());
	aead->finish(buf, 0); // buf becomes ciphertext||tag

	// 6) Pack & base64
	std::vector<uint8_t> out;
	out.reserve(eph_pub.size() + salt.size() + nonce.size() + buf.size());
	out.insert(out.end(), eph_pub.begin(), eph_pub.end()); // 32
	out.insert(out.end(), salt.begin(), salt.end());    // 32
	out.insert(out.end(), nonce.begin(), nonce.end());   // 12
	out.insert(out.end(), buf.begin(), buf.end());     // ct+tag

	EncResult res;
	res.token_base64 = b64(out);
	return res;
}
