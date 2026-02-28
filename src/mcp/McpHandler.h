//---------------------------------------------------------------------------
// McpHandler.h — HTTP handler adapter for TMcpServer
//
// Connects HttpTransport to TMcpServer, handles response formatting.
//---------------------------------------------------------------------------

#ifndef McpHandlerH
#define McpHandlerH

#include "McpServer.h"
#include "ITransportRequest.h"
#include "ITransportResponse.h"
#include <functional>

namespace Mcp {

//---------------------------------------------------------------------------
// Callback for tool usage logging
//---------------------------------------------------------------------------
using TToolUsageCallback = std::function<void(const std::string &toolName,
	bool success, const std::string &errorMessage)>;

//---------------------------------------------------------------------------
// Callback for request/response logging (UI display)
//---------------------------------------------------------------------------
using TLogCallback = std::function<void(const std::string &request,
	const std::string &response)>;

//---------------------------------------------------------------------------
// TMcpHandler — Adapts TMcpServer for HTTP transport
//---------------------------------------------------------------------------
class TMcpHandler
{
public:
	TMcpHandler(TMcpServer *server)
		: FServer(server)
	{}

	void SetToolUsageCallback(TToolUsageCallback callback)
	{
		FToolUsageCallback = std::move(callback);
		// Wire up to server's OnToolExecuted
		FServer->SetOnToolExecuted([this](const std::string &toolName, bool success,
			const std::string &errorMessage) {
			if (FToolUsageCallback)
				FToolUsageCallback(toolName, success, errorMessage);
		});
	}

	void SetLogCallback(TLogCallback callback)
	{
		FLogCallback = std::move(callback);
	}

	// Called by HttpTransport's request handler
	void HandleRequest(Transport::ITransportRequest &req, Transport::ITransportResponse &resp)
	{
		std::string body = req.GetBody();
		std::string response = FServer->HandleRequest(body);

		// Notifications have no response — don't overwrite the log
		if (response.empty())
		{
			resp.SetNoContent();
			return;
		}

		resp.SetStatus(200, "OK");
		resp.SetContentType("application/json; charset=utf-8");
		resp.SetBody(response);

		if (FLogCallback)
			FLogCallback(body, response);
	}

private:
	TMcpServer *FServer;
	TToolUsageCallback FToolUsageCallback;
	TLogCallback FLogCallback;
};

} // namespace Mcp

#endif
