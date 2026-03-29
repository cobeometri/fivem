/*
* This file is part of FiveM: https://fivem.net/
*
* See LICENSE and MENTIONS in the root of the source tree for information
* regarding licensing.
*/

#include "StdInc.h"
#include <ServerIdentityProvider.h>

#define STEAM_APPID_FIVEM 2676230
#define STEAM_APPID_REDM 4333400

// this imports pplxtasks somewhere?
#define _PPLTASK_ASYNC_LOGGING 0

#include <cpr/cpr.h>
#include <json.hpp>

#include <tbb/concurrent_queue.h>

#include <TcpServerManager.h>
#include <ServerInstanceBase.h>

#include <HttpClient.h>

#include "GameServer.h"

std::shared_ptr<ConVar<std::string>> g_steamApiKey;
std::shared_ptr<ConVar<std::string>> g_steamApiDomain;
std::shared_ptr<ConVar<bool>> g_enforceSteamAuth;

static int steamAppId;

using json = nlohmann::json;

template<typename Handle, class Class, void(Class::*Callable)()>
void UvCallback(Handle* handle)
{
	(reinterpret_cast<Class*>(handle->data)->*Callable)();
}

static InitFunction initFunction([]()
{
	static fx::ServerInstanceBase* serverInstance;
	
	static struct SteamIdProvider : public fx::ServerIdentityProviderBase
	{
		HttpClient* httpClient;

		SteamIdProvider()
		{
			httpClient = new HttpClient();
		}

		virtual std::string GetIdentifierPrefix() override
		{
			return "steam";
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
			// Bypass Steam authentication entirely
			cb({});
		}
	} idp;

	fx::ServerInstanceBase::OnServerCreate.Connect([](fx::ServerInstanceBase* instance)
	{
		g_steamApiKey = instance->AddVariable<std::string>("steam_webApiKey", ConVar_None, "");
		g_steamApiDomain = instance->AddVariable<std::string>("steam_webApiDomain", ConVar_None, "api.steampowered.com");
		g_enforceSteamAuth = instance->AddVariable<bool>("sv_enforceSteamAuth", ConVar_ServerInfo, false);

		const auto gameName = instance->GetComponent<fx::GameServer>()->GetGameName();

		if (gameName == fx::GameName::GTA5)
		{
			steamAppId = STEAM_APPID_FIVEM;
		}
		else if (gameName == fx::GameName::RDR3)
		{
			steamAppId = STEAM_APPID_REDM;
		}

		serverInstance = instance;
	});

	fx::RegisterServerIdentityProvider(&idp);
}, 152);
