//---------------------------------------------------------------------------
#pragma once

#include "IQueryStorage.h"
#include <FireDAC.Comp.Client.hpp>

class MsSqlQueryStorage : public IQueryStorage {
public:
	explicit MsSqlQueryStorage(TFDConnection* connection);

	std::vector<ModuleRecord> GetModules() override;
	std::vector<QueryRecord> GetQueriesByModule(int moduleID) override;
	std::vector<std::string> GetAllQueryNames() override;
	std::vector<QueryRecord> GetAllQueriesWithModules() override;
	QueryRecord GetQueryByName(const std::string& queryName) override;
	std::vector<ParamRecord> GetInputParams(int queryID) override;
	std::vector<FieldRecord> GetOutputFields(int queryID) override;
	std::vector<std::pair<std::string, std::string>> GetDistinctFieldNamesAndTypes() override;
	void SaveInputParams(int queryID, const std::vector<ParamRecord>& params) override;
	void SaveOutputFields(int queryID, const std::vector<FieldRecord>& fields) override;
	int SaveQuery(const QueryRecord& query) override;
	void DeleteQuery(int queryID) override;
	bool QueryExists(const std::string& queryName) override;

private:
	TFDConnection* m_conn;
};
//---------------------------------------------------------------------------
