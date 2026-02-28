//---------------------------------------------------------------------------
// TransportTypes.h — Shared transport types
//---------------------------------------------------------------------------

#ifndef TransportTypesH
#define TransportTypesH
//---------------------------------------------------------------------------
#include <string>
#include <vector>
//---------------------------------------------------------------------------

namespace Mcp { namespace Transport {

struct TCorsConfig
{
	bool AllowLocalhost = true;
	std::vector<std::string> AllowedOrigins; // Optional explicit allowlist
	std::string AllowMethods = "POST, OPTIONS";
	std::string AllowHeaders = "Content-Type, Accept";
};

struct TCorsResult
{
	bool Allowed = true;
	bool HasOrigin = false;
	bool IsPreflight = false;
	std::string Origin;
	int StatusCode = 200;
	std::string ErrorMessage;
};

struct TJsonRpcParseResult
{
	bool Ok = false;
	bool HasMethod = false;
	std::string Method;
	std::string ErrorMessage;
};

}} // namespace Mcp::Transport

//---------------------------------------------------------------------------
#endif
