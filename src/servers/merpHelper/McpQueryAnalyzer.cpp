//---------------------------------------------------------------------------
#pragma hdrstop
#include "McpQueryAnalyzer.h"
#include "DataTypeMapper.h"
#include "UcodeUtf8.h"
#include "sqlHelper.h"
#include <regex>
#include <unordered_map>
#include <algorithm>
//---------------------------------------------------------------------------
#pragma package(smart_init)

McpQueryAnalyzer::McpQueryAnalyzer(TFDConnection* conn)
	: FConn(conn)
{
}

// ---------------------------------------------------------------------------
// CorrectSqlSyntax — :param -> @param, duplicates get _dubN suffix
// ---------------------------------------------------------------------------
std::string McpQueryAnalyzer::CorrectSqlSyntax(const std::string& sql)
{
	std::unordered_map<std::string, int> paramCount;
	std::string result;
	size_t lastPos = 0;
	static const std::regex paramRegex(R"(\:([a-zA-Z_][a-zA-Z0-9_]*))", std::regex::icase);

	auto begin = std::sregex_iterator(sql.begin(), sql.end(), paramRegex);
	auto end = std::sregex_iterator();
	for (auto it = begin; it != end; ++it) {
		auto match = *it;
		size_t matchPos = match.position();
		result.append(sql.substr(lastPos, matchPos - lastPos));
		std::string paramName = match.str(1);
		int count = ++paramCount[paramName];

		std::string uniqueParam = "@" + paramName;
		if (count > 1)
			uniqueParam += "_dub" + std::to_string(count - 1);
		result.append(uniqueParam);
		lastPos = matchPos + match.length();
	}
	result.append(sql.substr(lastPos));
	return result;
}

// ---------------------------------------------------------------------------
std::string McpQueryAnalyzer::EscapeSql(const std::string& sql)
{
	std::string result;
	result.reserve(sql.size() + 16);
	for (char c : sql) {
		if (c == '\'') result += "''";
		else result += c;
	}
	return result;
}

// ---------------------------------------------------------------------------
std::pair<std::string, std::string> McpQueryAnalyzer::ExtractTableSchemaAndName(const std::string& sql)
{
	static const std::regex insertPattern(
		R"(INSERT\s+INTO\s+([a-zA-Z0-9_]+\.[a-zA-Z0-9_]+|[a-zA-Z0-9_]+))",
		std::regex::icase);
	static const std::regex updatePattern(
		R"(UPDATE\s+([a-zA-Z0-9_]+\.[a-zA-Z0-9_]+|[a-zA-Z0-9_]+))",
		std::regex::icase);
	std::smatch match;

	if (std::regex_search(sql, match, insertPattern) ||
		std::regex_search(sql, match, updatePattern))
	{
		std::string fullName = match[1].str();
		size_t dotPos = fullName.find('.');
		if (dotPos != std::string::npos)
			return { fullName.substr(0, dotPos), fullName.substr(dotPos + 1) };
		else
			return { "dbo", fullName };
	}
	return { "", "" };
}

// ---------------------------------------------------------------------------
bool McpQueryAnalyzer::GetIsNullable(const std::string& schema,
	const std::string& table, const std::string& column)
{
	if (table.empty() || column.empty())
		return false;

	auto q = std::make_unique<TFDQuery>(nullptr);
	q->Connection = FConn;
	q->SQL->Text =
		"SELECT IS_NULLABLE FROM INFORMATION_SCHEMA.COLUMNS "
		"WHERE TABLE_SCHEMA = :Schema AND TABLE_NAME = :TableName "
		"AND COLUMN_NAME = :ColumnName";
	q->ParamByName("Schema")->AsString = u(schema);
	q->ParamByName("TableName")->AsString = u(table);
	q->ParamByName("ColumnName")->AsString = u(column);
	q->Open();

	if (!q->Eof)
		return q->FieldByName("IS_NULLABLE")->AsString == "YES";
	return false;
}

