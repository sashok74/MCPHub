//---------------------------------------------------------------------------
// HttpResponse.h — Indy response adapter for ITransportResponse
//---------------------------------------------------------------------------

#ifndef HttpResponseH
#define HttpResponseH
//---------------------------------------------------------------------------
#include "ITransportResponse.h"
#include "UcodeUtf8.h"
#include <IdHTTPServer.hpp>
//---------------------------------------------------------------------------

namespace Mcp { namespace Transport {

class HttpResponse : public ITransportResponse
{
public:
	explicit HttpResponse(TIdHTTPResponseInfo *responseInfo)
		: FResponseInfo(responseInfo)
	{
	}

	void SetStatus(int code, const std::string &text = "") override
	{
		if (!FResponseInfo)
			return;
		FResponseInfo->ResponseNo = code;
		if (!text.empty())
			FResponseInfo->ResponseText = u(text);
	}

	void SetHeader(const std::string &name, const std::string &value) override
	{
		if (!FResponseInfo)
			return;
		FResponseInfo->CustomHeaders->Values[u(name)] = u(value);
	}

	void SetContentType(const std::string &contentType) override
	{
		if (!FResponseInfo)
			return;
		FResponseInfo->ContentType = u(contentType);
	}

	void SetBody(const std::string &body) override
	{
		if (!FResponseInfo)
			return;
		FResponseInfo->ContentText = u(body);
	}

	void SetNoContent() override
	{
		if (!FResponseInfo)
			return;
		FResponseInfo->ResponseNo = 202;
		FResponseInfo->ResponseText = "Accepted";
		FResponseInfo->ContentText = "";
		FResponseInfo->ContentType = "";
		FResponseInfo->ContentLength = 0;
	}

private:
	TIdHTTPResponseInfo *FResponseInfo = nullptr;
};

}} // namespace Mcp::Transport

//---------------------------------------------------------------------------
#endif
