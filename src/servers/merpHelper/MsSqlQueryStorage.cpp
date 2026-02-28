//---------------------------------------------------------------------------
#include "MsSqlQueryStorage.h"
#include "UcodeUtf8.h"
#include <memory>

//---------------------------------------------------------------------------
MsSqlQueryStorage::MsSqlQueryStorage(TFDConnection* connection)
	: m_conn(connection)
{
}
//---------------------------------------------------------------------------

// Вспомогательная функция: безопасное чтение строкового поля
static std::string readStr(TFDQuery* q, const char* fieldName)
{
	TField* f = q->FieldByName(fieldName);
	if (f->IsNull)
		return "";
	return utf8(f->AsString);
}

// Вспомогательная функция: безопасное чтение int поля
static int readInt(TFDQuery* q, const char* fieldName)
{
	TField* f = q->FieldByName(fieldName);
	if (f->IsNull)
		return 0;
	return f->AsInteger;
}

// Вспомогательная функция: безопасное чтение bool поля
static bool readBool(TFDQuery* q, const char* fieldName)
{
	TField* f = q->FieldByName(fieldName);
	if (f->IsNull)
		return false;
	return f->AsBoolean;
}

//---------------------------------------------------------------------------
std::vector<ModuleRecord> MsSqlQueryStorage::GetModules()
{
	std::vector<ModuleRecord> result;
	auto q = std::make_unique<TFDQuery>(nullptr);
	q->Connection = m_conn;
	q->SQL->Text =
		"SELECT IntVal AS ModuleID, StrVal AS SQLModuleName "
		"FROM Enumeration WHERE EntityName = 'SQLModule' ORDER BY StrVal";
	q->Open();

	for (q->First(); !q->Eof; q->Next()) {
		ModuleRecord rec;
		rec.moduleID = q->FieldByName("ModuleID")->AsInteger;
		rec.moduleName = utf8(q->FieldByName("SQLModuleName")->AsString);
		result.push_back(rec);
	}
	return result;
}
//---------------------------------------------------------------------------

std::vector<QueryRecord> MsSqlQueryStorage::GetQueriesByModule(int moduleID)
{
	std::vector<QueryRecord> result;
	auto q = std::make_unique<TFDQuery>(nullptr);
	q->Connection = m_conn;
	q->SQL->Text =
		"SELECT q.QueryID, q.QueryName, q.QueryType, q.Description, "
		"q.SourceSQL, q.PG_SQL, q.ModuleID, q.TableName, "
		"q.DS_Name, q.DS_Query_I, q.DS_Query_U, q.DS_Query_D, q.DS_Query_One_S "
		"FROM dbo.SqlQueries q "
		"WHERE q.ModuleID = :ModuleID AND q.IsActive = 1 "
		"ORDER BY q.QueryName";
	q->ParamByName("ModuleID")->AsInteger = moduleID;
	q->Open();

	for (q->First(); !q->Eof; q->Next()) {
		QueryRecord rec;
		rec.queryID    = readInt(q.get(), "QueryID");
		rec.queryName  = readStr(q.get(), "QueryName");
		rec.queryType  = readStr(q.get(), "QueryType");
		rec.description = readStr(q.get(), "Description");
		rec.sourceSQL  = readStr(q.get(), "SourceSQL");
		rec.pgSQL      = readStr(q.get(), "PG_SQL");
		rec.moduleID   = readInt(q.get(), "ModuleID");
		rec.tableName  = readStr(q.get(), "TableName");
		rec.dsName     = readStr(q.get(), "DS_Name");
		rec.dsQueryI   = readStr(q.get(), "DS_Query_I");
		rec.dsQueryU   = readStr(q.get(), "DS_Query_U");
		rec.dsQueryD   = readStr(q.get(), "DS_Query_D");
		rec.dsQueryOneS = readStr(q.get(), "DS_Query_One_S");
		result.push_back(rec);
	}
	return result;
}
//---------------------------------------------------------------------------

