//---------------------------------------------------------------------------
// HttpRequest.h — Indy request adapter for ITransportRequest
//---------------------------------------------------------------------------

#ifndef HttpRequestH
#define HttpRequestH
//---------------------------------------------------------------------------
#include "ITransportRequest.h"
#include "UcodeUtf8.h"
#include <System.Classes.hpp>
#include <System.SysUtils.hpp>
#include <IdHTTPServer.hpp>
#include <memory>
//---------------------------------------------------------------------------

namespace Mcp { namespace Transport {

class HttpRequest : public ITransportRequest
{
public:
	explicit HttpRequest(TIdHTTPRequestInfo *requestInfo,
		const std::string &bodyOverride = std::string(),
		bool hasOverride = false)
		: FRequestInfo(requestInfo),
		  FBodyOverride(bodyOverride),
		  FHasBodyOverride(hasOverride || !bodyOverride.empty())
	{
	}

	std::string GetMethod() const override
	{
		if (!FRequestInfo)
			return "";
		switch (FRequestInfo->CommandType)
		{
			case hcGET:    return "GET";
			case hcPOST:   return "POST";
			case hcPUT:    return "PUT";
			case hcDELETE: return "DELETE";
			case hcHEAD:   return "HEAD";
			case hcTRACE:  return "TRACE";
			default:       return "OTHER";
		}
		String cmd = FRequestInfo->Command;
		if (!cmd.IsEmpty())
			return utf8(cmd);
	}

	std::string GetPath() const override
	{
		if (!FRequestInfo)
			return "";
		return utf8(FRequestInfo->Document);
	}

	std::string GetHeader(const std::string &name) const override
	{
		if (!FRequestInfo)
			return "";
		String val = FRequestInfo->RawHeaders->Values[u(name)];
		return utf8(val);
	}

	std::string GetBody() const override
	{
		if (FHasBodyOverride)
			return FBodyOverride;
		if (FBodyLoaded)
			return FBody;
		FBodyLoaded = true;

		if (!FRequestInfo)
			return "";

		String body;
		if (FRequestInfo->PostStream != NULL)
		{
			TStringStream *ss = new TStringStream(L"", TEncoding::UTF8, false);
			ss->CopyFrom(FRequestInfo->PostStream, 0);
			body = ss->DataString;
			delete ss;
			// Reset stream position so others can read it if needed
			FRequestInfo->PostStream->Position = 0;
		}
		else
		{
			body = FRequestInfo->UnparsedParams;
		}

		FBody = utf8(body);
		return FBody;
	}

	TIdHTTPRequestInfo* GetNative() const
	{
		return FRequestInfo;
	}

private:
	TIdHTTPRequestInfo *FRequestInfo = nullptr;
	mutable bool FBodyLoaded = false;
	mutable std::string FBody;
	std::string FBodyOverride;
	bool FHasBodyOverride = false;
};

}} // namespace Mcp::Transport

//---------------------------------------------------------------------------
#endif
