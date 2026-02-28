//---------------------------------------------------------------------------

#include <vcl.h>
#pragma hdrstop

#include "FirebirdProvider.h"
#include <System.SysUtils.hpp>
#include <System.JSON.hpp>
//---------------------------------------------------------------------------
#pragma package(smart_init)
//---------------------------------------------------------------------------
// Helper: validate SQL identifier (letters, digits, underscore)
static bool IsValidIdentifier(const String &name)
{
	if (name.IsEmpty()) return false;
	for (int i = 1; i <= name.Length(); i++)
	{
		wchar_t ch = name[i];
		if (!((ch >= L'A' && ch <= L'Z') || (ch >= L'a' && ch <= L'z') ||
			  (ch >= L'0' && ch <= L'9') || ch == L'_' || ch == L'.'))
			return false;
	}
	return true;
}
//---------------------------------------------------------------------------

TFirebirdProvider::TFirebirdProvider(TFDConnection *mainConnection)
	: TDbProviderBase(mainConnection)
{
}
//---------------------------------------------------------------------------

TFirebirdProvider::~TFirebirdProvider()
{
}
//---------------------------------------------------------------------------
// Provider info
//---------------------------------------------------------------------------

String TFirebirdProvider::GetProviderName() const
{
	return "Firebird";
}
//---------------------------------------------------------------------------

String TFirebirdProvider::GetDefaultSchema() const
{
	return ""; // Firebird doesn't have schemas like MSSQL
}
//---------------------------------------------------------------------------

String TFirebirdProvider::ApplyRowLimit(const String &sql, int maxRows)
{
	// Firebird: SELECT FIRST N ...
	String sqlTrimmed = sql.Trim();
	String sqlUpper = sqlTrimmed.UpperCase();

	bool isSelect = sqlUpper.Pos("SELECT") == 1;
	bool hasLimit = sqlUpper.Pos("SELECT FIRST") == 1 ||
					sqlUpper.Pos("ROWS") > 0;

	if (!isSelect || hasLimit)
		return sql;

	// Insert FIRST after SELECT/SELECT DISTINCT
	bool isDistinct = sqlUpper.Pos("SELECT DISTINCT") == 1;
	int insertPos = isDistinct ? 16 : 7;

	return sqlTrimmed.SubString(1, insertPos) +
		"FIRST " + IntToStr(maxRows) + " " +
		sqlTrimmed.SubString(insertPos + 1, sqlTrimmed.Length() - insertPos);
}
//---------------------------------------------------------------------------
// Metadata queries
//---------------------------------------------------------------------------

String TFirebirdProvider::ListTables(const String &schemaFilter, bool includeViews)
{
	String sql =
		"SELECT "
		"  TRIM(r.RDB$RELATION_NAME) AS table_name, "
		"  CASE r.RDB$RELATION_TYPE "
		"    WHEN 0 THEN 'TABLE' "
		"    WHEN 1 THEN 'VIEW' "
		"    ELSE 'OTHER' "
		"  END AS table_type, "
		"  COALESCE(r.RDB$DESCRIPTION, '') AS description "
		"FROM RDB$RELATIONS r "
		"WHERE r.RDB$SYSTEM_FLAG = 0 ";

	if (!includeViews)
		sql += "AND r.RDB$RELATION_TYPE = 0 ";

	sql += "ORDER BY r.RDB$RELATION_NAME";

	return RunQueryToJSON(sql);
}
//---------------------------------------------------------------------------

String TFirebirdProvider::GetTableSchema(const String &table)
{
	String tableName = table.UpperCase().Trim();

	String sql =
		"SELECT "
		"  TRIM(rf.RDB$FIELD_NAME) AS column_name, "
		"  CASE f.RDB$FIELD_TYPE "
		"    WHEN 7 THEN 'SMALLINT' "
		"    WHEN 8 THEN 'INTEGER' "
		"    WHEN 10 THEN 'FLOAT' "
		"    WHEN 12 THEN 'DATE' "
		"    WHEN 13 THEN 'TIME' "
		"    WHEN 14 THEN 'CHAR' "
		"    WHEN 16 THEN 'BIGINT' "
		"    WHEN 27 THEN 'DOUBLE PRECISION' "
		"    WHEN 35 THEN 'TIMESTAMP' "
		"    WHEN 37 THEN 'VARCHAR' "
		"    WHEN 261 THEN 'BLOB' "
		"    ELSE 'OTHER' "
		"  END AS data_type, "
		"  f.RDB$FIELD_LENGTH AS max_length, "
		"  f.RDB$FIELD_PRECISION AS field_precision, "
		"  f.RDB$FIELD_SCALE AS field_scale, "
		"  CASE WHEN rf.RDB$NULL_FLAG = 1 THEN 0 ELSE 1 END AS is_nullable, "
		"  COALESCE(rf.RDB$DESCRIPTION, '') AS description, "
		"  rf.RDB$FIELD_POSITION AS ordinal_position "
		"FROM RDB$RELATION_FIELDS rf "
		"JOIN RDB$FIELDS f ON rf.RDB$FIELD_SOURCE = f.RDB$FIELD_NAME "
		"WHERE TRIM(rf.RDB$RELATION_NAME) = '" + tableName + "' "
		"ORDER BY rf.RDB$FIELD_POSITION";

	return RunQueryToJSON(sql);
}
//---------------------------------------------------------------------------

