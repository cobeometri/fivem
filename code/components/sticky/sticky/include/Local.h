#pragma once
#include <SoftPub.h>
#include <ScreenCapture.h>
#include <WinTrust.h>
#pragma comment (lib, "wintrust")
#include "../../../client/launcher/LauncherConfig.h"
#include <boost/algorithm/string/case_conv.hpp>
#include <GetFileMD5.h>
#include <HttpClient.h>
#include <json.hpp>
#include "DllDetector.h"
#include <signature.h>

using json = nlohmann::json;

static std::string encode_url(const std::string& s)
{
	const std::string safe_characters =
		"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~";
	std::ostringstream oss;
	for (auto c : s) {
		if (safe_characters.find(c) != std::string::npos)
			oss << c;
		else
			oss << '%' << std::setfill('0') << std::setw(2) <<
			std::uppercase << std::hex << (0xff & c);
	}
	return oss.str();
}



static void Checksum(std::string checksum, std::string path, const std::function<void(int, bool)>& callback)
{
	auto nonce = generate_nonce(16);
	auto encodedPath = encode_url(path);
	json payload;
	std::string k1 = AY_OBFUSCATE("launcher_name");
	std::string k2 = AY_OBFUSCATE("checksum");
	std::string k3 = AY_OBFUSCATE("nonce");
	std::string k4 = AY_OBFUSCATE("path");
	std::string k5 = AY_OBFUSCATE("data");
	payload[k1] = ToNarrow(PRODUCT_NAME);
	payload[k2] = checksum;
	payload[k3] = nonce;
	payload[k4] = path;
	auto encrypted_payload = encrypt_with_x25519_public_pem(payload.dump());
	json bodyPayload;
	bodyPayload[k5] = encrypted_payload.token_base64;
	const char* url = AY_OBFUSCATE("https://fxapi.grandrp.vn/checksum/v3");
	//trace("checksum %s %s\n", checksum, path);
	HttpRequestOptions options;
	options.timeoutNoResponse = std::chrono::milliseconds(5000);
	int timeoutMs = 5000;
	std::shared_ptr<std::atomic<bool>> done = std::make_shared<std::atomic<bool>>(false);

	std::thread([done,  timeoutMs]() {
		std::this_thread::sleep_for(std::chrono::milliseconds(timeoutMs));
		if (!done->load())
		{
			// timeout xảy ra
			exit(0);
		}
		}).detach();
	std::string sBody = bodyPayload.dump();
	std::vector<uint8_t> bodyBytes(sBody.begin(), sBody.end());
	/*SendToFxAPI("checksum", bodyBytes, [](bool success, std::string data) {
		trace("response: %s\n", data);
	});*/
	std::vector<FxFile> files;
	SendToFxAPI_V2("CHECKSUM", bodyPayload.dump(), files, [callback, done, checksum, nonce](bool success, std::string data) {
		//trace("CHECKSUM %s : %b-%s\n", checksum, success, data);
		std::string k1 = AY_OBFUSCATE("signature");
		std::string k2 = AY_OBFUSCATE("flag");
		std::string k3 = AY_OBFUSCATE("need_file");
		*done = true;
		if (!success)
		{
			//FatalError("Cannot connect");
			//return callback(0, false);
			exit(0);
		}
		try
		{
			//trace("response :%s \n", data);
			auto parsedBody = json::parse(data);
			std::string signature = parsedBody[k1];
			auto ok = verify_token(signature, nonce);
			if (!ok) {
				exit(1);
				return;
			}
			int status = ok->value(k2, 0);
			bool needFile = ok->value(k3, true);
			return callback(status, needFile);
		}
		catch (const std::exception& e)
		{
			exit(1);
		}
	});
	/*Instance<::HttpClient>::Get()->DoPostRequest(fmt::sprintf(url), bodyPayload.dump(), options, [callback, nonce, done](bool success, const char* data, size_t length)
	{
		std::string k1 = AY_OBFUSCATE("signature");
		std::string k2 = AY_OBFUSCATE("flag");
		std::string k3 = AY_OBFUSCATE("need_file");
		*done = true;
		if (!success)
		{
			exit(0);
		}
		try
		{
			auto parsedBody = json::parse(data);
			std::string signature = parsedBody[k1];
			auto ok = verify_token(signature, nonce);
			if (!ok) {
				exit(1);
				return;
			}
			int status = ok->value(k2, 0);
			bool needFile = ok->value(k3, true);
			return callback(status, needFile);
		}
		catch (const std::exception& e)
		{
			exit(1);
		}
			
	});*/
}


