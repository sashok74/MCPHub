//---------------------------------------------------------------------------
#include "ParamCollector.h"
#include "DataTypeMapper.h"
#include "UcodeUtf8.h"
#include <algorithm>
#include <sstream>

//---------------------------------------------------------------------------
ParamInfo ParamCollector::MapParamRecord(const ParamRecord& rec)
{
	ParamInfo p;
	p.CppType    = utf8(DataTypeMapper::getCppTypeBySqlType(u(rec.paramType)));
	p.sqlType    = rec.paramType;
	p.DbName     = rec.paramName;
	p.MemberName = utf8(DataTypeMapper::getCppPrefixByCppType(u(p.CppType))) + rec.paramName;
	p.IsKey      = rec.isKeyField;
	p.IsNullable = rec.isNullable;
	p.NullValue  = rec.nullValue;
	p.DefaultValue = GetResetValueForType(p.CppType, rec.defaultValue);
	p.MaxLength  = rec.maxLength;
	p.Precision  = rec.precision;
	p.Scale      = rec.scale;
	return p;
}
//---------------------------------------------------------------------------

ParamInfo ParamCollector::MapFieldRecord(const FieldRecord& rec)
{
	ParamInfo p;
	p.CppType    = utf8(DataTypeMapper::getCppTypeBySqlType(u(rec.fieldType)));
	p.sqlType    = rec.fieldType;
	p.DbName     = rec.fieldName;
	p.MemberName = utf8(DataTypeMapper::getCppPrefixByCppType(u(p.CppType))) + rec.fieldName;
	p.IsKey      = rec.isKeyField;
	p.IsTid      = (rec.treeIdent == "ID");
	p.IsPid      = (rec.treeIdent == "PID");
	p.IsNullable = rec.isNullable;
	p.DisplayZero = rec.displayZero;
	p.NullValue  = "";  // Нет NullValue для output fields
	p.DefaultValue = GetResetValueForType(p.CppType, rec.defaultValue);
	p.MaxLength  = rec.maxLength;
	p.Precision  = rec.precision;
	p.Scale      = rec.scale;
	return p;
}
//---------------------------------------------------------------------------

void ParamCollector::EnsureIdField(std::vector<ParamInfo>& params)
{
	// Проверяем наличие поля с DbName == "ID"
	for (const auto& p : params) {
		if (p.DbName == "ID")
			return;
	}

	// Добавляем ID если отсутствует
	ParamInfo idParam;
	idParam.CppType      = "int";
	idParam.sqlType      = "int";
	idParam.MemberName   = "m_iID";
	idParam.DbName       = "ID";
	idParam.IsKey        = false;
	idParam.IsNullable   = false;
	idParam.DisplayZero  = false;
	idParam.NullValue    = "";
	idParam.DefaultValue = "-1";
	idParam.MaxLength    = 0;
	idParam.Precision    = 0;
	idParam.Scale        = 0;
	params.insert(params.begin(), idParam);
}
//---------------------------------------------------------------------------

void ParamCollector::CollectFromStorage(
	const std::vector<ParamRecord>& params,
	const std::vector<FieldRecord>& fields,
	std::vector<ParamInfo>& outInParams,
	std::vector<ParamInfo>& outOutParams)
{
	outInParams.clear();
	outOutParams.clear();

	// Маппинг input параметров
	for (const auto& rec : params) {
		outInParams.push_back(MapParamRecord(rec));
	}
	EnsureIdField(outInParams);

	// Маппинг output полей
	for (const auto& rec : fields) {
		outOutParams.push_back(MapFieldRecord(rec));
	}
	EnsureIdField(outOutParams);
}
//---------------------------------------------------------------------------

SqlInfo ParamCollector::BuildSqlInfo(
	const QueryRecord& query,
	const std::vector<ParamRecord>& params,
	const std::vector<FieldRecord>& fields)
{
	SqlInfo s;
	s.queryName   = query.queryName;
	s.Description = query.description;
	s.QueryType   = query.queryType;
	s.methodName  = QueryNameToMethodName(query.queryName);
	s.TypeName    = "T" + s.methodName;
	s.inType      = s.TypeName + "::in inPrm";
	s.outType     = s.TypeName + "::out outPrm";
	s.MS_SQL      = query.sourceSQL;
	s.PG_SQL      = query.pgSQL;
	s.QueryID     = query.queryID;

	CollectFromStorage(params, fields, s.inParams, s.outParams);
	return s;
}
//---------------------------------------------------------------------------

std::string ParamCollector::QueryNameToMethodName(const std::string& queryName)
{
	std::string name = queryName;
	std::string suffix;

	// Определяем суффикс и убираем его из имени
	if (name.size() >= 2) {
		std::string lastTwo = name.substr(name.size() - 2);
		// Приводим к lowercase для сравнения
		std::string lastTwoLower = lastTwo;
		std::transform(lastTwoLower.begin(), lastTwoLower.end(),
					   lastTwoLower.begin(), ::tolower);

		if (lastTwoLower == "_s") {
			suffix = "S";
			name = name.substr(0, name.size() - 2);
		} else if (lastTwoLower == "_i") {
			suffix = "I";
			name = name.substr(0, name.size() - 2);
		} else if (lastTwoLower == "_u") {
			suffix = "U";
			name = name.substr(0, name.size() - 2);
		} else if (lastTwoLower == "_d") {
			suffix = "D";
			name = name.substr(0, name.size() - 2);
		}
	}

	// Разбиваем по '_' и преобразуем в CamelCase
	std::string result;
	std::istringstream stream(name);
	std::string part;

	while (std::getline(stream, part, '_')) {
		if (!part.empty()) {
			// Первая буква — uppercase, остальные — lowercase
			part[0] = static_cast<char>(::toupper(static_cast<unsigned char>(part[0])));
			for (size_t i = 1; i < part.size(); ++i) {
				part[i] = static_cast<char>(::tolower(static_cast<unsigned char>(part[i])));
			}
			result += part;
		}
	}

	result += suffix;
	return result;
}
//---------------------------------------------------------------------------

std::string ParamCollector::GetResetValueForType(
	const std::string& cppType, const std::string& defaultValue)
{
	if (!defaultValue.empty())
		return defaultValue;

	return utf8(DataTypeMapper::getDefaultValueByCppType(u(cppType)));
}
//---------------------------------------------------------------------------