String TFirebirdProvider::GetTableRelations(const String &table)
{
	String tableName = table.UpperCase().Trim();

	String sql =
		"SELECT "
		"  TRIM(rc.RDB$CONSTRAINT_NAME) AS constraint_name, "
		"  TRIM(iseg.RDB$FIELD_NAME) AS column_name, "
		"  TRIM(refc.RDB$CONST_NAME_UQ) AS referenced_constraint, "
		"  TRIM(idx2.RDB$RELATION_NAME) AS referenced_table, "
		"  TRIM(iseg2.RDB$FIELD_NAME) AS referenced_column "
		"FROM RDB$RELATION_CONSTRAINTS rc "
		"JOIN RDB$INDEX_SEGMENTS iseg ON rc.RDB$INDEX_NAME = iseg.RDB$INDEX_NAME "
		"JOIN RDB$REF_CONSTRAINTS refc ON rc.RDB$CONSTRAINT_NAME = refc.RDB$CONSTRAINT_NAME "
		"JOIN RDB$RELATION_CONSTRAINTS rc2 ON refc.RDB$CONST_NAME_UQ = rc2.RDB$CONSTRAINT_NAME "
		"JOIN RDB$INDICES idx2 ON rc2.RDB$INDEX_NAME = idx2.RDB$INDEX_NAME "
		"JOIN RDB$INDEX_SEGMENTS iseg2 ON idx2.RDB$INDEX_NAME = iseg2.RDB$INDEX_NAME "
		"  AND iseg.RDB$FIELD_POSITION = iseg2.RDB$FIELD_POSITION "
		"WHERE rc.RDB$CONSTRAINT_TYPE = 'FOREIGN KEY' "
		"  AND TRIM(rc.RDB$RELATION_NAME) = '" + tableName + "' "
		"ORDER BY rc.RDB$CONSTRAINT_NAME, iseg.RDB$FIELD_POSITION";

	return RunQueryToJSON(sql);
}
//---------------------------------------------------------------------------

String TFirebirdProvider::ListDatabases()
{
	// Firebird doesn't have a concept of multiple databases in one server
	// Return current database info
	String sql =
		"SELECT "
		"  'current' AS database_name, "
		"  MON$DATABASE_NAME AS file_path "
		"FROM MON$DATABASE";

	return RunQueryToJSON(sql);
}
//---------------------------------------------------------------------------

