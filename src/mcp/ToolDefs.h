//---------------------------------------------------------------------------
// ToolDefs.h — Data-driven tool registration infrastructure
//
// Provides:
//   - ToolDef: Structure defining a tool as data
//   - ToolList: Container for tool definitions
//   - RegisterTools(): Bulk registration function
//
// Usage:
//   auto tools = GetDbTools(provider);
//   RegisterTools(server, tools);
//---------------------------------------------------------------------------

#ifndef ToolDefsH
#define ToolDefsH

#include "McpServer.h"
#include <vector>

namespace Mcp { namespace Tools {

//---------------------------------------------------------------------------
// ToolDef — Tool definition as data
//---------------------------------------------------------------------------
struct ToolDef
{
	std::string Name;
	std::string Description;
	TMcpToolSchema Schema;
	std::function<TMcpToolResult(const json&, TMcpToolContext&)> Execute;
};

//---------------------------------------------------------------------------
// ToolList — Container for tool definitions
//---------------------------------------------------------------------------
using ToolList = std::vector<ToolDef>;

//---------------------------------------------------------------------------
// RegisterTools — Bulk registration of tools from a list
//---------------------------------------------------------------------------
inline void RegisterTools(TMcpServer& server, const ToolList& tools)
{
	for (const auto& t : tools)
	{
		server.RegisterLambda(t.Name, t.Description, t.Schema, t.Execute);
	}
}

}} // namespace Mcp::Tools

#endif
