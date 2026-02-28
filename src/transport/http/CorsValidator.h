//---------------------------------------------------------------------------
// CorsValidator.h — HTTP CORS validation helpers
//---------------------------------------------------------------------------

#ifndef CorsValidatorH
#define CorsValidatorH
//---------------------------------------------------------------------------
#include "ITransportRequest.h"
#include "ITransportResponse.h"
#include "TransportTypes.h"
//---------------------------------------------------------------------------

namespace Mcp { namespace Transport {

class CorsValidator
{
public:
	explicit CorsValidator(const TCorsConfig &config);

	TCorsResult Validate(const ITransportRequest &req, bool isPreflight) const;
	void ApplyHeaders(const TCorsResult &result, ITransportResponse &resp) const;

private:
	TCorsConfig FConfig;

	bool IsOriginAllowed(const std::string &origin) const;
	static std::string ToLower(const std::string &s);
};

}} // namespace Mcp::Transport

//---------------------------------------------------------------------------
#endif
