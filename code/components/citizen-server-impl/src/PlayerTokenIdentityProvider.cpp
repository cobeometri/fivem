/*
* PlayerToken & Discord Identity Provider for Samael2026
*
* This identity provider reads "playerToken" and "discordId" from the
* client's initConnect POST data and produces identifiers such as:
*   license:<hash-of-playerToken>
*   discord:<discordId>
*
* This replaces the default CfxTicket-based identity when ROS is disabled.
*/

#include "StdInc.h"
#include <ServerIdentityProvider.h>

#include <botan/hash.h>
#include <botan/hex.h>

static InitFunction initFunction([]()
{
	// --- PlayerToken -> license:xxx ---
	static struct PlayerTokenIdProvider : public fx::ServerIdentityProviderBase
	{
		virtual std::string GetIdentifierPrefix() override
		{
			return "license";
		}

		virtual int GetVarianceLevel() override
		{
			return 1;
		}

		virtual int GetTrustLevel() override
		{
			return 5;
		}

		virtual void RunAuthentication(const fx::ClientSharedPtr& clientPtr, const std::map<std::string, std::string>& postMap, const std::function<void(boost::optional<std::string>)>& cb) override
		{
			auto it = postMap.find("playerToken");

			if (it != postMap.end() && !it->second.empty() && it->second != "unk")
			{
				// Hash the JWT token with SHA-1 to produce a stable 40-char hex license identifier
				auto hasher = Botan::HashFunction::create("SHA-1");
				hasher->update(reinterpret_cast<const uint8_t*>(it->second.data()), it->second.size());
				auto digest = hasher->final();

				std::string hexHash = Botan::hex_encode(digest, false);

				clientPtr->AddIdentifier(fmt::sprintf("license:%s", hexHash));
			}

			cb({});
		}
	} tokenIdp;

	fx::RegisterServerIdentityProvider(&tokenIdp);

	// --- discordId -> discord:xxx ---
	static struct DiscordIdProvider : public fx::ServerIdentityProviderBase
	{
		virtual std::string GetIdentifierPrefix() override
		{
			return "discord";
		}

		virtual int GetVarianceLevel() override
		{
			return 1;
		}

		virtual int GetTrustLevel() override
		{
			return 5;
		}

		virtual void RunAuthentication(const fx::ClientSharedPtr& clientPtr, const std::map<std::string, std::string>& postMap, const std::function<void(boost::optional<std::string>)>& cb) override
		{
			auto it = postMap.find("discordId");

			if (it != postMap.end() && !it->second.empty() && it->second != "unk")
			{
				clientPtr->AddIdentifier(fmt::sprintf("discord:%s", it->second));
			}

			cb({});
		}
	} discordIdp;

	fx::RegisterServerIdentityProvider(&discordIdp);
}, 152);
