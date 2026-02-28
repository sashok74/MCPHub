//---------------------------------------------------------------------------
#pragma once

#include "QueryDataTypes.h"
#include <vector>
#include <string>
#include <utility>

class IQueryStorage {
public:
	virtual ~IQueryStorage() = default;

	// Модули
	virtual std::vector<ModuleRecord> GetModules() = 0;

	// Запросы
	virtual std::vector<QueryRecord> GetQueriesByModule(int moduleID) = 0;
	virtual std::vector<std::string> GetAllQueryNames() = 0;
	virtual std::vector<QueryRecord> GetAllQueriesWithModules() = 0;
	virtual QueryRecord GetQueryByName(const std::string& queryName) = 0;

	// Параметры
	virtual std::vector<ParamRecord> GetInputParams(int queryID) = 0;
	virtual std::vector<FieldRecord> GetOutputFields(int queryID) = 0;

	// Для AssignCommonFields
	virtual std::vector<std::pair<std::string, std::string>> GetDistinctFieldNamesAndTypes() = 0;

	// Сохранение (для синхронизации из UI)
	virtual void SaveInputParams(int queryID, const std::vector<ParamRecord>& params) = 0;
	virtual void SaveOutputFields(int queryID, const std::vector<FieldRecord>& fields) = 0;

	// Сохранение/удаление запросов
	virtual int SaveQuery(const QueryRecord& query) = 0;  // INSERT(queryID==0) или UPDATE, возвращает queryID
	virtual void DeleteQuery(int queryID) = 0;
	virtual bool QueryExists(const std::string& queryName) = 0;
};
//---------------------------------------------------------------------------
