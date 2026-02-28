//---------------------------------------------------------------------------
// CorsValidator.cpp — HTTP CORS validation helpers
//---------------------------------------------------------------------------

#include "CorsValidator.h"

namespace Mcp { namespace Transport {

CorsValidator::CorsValidator(const TCorsConfig &config)
	: FConfig(config)
{
}

TCorsResult CorsValidator::Validate(const ITransportRequest &req, bool isPreflight) const
{
	TCorsResult result;
	result.IsPreflight = isPreflight;

	// Accept header validation (skip for preflight)
	if (!isPreflight)
	{
		std::string accept = req.GetHeader("Accept");
		if (accept.empty())
		{
			result.Allowed = false;
			result.StatusCode = 406;
			result.ErrorMessage = "Missing Accept header. Must include application/json.";
			return result;
		}

		std::string acceptLower = ToLower(accept);
		if (acceptLower.find("application/json") == std::string::npos &&
			acceptLower.find("*/*") == std::string::npos)
		{
			result.Allowed = false;
			result.StatusCode = 406;
			result.ErrorMessage = "Accept header must include application/json.";
			return result;
		}
	}

	// Origin validation (if present)
	std::string origin = req.GetHeader("Origin");
	if (!origin.empty())
	{
		result.HasOrigin = true;
		result.Origin = origin;
		if (!IsOriginAllowed(origin))
		{
			result.Allowed = false;
			result.StatusCode = 403;
			result.ErrorMessage = "Origin not allowed: " + origin;
			return result;
		}
	}

	result.Allowed = true;
	result.StatusCode = 200;
	return result;
}

void CorsValidator::ApplyHeaders(const TCorsResult &result, ITransportResponse &resp) const
{
	if (!result.HasOrigin)
		return;

	resp.SetHeader("Access-Control-Allow-Origin", result.Origin);
	resp.SetHeader("Access-Control-Allow-Methods", FConfig.AllowMethods);
	resp.SetHeader("Access-Control-Allow-Headers", FConfig.AllowHeaders);
	resp.SetHeader("Vary", "Origin");
}

bool CorsValidator::IsOriginAllowed(const std::string &origin) const
{
	std::string originLower = ToLower(origin);

	for (const auto &allowed : FConfig.AllowedOrigins)
	{
		if (ToLower(allowed) == originLower)
			return true;
	}

	if (FConfig.AllowLocalhost)
	{
		if (originLower.find("localhost") != std::string::npos ||
			originLower.find("127.0.0.1") != std::string::npos ||
			originLower.find("::1") != std::string::npos)
		{
			return true;
		}
	}

	return false;
}

std::string CorsValidator::ToLower(const std::string &s)
{
	std::string out;
	out.reserve(s.size());
	for (unsigned char c : s)
		out += static_cast<char>(::tolower(c));
	return out;
}

}} // namespace Mcp::Transport
