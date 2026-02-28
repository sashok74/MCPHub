//---------------------------------------------------------------------------
// StatsTools.h — Statistics and feature request tools for MCP server
//
// Tools for local SQLite database operations (tool usage stats, feature requests).
//---------------------------------------------------------------------------

#ifndef StatsToolsH
#define StatsToolsH

#include "ToolDefs.h"
#include "LocalMcpDb.h"

namespace Mcp { namespace Tools {

//---------------------------------------------------------------------------
// GetStatsTools — Returns stats and feature request tool definitions
//
// @param sqlite: Local SQLite database (captured by lambda)
// @return: List of tool definitions for registration
//---------------------------------------------------------------------------
inline ToolList GetStatsTools(LocalMcpDb* sqlite)
{
	return {
		// 1. get_tool_stats
		{
			"get_tool_stats",
			"Get usage statistics for MCP tools. Shows call counts, success rates, and last used times.",
			TMcpToolSchema()
				.AddString("tool_name", "Filter by specific tool name")
				.AddString("since", "Only count calls since this datetime (ISO format)"),
			[sqlite](const json& args, TMcpToolContext&) -> TMcpToolResult
			{
				if (!sqlite || !sqlite->IsOpen())
					return TMcpToolResult::Error("Local database not available");

				std::string toolName = TMcpToolBase::GetString(args, "tool_name");
				std::string since = TMcpToolBase::GetString(args, "since");

				try
				{
					std::string sql =
						"SELECT tool_name, "
						"COUNT(*) as call_count, "
						"SUM(CASE WHEN success = 1 THEN 1 ELSE 0 END) as success_count, "
						"SUM(CASE WHEN success = 0 THEN 1 ELSE 0 END) as error_count, "
						"MAX(called_at) as last_called "
						"FROM tool_usage";

					LocalMcpDb::Params params;
					std::string whereClause;

					if (!toolName.empty())
					{
						whereClause = " WHERE tool_name = :tool_name";
						params["tool_name"] = toolName;
					}
					if (!since.empty())
					{
						if (whereClause.empty())
							whereClause = " WHERE called_at >= :since";
						else
							whereClause += " AND called_at >= :since";
						params["since"] = since;
					}

					sql += whereClause + " GROUP BY tool_name ORDER BY call_count DESC";

					auto rows = sqlite->Query(sql, params);

					int totalCalls = 0;
					for (const auto &row : rows)
					{
						if (row.contains("call_count") && row["call_count"].is_number_integer())
							totalCalls += row["call_count"].get<int>();
					}

					json resp;
					resp["stats"] = rows;
					resp["total_calls"] = totalCalls;
					resp["period"] = since.empty() ? "all_time" : ("since " + since);

					return TMcpToolResult::Success(resp);
				}
				catch (const std::exception &e)
				{
					return TMcpToolResult::Error(e.what());
				}
			}
		},

		// 2. submit_feature_request
		{
			"submit_feature_request",
			"Submit a feature request or suggest additional functionality for the dbMCP server.",
			TMcpToolSchema()
				.AddString("title", "Short title for the feature request", true)
				.AddString("description", "Detailed description of the feature", true)
				.AddString("use_case", "Example use case or scenario")
				.AddEnum("priority", "Priority level", {"low", "normal", "high"}),
			[sqlite](const json& args, TMcpToolContext&) -> TMcpToolResult
			{
				if (!sqlite || !sqlite->IsOpen())
					return TMcpToolResult::Error("Local database not available");

				std::string title = TMcpToolBase::GetString(args, "title");
				std::string description = TMcpToolBase::GetString(args, "description");

				if (title.empty() || description.empty())
					return TMcpToolResult::Error("Missing required parameters: title, description");

				std::string useCase = TMcpToolBase::GetString(args, "use_case");
				std::string priority = TMcpToolBase::GetString(args, "priority", "normal");

				LocalMcpDb::Params params;
				params["title"] = title;
				params["description"] = description;
				params["use_case"] = useCase;
				params["priority"] = priority;

				try
				{
					sqlite->Execute(
						"INSERT INTO feature_requests (title, description, use_case, priority) "
						"VALUES (:title, :description, :use_case, :priority)",
						params);
					long long rowId = sqlite->LastInsertRowId();

					json resp;
					resp["success"] = true;
					resp["id"] = rowId;
					resp["message"] = "Feature request submitted: " + title;

					return TMcpToolResult::Success(resp);
				}
				catch (const std::exception &e)
				{
					return TMcpToolResult::Error(e.what());
				}
			}
		},

		// 3. get_feature_requests
		{
			"get_feature_requests",
			"Get list of submitted feature requests.",
			TMcpToolSchema()
				.AddString("status", "Filter by status (pending, implemented, rejected)"),
			[sqlite](const json& args, TMcpToolContext&) -> TMcpToolResult
			{
				if (!sqlite || !sqlite->IsOpen())
					return TMcpToolResult::Error("Local database not available");

				std::string status = TMcpToolBase::GetString(args, "status");

				try
				{
					json rows;
					if (!status.empty())
					{
						LocalMcpDb::Params params;
						params["status"] = status;
						rows = sqlite->Query(
							"SELECT * FROM feature_requests WHERE status = :status ORDER BY created_at DESC",
							params);
					}
					else
					{
						rows = sqlite->Query(
							"SELECT * FROM feature_requests ORDER BY created_at DESC");
					}

					json resp;
					resp["requests"] = rows;
					resp["count"] = rows.size();

					return TMcpToolResult::Success(resp);
				}
				catch (const std::exception &e)
				{
					return TMcpToolResult::Error(e.what());
				}
			}
		}
	};
}

}} // namespace Mcp::Tools

#endif