std::vector<std::string> MsSqlQueryStorage::GetAllQueryNames()
{
	std::vector<std::string> result;
	auto q = std::make_unique<TFDQuery>(nullptr);
	q->Connection = m_conn;
	q->SQL->Text =
		"SELECT q.QueryName FROM SqlQueries q "
		"WHERE q.IsActive = 1 ORDER BY q.QueryName";
	q->Open();

	for (q->First(); !q->Eof; q->Next()) {
		result.push_back(utf8(q->FieldByName("QueryName")->AsString));
	}
	return result;
}
//---------------------------------------------------------------------------

std::vector<QueryRecord> MsSqlQueryStorage::GetAllQueriesWithModules()
{
	std::vector<QueryRecord> result;
	auto q = std::make_unique<TFDQuery>(nullptr);
	q->Connection = m_conn;
	q->SQL->Text =
		"SELECT e.StrVal AS Module, q.QueryID, q.QueryName, q.Description, "
		"q.QueryType, q.SourceSQL, "
		"COALESCE(NULLIF(q.PG_SQL, ''), q.SourceSQL) AS PG_SQL, "
		"q.ModuleID, q.TableName "
		"FROM dbo.SqlQueries q "
		"JOIN dbo.Enumeration e ON e.IntVal = q.ModuleID AND e.EntityName = 'SQLModule' "
		"ORDER BY e.StrVal, q.QueryName";
	q->Open();

	for (q->First(); !q->Eof; q->Next()) {
		QueryRecord rec;
		rec.queryID    = readInt(q.get(), "QueryID");
		rec.queryName  = readStr(q.get(), "QueryName");
		rec.queryType  = readStr(q.get(), "QueryType");
		rec.description = readStr(q.get(), "Description");
		rec.sourceSQL  = readStr(q.get(), "SourceSQL");
		rec.pgSQL      = readStr(q.get(), "PG_SQL");
		rec.moduleID   = readInt(q.get(), "ModuleID");
		rec.moduleName = readStr(q.get(), "Module");
		rec.tableName  = readStr(q.get(), "TableName");
		result.push_back(rec);
	}
	return result;
}
//---------------------------------------------------------------------------

QueryRecord MsSqlQueryStorage::GetQueryByName(const std::string& queryName)
{
	QueryRecord rec;
	auto q = std::make_unique<TFDQuery>(nullptr);
	q->Connection = m_conn;
	q->SQL->Text =
		"SELECT q.QueryID, q.QueryName, q.QueryType, q.Description, "
		"q.SourceSQL, q.PG_SQL, q.ModuleID, q.TableName, "
		"q.DS_Name, q.DS_Query_I, q.DS_Query_U, q.DS_Query_D, q.DS_Query_One_S, "
		"e.StrVal AS ModuleName "
		"FROM dbo.SqlQueries q "
		"LEFT JOIN dbo.Enumeration e ON e.IntVal = q.ModuleID AND e.EntityName = 'SQLModule' "
		"WHERE q.QueryName = :QueryName";
	q->ParamByName("QueryName")->AsString = u(queryName);
	q->Open();

	if (!q->Eof) {
		rec.queryID    = readInt(q.get(), "QueryID");
		rec.queryName  = readStr(q.get(), "QueryName");
		rec.queryType  = readStr(q.get(), "QueryType");
		rec.description = readStr(q.get(), "Description");
		rec.sourceSQL  = readStr(q.get(), "SourceSQL");
		rec.pgSQL      = readStr(q.get(), "PG_SQL");
		rec.moduleID   = readInt(q.get(), "ModuleID");
		rec.tableName  = readStr(q.get(), "TableName");
		rec.dsName     = readStr(q.get(), "DS_Name");
		rec.dsQueryI   = readStr(q.get(), "DS_Query_I");
		rec.dsQueryU   = readStr(q.get(), "DS_Query_U");
		rec.dsQueryD   = readStr(q.get(), "DS_Query_D");
		rec.dsQueryOneS = readStr(q.get(), "DS_Query_One_S");
		rec.moduleName = readStr(q.get(), "ModuleName");
	}
	return rec;
}
//---------------------------------------------------------------------------