String TFirebirdProvider::ListObjects(const String &objType, const String &schemaFilter,
	const String &namePattern)
{
	String typeUpper = objType.UpperCase();
	String pattern = namePattern.IsEmpty() ? "%" : namePattern;

	String sql;

	if (typeUpper == "ALL" || typeUpper == "TABLE")
	{
		sql = "SELECT TRIM(RDB$RELATION_NAME) AS name, 'TABLE' AS type "
			  "FROM RDB$RELATIONS "
			  "WHERE RDB$SYSTEM_FLAG = 0 AND RDB$RELATION_TYPE = 0 "
			  "AND TRIM(RDB$RELATION_NAME) LIKE '" + pattern + "' ";
	}

	if (typeUpper == "ALL" || typeUpper == "VIEW")
	{
		if (!sql.IsEmpty()) sql += "UNION ALL ";
		sql += "SELECT TRIM(RDB$RELATION_NAME) AS name, 'VIEW' AS type "
			   "FROM RDB$RELATIONS "
			   "WHERE RDB$SYSTEM_FLAG = 0 AND RDB$RELATION_TYPE = 1 "
			   "AND TRIM(RDB$RELATION_NAME) LIKE '" + pattern + "' ";
	}

	if (typeUpper == "ALL" || typeUpper == "PROCEDURE")
	{
		if (!sql.IsEmpty()) sql += "UNION ALL ";
		sql += "SELECT TRIM(RDB$PROCEDURE_NAME) AS name, 'PROCEDURE' AS type "
			   "FROM RDB$PROCEDURES "
			   "WHERE RDB$SYSTEM_FLAG = 0 "
			   "AND TRIM(RDB$PROCEDURE_NAME) LIKE '" + pattern + "' ";
	}

	if (typeUpper == "ALL" || typeUpper == "FUNCTION")
	{
		if (!sql.IsEmpty()) sql += "UNION ALL ";
		sql += "SELECT TRIM(RDB$FUNCTION_NAME) AS name, 'FUNCTION' AS type "
			   "FROM RDB$FUNCTIONS "
			   "WHERE RDB$SYSTEM_FLAG = 0 "
			   "AND TRIM(RDB$FUNCTION_NAME) LIKE '" + pattern + "' ";
	}

	if (typeUpper == "ALL" || typeUpper == "TRIGGER")
	{
		if (!sql.IsEmpty()) sql += "UNION ALL ";
		sql += "SELECT TRIM(RDB$TRIGGER_NAME) AS name, 'TRIGGER' AS type "
			   "FROM RDB$TRIGGERS "
			   "WHERE RDB$SYSTEM_FLAG = 0 "
			   "AND TRIM(RDB$TRIGGER_NAME) LIKE '" + pattern + "' ";
	}

	if (sql.IsEmpty())
		return MakeErrorJSON("Unknown object type: " + objType);

	sql += " ORDER BY 2, 1";

	return RunQueryToJSON(sql);
}
//---------------------------------------------------------------------------

String TFirebirdProvider::GetObjectDefinition(const String &objName, const String &objType)
{
	String name = objName.UpperCase().Trim();

	// Try procedures first
	String sql =
		"SELECT "
		"  TRIM(RDB$PROCEDURE_NAME) AS name, "
		"  'PROCEDURE' AS type, "
		"  RDB$PROCEDURE_SOURCE AS definition "
		"FROM RDB$PROCEDURES "
		"WHERE TRIM(RDB$PROCEDURE_NAME) = '" + name + "' "
		"UNION ALL "
		"SELECT "
		"  TRIM(RDB$TRIGGER_NAME) AS name, "
		"  'TRIGGER' AS type, "
		"  RDB$TRIGGER_SOURCE AS definition "
		"FROM RDB$TRIGGERS "
		"WHERE TRIM(RDB$TRIGGER_NAME) = '" + name + "' "
		"UNION ALL "
		"SELECT "
		"  TRIM(RDB$RELATION_NAME) AS name, "
		"  'VIEW' AS type, "
		"  RDB$VIEW_SOURCE AS definition "
		"FROM RDB$RELATIONS "
		"WHERE RDB$RELATION_TYPE = 1 AND TRIM(RDB$RELATION_NAME) = '" + name + "'";

	return RunQueryToJSON(sql);
}
//---------------------------------------------------------------------------

String TFirebirdProvider::GetDependencies(const String &objName, const String &direction)
{
	String name = objName.UpperCase().Trim();
	String dirUpper = direction.UpperCase();

	String sql;

	if (dirUpper == "BOTH" || dirUpper == "USES")
	{
		sql = "SELECT "
			  "  'USES' AS direction, "
			  "  TRIM(RDB$DEPENDED_ON_NAME) AS name, "
			  "  CASE RDB$DEPENDED_ON_TYPE "
			  "    WHEN 0 THEN 'TABLE' "
			  "    WHEN 1 THEN 'VIEW' "
			  "    WHEN 5 THEN 'PROCEDURE' "
			  "    WHEN 2 THEN 'TRIGGER' "
			  "    ELSE 'OTHER' "
			  "  END AS type "
			  "FROM RDB$DEPENDENCIES "
			  "WHERE TRIM(RDB$DEPENDENT_NAME) = '" + name + "' ";
	}

	if (dirUpper == "BOTH" || dirUpper == "USED_BY")
	{
		if (!sql.IsEmpty()) sql += "UNION ALL ";
		sql += "SELECT "
			   "  'USED_BY' AS direction, "
			   "  TRIM(RDB$DEPENDENT_NAME) AS name, "
			   "  CASE RDB$DEPENDENT_TYPE "
			   "    WHEN 0 THEN 'TABLE' "
			   "    WHEN 1 THEN 'VIEW' "
			   "    WHEN 5 THEN 'PROCEDURE' "
			   "    WHEN 2 THEN 'TRIGGER' "
			   "    ELSE 'OTHER' "
			   "  END AS type "
			   "FROM RDB$DEPENDENCIES "
			   "WHERE TRIM(RDB$DEPENDED_ON_NAME) = '" + name + "' ";
	}

	sql += "ORDER BY 1, 2";  // Firebird requires column positions in UNION ORDER BY

	return RunQueryToJSON(sql);
}
//---------------------------------------------------------------------------
// Search & analysis
//---------------------------------------------------------------------------

