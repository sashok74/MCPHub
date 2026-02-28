//---------------------------------------------------------------------------
// MSSQLProvider.h — Microsoft SQL Server database provider
//
// Implements IVclDbProvider for MS SQL Server using:
// - System catalog views (sys.tables, sys.columns, sys.schemas, etc.)
// - INFORMATION_SCHEMA views
// - T-SQL specific features (SET SHOWPLAN_XML, sys.dm_exec_query_plan, etc.)
//---------------------------------------------------------------------------

#ifndef MSSQLProviderH
#define MSSQLProviderH
//---------------------------------------------------------------------------
#include "DbProviderBase.h"
//---------------------------------------------------------------------------

/// <summary>
/// Microsoft SQL Server provider implementation.
/// </summary>
class TMSSQLProvider : public TDbProviderBase
{
private:
	/// <summary>Build column schema query for MSSQL</summary>
	String BuildColumnSchemaSQL();

	/// <summary>Build foreign key relations query for MSSQL</summary>
	String BuildRelationsSQL();

	/// <summary>Get XML execution plan via raw ODBC (bypasses FireDAC)</summary>
	String GetShowplanXml(const String &sql);

public:
	TMSSQLProvider(TFDConnection *mainConnection);
	virtual ~TMSSQLProvider();

	//-----------------------------------------------------------------------
	// Provider info
	//-----------------------------------------------------------------------
	virtual String GetProviderName() const override;
	virtual String GetDefaultSchema() const override;

	//-----------------------------------------------------------------------
	// Metadata queries — MSSQL specific SQL
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
