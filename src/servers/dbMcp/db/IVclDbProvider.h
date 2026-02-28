//---------------------------------------------------------------------------
// IVclDbProvider.h — VCL-based database provider interface
//
// Defines abstract interface for database operations using VCL String.
// Implemented by FireDAC-based providers (MSSQL, PostgreSQL, Firebird, etc.).
//---------------------------------------------------------------------------

#ifndef IVclDbProviderH
#define IVclDbProviderH
//---------------------------------------------------------------------------
#include <System.Classes.hpp>
#include <System.JSON.hpp>
//---------------------------------------------------------------------------

/// <summary>
/// Abstract interface for database metadata and query operations (VCL String).
/// Implementations must be thread-safe for concurrent worker connections.
/// </summary>
class IVclDbProvider
{
public:
	virtual ~IVclDbProvider() {}

	//-----------------------------------------------------------------------
	// Provider info
	//-----------------------------------------------------------------------

	/// <summary>Provider name for display (e.g., "Microsoft SQL Server")</summary>
	virtual String GetProviderName() const = 0;

	//-----------------------------------------------------------------------
	// Metadata queries — return JSON string
	//-----------------------------------------------------------------------

	/// <summary>
	/// List all tables in current database.
	/// schemaFilter: optional schema name to filter (empty = all schemas)
	/// includeViews: true to include views along with tables
	/// Returns: [{schema, table, type, row_count, description}, ...]
	/// </summary>
	virtual String ListTables(const String &schemaFilter = "", bool includeViews = false) = 0;

	/// <summary>
	/// Get schema (columns) for specified table.
	/// table: schema.table or just table (default schema assumed)
	/// Returns: [{field, type, max_length, nullable, is_primary_key, ...}, ...]
	/// </summary>
	virtual String GetTableSchema(const String &table) = 0;

	/// <summary>
	/// Get foreign key relations for specified table.
	/// Returns: [{direction, constraint_name, from_table, to_table, ...}, ...]
	/// </summary>
	virtual String GetTableRelations(const String &table) = 0;

	/// <summary>
	/// List all databases on server.
	/// Returns: [{database_name, state, size_mb, ...}, ...]
	/// </summary>
	virtual String ListDatabases() = 0;

	/// <summary>
	/// List database objects of specified type.
	/// objType: "TABLE", "VIEW", "PROCEDURE", "FUNCTION", "TRIGGER", "ALL"
	/// schemaFilter: optional schema name filter (empty = all schemas)
	/// namePattern: optional name pattern with wildcards (empty or "%" = all)
	/// Returns: [{schema, name, type}, ...]
	/// </summary>
	virtual String ListObjects(const String &objType, const String &schemaFilter = "",
		const String &namePattern = "%") = 0;

	/// <summary>
	/// Get DDL/source code for database object (view, procedure, function).
	/// Returns: [{object_name, object_type, definition}, ...]
	/// </summary>
	virtual String GetObjectDefinition(const String &objName, const String &objType) = 0;

	/// <summary>
	/// Get dependencies for database object (what it references / what references it).
	/// direction: "USES", "USED_BY", or "BOTH" (default)
	/// Returns: [{direction, name, type, schema}, ...]
	/// </summary>
	virtual String GetDependencies(const String &objName, const String &direction = "BOTH") = 0;

	//-----------------------------------------------------------------------
	// Search & analysis — return JSON string
	//-----------------------------------------------------------------------

	/// <summary>
	/// Search for columns matching pattern across all tables.
	/// pattern: column name pattern (supports wildcards: %, _)
	/// Returns: [{table_schema, table_name, column_name, data_type}, ...]
	/// </summary>
	virtual String SearchColumns(const String &pattern) = 0;

	/// <summary>
	/// Search in source code of stored procedures, views, functions.
	/// pattern: text pattern to search for
	/// Returns: [{schema, name, type, line_number, line_text}, ...]
	/// </summary>
	virtual String SearchObjectSource(const String &pattern) = 0;

	/// <summary>
	/// Profile column statistics (distinct values, nulls, min/max, top values).
	/// Returns: {distinct_count, null_count, min_value, max_value, top_values:[...]}
	/// </summary>
	virtual String ProfileColumn(const String &table, const String &column) = 0;

	//-----------------------------------------------------------------------
	// Query operations — return JSON string
	//-----------------------------------------------------------------------

	/// <summary>
	/// Execute arbitrary SQL query (SELECT/INSERT/UPDATE/DELETE/DDL).
	/// Returns: {"rows": [...], "rowCount": N} or {"rowsAffected": N}
	/// </summary>
	virtual String ExecuteQuery(const String &sql) = 0;

	/// <summary>
	/// Execute SQL query with row limit for SELECT queries.
	/// maxRows: 0 = no limit, >0 = add TOP/LIMIT/FIRST clause
	/// Provider applies DB-specific syntax (TOP for MSSQL, LIMIT for PG, FIRST for FB)
	/// </summary>
	virtual String ExecuteQuery(const String &sql, int maxRows) = 0;

	/// <summary>
	/// Get sample rows from table (limited result set).
	/// limit: max number of rows (default 100)
	/// Returns: {"rows": [...], "rowCount": N}
	/// </summary>
	virtual String GetTableSample(const String &table, int limit = 100) = 0;

	/// <summary>
	/// Get query execution plan (EXPLAIN / SHOWPLAN).
	/// Returns: [{plan_step, operation, cost, ...}, ...]
	/// </summary>
	virtual String ExplainQuery(const String &sql) = 0;

	//-----------------------------------------------------------------------
	// Advanced operations — return JSON string
	//-----------------------------------------------------------------------

	/// <summary>
	/// Compare schemas of two tables.
	/// Returns: {differences: [{column, table1_type, table2_type}, ...]}
	/// </summary>
	virtual String CompareTables(const String &table1, const String &table2) = 0;

	/// <summary>
	/// Trace foreign key path between two tables.
	/// Returns: {path: [{from_table, to_table, via_column}, ...]}
	/// </summary>
	virtual String TraceFkPath(const String &fromTable, const String &toTable, int maxDepth = 5) = 0;

	/// <summary>
	/// Get overview of database module/schema (table count, object count, etc).
	/// Returns: {table_count, view_count, procedure_count, function_count, ...}
	/// </summary>
	virtual String GetModuleOverview(const String &schemaName) = 0;
};

//---------------------------------------------------------------------------
#endif
