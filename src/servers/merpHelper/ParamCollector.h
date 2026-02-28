//---------------------------------------------------------------------------
#pragma once

#include "QueryDataTypes.h"
#include <vector>
#include <string>

class ParamCollector {
public:
	// Основной метод: raw records -> ParamInfo vectors
	static void CollectFromStorage(
		const std::vector<ParamRecord>& params,
		const std::vector<FieldRecord>& fields,
		std::vector<ParamInfo>& outInParams,
		std::vector<ParamInfo>& outOutParams
	);

	// Сборка полного SqlInfo
	static SqlInfo BuildSqlInfo(
		const QueryRecord& query,
		const std::vector<ParamRecord>& params,
		const std::vector<FieldRecord>& fields
	);

	// Утилиты
	static std::string QueryNameToMethodName(const std::string& queryName);
	static std::string GetResetValueForType(const std::string& cppType,
											const std::string& defaultValue);

private:
	// Маппинг одной записи -> ParamInfo (через DataTypeMapper)
	static ParamInfo MapParamRecord(const ParamRecord& rec);
	static ParamInfo MapFieldRecord(const FieldRecord& rec);

	// Автодобавление ID если отсутствует
	static void EnsureIdField(std::vector<ParamInfo>& params);
};
//---------------------------------------------------------------------------
