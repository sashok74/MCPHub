//---------------------------------------------------------------------------
// HttpTransport.h — Indy HTTP transport for MCP
//---------------------------------------------------------------------------

#ifndef HttpTransportH
#define HttpTransportH
//---------------------------------------------------------------------------
#include "ITransport.h"
#include "TransportTypes.h"
#include "CorsValidator.h"
#include "HttpRequest.h"
#include "HttpResponse.h"
#include "McpHttpRouter.h"
#include <IdHTTPServer.hpp>
#include <memory>
//---------------------------------------------------------------------------

namespace Mcp { namespace Transport {

class HttpTransport : public ITransport
{
public:
	explicit HttpTransport(TIdHTTPServer *server,
		const TCorsConfig &corsConfig = TCorsConfig());

	void Start() override;
	void Stop() override;
	bool IsRunning() const override;
	std::string GetName() const override { return "http"; }
	void SetRequestHandler(TMcpRequestHandler handler) override;

	// Indy event adapter (call from OnCommandGet)
	void HandleCommandGet(TIdContext *context, TIdHTTPRequestInfo *requestInfo,
		TIdHTTPResponseInfo *responseInfo);

private:
	TIdHTTPServer *FServer;
	TMcpRequestHandler FHandler;
	CorsValidator FCorsValidator;

	void HandleMcpRequest(HttpRequest &req, HttpResponse &resp);
	static std::string MakeJsonRpcError(const std::string &id, int code,
		const std::string &message);
};

}} // namespace Mcp::Transport

//---------------------------------------------------------------------------
#endif