std::vector<ParamRecord> MsSqlQueryStorage::GetInputParams(int queryID)
{
	std::vector<ParamRecord> result;
	auto q = std::make_unique<TFDQuery>(nullptr);
	q->Connection = m_conn;
	q->SQL->Text =
		"SELECT * FROM dbo.SqlQueryParams "
		"WHERE QueryID = :QueryID ORDER BY ParamOrder";
	q->ParamByName("QueryID")->AsInteger = queryID;
	q->Open();

	for (q->First(); !q->Eof; q->Next()) {
		ParamRecord rec;
		rec.paramID    = readInt(q.get(), "ParamID");
		rec.queryID    = readInt(q.get(), "QueryID");
		rec.paramName  = readStr(q.get(), "ParamName");
		rec.paramType  = readStr(q.get(), "ParamType");
		rec.isNullable = readBool(q.get(), "IsNullable");
		rec.defaultValue = readStr(q.get(), "DefaultValue");
		rec.paramOrder = readInt(q.get(), "ParamOrder");
		rec.maxLength  = readInt(q.get(), "MaxLength");
		rec.precision  = readInt(q.get(), "NPrecision");
		rec.nullValue  = readStr(q.get(), "NullValue");
		rec.isKeyField = readBool(q.get(), "IsKeyField");
		result.push_back(rec);
	}
	return result;
}
//---------------------------------------------------------------------------

std::vector<FieldRecord> MsSqlQueryStorage::GetOutputFields(int queryID)
{
	std::vector<FieldRecord> result;
	auto q = std::make_unique<TFDQuery>(nullptr);
	q->Connection = m_conn;
	q->SQL->Text =
		"SELECT * FROM dbo.SqlQueryFields "
		"WHERE QueryID = :QueryID ORDER BY FieldOrder";
	q->ParamByName("QueryID")->AsInteger = queryID;
	q->Open();

	for (q->First(); !q->Eof; q->Next()) {
		FieldRecord rec;
		rec.fieldID    = readInt(q.get(), "FieldID");
		rec.queryID    = readInt(q.get(), "QueryID");
		rec.fieldName  = readStr(q.get(), "FieldName");
		rec.fieldType  = readStr(q.get(), "FieldType");
		rec.treeIdent  = readStr(q.get(), "TreeIdent");
		rec.isNullable = readBool(q.get(), "IsNullable");
		rec.displayZero = (readInt(q.get(), "DisplayZero") == 1);
		rec.fieldOrder = readInt(q.get(), "FieldOrder");
		rec.maxLength  = readInt(q.get(), "MaxLength");
		rec.precision  = readInt(q.get(), "NPrecision");
		rec.isKeyField = readBool(q.get(), "IsKeyField");
		rec.defaultValue = readStr(q.get(), "DefaultValue");
		result.push_back(rec);
	}
	return result;
}
//---------------------------------------------------------------------------

std::vector<std::pair<std::string, std::string>> MsSqlQueryStorage::GetDistinctFieldNamesAndTypes()
{
	std::vector<std::pair<std::string, std::string>> result;
	auto q = std::make_unique<TFDQuery>(nullptr);
	q->Connection = m_conn;
	q->SQL->Text =
		"SELECT DISTINCT RTRIM(LTRIM(FieldName)) AS FieldName, FieldType "
		"FROM dbo.SqlQueryFields";
	q->Open();

	for (q->First(); !q->Eof; q->Next()) {
		result.emplace_back(
			utf8(q->FieldByName("FieldName")->AsString),
			utf8(q->FieldByName("FieldType")->AsString)
		);
	}
	return result;
}
//---------------------------------------------------------------------------

