#include <StdInc.h>

#include <CoreConsole.h>

#include <GameServer.h>
#include <HttpClient.h>

#include <ResourceEventComponent.h>
#include <ResourceManager.h>

#include <ServerInstanceBase.h>
#include <ServerInstanceBaseRef.h>

#include <ServerLicensingComponent.h>

#include <TcpListenManager.h>
#include <ReverseTcpServer.h>

#include <StructuredTrace.h>

#include <NoticeLogicProcessor.h>

// 100KiB cap, conditional notices should fit more than comfortably under this limit
#define MAX_NOTICE_FILESIZE 102400

static void DownloadAndProcessNotices(fx::ServerInstanceBase* server, HttpClient* httpClient)
{
	HttpRequestOptions options;
	options.maxFilesize = MAX_NOTICE_FILESIZE;
	httpClient->DoGetRequest("https://content.grandrp.vn/promotions_targeting.json", options, [server, httpClient](bool success, const char* data, size_t length)
	{
		// Double checking received size because CURL will let bigger files through if the server doesn't specify Content-Length outright
		if (success && length <= MAX_NOTICE_FILESIZE)
		{
			try
			{
				auto noticesBlob = nlohmann::json::parse(data, data + length);
				fx::NoticeLogicProcessor::BeginProcessingNotices(server, noticesBlob);
			}
			catch (std::exception& e)
			{
				trace("Notice error: %s\n", e.what());
			}
		}
	});
}

static InitFunction initFunction([]()
{
	static auto httpClient = new HttpClient();

	static ConsoleCommand printCmd("print", [](const std::string& str)
	{
		trace("%s\n", str);
	});

	fx::ServerInstanceBase::OnServerCreate.Connect([](fx::ServerInstanceBase* instance)
	{
		// CFX Nucleus registration disabled - no calls to cfx.re
	}, INT32_MAX);
});
