//---------------------------------------------------------------------------
// ITransportResponse.h — Abstract transport response interface
//---------------------------------------------------------------------------

#ifndef ITransportResponseH
#define ITransportResponseH
//---------------------------------------------------------------------------
#include <string>
//---------------------------------------------------------------------------

namespace Mcp { namespace Transport {

class ITransportResponse
{
public:
	virtual ~ITransportResponse() {}

	virtual void SetStatus(int code, const std::string &text = "") = 0;
	virtual void SetHeader(const std::string &name, const std::string &value) = 0;
	virtual void SetContentType(const std::string &contentType) = 0;
	virtual void SetBody(const std::string &body) = 0;
	virtual void SetNoContent() = 0; // For notifications (HTTP 202)
};

}} // namespace Mcp::Transport

//---------------------------------------------------------------------------
#endif