// ---------------------------------------------------------------------------
bool McpQueryAnalyzer::GetIsKeyField(const std::string& schema,
	const std::string& table, const std::string& column)
{
	if (table.empty() || column.empty())
		return false;

	auto q = std::make_unique<TFDQuery>(nullptr);
	q->Connection = FConn;
	q->SQL->Text =
		"SELECT COUNT(*) as cnt FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS tc "
		"JOIN INFORMATION_SCHEMA.KEY_COLUMN_USAGE kcu "
		"ON tc.CONSTRAINT_NAME = kcu.CONSTRAINT_NAME "
		"WHERE tc.TABLE_SCHEMA = :Schema AND tc.TABLE_NAME = :TableName "
		"AND tc.CONSTRAINT_TYPE = 'PRIMARY KEY' "
		"AND kcu.COLUMN_NAME = :ColumnName";
	q->ParamByName("Schema")->AsString = u(schema);
	q->ParamByName("TableName")->AsString = u(table);
	q->ParamByName("ColumnName")->AsString = u(column);
	q->Open();

	return q->FieldByName("cnt")->AsInteger > 0;
}

// ---------------------------------------------------------------------------
// AnalyzeInputParams — sp_describe_undeclared_parameters
// ---------------------------------------------------------------------------
std::vector<ParamRecord> McpQueryAnalyzer::AnalyzeInputParams(const std::string& sql)
{
	std::string corrected = CorrectSqlSyntax(sql);
	auto [schema, tableName] = ExtractTableSchemaAndName(corrected);

	std::string safeSql = EscapeSql(corrected);

	auto q = std::make_unique<TFDQuery>(nullptr);
	q->Connection = FConn;
	q->SQL->Text = u("EXEC sp_describe_undeclared_parameters N'" + safeSql + "'");
	q->Open();

	std::vector<ParamRecord> result;
	while (!q->Eof) {
		ParamRecord rec;
		UnicodeString rawName = q->FieldByName("name")->AsString;
		// Strip leading @
		if (!rawName.IsEmpty() && rawName[1] == L'@')
			rawName = rawName.SubString(2, rawName.Length() - 1);
		std::string name = AnsiString(rawName).c_str();

		// Skip _dub duplicates
		if (name.find("_dub") != std::string::npos) {
			q->Next();
			continue;
		}

		rec.paramName = name;
		int typeID = q->FieldByName("suggested_system_type_id")->AsInteger;
		rec.paramType = AnsiString(DataTypeMapper::getSqlTypeById(typeID)).c_str();
		rec.maxLength = q->FieldByName("suggested_max_length")->AsInteger;
		rec.direction = q->FieldByName("suggested_is_input")->AsBoolean ? "IN" : "OUT";
		rec.paramOrder = q->FieldByName("parameter_ordinal")->AsInteger;
		rec.precision = q->FieldByName("suggested_precision")->AsInteger;
		rec.scale = q->FieldByName("suggested_scale")->AsInteger;
		rec.isNullable = GetIsNullable(schema, tableName, name);
		rec.isKeyField = GetIsKeyField(schema, tableName, name);

		result.push_back(rec);
		q->Next();
	}
	return result;
}

