//---------------------------------------------------------------------------
// VclDbProviderAdapter.h — Adapter from IVclDbProvider to IDbProvider (UTF-8)
//---------------------------------------------------------------------------

#ifndef VclDbProviderAdapterH
#define VclDbProviderAdapterH
//---------------------------------------------------------------------------
#include "IDbProvider.h"
#include "IVclDbProvider.h"
//---------------------------------------------------------------------------

class TVclDbProviderAdapter : public IDbProvider
{
public:
	explicit TVclDbProviderAdapter(IVclDbProvider *provider);

	void SetProvider(IVclDbProvider *provider);
	IVclDbProvider* GetProvider() const;

	bool IsAvailable() const override;
	std::string GetProviderName() const override;

	std::string ListTables(const std::string &schemaFilter = "",
		bool includeViews = false) override;
	std::string GetTableSchema(const std::string &table) override;
	std::string GetTableRelations(const std::string &table) override;
	std::string ListDatabases() override;
	std::string ListObjects(const std::string &objType,
		const std::string &schemaFilter = "",
		const std::string &namePattern = "%") override;
	std::string GetObjectDefinition(const std::string &objName,
		const std::string &objType) override;
	std::string GetDependencies(const std::string &objName,
		const std::string &direction = "BOTH") override;
	std::string SearchColumns(const std::string &pattern) override;
	std::string SearchObjectSource(const std::string &pattern) override;
	std::string ProfileColumn(const std::string &table,
		const std::string &column) override;
	std::string ExecuteQuery(const std::string &sql) override;
	std::string ExecuteQuery(const std::string &sql, int maxRows) override;
	std::string GetTableSample(const std::string &table, int limit = 100) override;
	std::string ExplainQuery(const std::string &sql) override;
	std::string CompareTables(const std::string &table1,
		const std::string &table2) override;
	std::string TraceFkPath(const std::string &fromTable,
		const std::string &toTable, int maxDepth = 5) override;
	std::string GetModuleOverview(const std::string &schemaName) override;

private:
	IVclDbProvider *FProvider;

	std::string MakeErrorJson(const std::string &message) const;
};

//---------------------------------------------------------------------------
#endif
