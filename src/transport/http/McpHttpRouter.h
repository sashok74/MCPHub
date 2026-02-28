//---------------------------------------------------------------------------
// McpHttpRouter.h — Legacy path routing for MCP HTTP transport
//---------------------------------------------------------------------------

#ifndef McpHttpRouterH
#define McpHttpRouterH
//---------------------------------------------------------------------------
#include "TransportTypes.h"
#include "nlohmann/json.hpp"
#include <string>
//---------------------------------------------------------------------------

namespace Mcp { namespace Transport {

class McpHttpRouter
{
public:
	static bool IsMcpPath(const std::string &path)
	{
		return path == "/mcp" ||
			path == "/mcp/initialize" ||
			path == "/mcp/tools/list" ||
			path == "/mcp/tools/call";
	}

	static std::string ApplyLegacyRouting(const std::string &path, const std::string &body)
	{
		std::string legacyMethod = LegacyMethodForPath(path);
		if (legacyMethod.empty())
			return body;

		nlohmann::json j;
		TJsonRpcParseResult parsed = ParseJson(body, j);
		if (!parsed.Ok)
			return body; // Let TMcpServer return parse error

		if (!j.is_object())
			return body;

		// Only inject if method is missing (keep existing value)
		if (!j.contains("method"))
		{
			j["method"] = legacyMethod;
			return j.dump();
		}

		return body;
	}

private:
	static std::string LegacyMethodForPath(const std::string &path)
	{
		if (path == "/mcp/initialize")
			return "initialize";
		if (path == "/mcp/tools/list")
			return "tools/list";
		if (path == "/mcp/tools/call")
			return "tools/call";
		return "";
	}

	static TJsonRpcParseResult ParseJson(const std::string &body, nlohmann::json &out)
	{
		TJsonRpcParseResult res;
		try
		{
			out = nlohmann::json::parse(body);
			res.Ok = true;
			if (out.is_object() && out.contains("method") && out["method"].is_string())
			{
				res.HasMethod = true;
				res.Method = out["method"].get<std::string>();
			}
			return res;
		}
		catch (const std::exception &e)
		{
			res.Ok = false;
			res.ErrorMessage = e.what();
			return res;
		}
	}
};

}} // namespace Mcp::Transport

//---------------------------------------------------------------------------
#endif
