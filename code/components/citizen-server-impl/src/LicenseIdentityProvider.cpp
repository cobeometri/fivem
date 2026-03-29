/*
* This file is part of FiveM: https://fivem.net/
*
* See LICENSE and MENTIONS in the root of the source tree for information
* regarding licensing.
*/

#include "StdInc.h"
#include <ServerIdentityProvider.h>

#include <json.hpp>

using json = nlohmann::json;

static InitFunction initFunction([]()
{
	static struct LicenseIdProvider : public fx::ServerIdentityProviderBase
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
			// Bypass CFX license authentication entirely
			cb({});
		}
	} idp;

	fx::RegisterServerIdentityProvider(&idp);
}, 151);
