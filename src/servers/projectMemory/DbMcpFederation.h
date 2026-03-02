//---------------------------------------------------------------------------
// DbMcpFederation.h — HTTP client for calling dbmcp tools via JSON-RPC
//
// Used by ProjectMemory to auto-fetch schema/FK data from a running dbmcp
// instance on localhost. Header-only, uses Indy TIdHTTP (already linked).
//---------------------------------------------------------------------------

#ifndef DbMcpFederationH
#define DbMcpFederationH

#include <string>
#include <atomic>
#include <memory>
#include <IdHTTP.hpp>
#include <IdSSLOpenSSL.hpp>
#include "nlohmann/json.hpp"

namespace Federation {

using json = nlohmann::json;

class DbMcpClient
{
	std::string FUrl;
	std::atomic<int> FRequestId{0};

public:
	explicit DbMcpClient(int port)
		: FUrl("http://localhost:" + std::to_string(port) + "/mcp")
	{}

	bool IsConfigured() const { return !FUrl.empty(); }

	// Low-level: send JSON-RPC tools/call, return parsed result content
	// Returns json() (null) on any error
	json CallTool(const std::string& toolName, const json& args)
	{
		try {
			json request = {
				{"jsonrpc", "2.0"},
				{"id", ++FRequestId},
				{"method", "tools/call"},
				{"params", {
					{"name", toolName},
					{"arguments", args}
				}}
			};

			std::string body = request.dump();

			// TIdHTTP created per-call → thread-safe without mutex
			std::unique_ptr<TIdHTTP> http(new TIdHTTP(nullptr));
			http->ConnectTimeout = 5000;
			http->ReadTimeout = 10000;
			http->Request->ContentType = "application/json";

			std::unique_ptr<TStringStream> reqStream(
				new TStringStream(UnicodeString(body.c_str()), TEncoding::UTF8, false));
			std::unique_ptr<TStringStream> respStream(
				new TStringStream(L"", TEncoding::UTF8, false));

			http->Post(FUrl.c_str(), reqStream.get(), respStream.get());

			std::string respStr = AnsiString(respStream->DataString).c_str();
			json resp = json::parse(respStr);

			// MCP response: result.content[0].text contains JSON string
			if (resp.contains("result") &&
				resp["result"].contains("content") &&
				resp["result"]["content"].is_array() &&
				!resp["result"]["content"].empty() &&
				resp["result"]["content"][0].contains("text"))
			{
				std::string text = resp["result"]["content"][0]["text"].get<std::string>();
				return json::parse(text);
			}

			return json(); // null — unexpected format
		}
		catch (...) {
			return json(); // connection error, parse error, etc. → skip silently
		}
	}

	// High-level: get_table_schema
	json GetTableSchema(const std::string& table)
	{
		return CallTool("get_table_schema", {{"table", table}});
	}

	// High-level: get_table_relations
	json GetTableRelations(const std::string& table)
	{
		return CallTool("get_table_relations", {{"table_name", table}});
	}
};

} // namespace Federation

#endif