// ---------------------------------------------------------------------------
// AnalyzeOutputFields — sp_describe_first_result_set + fallback
// ---------------------------------------------------------------------------
std::vector<FieldRecord> McpQueryAnalyzer::AnalyzeOutputFields(const std::string& sql)
{
	std::string corrected = CorrectSqlSyntax(sql);
	std::string safeSql = EscapeSql(corrected);

	auto q = std::make_unique<TFDQuery>(nullptr);
	q->Connection = FConn;
	q->SQL->Text = u("EXEC sp_describe_first_result_set N'" + safeSql + "'");

	try {
		q->Open();
		std::vector<FieldRecord> result;
		while (!q->Eof) {
			FieldRecord rec;
			rec.fieldName = AnsiString(q->FieldByName("name")->AsString).c_str();

			// Skip _dub duplicates
			if (rec.fieldName.find("_dub") != std::string::npos) {
				q->Next();
				continue;
			}

			int typeID = q->FieldByName("system_type_id")->AsInteger;
			rec.fieldType = AnsiString(DataTypeMapper::getSqlTypeById(typeID)).c_str();
			rec.maxLength = q->FieldByName("max_length")->AsInteger;
			rec.precision = q->FieldByName("precision")->AsInteger;
			rec.scale = q->FieldByName("scale")->AsInteger;
			rec.fieldOrder = q->FieldByName("column_ordinal")->AsInteger;
			rec.isNullable = q->FieldByName("is_nullable")->AsBoolean;
			rec.isKeyField = false;

			result.push_back(rec);
			q->Next();
		}
		return result;
	}
	catch (...) {
		// Fallback: execute the query with empty params and read Fields
		return FetchOutputFieldsFallback(sql, corrected);
	}
}

// ---------------------------------------------------------------------------
// FetchOutputFieldsFallback — direct execution with empty params
// ---------------------------------------------------------------------------
std::vector<FieldRecord> McpQueryAnalyzer::FetchOutputFieldsFallback(
	const std::string& originalSql, const std::string& correctedSql)
{
	auto q = std::make_unique<TFDQuery>(nullptr);
	q->Connection = FConn;
	q->SQL->Text = u(originalSql);

	// Extract :params and add as empty string params
	std::string dutyStr = originalSql;
	std::vector<std::string> sourceParams;
	find_parameters(dutyStr, sourceParams);
	q->Params->Clear();
	for (const auto& pName : sourceParams) {
		if (pName.empty()) continue;
		bool exists = false;
		for (int j = 0; j < q->Params->Count; j++) {
			if (q->Params->Items[j]->Name == pName.c_str()) {
				exists = true;
				break;
			}
		}
		if (!exists) {
			TFDParam* param = q->Params->Add();
			param->Name = pName.c_str();
			param->DataType = ftString;
			param->Size = 20;
			param->Value = "";
		}
	}

	try {
		q->Open();
	} catch (...) {
		return {};
	}

	std::vector<FieldRecord> result;
	int fieldOrder = 1;
	for (int i = 0; i < q->FieldCount; i++) {
		FieldRecord rec;
		rec.fieldName = AnsiString(q->Fields->Fields[i]->FieldName).c_str();
		TFieldType fldType = q->Fields->Fields[i]->DataType;
		rec.fieldType = AnsiString(DataTypeMapper::getSqlTypeByFireDac(fldType)).c_str();
		rec.maxLength = q->Fields->Fields[i]->Size;
		rec.fieldOrder = fieldOrder++;
		result.push_back(rec);
	}
	q->Close();
	return result;
}