String TFirebirdProvider::SearchColumns(const String &pattern)
{
	String sql =
		"SELECT "
		"  TRIM(rf.RDB$RELATION_NAME) AS table_name, "
		"  TRIM(rf.RDB$FIELD_NAME) AS column_name, "
		"  CASE f.RDB$FIELD_TYPE "
		"    WHEN 7 THEN 'SMALLINT' "
		"    WHEN 8 THEN 'INTEGER' "
		"    WHEN 14 THEN 'CHAR' "
		"    WHEN 16 THEN 'BIGINT' "
		"    WHEN 37 THEN 'VARCHAR' "
		"    ELSE 'OTHER' "
		"  END AS data_type "
		"FROM RDB$RELATION_FIELDS rf "
		"JOIN RDB$FIELDS f ON rf.RDB$FIELD_SOURCE = f.RDB$FIELD_NAME "
		"JOIN RDB$RELATIONS r ON rf.RDB$RELATION_NAME = r.RDB$RELATION_NAME "
		"WHERE r.RDB$SYSTEM_FLAG = 0 "
		"  AND TRIM(rf.RDB$FIELD_NAME) LIKE '" + pattern + "' "
		"ORDER BY rf.RDB$RELATION_NAME, rf.RDB$FIELD_NAME";

	return RunQueryToJSON(sql);
}
//---------------------------------------------------------------------------

String TFirebirdProvider::SearchObjectSource(const String &pattern)
{
	String sql =
		"SELECT "
		"  TRIM(RDB$PROCEDURE_NAME) AS name, "
		"  'PROCEDURE' AS type, "
		"  SUBSTRING(RDB$PROCEDURE_SOURCE FROM 1 FOR 200) AS snippet "
		"FROM RDB$PROCEDURES "
		"WHERE RDB$PROCEDURE_SOURCE CONTAINING '" + pattern + "' "
		"UNION ALL "
		"SELECT "
		"  TRIM(RDB$TRIGGER_NAME) AS name, "
		"  'TRIGGER' AS type, "
		"  SUBSTRING(RDB$TRIGGER_SOURCE FROM 1 FOR 200) AS snippet "
		"FROM RDB$TRIGGERS "
		"WHERE RDB$TRIGGER_SOURCE CONTAINING '" + pattern + "'";

	return RunQueryToJSON(sql);
}
//---------------------------------------------------------------------------

String TFirebirdProvider::ProfileColumn(const String &table, const String &column)
{
	String tableName = table.UpperCase().Trim();
	String colName = column.UpperCase().Trim();

	// Validate identifiers
	if (!IsValidIdentifier(tableName) || !IsValidIdentifier(colName))
		return MakeErrorJSON("Invalid table or column name");

	// Firebird: use unquoted uppercase identifiers (quoting makes them case-sensitive)
	String sql =
		"SELECT "
		"  COUNT(*) AS total_rows, "
		"  COUNT(DISTINCT " + colName + ") AS distinct_count, "
		"  SUM(CASE WHEN " + colName + " IS NULL THEN 1 ELSE 0 END) AS null_count, "
		"  MIN(" + colName + ") AS min_value, "
		"  MAX(" + colName + ") AS max_value "
		"FROM " + tableName;

	return RunQueryToJSON(sql);
}
//---------------------------------------------------------------------------

String TFirebirdProvider::ExplainQuery(const String &sql)
{
	// Firebird doesn't have a simple EXPLAIN like other databases
	// You need to use MON$ tables or external tools
	return MakeErrorJSON("EXPLAIN not supported for Firebird in this version");
}
//---------------------------------------------------------------------------
// Query operations
//---------------------------------------------------------------------------

