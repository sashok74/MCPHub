//---------------------------------------------------------------------------
// ITransportRequest.h — Abstract transport request interface
//---------------------------------------------------------------------------

#ifndef ITransportRequestH
#define ITransportRequestH
//---------------------------------------------------------------------------
#include <string>
//---------------------------------------------------------------------------

namespace Mcp { namespace Transport {

class ITransportRequest
{
public:
	virtual ~ITransportRequest() {}

	virtual std::string GetMethod() const = 0;  // "GET", "POST", "OPTIONS"
	virtual std::string GetPath() const = 0;    // "/mcp", "/mcp/tools/call"
	virtual std::string GetHeader(const std::string &name) const = 0;
	virtual std::string GetBody() const = 0;    // JSON-RPC body (cached)
};

}} // namespace Mcp::Transport

//---------------------------------------------------------------------------
#endif
