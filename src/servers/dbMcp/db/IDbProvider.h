//---------------------------------------------------------------------------
// IDbProvider.h — VCL-free database provider interface (UTF-8)
//
// Defines abstract interface for database operations using std::string.
// All strings are UTF-8; all methods return JSON strings:
//   Success: {"rows": [...], "rowCount": N, "duration_ms": N}
//   Error:   {"error": "message"}
//---------------------------------------------------------------------------

#ifndef IDbProviderH
#define IDbProviderH
//---------------------------------------------------------------------------
#include <string>
//---------------------------------------------------------------------------

/// <summary>
/// Abstract interface for database metadata and query operations (UTF-8).
/// Implementations must be thread-safe for concurrent worker connections.
/// </summary>
class IDbProvider
{
public:
	virtual ~IDbProvider() {}

	//-----------------------------------------------------------------------
	// Provider info
	//-----------------------------------------------------------------------

	/// <summary>True if provider is available/connected.</summary>
	virtual bool IsAvailable() const = 0;

	/// <summary>Provider name for display (e.g., "Microsoft SQL Server")</summary>
	virtual std::string GetProviderName() const = 0;

	//-----------------------------------------------------------------------
	// Metadata queries — return JSON string
	//-----------------------------------------------------------------------

	/// <summary>
	/// List all tables in current database.
	/// schemaFilter: optional schema name to filter (empty = all schemas)
	/// includeViews: true to include views along with tables
	/// Returns: [{schema, table, type, row_count, description}, ...]
	/// </summary>
	virtual std::string ListTables(const std::string &schemaFilter = "",
		bool includeViews = false) = 0;

	/// <summary>
	/// Get schema (columns) for specified table.
	/// table: schema.table or just table (default schema assumed)
	/// Returns: [{field, type, max_length, nullable, is_primary_key, ...}, ...]
	/// </summary>
	virtual std::string GetTableSchema(const std::string &table) = 0;

	/// <summary>
	/// Get foreign key relations for specified table.
	/// Returns: [{direction, constraint_name, from_table, to_table, ...}, ...]
	/// </summary>
	virtual std::string GetTableRelations(const std::string &table) = 0;

	/// <summary>
	/// List all databases on server.
	/// Returns: [{database_name, state, size_mb, ...}, ...]
	/// </summary>
	virtual std::string ListDatabases() = 0;

	/// <summary>
	/// List database objects of specified type.
	/// objType: "TABLE", "VIEW", "PROCEDURE", "FUNCTION", "TRIGGER", "ALL"
	/// schemaFilter: optional schema name filter (empty = all schemas)
	/// namePattern: optional name pattern with wildcards (empty or "%" = all)
	/// Returns: [{schema, name, type}, ...]
	/// </summary>
	virtual std::string ListObjects(const std::string &objType,
		const std::string &schemaFilter = "",
		const std::string &namePattern = "%") = 0;

	/// <summary>
	/// Get DDL/source code for database object (view, procedure, function).
	/// Returns: [{object_name, object_type, definition}, ...]
	/// </summary>
	virtual std::string GetObjectDefinition(const std::string &objName,
		const std::string &objType) = 0;

	/// <summary>
	/// Get dependencies for database object (what it references / what references it).
	/// direction: "USES", "USED_BY", or "BOTH" (default)
	/// Returns: [{direction, name, type, schema}, ...]
	/// </summary>
	virtual std::string GetDependencies(const std::string &objName,
		const std::string &direction = "BOTH") = 0;

	//-----------------------------------------------------------------------
	// Search & analysis — return JSON string
	//-----------------------------------------------------------------------

	/// <summary>
	/// Search for columns matching pattern across all tables.
	/// pattern: column name pattern (supports wildcards: %, _)
	/// Returns: [{table_schema, table_name, column_name, data_type}, ...]
	/// </summary>
	virtual std::string SearchColumns(const std::string &pattern) = 0;

	/// <summary>
	/// Search in source code of stored procedures, views, functions.
	/// pattern: text pattern to search for
	/// Returns: [{schema, name, type, line_number, line_text}, ...]
	/// </summary>
	virtual std::string SearchObjectSource(const std::string &pattern) = 0;

	/// <summary>
	/// Profile column statistics (distinct values, nulls, min/max, top values).
	/// Returns: {distinct_count, null_count, min_value, max_value, top_values:[...]}
	/// </summary>
	virtual std::string ProfileColumn(const std::string &table,
		const std::string &column) = 0;

	//-----------------------------------------------------------------------
	// Query operations — return JSON string
	//-----------------------------------------------------------------------

	/// <summary>
	/// Execute arbitrary SQL query (SELECT/INSERT/UPDATE/DELETE/DDL).
	/// Returns: {"rows": [...], "rowCount": N} or {"rowsAffected": N}
	/// </summary>
	virtual std::string ExecuteQuery(const std::string &sql) = 0;

	/// <summary>
	/// Execute SQL query with row limit for SELECT queries.
	/// maxRows: 0 = no limit, >0 = add TOP/LIMIT/FIRST clause
	/// Provider applies DB-specific syntax (TOP for MSSQL, LIMIT for PG, FIRST for FB)
	/// </summary>
	virtual std::string ExecuteQuery(const std::string &sql, int maxRows) = 0;

	/// <summary>
	/// Get sample rows from table (limited result set).
	/// limit: max number of rows (default 100)
	/// Returns: {"rows": [...], "rowCount": N}
	/// </summary>
	virtual std::string GetTableSample(const std::string &table, int limit = 100) = 0;

	/// <summary>
	/// Get query execution plan (EXPLAIN / SHOWPLAN).
	/// Returns: [{plan_step, operation, cost, ...}, ...]
	/// </summary>
	virtual std::string ExplainQuery(const std::string &sql) = 0;

	//-----------------------------------------------------------------------
	// Advanced operations — return JSON string
	//-----------------------------------------------------------------------

	/// <summary>
	/// Compare schemas of two tables.
	/// Returns: {differences: [{column, table1_type, table2_type}, ...]}
	/// </summary>
	virtual std::string CompareTables(const std::string &table1,
		const std::string &table2) = 0;

	/// <summary>
	/// Trace foreign key path between two tables.
	/// Returns: {path: [{from_table, to_table, via_column}, ...]}
	/// </summary>
	virtual std::string TraceFkPath(const std::string &fromTable,
		const std::string &toTable, int maxDepth = 5) = 0;

	/// <summary>
	/// Get overview of database module/schema (table count, object count, etc).
	/// Returns: {table_count, view_count, procedure_count, function_count, ...}
	/// </summary>
	virtual std::string GetModuleOverview(const std::string &schemaName) = 0;
};

//---------------------------------------------------------------------------
#endif
