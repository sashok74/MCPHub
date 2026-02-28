//---------------------------------------------------------------------------
// DbProviderBase.h — Base class for database providers
//
// Provides common implementation for:
// - Connection management (worker connections)
// - DataSet to JSON conversion
// - Query execution with error handling
// - JSON response formatting
//
// Derived classes (MSSQLProvider, PostgreSQLProvider, etc.) implement
// DB-specific SQL queries for metadata operations.
//---------------------------------------------------------------------------

#ifndef DbProviderBaseH
#define DbProviderBaseH
//---------------------------------------------------------------------------
#include "IVclDbProvider.h"
#include <System.SyncObjs.hpp>
#include <Data.DB.hpp>
#include <FireDAC.Comp.Client.hpp>
#include <FireDAC.Stan.Param.hpp>
#include <functional>

// Explicit namespace qualification to avoid ambiguity
using Firedac::Comp::Client::TFDConnection;
using Firedac::Comp::Client::TFDQuery;
using Data::Db::TDataSet;
using Data::Db::TField;
//---------------------------------------------------------------------------

/// <summary>
/// Base implementation of IVclDbProvider with common query execution logic.
/// Derived classes override metadata query methods with DB-specific SQL.
/// </summary>
class TDbProviderBase : public IVclDbProvider
{
private:
	TFDConnection *FMainConnection;      // Main connection (owned externally)

protected:
	TCriticalSection *FConnParamsLock;   // Lock for connection params
	TStringList *FConnParams;            // Cached connection params

	//-----------------------------------------------------------------------
	// Connection management
	//-----------------------------------------------------------------------

	/// <summary>Get main connection (read-only access)</summary>
	TFDConnection* GetMainConnection() const { return FMainConnection; }

	/// <summary>
	/// Create worker connection (copy of main connection).
	/// Caller owns returned connection and must delete it.
	/// </summary>
	TFDConnection* CreateWorkerConnection();

	/// <summary>Update cached connection params from main connection</summary>
	void UpdateConnectionParams();

	//-----------------------------------------------------------------------
	// Query execution helpers
	//-----------------------------------------------------------------------

	/// <summary>
	/// Execute query and return JSON result.
	/// sql: SQL query (SELECT/INSERT/UPDATE/DELETE)
	/// params: optional parameter list ("name=value" pairs)
	/// Returns: JSON string with rows/rowCount or rowsAffected
	/// </summary>
	String RunQueryToJSON(const String &sql, TStringList *params = NULL);

	/// <summary>
	/// Execute query with single named parameter and return JSON.
	/// Convenience wrapper for metadata queries.
	/// </summary>
	String RunQueryWithParam(const String &sql, const String &paramName,
		const String &paramValue);

	//-----------------------------------------------------------------------
	// Data conversion
	//-----------------------------------------------------------------------

	/// <summary>
	/// Convert TDataSet to TJSONArray of TJSONObjects.
	/// Handles NULL values and type conversions.
	/// rowCount: output parameter with row count
	/// Returns: TJSONArray (caller takes ownership)
	/// </summary>
	TJSONArray* DataSetToJSON(TDataSet *ds, int &rowCount);

	//-----------------------------------------------------------------------
	// Error handling
	//-----------------------------------------------------------------------

	/// <summary>Create JSON error response: {"error": "message"}</summary>
	String MakeErrorJSON(const String &message);

	/// <summary>Escape string for JSON</summary>
	String EscapeJSON(const String &str);

	//-----------------------------------------------------------------------
	// Utilities
	//-----------------------------------------------------------------------

	/// <summary>Extract schema and table from "schema.table" or "table"</summary>
	void ParseTableName(const String &fullName, String &schema, String &table);

	/// <summary>Get default schema name for this DB type</summary>
	virtual String GetDefaultSchema() const { return "dbo"; }

	/// <summary>
	/// Apply row limit to SELECT query using DB-specific syntax.
	/// Override in derived classes for TOP (MSSQL), LIMIT (PG), FIRST (FB).
	/// Returns modified SQL or original if not a SELECT or already has limit.
	/// </summary>
	virtual String ApplyRowLimit(const String &sql, int maxRows);

public:
	TDbProviderBase(TFDConnection *mainConnection);
	virtual ~TDbProviderBase();

	//-----------------------------------------------------------------------
	// IVclDbProvider implementation — ExecuteQuery is common for all DBs
	//-----------------------------------------------------------------------

	/// <summary>Execute arbitrary SQL (uses RunQueryToJSON)</summary>
	virtual String ExecuteQuery(const String &sql) override;

	/// <summary>Execute SQL with optional row limit (DB-specific syntax)</summary>
	virtual String ExecuteQuery(const String &sql, int maxRows) override;

	/// <summary>Execute parameterized SQL query (public wrapper for RunQueryToJSON)</summary>
	String ExecuteParameterizedQuery(const String &sql, TStringList *params);

	/// <summary>Get table sample with LIMIT/TOP (DB-specific in derived)</summary>
	virtual String GetTableSample(const String &table, int limit = 100) override = 0;

	//-----------------------------------------------------------------------
	// All metadata methods are pure virtual — must be implemented by derived
	//-----------------------------------------------------------------------

	// Provider info
	virtual String GetProviderName() const override = 0;

	// Metadata
	virtual String ListTables(const String &schemaFilter = "", bool includeViews = false) override = 0;
	virtual String GetTableSchema(const String &table) override = 0;
	virtual String GetTableRelations(const String &table) override = 0;
	virtual String ListDatabases() override = 0;
	virtual String ListObjects(const String &objType, const String &schemaFilter = "",
		const String &namePattern = "%") override = 0;
	virtual String GetObjectDefinition(const String &objName, const String &objType) override = 0;
	virtual String GetDependencies(const String &objName, const String &direction = "BOTH") override = 0;

	// Search & analysis
	virtual String SearchColumns(const String &pattern) override = 0;
	virtual String SearchObjectSource(const String &pattern) override = 0;
	virtual String ProfileColumn(const String &table, const String &column) override = 0;
	virtual String ExplainQuery(const String &sql) override = 0;

	// Advanced
	virtual String CompareTables(const String &table1, const String &table2) override = 0;
	virtual String TraceFkPath(const String &fromTable, const String &toTable, int maxDepth = 5) override = 0;
	virtual String GetModuleOverview(const String &schemaName) override = 0;
};

//---------------------------------------------------------------------------
#endif
