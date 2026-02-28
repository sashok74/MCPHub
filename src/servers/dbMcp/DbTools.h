//---------------------------------------------------------------------------
// DbTools.h — Database tools for MCP server
//
// Tools for database operations using IDbProvider (UTF-8).
// Each tool is defined as data and registered via GetDbTools().
//---------------------------------------------------------------------------

#ifndef DbToolsH
#define DbToolsH

#include "ToolDefs.h"
#include "IDbProvider.h"

namespace Mcp { namespace Tools {

//---------------------------------------------------------------------------
static inline bool IsBlankStr(const std::string &s)
{
	for (unsigned char c : s)
	{
		if (c > ' ')
			return false;
	}
	return true;
}

//---------------------------------------------------------------------------
// GetDbTools — Returns list of database tool definitions
//
// @param db: Database provider (captured by lambda)
// @return: List of tool definitions for registration
//---------------------------------------------------------------------------
inline ToolList GetDbTools(IDbProvider* db)
{
	return {
		// 1. execute_query
		{
			"execute_query",
			"Execute a SQL query against the connected database. "
			"Supports SELECT, INSERT, UPDATE, DELETE. "
			"SELECT queries without TOP/LIMIT are automatically limited to max_rows.",
			TMcpToolSchema()
				.AddString("sql", "SQL query to execute", true)
				.AddInteger("max_rows", "Maximum rows for SELECT (default: 1000, max: 50000)"),
			[db](const json& args, TMcpToolContext&) -> TMcpToolResult
			{
				if (!db || !db->IsAvailable())
					return TMcpToolResult::Error("Database provider not initialized");

				std::string sql = TMcpToolBase::GetString(args, "sql");
				if (IsBlankStr(sql))
					return TMcpToolResult::Error("Missing required parameter: sql");

				int maxRows = TMcpToolBase::GetInt(args, "max_rows", 1000);
				if (maxRows < 1) maxRows = 1;
				if (maxRows > 50000) maxRows = 50000;

				return TMcpToolResult::Success(db->ExecuteQuery(sql, maxRows));
			}
		},

		// 2. list_tables
		{
			"list_tables",
			"List all tables in the connected database with schema, type, row count and description.",
			TMcpToolSchema()
				.AddString("schema", "Filter by schema name")
				.AddBoolean("include_views", "Include views (default: false)"),
			[db](const json& args, TMcpToolContext&) -> TMcpToolResult
			{
				if (!db || !db->IsAvailable())
					return TMcpToolResult::Error("Database provider not initialized");

				std::string schema = TMcpToolBase::GetString(args, "schema");
				bool includeViews = TMcpToolBase::GetBool(args, "include_views");

				return TMcpToolResult::Success(db->ListTables(schema, includeViews));
			}
		},

		// 3. get_table_schema
		{
			"get_table_schema",
			"Get detailed column information for a table including data types, "
			"primary keys, foreign keys, indexes and descriptions.",
			TMcpToolSchema()
				.AddString("table", "Table name (e.g. 'dbo.Users' or 'Users')", true),
			[db](const json& args, TMcpToolContext&) -> TMcpToolResult
			{
				if (!db || !db->IsAvailable())
					return TMcpToolResult::Error("Database provider not initialized");

				std::string table = TMcpToolBase::GetString(args, "table");
				if (IsBlankStr(table))
					return TMcpToolResult::Error("Missing required parameter: table");

				return TMcpToolResult::Success(db->GetTableSchema(table));
			}
		},

		// 4. list_databases
		{
			"list_databases",
			"List all databases on the server with state, recovery model and size.",
			TMcpToolSchema(),
			[db](const json& args, TMcpToolContext&) -> TMcpToolResult
			{
				if (!db || !db->IsAvailable())
					return TMcpToolResult::Error("Database provider not initialized");

				return TMcpToolResult::Success(db->ListDatabases());
			}
		},

		// 5. get_table_sample
		{
			"get_table_sample",
			"Get sample rows from a table to understand its data. Returns top N rows.",
			TMcpToolSchema()
				.AddString("table", "Table name", true)
				.AddInteger("limit", "Number of rows (default: 5, max: 100)"),
			[db](const json& args, TMcpToolContext&) -> TMcpToolResult
			{
				if (!db || !db->IsAvailable())
					return TMcpToolResult::Error("Database provider not initialized");

				std::string table = TMcpToolBase::GetString(args, "table");
				if (IsBlankStr(table))
					return TMcpToolResult::Error("Missing required parameter: table");

				int limit = TMcpToolBase::GetInt(args, "limit", 5);
				if (limit < 1) limit = 1;
				if (limit > 100) limit = 100;

				return TMcpToolResult::Success(db->GetTableSample(table, limit));
			}
		},

		// 6. search_columns
		{
			"search_columns",
			"Search for columns across all tables by name pattern. "
			"Useful for finding where specific data is stored.",
			TMcpToolSchema()
				.AddString("pattern", "Column name pattern (SQL LIKE, e.g. '%email%')", true),
			[db](const json& args, TMcpToolContext&) -> TMcpToolResult
			{
				if (!db || !db->IsAvailable())
					return TMcpToolResult::Error("Database provider not initialized");

				std::string pattern = TMcpToolBase::GetString(args, "pattern");
				if (IsBlankStr(pattern))
					return TMcpToolResult::Error("Missing required parameter: pattern");

				return TMcpToolResult::Success(db->SearchColumns(pattern));
			}
		},

		// 7. get_object_definition
		{
			"get_object_definition",
			"Get the source code of a function, stored procedure, view, or trigger.",
			TMcpToolSchema()
				.AddString("object_name", "Object name (e.g. 'dbo.MyFunction')", true)
				.AddString("type", "Object type (optional)"),
			[db](const json& args, TMcpToolContext&) -> TMcpToolResult
			{
				if (!db || !db->IsAvailable())
					return TMcpToolResult::Error("Database provider not initialized");

				std::string objName = TMcpToolBase::GetString(args, "object_name");
				if (IsBlankStr(objName))
					return TMcpToolResult::Error("Missing required parameter: object_name");

				std::string objType = TMcpToolBase::GetString(args, "type");

				return TMcpToolResult::Success(db->GetObjectDefinition(objName, objType));
			}
		},

		// 8. list_objects
		{
			"list_objects",
			"List database objects with optional filtering by type, schema, and name pattern. "
			"Default type=ALL searches tables, views, functions, procedures, triggers.",
			TMcpToolSchema()
				.AddEnum("type", "Object type filter",
					{"ALL", "TABLE", "VIEW", "FUNCTION", "PROCEDURE", "TRIGGER"})
				.AddString("schema", "Filter by schema name")
				.AddString("name_pattern", "Name pattern (SQL LIKE, e.g. '%User%')"),
			[db](const json& args, TMcpToolContext&) -> TMcpToolResult
			{
				if (!db || !db->IsAvailable())
					return TMcpToolResult::Error("Database provider not initialized");

				std::string typeFilter = TMcpToolBase::GetString(args, "type", "ALL");
				std::string schema = TMcpToolBase::GetString(args, "schema");
				std::string namePattern = TMcpToolBase::GetString(args, "name_pattern", "%");

				return TMcpToolResult::Success(db->ListObjects(typeFilter, schema, namePattern));
			}
		},

		// 9. get_dependencies
		{
			"get_dependencies",
			"Find SQL code dependencies: what objects a given object references (USES), "
			"and what objects reference it (USED_BY). "
			"NOTE: For FK relationships use get_table_relations instead.",
			TMcpToolSchema()
				.AddString("object_name", "Object name to analyze", true)
				.AddEnum("direction", "Direction of dependencies",
					{"BOTH", "USES", "USED_BY"}),
			[db](const json& args, TMcpToolContext&) -> TMcpToolResult
			{
				if (!db || !db->IsAvailable())
					return TMcpToolResult::Error("Database provider not initialized");

				std::string objName = TMcpToolBase::GetString(args, "object_name");
				if (IsBlankStr(objName))
					return TMcpToolResult::Error("Missing required parameter: object_name");

				std::string direction = TMcpToolBase::GetString(args, "direction", "BOTH");

				return TMcpToolResult::Success(db->GetDependencies(objName, direction));
			}
		},

		// 10. get_table_relations
		{
			"get_table_relations",
			"Get all foreign key relationships for a table - both outgoing and incoming. "
			"Useful for understanding data model and join paths.",
			TMcpToolSchema()
				.AddString("table_name", "Table name (e.g. 'dbo.Orders')", true),
			[db](const json& args, TMcpToolContext&) -> TMcpToolResult
			{
				if (!db || !db->IsAvailable())
					return TMcpToolResult::Error("Database provider not initialized");

				std::string tableName = TMcpToolBase::GetString(args, "table_name");
				if (IsBlankStr(tableName))
					return TMcpToolResult::Error("Missing required parameter: table_name");

				return TMcpToolResult::Success(db->GetTableRelations(tableName));
			}
		},

		// 11. explain_query
		{
			"explain_query",
			"Show the estimated execution plan for a SQL query without running it. "
			"Useful for optimizing slow queries and finding missing indexes.",
			TMcpToolSchema()
				.AddString("sql", "SQL query to analyze", true),
			[db](const json& args, TMcpToolContext&) -> TMcpToolResult
			{
				if (!db || !db->IsAvailable())
					return TMcpToolResult::Error("Database provider not initialized");

				std::string sql = TMcpToolBase::GetString(args, "sql");
				if (IsBlankStr(sql))
					return TMcpToolResult::Error("Missing required parameter: sql");

				return TMcpToolResult::Success(db->ExplainQuery(sql));
			}
		},

		// 12. search_object_source
		{
			"search_object_source",
			"Search (grep) through source code of all SQL objects for a pattern. "
			"Returns matching objects with context snippet.",
			TMcpToolSchema()
				.AddString("pattern", "LIKE pattern to search (e.g. '%OrderTotal%')", true),
			[db](const json& args, TMcpToolContext&) -> TMcpToolResult
			{
				if (!db || !db->IsAvailable())
					return TMcpToolResult::Error("Database provider not initialized");

				std::string pattern = TMcpToolBase::GetString(args, "pattern");
				if (IsBlankStr(pattern))
					return TMcpToolResult::Error("Missing required parameter: pattern");

				return TMcpToolResult::Success(db->SearchObjectSource(pattern));
			}
		},

		// 13. profile_column
		{
			"profile_column",
			"Profile a column's data: distinct count, null count, min/max values, "
			"and top 10 most frequent values.",
			TMcpToolSchema()
				.AddString("table", "Table name", true)
				.AddString("column", "Column name to profile", true),
			[db](const json& args, TMcpToolContext&) -> TMcpToolResult
			{
				if (!db || !db->IsAvailable())
					return TMcpToolResult::Error("Database provider not initialized");

				std::string table = TMcpToolBase::GetString(args, "table");
				std::string column = TMcpToolBase::GetString(args, "column");

				if (IsBlankStr(table) || IsBlankStr(column))
					return TMcpToolResult::Error("Missing required parameters: table, column");

				return TMcpToolResult::Success(db->ProfileColumn(table, column));
			}
		},

		// 14. compare_tables
		{
			"compare_tables",
			"Compare schemas of two tables side by side. Shows common columns, "
			"columns only in one table, and type differences.",
			TMcpToolSchema()
				.AddString("table1", "First table name", true)
				.AddString("table2", "Second table name", true),
			[db](const json& args, TMcpToolContext&) -> TMcpToolResult
			{
				if (!db || !db->IsAvailable())
					return TMcpToolResult::Error("Database provider not initialized");

				std::string t1 = TMcpToolBase::GetString(args, "table1");
				std::string t2 = TMcpToolBase::GetString(args, "table2");

				if (IsBlankStr(t1) || IsBlankStr(t2))
					return TMcpToolResult::Error("Missing required parameters: table1, table2");

				return TMcpToolResult::Success(db->CompareTables(t1, t2));
			}
		},

		// 15. trace_fk_path
		{
			"trace_fk_path",
			"Find foreign key paths (join chains) between two tables. "
			"Uses BFS to discover how tables are connected.",
			TMcpToolSchema()
				.AddString("from_table", "Starting table name", true)
				.AddString("to_table", "Target table name", true)
				.AddInteger("max_depth", "Maximum FK hops (default: 5, max: 10)"),
			[db](const json& args, TMcpToolContext&) -> TMcpToolResult
			{
				if (!db || !db->IsAvailable())
					return TMcpToolResult::Error("Database provider not initialized");

				std::string from = TMcpToolBase::GetString(args, "from_table");
				std::string to = TMcpToolBase::GetString(args, "to_table");

				if (IsBlankStr(from) || IsBlankStr(to))
					return TMcpToolResult::Error("Missing required parameters: from_table, to_table");

				int maxDepth = TMcpToolBase::GetInt(args, "max_depth", 5);
				if (maxDepth < 1) maxDepth = 1;
				if (maxDepth > 10) maxDepth = 10;

				return TMcpToolResult::Success(db->TraceFkPath(from, to, maxDepth));
			}
		},

		// 16. module_overview
		{
			"module_overview",
			"Get summary overview of multiple tables: columns, row counts, FKs. "
			"Pass comma-separated table names.",
			TMcpToolSchema()
				.AddString("tables", "Comma-separated table names", true),
			[db](const json& args, TMcpToolContext&) -> TMcpToolResult
			{
				if (!db || !db->IsAvailable())
					return TMcpToolResult::Error("Database provider not initialized");

				std::string tables = TMcpToolBase::GetString(args, "tables");
				if (IsBlankStr(tables))
					return TMcpToolResult::Error("Missing required parameter: tables");

				return TMcpToolResult::Success(db->GetModuleOverview(tables));
			}
		}
	};
}

}} // namespace Mcp::Tools

#endif
