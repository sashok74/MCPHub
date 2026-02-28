//---------------------------------------------------------------------------
// PostgreSQLProvider.h — PostgreSQL database provider
//
// Implements IVclDbProvider for PostgreSQL using:
// - information_schema views
// - pg_catalog system tables
// - PostgreSQL specific features
//---------------------------------------------------------------------------

#ifndef PostgreSQLProviderH
#define PostgreSQLProviderH
//---------------------------------------------------------------------------
#include "DbProviderBase.h"
//---------------------------------------------------------------------------

/// <summary>
/// PostgreSQL provider implementation.
/// </summary>
class TPostgreSQLProvider : public TDbProviderBase
{
public:
	TPostgreSQLProvider(TFDConnection *mainConnection);
	virtual ~TPostgreSQLProvider();

	//-----------------------------------------------------------------------
	// Provider info
	//-----------------------------------------------------------------------
	virtual String GetProviderName() const override;
	virtual String GetDefaultSchema() const override;

protected:
	/// <summary>PostgreSQL uses LIMIT N at end of query</summary>
	virtual String ApplyRowLimit(const String &sql, int maxRows) override;

	//-----------------------------------------------------------------------
	// Metadata queries — PostgreSQL specific SQL
	//-----------------------------------------------------------------------
	virtual String ListTables(const String &schemaFilter = "", bool includeViews = false) override;
	virtual String GetTableSchema(const String &table) override;
	virtual String GetTableRelations(const String &table) override;
	virtual String ListDatabases() override;
	virtual String ListObjects(const String &objType, const String &schemaFilter = "",
		const String &namePattern = "%") override;
	virtual String GetObjectDefinition(const String &objName, const String &objType) override;
	virtual String GetDependencies(const String &objName, const String &direction = "BOTH") override;

	//-----------------------------------------------------------------------
	// Search & analysis
	//-----------------------------------------------------------------------
	virtual String SearchColumns(const String &pattern) override;
	virtual String SearchObjectSource(const String &pattern) override;
	virtual String ProfileColumn(const String &table, const String &column) override;
	virtual String ExplainQuery(const String &sql) override;

	//-----------------------------------------------------------------------
	// Query operations
	//-----------------------------------------------------------------------
	virtual String GetTableSample(const String &table, int limit = 100) override;

	//-----------------------------------------------------------------------
	// Advanced operations
	//-----------------------------------------------------------------------
	virtual String CompareTables(const String &table1, const String &table2) override;
	virtual String TraceFkPath(const String &fromTable, const String &toTable, int maxDepth = 5) override;
	virtual String GetModuleOverview(const String &schemaName) override;
};

//---------------------------------------------------------------------------
#endif
