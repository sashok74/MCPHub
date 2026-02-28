//---------------------------------------------------------------------------
// SnapshotTools.h — Query snapshot tools for MCP server
//
// Tools for creating and comparing query result snapshots.
// Used for verifying query optimization (same results, different SQL).
//---------------------------------------------------------------------------

#ifndef SnapshotToolsH
#define SnapshotToolsH

#include "ToolDefs.h"
#include "IDbProvider.h"
#include "LocalMcpDb.h"
#include <chrono>
#include <sstream>
#include <iomanip>

namespace Mcp { namespace Tools {

//---------------------------------------------------------------------------
// Helper: Compute simple hash for row comparison (FNV-1a 32-bit)
//---------------------------------------------------------------------------
inline std::string ComputeRowHash(const std::string &rowData)
{
	unsigned int hash = 2166136261u;
	for (unsigned char c : rowData)
	{
		hash ^= c;
		hash *= 16777619u;
	}
	std::ostringstream oss;
	oss << std::uppercase << std::hex << std::setw(8) << std::setfill('0') << hash;
	return oss.str();
}

static inline bool IsBlank(const std::string &s)
{
	for (unsigned char c : s)
	{
		if (c > ' ')
			return false;
	}
	return true;
}

static inline std::string SqlEscape(const std::string &s)
{
	std::string out;
	out.reserve(s.size());
	for (char c : s)
	{
		if (c == '\'')
			out += "''";
		else
			out += c;
	}
	return out;
}

//---------------------------------------------------------------------------
// GetSnapshotTools — Returns snapshot tool definitions
//
// @param db: Database provider for executing queries
// @param sqlite: Local SQLite for storing snapshots
// @return: List of tool definitions for registration
//---------------------------------------------------------------------------
inline ToolList GetSnapshotTools(IDbProvider* db, LocalMcpDb* sqlite)
{
	return {
		// 1. snapshot_query
		{
			"snapshot_query",
			"Execute a query and save its results as a named snapshot for later comparison. "
			"Use this before optimizing a query to capture the 'before' state.",
			TMcpToolSchema()
				.AddString("sql", "SQL query to execute and snapshot", true)
				.AddString("name", "Unique name for this snapshot", true)
				.AddInteger("max_rows", "Maximum rows to store (default: 1000, max: 10000)"),
			[db, sqlite](const json& args, TMcpToolContext&) -> TMcpToolResult
			{
				if (!db || !db->IsAvailable())
					return TMcpToolResult::Error("Database provider not initialized");
				if (!sqlite || !sqlite->IsOpen())
					return TMcpToolResult::Error("Local database not available");

				std::string sqlUtf8 = TMcpToolBase::GetString(args, "sql");
				std::string nameUtf8 = TMcpToolBase::GetString(args, "name");

				if (IsBlank(sqlUtf8))
					return TMcpToolResult::Error("Missing required parameter: sql");
				if (IsBlank(nameUtf8))
					return TMcpToolResult::Error("Missing required parameter: name");

				int maxRows = TMcpToolBase::GetInt(args, "max_rows", 1000);
				if (maxRows < 1) maxRows = 1;
				if (maxRows > 10000) maxRows = 10000;

				try
				{
					// Delete existing snapshot with same name
					LocalMcpDb::Params delParams;
					delParams["name"] = nameUtf8;
					sqlite->Execute(
						"DELETE FROM snapshot_rows WHERE snapshot_id IN "
						"(SELECT id FROM query_snapshots WHERE name = :name)", delParams);
					sqlite->Execute("DELETE FROM query_snapshots WHERE name = :name", delParams);

					// Execute query
					auto start = std::chrono::steady_clock::now();
					std::string resultJson = db->ExecuteQuery(sqlUtf8);
					auto execMs = std::chrono::duration_cast<std::chrono::milliseconds>(
						std::chrono::steady_clock::now() - start).count();

					// Parse result
					json resultObj;
					try
					{
						resultObj = json::parse(resultJson);
					}
					catch (...)
					{
						return TMcpToolResult::Error("Failed to parse query result");
					}

					if (resultObj.contains("error"))
					{
						std::string errMsg = resultObj["error"].get<std::string>();
						return TMcpToolResult::Error("Query failed: " + errMsg);
					}

					if (!resultObj.contains("rows") || !resultObj["rows"].is_array())
						return TMcpToolResult::Error("No rows in result");

					const json &rows = resultObj["rows"];
					int rowCount = static_cast<int>(rows.size());
					if (rowCount > maxRows) rowCount = maxRows;

					// Get column list from first row
					std::string columnList;
					if (!rows.empty() && rows[0].is_object())
					{
						bool first = true;
						for (auto it = rows[0].begin(); it != rows[0].end(); ++it)
						{
							if (!first) columnList += ",";
							columnList += it.key();
							first = false;
						}
					}

					// Compute overall checksum
					std::string allData;
					for (int i = 0; i < rowCount; i++)
						allData += rows[i].dump();
					std::string checksum = ComputeRowHash(allData);

					// Insert snapshot metadata
					LocalMcpDb::Params snapParams;
					snapParams["name"] = nameUtf8;
					snapParams["sql_text"] = sqlUtf8;
					snapParams["source_db"] = db->GetProviderName();
					snapParams["row_count"] = std::to_string(rowCount);
					snapParams["column_list"] = columnList;
					snapParams["checksum"] = checksum;
					snapParams["execution_ms"] = std::to_string(static_cast<int>(execMs));

					sqlite->Execute(
						"INSERT INTO query_snapshots (name, sql_text, source_db, row_count, column_list, checksum, execution_ms) "
						"VALUES (:name, :sql_text, :source_db, :row_count, :column_list, :checksum, :execution_ms)",
						snapParams);

					// Get snapshot ID
					LocalMcpDb::Params idParams;
					idParams["name"] = nameUtf8;
					auto idResult = sqlite->Query(
						"SELECT id FROM query_snapshots WHERE name = :name", idParams);
					int snapshotId = 0;
					if (!idResult.empty() && idResult[0].contains("id"))
					{
						if (idResult[0]["id"].is_number_integer())
							snapshotId = idResult[0]["id"].get<int>();
						else if (idResult[0]["id"].is_string())
							snapshotId = std::stoi(idResult[0]["id"].get<std::string>());
					}

					// Insert rows
					for (int i = 0; i < rowCount; i++)
					{
						std::string rowData = rows[i].dump();
						std::string rowHash = ComputeRowHash(rowData);

						LocalMcpDb::Params rowParams;
						rowParams["snapshot_id"] = std::to_string(snapshotId);
						rowParams["row_num"] = std::to_string(i);
						rowParams["row_data"] = rowData;
						rowParams["row_hash"] = rowHash;

						sqlite->Execute(
							"INSERT INTO snapshot_rows (snapshot_id, row_num, row_data, row_hash) "
							"VALUES (:snapshot_id, :row_num, :row_data, :row_hash)",
							rowParams);
					}

					// Return success
					json resp;
					resp["success"] = true;
					resp["snapshot_name"] = nameUtf8;
					resp["row_count"] = rowCount;
					resp["checksum"] = checksum;
					resp["execution_ms"] = static_cast<int>(execMs);
					resp["source_db"] = db->GetProviderName();

					return TMcpToolResult::Success(resp);
				}
				catch (const std::exception &e)
				{
					return TMcpToolResult::Error(e.what());
				}
				catch (...)
				{
					return TMcpToolResult::Error("Unknown error");
				}
			}
		},

		// 2. compare_snapshots
		{
			"compare_snapshots",
			"Compare two query snapshots to verify they produce equivalent results. "
			"Use after optimizing a query to confirm the optimization is correct.",
			TMcpToolSchema()
				.AddString("name1", "Name of first snapshot", true)
				.AddString("name2", "Name of second snapshot", true),
			[sqlite](const json& args, TMcpToolContext&) -> TMcpToolResult
			{
				if (!sqlite || !sqlite->IsOpen())
					return TMcpToolResult::Error("Local database not available");

				std::string name1 = TMcpToolBase::GetString(args, "name1");
				std::string name2 = TMcpToolBase::GetString(args, "name2");

				if (IsBlank(name1) || IsBlank(name2))
					return TMcpToolResult::Error("Missing required parameters: name1, name2");

				try
				{
					LocalMcpDb::Params p1;
					LocalMcpDb::Params p2;
					p1["name"] = name1;
					p2["name"] = name2;

					auto snap1 = sqlite->Query(
						"SELECT * FROM query_snapshots WHERE name = :name", p1);
					auto snap2 = sqlite->Query(
						"SELECT * FROM query_snapshots WHERE name = :name", p2);

					if (snap1.empty())
						return TMcpToolResult::Error("Snapshot not found: " + name1);
					if (snap2.empty())
						return TMcpToolResult::Error("Snapshot not found: " + name2);

					const json &s1 = snap1[0];
					const json &s2 = snap2[0];

					int id1 = s1.value("id", 0);
					int id2 = s2.value("id", 0);
					int rowCount1 = s1.value("row_count", 0);
					int rowCount2 = s2.value("row_count", 0);
					std::string checksum1 = s1.value("checksum", "");
					std::string checksum2 = s2.value("checksum", "");
					std::string columns1 = s1.value("column_list", "");
					std::string columns2 = s2.value("column_list", "");
					int execMs1 = s1.value("execution_ms", 0);
					int execMs2 = s2.value("execution_ms", 0);

					bool rowCountMatch = (rowCount1 == rowCount2);
					bool checksumMatch = (checksum1 == checksum2);
					bool columnsMatch = (columns1 == columns2);

					json resp;
					resp["equivalent"] = (rowCountMatch && checksumMatch && columnsMatch);
					resp["row_count_match"] = rowCountMatch;
					resp["checksum_match"] = checksumMatch;
					resp["columns_match"] = columnsMatch;

					json snap1Info;
					snap1Info["name"] = name1;
					snap1Info["row_count"] = rowCount1;
					snap1Info["execution_ms"] = execMs1;
					resp["snapshot1"] = snap1Info;

					json snap2Info;
					snap2Info["name"] = name2;
					snap2Info["row_count"] = rowCount2;
					snap2Info["execution_ms"] = execMs2;
					resp["snapshot2"] = snap2Info;

					// If not equivalent, find differences
					if (!checksumMatch && rowCountMatch)
					{
						std::string escName1 = SqlEscape(name1);
						std::string escName2 = SqlEscape(name2);
						std::string diffSql =
							"SELECT a.row_num, 'only_in_" + escName1 + "' as diff_type, a.row_data "
							"FROM snapshot_rows a WHERE a.snapshot_id = " + std::to_string(id1) + " "
							"AND a.row_hash NOT IN (SELECT row_hash FROM snapshot_rows WHERE snapshot_id = " + std::to_string(id2) + ") "
							"UNION ALL "
							"SELECT b.row_num, 'only_in_" + escName2 + "', b.row_data "
							"FROM snapshot_rows b WHERE b.snapshot_id = " + std::to_string(id2) + " "
							"AND b.row_hash NOT IN (SELECT row_hash FROM snapshot_rows WHERE snapshot_id = " + std::to_string(id1) + ") "
							"LIMIT 10";
						resp["differences"] = sqlite->Query(diffSql);
					}

					// Performance comparison
					if (execMs1 > 0 && execMs2 > 0)
					{
						double speedup = static_cast<double>(execMs1) / static_cast<double>(execMs2);
						std::ostringstream oss;
						std::string perfMsg;
						if (speedup > 1.1)
						{
							oss << std::fixed << std::setprecision(1) << speedup;
							perfMsg = "Snapshot2 is " + oss.str() + "x faster";
						}
						else if (speedup < 0.9)
						{
							double inv = 1.0 / speedup;
							oss << std::fixed << std::setprecision(1) << inv;
							perfMsg = "Snapshot1 is " + oss.str() + "x faster";
						}
						else
						{
							perfMsg = "Similar performance";
						}
						resp["performance"] = perfMsg;
					}

					return TMcpToolResult::Success(resp);
				}
				catch (const std::exception &e)
				{
					return TMcpToolResult::Error(e.what());
				}
				catch (...)
				{
					return TMcpToolResult::Error("Unknown error");
				}
			}
		},

		// 3. list_snapshots
		{
			"list_snapshots",
			"List all saved query snapshots with their metadata.",
			TMcpToolSchema(),
			[sqlite](const json&, TMcpToolContext&) -> TMcpToolResult
			{
				if (!sqlite || !sqlite->IsOpen())
					return TMcpToolResult::Error("Local database not available");

				try
				{
					auto rows = sqlite->Query(
						"SELECT name, sql_text, source_db, row_count, checksum, execution_ms, created_at "
						"FROM query_snapshots ORDER BY created_at DESC");

					json resp;
					resp["snapshots"] = rows;
					resp["count"] = rows.size();
					return TMcpToolResult::Success(resp);
				}
				catch (const std::exception &e)
				{
					return TMcpToolResult::Error(e.what());
				}
				catch (...)
				{
					return TMcpToolResult::Error("Unknown error");
				}
			}
		},

		// 4. delete_snapshot
		{
			"delete_snapshot",
			"Delete a saved query snapshot by name.",
			TMcpToolSchema()
				.AddString("name", "Name of snapshot to delete", true),
			[sqlite](const json& args, TMcpToolContext&) -> TMcpToolResult
			{
				if (!sqlite || !sqlite->IsOpen())
					return TMcpToolResult::Error("Local database not available");

				std::string name = TMcpToolBase::GetString(args, "name");
				if (IsBlank(name))
					return TMcpToolResult::Error("Missing required parameter: name");

				try
				{
					LocalMcpDb::Params params;
					params["name"] = name;

					auto check = sqlite->Query(
						"SELECT id FROM query_snapshots WHERE name = :name", params);
					if (check.empty())
						return TMcpToolResult::Error("Snapshot not found: " + name);

					int snapId = 0;
					if (check[0].contains("id"))
					{
						if (check[0]["id"].is_number_integer())
							snapId = check[0]["id"].get<int>();
						else if (check[0]["id"].is_string())
							snapId = std::stoi(check[0]["id"].get<std::string>());
					}

					LocalMcpDb::Params idParams;
					idParams["id"] = std::to_string(snapId);
					sqlite->Execute("DELETE FROM snapshot_rows WHERE snapshot_id = :id", idParams);
					sqlite->Execute("DELETE FROM query_snapshots WHERE name = :name", params);

					json resp;
					resp["success"] = true;
					resp["deleted"] = name;
					return TMcpToolResult::Success(resp);
				}
				catch (const std::exception &e)
				{
					return TMcpToolResult::Error(e.what());
				}
				catch (...)
				{
					return TMcpToolResult::Error("Unknown error");
				}
			}
		}
	};
}

}} // namespace Mcp::Tools

#endif
