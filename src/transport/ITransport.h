//---------------------------------------------------------------------------
// ITransport.h — Abstract transport interface
//---------------------------------------------------------------------------

#ifndef ITransportH
#define ITransportH
//---------------------------------------------------------------------------
#include <functional>
#include <string>
#include "ITransportRequest.h"
#include "ITransportResponse.h"
//---------------------------------------------------------------------------

namespace Mcp { namespace Transport {

using TMcpRequestHandler = std::function<void(ITransportRequest&, ITransportResponse&)>;

class ITransport
{
public:
	virtual ~ITransport() {}

	virtual void Start() = 0;
	virtual void Stop() = 0;
	virtual bool IsRunning() const = 0;
	virtual std::string GetName() const = 0;
	virtual void SetRequestHandler(TMcpRequestHandler handler) = 0;
};

}} // namespace Mcp::Transport

//---------------------------------------------------------------------------
#endif
