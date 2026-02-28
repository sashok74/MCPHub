//---------------------------------------------------------------------------
// FirebirdProvider.h — Firebird database provider
//
// Implements IVclDbProvider for Firebird using:
// - System tables (RDB$RELATIONS, RDB$RELATION_FIELDS, RDB$DEPENDENCIES, etc.)
// - Firebird specific features
//---------------------------------------------------------------------------

#ifndef FirebirdProviderH
#define FirebirdProviderH
//---------------------------------------------------------------------------
#include "DbProviderBase.h"
//---------------------------------------------------------------------------

/// <summary>
/// Firebird provider implementation.
/// </summary>
class TFirebirdProvider : public TDbProviderBase
{
public:
	TFirebirdProvider(TFDConnection *mainConnection);
	virtual ~TFirebirdProvider();

	//-----------------------------------------------------------------------
	// Provider info
	//-----------------------------------------------------------------------
	virtual String GetProviderName() const override;
	virtual String GetDefaultSchema() const override;

protected:
	/// <summary>Firebird uses SELECT FIRST N</summary>
	virtual String ApplyRowLimit(const String &sql, int maxRows) override;

	//-----------------------------------------------------------------------
	// Metadata queries — Firebird specific SQL
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
