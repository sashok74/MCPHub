//---------------------------------------------------------------------------
// HttpTransport.cpp — Indy HTTP transport for MCP
//---------------------------------------------------------------------------

#include "HttpTransport.h"
#include <cstdio>

namespace Mcp { namespace Transport {

HttpTransport::HttpTransport(TIdHTTPServer *server, const TCorsConfig &corsConfig)
	: FServer(server), FCorsValidator(corsConfig)
{
}

void HttpTransport::Start()
{
	if (FServer)
		FServer->Active = true;
}

void HttpTransport::Stop()
{
	if (FServer)
		FServer->Active = false;
}

bool HttpTransport::IsRunning() const
{
	return FServer && FServer->Active;
}

void HttpTransport::SetRequestHandler(TMcpRequestHandler handler)
{
	FHandler = handler;
}

void HttpTransport::HandleCommandGet(TIdContext *, TIdHTTPRequestInfo *requestInfo,
	TIdHTTPResponseInfo *responseInfo)
{
	// Indy context not used in transport logic
	if (!requestInfo || !responseInfo)
		return;

	HttpRequest req(requestInfo);
	HttpResponse resp(responseInfo);

	HandleMcpRequest(req, resp);
}

void HttpTransport::HandleMcpRequest(HttpRequest &req, HttpResponse &resp)
{
	std::string path = req.GetPath();
	if (!McpHttpRouter::IsMcpPath(path))
	{
		resp.SetStatus(404, "Not Found");
		resp.SetContentType("application/json; charset=utf-8");
		resp.SetBody("{\"error\":\"not found\"}");
		return;
	}

	std::string method = req.GetMethod();
	if (method == "GET" && path == "/mcp")
	{
		resp.SetStatus(405, "Method Not Allowed");
		resp.SetContentType("application/json; charset=utf-8");
		resp.SetBody(MakeJsonRpcError("null", -32600,
			"SSE transport not supported. Use POST."));
		return;
	}

	bool isPreflight = (method == "OPTIONS");
	if (isPreflight)
	{
		TCorsResult cors = FCorsValidator.Validate(req, true);
		if (!cors.Allowed)
		{
			if (cors.HasOrigin)
				FCorsValidator.ApplyHeaders(cors, resp);
			resp.SetStatus(cors.StatusCode);
			resp.SetContentType("application/json; charset=utf-8");
			resp.SetBody(MakeJsonRpcError("null", -32600, cors.ErrorMessage));
			return;
		}

		if (cors.HasOrigin)
			FCorsValidator.ApplyHeaders(cors, resp);

		resp.SetStatus(204, "No Content");
		resp.SetContentType("");
		resp.SetBody("");
		return;
	}

	if (method != "POST")
	{
		resp.SetStatus(405, "Method Not Allowed");
		resp.SetContentType("application/json; charset=utf-8");
		resp.SetBody(MakeJsonRpcError("null", -32600,
			"Method not allowed. Use POST."));
		return;
	}

	TCorsResult cors = FCorsValidator.Validate(req, false);
	if (!cors.Allowed)
	{
		if (cors.HasOrigin)
			FCorsValidator.ApplyHeaders(cors, resp);
		resp.SetStatus(cors.StatusCode);
		resp.SetContentType("application/json; charset=utf-8");
		resp.SetBody(MakeJsonRpcError("null", -32600, cors.ErrorMessage));
		return;
	}

	if (cors.HasOrigin)
		FCorsValidator.ApplyHeaders(cors, resp);

	std::string body = req.GetBody();
	std::string routedBody = McpHttpRouter::ApplyLegacyRouting(path, body);
	HttpRequest routedReq(req.GetNative(), routedBody, true);

	if (!FHandler)
	{
		resp.SetStatus(500, "Internal Server Error");
		resp.SetContentType("application/json; charset=utf-8");
		resp.SetBody(MakeJsonRpcError("null", -32603,
			"MCP handler not initialized"));
		return;
	}

	// Reuse response adapter; handler writes response
	FHandler(routedReq, resp);
}

std::string HttpTransport::MakeJsonRpcError(const std::string &id, int code,
	const std::string &message)
{
	std::string escaped;
	escaped.reserve(message.size());
	for (unsigned char c : message)
	{
		switch (c)
		{
		case '\\': escaped += "\\\\"; break;
		case '"':  escaped += "\\\""; break;
		case '\r': escaped += "\\r"; break;
		case '\n': escaped += "\\n"; break;
		case '\t': escaped += "\\t"; break;
		default:
			if (c < 0x20)
			{
				char buf[7];
				snprintf(buf, sizeof(buf), "\\u%04X", c);
				escaped += buf;
			}
			else
			{
				escaped += static_cast<char>(c);
			}
		}
	}

	return std::string("{\"jsonrpc\":\"2.0\",\"id\":") + id +
		",\"error\":{\"code\":" + std::to_string(code) +
		",\"message\":\"" + escaped + "\"}}";
}

}} // namespace Mcp::Transport