void MsSqlQueryStorage::SaveInputParams(int queryID, const std::vector<ParamRecord>& params)
{
	auto q = std::make_unique<TFDQuery>(nullptr);
	q->Connection = m_conn;

	m_conn->StartTransaction();
	try {
		// Удаляем старые записи
		q->SQL->Text = "DELETE FROM dbo.SqlQueryParams WHERE QueryID = :QueryID";
		q->ParamByName("QueryID")->AsInteger = queryID;
		q->ExecSQL();

		// Вставляем новые
		q->SQL->Text =
			"INSERT INTO dbo.SqlQueryParams "
			"(QueryID, ParamName, ParamType, Direction, IsNullable, DefaultValue, "
			"ParamOrder, MaxLength, NPrecision, Scale, NullValue, IsKeyField) "
			"VALUES (:QueryID, :ParamName, :ParamType, :Direction, :IsNullable, :DefaultValue, "
			":ParamOrder, :MaxLength, :NPrecision, :Scale, :NullValue, :IsKeyField)";

		int order = 0;
		for (const auto& p : params) {
			q->ParamByName("QueryID")->AsInteger = queryID;
			q->ParamByName("ParamName")->AsString = u(p.paramName);
			q->ParamByName("ParamType")->AsString = u(p.paramType);
			q->ParamByName("Direction")->AsString = u(p.direction.empty() ? "IN" : p.direction);
			q->ParamByName("IsNullable")->AsBoolean = p.isNullable;
			q->ParamByName("DefaultValue")->AsString = u(p.defaultValue);
			q->ParamByName("ParamOrder")->AsInteger = order++;
			q->ParamByName("MaxLength")->AsInteger = p.maxLength;
			q->ParamByName("NPrecision")->AsInteger = p.precision;
			q->ParamByName("Scale")->AsInteger = p.scale;
			q->ParamByName("NullValue")->AsString = u(p.nullValue);
			q->ParamByName("IsKeyField")->AsBoolean = p.isKeyField;
			q->ExecSQL();
		}
		m_conn->Commit();
	}
	catch (...) {
		m_conn->Rollback();
		throw;
	}
}
//---------------------------------------------------------------------------

int MsSqlQueryStorage::SaveQuery(const QueryRecord& query)
{
	auto q = std::make_unique<TFDQuery>(nullptr);
	q->Connection = m_conn;

	if (query.queryID == 0) {
		// INSERT
		q->SQL->Text =
			"INSERT INTO dbo.SqlQueries "
			"(ModuleID, QueryName, QueryType, SourceSQL, PG_SQL, Description, "
			"IsActive, ModifiedDate, ResultClass, TableName, TestInfo, "
			"DS_Name, DS_Query_I, DS_Query_U, DS_Query_D, DS_Query_One_S) "
			"OUTPUT INSERTED.QueryID "
			"VALUES (:ModuleID, :QueryName, :QueryType, :SourceSQL, :PG_SQL, :Description, "
			":IsActive, GETUTCDATE(), :ResultClass, :TableName, :TestInfo, "
			":DS_Name, :DS_Query_I, :DS_Query_U, :DS_Query_D, :DS_Query_One_S)";
	} else {
		// UPDATE
		q->SQL->Text =
			"UPDATE dbo.SqlQueries SET "
			"ModuleID = :ModuleID, QueryName = :QueryName, QueryType = :QueryType, "
			"SourceSQL = :SourceSQL, PG_SQL = :PG_SQL, Description = :Description, "
			"IsActive = :IsActive, ModifiedDate = GETUTCDATE(), "
			"ResultClass = :ResultClass, TableName = :TableName, TestInfo = :TestInfo, "
			"DS_Name = :DS_Name, DS_Query_I = :DS_Query_I, DS_Query_U = :DS_Query_U, "
			"DS_Query_D = :DS_Query_D, DS_Query_One_S = :DS_Query_One_S "
			"WHERE QueryID = :QueryID";
		q->ParamByName("QueryID")->AsInteger = query.queryID;
	}

	q->ParamByName("ModuleID")->AsInteger = query.moduleID;
	q->ParamByName("QueryName")->AsString = u(query.queryName);
	q->ParamByName("QueryType")->AsString = u(query.queryType);
	q->ParamByName("SourceSQL")->AsString = u(query.sourceSQL);
	q->ParamByName("PG_SQL")->AsString = u(query.pgSQL);
	q->ParamByName("Description")->AsString = u(query.description);
	q->ParamByName("IsActive")->AsBoolean = query.isActive;
	q->ParamByName("ResultClass")->AsString = u(query.resultClass);
	q->ParamByName("TableName")->AsString = u(query.tableName);
	q->ParamByName("TestInfo")->AsString = u(query.testInfo);
	q->ParamByName("DS_Name")->AsString = u(query.dsName);
	q->ParamByName("DS_Query_I")->AsString = u(query.dsQueryI);
	q->ParamByName("DS_Query_U")->AsString = u(query.dsQueryU);
	q->ParamByName("DS_Query_D")->AsString = u(query.dsQueryD);
	q->ParamByName("DS_Query_One_S")->AsString = u(query.dsQueryOneS);

	if (query.queryID == 0) {
		q->Open();
		int newID = q->Fields->Fields[0]->AsInteger;
		return newID;
	} else {
		q->ExecSQL();
		return query.queryID;
	}
}
//---------------------------------------------------------------------------