// ---------------------------------------------------------------------------
// GenerateSQL — dynamic CRUD SQL from table name
// ---------------------------------------------------------------------------
std::string McpQueryAnalyzer::GenerateSQL(const std::string& tableName,
	const std::string& operation)
{
	// Parse schema.table
	std::string schema, table;
	size_t dotPos = tableName.find('.');
	if (dotPos != std::string::npos) {
		schema = tableName.substr(0, dotPos);
		table = tableName.substr(dotPos + 1);
	} else {
		schema = "dbo";
		table = tableName;
	}

	// Build alias: first letter lowercase + first two consonants
	std::string alias;
	if (!table.empty()) {
		alias += (char)tolower(table[0]);
		int consFound = 0;
		for (size_t i = 1; i < table.size() && consFound < 2; i++) {
			char ch = (char)tolower(table[i]);
			if (ch != 'a' && ch != 'e' && ch != 'i' && ch != 'o' && ch != 'u') {
				alias += ch;
				consFound++;
			}
		}
	}

	// Get columns from INFORMATION_SCHEMA.COLUMNS
	std::vector<std::string> columns;
	{
		auto q = std::make_unique<TFDQuery>(nullptr);
		q->Connection = FConn;
		q->SQL->Text =
			u("SELECT COLUMN_NAME FROM INFORMATION_SCHEMA.COLUMNS "
			  "WHERE TABLE_SCHEMA = '" + schema + "' AND TABLE_NAME = '" + table + "' "
			  "ORDER BY ORDINAL_POSITION");
		q->Open();
		while (!q->Eof) {
			columns.push_back(AnsiString(q->FieldByName("COLUMN_NAME")->AsString).c_str());
			q->Next();
		}
	}

	// Get PK columns
	std::vector<std::string> pkColumns;
	{
		auto q = std::make_unique<TFDQuery>(nullptr);
		q->Connection = FConn;
		q->SQL->Text =
			u("SELECT K.COLUMN_NAME FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS AS TC "
			  "JOIN INFORMATION_SCHEMA.KEY_COLUMN_USAGE AS K "
			  "ON TC.CONSTRAINT_NAME = K.CONSTRAINT_NAME "
			  "WHERE TC.TABLE_SCHEMA = '" + schema + "' AND TC.TABLE_NAME = '" + table + "' "
			  "AND TC.CONSTRAINT_TYPE = 'PRIMARY KEY' "
			  "ORDER BY K.ORDINAL_POSITION");
		q->Open();
		while (!q->Eof) {
			pkColumns.push_back(AnsiString(q->FieldByName("COLUMN_NAME")->AsString).c_str());
			q->Next();
		}
	}

	// Build SQL based on operation
	std::string op = operation;
	std::transform(op.begin(), op.end(), op.begin(), ::toupper);

	if (op == "SELECT") {
		std::string colList;
		for (size_t i = 0; i < columns.size(); i++) {
			colList += alias + "." + columns[i];
			if (i < columns.size() - 1) colList += ", ";
		}
		return "SELECT " + colList + " FROM " + schema + "." + table + " AS " + alias;
	}
	else if (op == "DELETE") {
		std::string sql = "DELETE FROM " + schema + "." + table;
		if (!pkColumns.empty()) {
			std::string whereClause;
			for (size_t i = 0; i < pkColumns.size(); i++) {
				if (!whereClause.empty()) whereClause += " AND ";
				whereClause += pkColumns[i] + " = :" + pkColumns[i];
			}
			sql += " WHERE " + whereClause;
		}
		return sql;
	}
	else if (op == "UPDATE") {
		std::string setClause;
		for (size_t i = 0; i < columns.size(); i++) {
			if (std::find(pkColumns.begin(), pkColumns.end(), columns[i]) != pkColumns.end())
				continue;
			if (!setClause.empty()) setClause += ",\n";
			setClause += "   " + columns[i] + " = :" + columns[i];
		}
		std::string sql = "UPDATE " + schema + "." + table + " SET\n" + setClause;
		if (!pkColumns.empty()) {
			std::string whereClause;
			for (size_t i = 0; i < pkColumns.size(); i++) {
				if (!whereClause.empty()) whereClause += " AND ";
				whereClause += pkColumns[i] + " = :" + pkColumns[i];
			}
			sql += "\nWHERE " + whereClause;
		}
		return sql;
	}
	else if (op == "INSERT") {
		std::string colList, paramList;
		for (size_t i = 0; i < columns.size(); i++) {
			colList += columns[i];
			paramList += ":" + columns[i];
			if (i < columns.size() - 1) {
				colList += ", ";
				paramList += ", ";
			}
		}
		return "INSERT INTO " + schema + "." + table +
			   " (" + colList + ") VALUES (" + paramList + ")";
	}

	return "";
}
//---------------------------------------------------------------------------