String TFirebirdProvider::GetTableSample(const String &table, int limit)
{
	String tableName = table.UpperCase().Trim();

	if (!IsValidIdentifier(tableName))
		return MakeErrorJSON("Invalid table name");

	if (limit < 1) limit = 5;
	if (limit > 100) limit = 100;

	// Firebird: use unquoted uppercase identifiers
	String sql = "SELECT FIRST " + IntToStr(limit) + " * FROM " + tableName;

	return RunQueryToJSON(sql);
}
//---------------------------------------------------------------------------
// Advanced operations
//---------------------------------------------------------------------------

String TFirebirdProvider::CompareTables(const String &table1, const String &table2)
{
	String t1 = table1.UpperCase().Trim();
	String t2 = table2.UpperCase().Trim();

	// Use rf.RDB$FIELD_NAME (from RDB$RELATION_FIELDS) to avoid ambiguity with f.RDB$FIELD_NAME
	String sql =
		"SELECT "
		"  COALESCE(t1.col, t2.col) AS column_name, "
		"  t1.data_type AS table1_type, "
		"  t2.data_type AS table2_type, "
		"  CASE "
		"    WHEN t1.col IS NULL THEN 'only_in_table2' "
		"    WHEN t2.col IS NULL THEN 'only_in_table1' "
		"    WHEN t1.data_type <> t2.data_type THEN 'type_mismatch' "
		"    ELSE 'match' "
		"  END AS status "
		"FROM "
		"  (SELECT TRIM(rf.RDB$FIELD_NAME) AS col, "
		"    CASE f.RDB$FIELD_TYPE WHEN 7 THEN 'SMALLINT' WHEN 8 THEN 'INTEGER' "
		"      WHEN 14 THEN 'CHAR' WHEN 16 THEN 'BIGINT' WHEN 37 THEN 'VARCHAR' ELSE 'OTHER' END AS data_type "
		"   FROM RDB$RELATION_FIELDS rf JOIN RDB$FIELDS f ON rf.RDB$FIELD_SOURCE = f.RDB$FIELD_NAME "
		"   WHERE TRIM(rf.RDB$RELATION_NAME) = '" + t1 + "') t1 "
		"FULL OUTER JOIN "
		"  (SELECT TRIM(rf.RDB$FIELD_NAME) AS col, "
		"    CASE f.RDB$FIELD_TYPE WHEN 7 THEN 'SMALLINT' WHEN 8 THEN 'INTEGER' "
		"      WHEN 14 THEN 'CHAR' WHEN 16 THEN 'BIGINT' WHEN 37 THEN 'VARCHAR' ELSE 'OTHER' END AS data_type "
		"   FROM RDB$RELATION_FIELDS rf JOIN RDB$FIELDS f ON rf.RDB$FIELD_SOURCE = f.RDB$FIELD_NAME "
		"   WHERE TRIM(rf.RDB$RELATION_NAME) = '" + t2 + "') t2 "
		"ON t1.col = t2.col "
		"ORDER BY 1";

	return RunQueryToJSON(sql);
}
//---------------------------------------------------------------------------

String TFirebirdProvider::TraceFkPath(const String &fromTable, const String &toTable, int maxDepth)
{
	// Simplified version - just return direct FK relationships
	return MakeErrorJSON("TraceFkPath not fully implemented for Firebird");
}
//---------------------------------------------------------------------------

String TFirebirdProvider::GetModuleOverview(const String &tablesList)
{
	// Parse comma-separated table names
	TJSONObject *result = new TJSONObject();
	TJSONArray *tables = new TJSONArray();

	TStringList *names = new TStringList();
	names->Delimiter = L',';
	names->StrictDelimiter = true;
	names->DelimitedText = tablesList;

	for (int i = 0; i < names->Count; i++)
	{
		String tableName = names->Strings[i].Trim().UpperCase();
		if (tableName.IsEmpty()) continue;

		TJSONObject *tableInfo = new TJSONObject();
		tableInfo->AddPair("name", tableName);

		// Get column count
		String sql = "SELECT COUNT(*) AS cnt FROM RDB$RELATION_FIELDS "
					 "WHERE TRIM(RDB$RELATION_NAME) = '" + tableName + "'";
		String colResult = RunQueryToJSON(sql);

		tableInfo->AddPair("columns", TJSONObject::ParseJSONValue(colResult));
		tables->AddElement(tableInfo);
	}

	delete names;

	result->AddPair("tables", tables);
	String json = result->ToJSON();
	delete result;
	return json;
}
//---------------------------------------------------------------------------