void MsSqlQueryStorage::DeleteQuery(int queryID)
{
	auto q = std::make_unique<TFDQuery>(nullptr);
	q->Connection = m_conn;

	m_conn->StartTransaction();
	try {
		q->SQL->Text = "DELETE FROM dbo.SqlQueryParams WHERE QueryID = :QueryID";
		q->ParamByName("QueryID")->AsInteger = queryID;
		q->ExecSQL();

		q->SQL->Text = "DELETE FROM dbo.SqlQueryFields WHERE QueryID = :QueryID";
		q->ParamByName("QueryID")->AsInteger = queryID;
		q->ExecSQL();

		q->SQL->Text = "DELETE FROM dbo.SqlQueries WHERE QueryID = :QueryID";
		q->ParamByName("QueryID")->AsInteger = queryID;
		q->ExecSQL();

		m_conn->Commit();
	}
	catch (...) {
		m_conn->Rollback();
		throw;
	}
}
//---------------------------------------------------------------------------

bool MsSqlQueryStorage::QueryExists(const std::string& queryName)
{
	auto q = std::make_unique<TFDQuery>(nullptr);
	q->Connection = m_conn;
	q->SQL->Text =
		"SELECT COUNT(*) AS cnt FROM dbo.SqlQueries WHERE QueryName = :QueryName";
	q->ParamByName("QueryName")->AsString = u(queryName);
	q->Open();
	return q->FieldByName("cnt")->AsInteger > 0;
}
//---------------------------------------------------------------------------

void MsSqlQueryStorage::SaveOutputFields(int queryID, const std::vector<FieldRecord>& fields)
{
	auto q = std::make_unique<TFDQuery>(nullptr);
	q->Connection = m_conn;

	m_conn->StartTransaction();
	try {
		// Удаляем старые записи
		q->SQL->Text = "DELETE FROM dbo.SqlQueryFields WHERE QueryID = :QueryID";
		q->ParamByName("QueryID")->AsInteger = queryID;
		q->ExecSQL();

		// Вставляем новые
		q->SQL->Text =
			"INSERT INTO dbo.SqlQueryFields "
			"(QueryID, FieldName, FieldType, TreeIdent, IsNullable, DisplayZero, "
			"FieldOrder, MaxLength, NPrecision, Scale, IsKeyField, DefaultValue) "
			"VALUES (:QueryID, :FieldName, :FieldType, :TreeIdent, :IsNullable, :DisplayZero, "
			":FieldOrder, :MaxLength, :NPrecision, :Scale, :IsKeyField, :DefaultValue)";

		int order = 0;
		for (const auto& f : fields) {
			q->ParamByName("QueryID")->AsInteger = queryID;
			q->ParamByName("FieldName")->AsString = u(f.fieldName);
			q->ParamByName("FieldType")->AsString = u(f.fieldType);
			q->ParamByName("TreeIdent")->AsString = u(f.treeIdent);
			q->ParamByName("IsNullable")->AsBoolean = f.isNullable;
			q->ParamByName("DisplayZero")->AsInteger = f.displayZero ? 1 : 0;
			q->ParamByName("FieldOrder")->AsInteger = order++;
			q->ParamByName("MaxLength")->AsInteger = f.maxLength;
			q->ParamByName("NPrecision")->AsInteger = f.precision;
			q->ParamByName("Scale")->AsInteger = f.scale;
			q->ParamByName("IsKeyField")->AsBoolean = f.isKeyField;
			q->ParamByName("DefaultValue")->AsString = u(f.defaultValue);
			q->ExecSQL();
		}
		m_conn->Commit();
	}
	catch (...) {
		m_conn->Rollback();
		throw;
	}
}
//---------------------------------------------------------------------------
