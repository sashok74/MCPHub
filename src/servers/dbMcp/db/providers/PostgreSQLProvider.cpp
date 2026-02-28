//---------------------------------------------------------------------------

#include <vcl.h>
#pragma hdrstop

#include "PostgreSQLProvider.h"
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

TPostgreSQLProvider::TPostgreSQLProvider(TFDConnection *mainConnection)
	: TDbProviderBase(mainConnection)
{
}
//---------------------------------------------------------------------------

TPostgreSQLProvider::~TPostgreSQLProvider()
{
}
//---------------------------------------------------------------------------
// Provider info
//---------------------------------------------------------------------------

String TPostgreSQLProvider::GetProviderName() const
{
	return "PostgreSQL";
}
//---------------------------------------------------------------------------

String TPostgreSQLProvider::GetDefaultSchema() const
{
	return "public";
}
//---------------------------------------------------------------------------

String TPostgreSQLProvider::ApplyRowLimit(const String &sql, int maxRows)
{
	// PostgreSQL: SELECT ... LIMIT N
	String sqlTrimmed = sql.Trim();
	String sqlUpper = sqlTrimmed.UpperCase();

	bool isSelect = sqlUpper.Pos("SELECT") == 1;
	bool hasLimit = sqlUpper.Pos("LIMIT") > 0 ||
					sqlUpper.Pos("OFFSET") > 0;

	if (!isSelect || hasLimit)
		return sql;

	return sqlTrimmed + " LIMIT " + IntToStr(maxRows);
}
//---------------------------------------------------------------------------
// Metadata queries
//---------------------------------------------------------------------------

String TPostgreSQLProvider::ListTables(const String &schemaFilter, bool includeViews)
{
	String sql =
		"SELECT "
		"  table_schema AS schema, "
		"  table_name AS table_name, "
		"  table_type, "
		"  COALESCE(obj_description((quote_ident(table_schema) || '.' || quote_ident(table_name))::regclass), '') AS description "
		"FROM information_schema.tables "
		"WHERE table_schema NOT IN ('pg_catalog', 'information_schema') ";

	if (!includeViews)
		sql += "AND table_type = 'BASE TABLE' ";

	if (!schemaFilter.IsEmpty())
		sql += "AND table_schema = '" + schemaFilter + "' ";

	sql += "ORDER BY table_schema, table_name";

	return RunQueryToJSON(sql);
}
//---------------------------------------------------------------------------

String TPostgreSQLProvider::GetTableSchema(const String &table)
{
	// Parse schema.table or just table
	String schema = "public";
	String tableName = table.Trim();

	int dotPos = tableName.Pos(".");
	if (dotPos > 0)
	{
		schema = tableName.SubString(1, dotPos - 1);
		tableName = tableName.SubString(dotPos + 1, tableName.Length() - dotPos);
	}

	String sql =
		"SELECT "
		"  c.column_name, "
		"  c.data_type, "
		"  c.character_maximum_length AS max_length, "
		"  c.numeric_precision AS precision, "
		"  c.numeric_scale AS scale, "
		"  c.is_nullable, "
		"  c.column_default AS default_value, "
		"  CASE WHEN pk.column_name IS NOT NULL THEN 1 ELSE 0 END AS is_primary_key, "
		"  CASE WHEN fk.column_name IS NOT NULL THEN 1 ELSE 0 END AS is_foreign_key, "
		"  COALESCE(fk.foreign_table, '') AS references_table, "
		"  COALESCE(fk.foreign_column, '') AS references_column, "
		"  COALESCE(col_description((quote_ident(c.table_schema) || '.' || quote_ident(c.table_name))::regclass, c.ordinal_position), '') AS description "
		"FROM information_schema.columns c "
		"LEFT JOIN ( "
		"  SELECT ku.table_schema, ku.table_name, ku.column_name "
		"  FROM information_schema.table_constraints tc "
		"  JOIN information_schema.key_column_usage ku ON tc.constraint_name = ku.constraint_name "
		"    AND tc.table_schema = ku.table_schema "
		"  WHERE tc.constraint_type = 'PRIMARY KEY' "
		") pk ON c.table_schema = pk.table_schema AND c.table_name = pk.table_name AND c.column_name = pk.column_name "
		"LEFT JOIN ( "
		"  SELECT "
		"    kcu.table_schema, kcu.table_name, kcu.column_name, "
		"    ccu.table_name AS foreign_table, ccu.column_name AS foreign_column "
		"  FROM information_schema.table_constraints tc "
		"  JOIN information_schema.key_column_usage kcu ON tc.constraint_name = kcu.constraint_name "
		"    AND tc.table_schema = kcu.table_schema "
		"  JOIN information_schema.constraint_column_usage ccu ON tc.constraint_name = ccu.constraint_name "
		"  WHERE tc.constraint_type = 'FOREIGN KEY' "
		") fk ON c.table_schema = fk.table_schema AND c.table_name = fk.table_name AND c.column_name = fk.column_name "
		"WHERE c.table_schema = '" + schema + "' AND c.table_name = '" + tableName + "' "
		"ORDER BY c.ordinal_position";

	return RunQueryToJSON(sql);
}
//---------------------------------------------------------------------------

String TPostgreSQLProvider::GetTableRelations(const String &table)
{
	String schema = "public";
	String tableName = table.Trim();

	int dotPos = tableName.Pos(".");
	if (dotPos > 0)
	{
		schema = tableName.SubString(1, dotPos - 1);
		tableName = tableName.SubString(dotPos + 1, tableName.Length() - dotPos);
	}

	String sql =
		"SELECT "
		"  tc.constraint_name, "
		"  kcu.column_name, "
		"  ccu.table_schema AS referenced_schema, "
		"  ccu.table_name AS referenced_table, "
		"  ccu.column_name AS referenced_column "
		"FROM information_schema.table_constraints tc "
		"JOIN information_schema.key_column_usage kcu "
		"  ON tc.constraint_name = kcu.constraint_name AND tc.table_schema = kcu.table_schema "
		"JOIN information_schema.constraint_column_usage ccu "
		"  ON tc.constraint_name = ccu.constraint_name "
		"WHERE tc.constraint_type = 'FOREIGN KEY' "
		"  AND tc.table_schema = '" + schema + "' "
		"  AND tc.table_name = '" + tableName + "' "
		"ORDER BY tc.constraint_name, kcu.ordinal_position";

	return RunQueryToJSON(sql);
}
//---------------------------------------------------------------------------

String TPostgreSQLProvider::ListDatabases()
{
	String sql =
		"SELECT datname AS database_name, "
		"  pg_database_size(datname) AS size_bytes, "
		"  pg_encoding_to_char(encoding) AS encoding "
		"FROM pg_database "
		"WHERE datistemplate = false "
		"ORDER BY datname";

	return RunQueryToJSON(sql);
}
//---------------------------------------------------------------------------

String TPostgreSQLProvider::ListObjects(const String &objType, const String &schemaFilter,
	const String &namePattern)
{
	String typeUpper = objType.UpperCase();
	String pattern = namePattern.IsEmpty() ? "%" : namePattern;
	String schemaClause = schemaFilter.IsEmpty() ?
		"n.nspname NOT IN ('pg_catalog', 'information_schema')" :
		"n.nspname = '" + schemaFilter + "'";

	String sql;

	if (typeUpper == "ALL" || typeUpper == "TABLE")
	{
		sql = "SELECT n.nspname AS schema, c.relname AS name, 'TABLE' AS type "
			  "FROM pg_class c JOIN pg_namespace n ON c.relnamespace = n.oid "
			  "WHERE c.relkind = 'r' AND " + schemaClause + " "
			  "AND c.relname LIKE '" + pattern + "' ";
	}

	if (typeUpper == "ALL" || typeUpper == "VIEW")
	{
		if (!sql.IsEmpty()) sql += "UNION ALL ";
		sql += "SELECT n.nspname AS schema, c.relname AS name, 'VIEW' AS type "
			   "FROM pg_class c JOIN pg_namespace n ON c.relnamespace = n.oid "
			   "WHERE c.relkind = 'v' AND " + schemaClause + " "
			   "AND c.relname LIKE '" + pattern + "' ";
	}

	if (typeUpper == "ALL" || typeUpper == "PROCEDURE" || typeUpper == "FUNCTION")
	{
		if (!sql.IsEmpty()) sql += "UNION ALL ";
		sql += "SELECT n.nspname AS schema, p.proname AS name, "
			   "CASE p.prokind WHEN 'f' THEN 'FUNCTION' WHEN 'p' THEN 'PROCEDURE' ELSE 'FUNCTION' END AS type "
			   "FROM pg_proc p JOIN pg_namespace n ON p.pronamespace = n.oid "
			   "WHERE " + schemaClause + " "
			   "AND p.proname LIKE '" + pattern + "' ";
	}

	if (typeUpper == "ALL" || typeUpper == "TRIGGER")
	{
		if (!sql.IsEmpty()) sql += "UNION ALL ";
		sql += "SELECT n.nspname AS schema, t.tgname AS name, 'TRIGGER' AS type "
			   "FROM pg_trigger t "
			   "JOIN pg_class c ON t.tgrelid = c.oid "
			   "JOIN pg_namespace n ON c.relnamespace = n.oid "
			   "WHERE NOT t.tgisinternal AND " + schemaClause + " "
			   "AND t.tgname LIKE '" + pattern + "' ";
	}

	if (typeUpper == "ALL" || typeUpper == "INDEX")
	{
		if (!sql.IsEmpty()) sql += "UNION ALL ";
		sql += "SELECT n.nspname AS schema, c.relname AS name, 'INDEX' AS type "
			   "FROM pg_class c JOIN pg_namespace n ON c.relnamespace = n.oid "
			   "WHERE c.relkind = 'i' AND " + schemaClause + " "
			   "AND c.relname LIKE '" + pattern + "' ";
	}

	if (sql.IsEmpty())
		return MakeErrorJSON("Unknown object type: " + objType);

	sql += "ORDER BY 3, 1, 2";

	return RunQueryToJSON(sql);
}
//---------------------------------------------------------------------------

String TPostgreSQLProvider::GetObjectDefinition(const String &objName, const String &objType)
{
	String schema = "public";
	String name = objName.Trim();

	int dotPos = name.Pos(".");
	if (dotPos > 0)
	{
		schema = name.SubString(1, dotPos - 1);
		name = name.SubString(dotPos + 1, name.Length() - dotPos);
	}

	// Try to get function/procedure definition
	String sql =
		"SELECT "
		"  n.nspname AS schema, "
		"  p.proname AS name, "
		"  CASE p.prokind WHEN 'f' THEN 'FUNCTION' WHEN 'p' THEN 'PROCEDURE' ELSE 'FUNCTION' END AS type, "
		"  pg_get_functiondef(p.oid) AS definition "
		"FROM pg_proc p "
		"JOIN pg_namespace n ON p.pronamespace = n.oid "
		"WHERE n.nspname = '" + schema + "' AND p.proname = '" + name + "' "
		"UNION ALL "
		"SELECT "
		"  n.nspname AS schema, "
		"  c.relname AS name, "
		"  'VIEW' AS type, "
		"  pg_get_viewdef(c.oid, true) AS definition "
		"FROM pg_class c "
		"JOIN pg_namespace n ON c.relnamespace = n.oid "
		"WHERE c.relkind = 'v' AND n.nspname = '" + schema + "' AND c.relname = '" + name + "'";

	return RunQueryToJSON(sql);
}
//---------------------------------------------------------------------------

String TPostgreSQLProvider::GetDependencies(const String &objName, const String &direction)
{
	String schema = "public";
	String name = objName.Trim();
	String dirUpper = direction.UpperCase();

	int dotPos = name.Pos(".");
	if (dotPos > 0)
	{
		schema = name.SubString(1, dotPos - 1);
		name = name.SubString(dotPos + 1, name.Length() - dotPos);
	}

	String sql;

	if (dirUpper == "BOTH" || dirUpper == "USES")
	{
		sql = "SELECT 'USES' AS direction, "
			  "  dep_ns.nspname || '.' || dep_cl.relname AS name, "
			  "  CASE dep_cl.relkind "
			  "    WHEN 'r' THEN 'TABLE' "
			  "    WHEN 'v' THEN 'VIEW' "
			  "    WHEN 'i' THEN 'INDEX' "
			  "    ELSE 'OTHER' "
			  "  END AS type "
			  "FROM pg_depend d "
			  "JOIN pg_class cl ON d.objid = cl.oid "
			  "JOIN pg_namespace ns ON cl.relnamespace = ns.oid "
			  "JOIN pg_class dep_cl ON d.refobjid = dep_cl.oid "
			  "JOIN pg_namespace dep_ns ON dep_cl.relnamespace = dep_ns.oid "
			  "WHERE ns.nspname = '" + schema + "' AND cl.relname = '" + name + "' "
			  "AND d.deptype = 'n' ";
	}

	if (dirUpper == "BOTH" || dirUpper == "USED_BY")
	{
		if (!sql.IsEmpty()) sql += "UNION ALL ";
		sql += "SELECT 'USED_BY' AS direction, "
			   "  dep_ns.nspname || '.' || dep_cl.relname AS name, "
			   "  CASE dep_cl.relkind "
			   "    WHEN 'r' THEN 'TABLE' "
			   "    WHEN 'v' THEN 'VIEW' "
			   "    WHEN 'i' THEN 'INDEX' "
			   "    ELSE 'OTHER' "
			   "  END AS type "
			   "FROM pg_depend d "
			   "JOIN pg_class cl ON d.refobjid = cl.oid "
			   "JOIN pg_namespace ns ON cl.relnamespace = ns.oid "
			   "JOIN pg_class dep_cl ON d.objid = dep_cl.oid "
			   "JOIN pg_namespace dep_ns ON dep_cl.relnamespace = dep_ns.oid "
			   "WHERE ns.nspname = '" + schema + "' AND cl.relname = '" + name + "' "
			   "AND d.deptype = 'n' ";
	}

	sql += "ORDER BY 1, 2";

	return RunQueryToJSON(sql);
}
//---------------------------------------------------------------------------
// Search & analysis
//---------------------------------------------------------------------------

String TPostgreSQLProvider::SearchColumns(const String &pattern)
{
	String sql =
		"SELECT "
		"  table_schema AS schema, "
		"  table_name, "
		"  column_name, "
		"  data_type "
		"FROM information_schema.columns "
		"WHERE table_schema NOT IN ('pg_catalog', 'information_schema') "
		"  AND column_name ILIKE '%" + pattern + "%' "
		"ORDER BY table_schema, table_name, column_name";

	return RunQueryToJSON(sql);
}
//---------------------------------------------------------------------------

String TPostgreSQLProvider::SearchObjectSource(const String &pattern)
{
	// Note: prokind = 'a' is aggregate function, which pg_get_functiondef() doesn't support
	// Filter to only 'f' (function) and 'p' (procedure)
	String sql =
		"SELECT "
		"  n.nspname AS schema, "
		"  p.proname AS name, "
		"  CASE p.prokind WHEN 'f' THEN 'FUNCTION' WHEN 'p' THEN 'PROCEDURE' ELSE 'FUNCTION' END AS type, "
		"  SUBSTRING(pg_get_functiondef(p.oid) FROM 1 FOR 200) AS snippet "
		"FROM pg_proc p "
		"JOIN pg_namespace n ON p.pronamespace = n.oid "
		"WHERE n.nspname NOT IN ('pg_catalog', 'information_schema') "
		"  AND p.prokind IN ('f', 'p') "
		"  AND pg_get_functiondef(p.oid) ILIKE '%" + pattern + "%' "
		"ORDER BY n.nspname, p.proname";

	return RunQueryToJSON(sql);
}
//---------------------------------------------------------------------------

String TPostgreSQLProvider::ProfileColumn(const String &table, const String &column)
{
	String schema = "public";
	String tableName = table.Trim();
	String colName = column.Trim();

	int dotPos = tableName.Pos(".");
	if (dotPos > 0)
	{
		schema = tableName.SubString(1, dotPos - 1);
		tableName = tableName.SubString(dotPos + 1, tableName.Length() - dotPos);
	}

	if (!IsValidIdentifier(tableName) || !IsValidIdentifier(colName))
		return MakeErrorJSON("Invalid table or column name");

	String fullTable = "\"" + schema + "\".\"" + tableName + "\"";

	String sql =
		"SELECT "
		"  COUNT(*) AS total_rows, "
		"  COUNT(DISTINCT \"" + colName + "\") AS distinct_count, "
		"  SUM(CASE WHEN \"" + colName + "\" IS NULL THEN 1 ELSE 0 END) AS null_count, "
		"  MIN(\"" + colName + "\")::text AS min_value, "
		"  MAX(\"" + colName + "\")::text AS max_value "
		"FROM " + fullTable;

	return RunQueryToJSON(sql);
}
//---------------------------------------------------------------------------

String TPostgreSQLProvider::ExplainQuery(const String &sql)
{
	String explainSql = "EXPLAIN (FORMAT JSON) " + sql;
	return RunQueryToJSON(explainSql);
}
//---------------------------------------------------------------------------
// Query operations
//---------------------------------------------------------------------------

String TPostgreSQLProvider::GetTableSample(const String &table, int limit)
{
	String schema = "public";
	String tableName = table.Trim();

	int dotPos = tableName.Pos(".");
	if (dotPos > 0)
	{
		schema = tableName.SubString(1, dotPos - 1);
		tableName = tableName.SubString(dotPos + 1, tableName.Length() - dotPos);
	}

	if (!IsValidIdentifier(tableName))
		return MakeErrorJSON("Invalid table name");

	if (limit < 1) limit = 5;
	if (limit > 100) limit = 100;

	String sql = "SELECT * FROM \"" + schema + "\".\"" + tableName + "\" LIMIT " + IntToStr(limit);

	return RunQueryToJSON(sql);
}
//---------------------------------------------------------------------------
// Advanced operations
//---------------------------------------------------------------------------

String TPostgreSQLProvider::CompareTables(const String &table1, const String &table2)
{
	// Parse schema.table for both
	String schema1 = "public", name1 = table1.Trim();
	String schema2 = "public", name2 = table2.Trim();

	int dotPos = name1.Pos(".");
	if (dotPos > 0) { schema1 = name1.SubString(1, dotPos - 1); name1 = name1.SubString(dotPos + 1, name1.Length() - dotPos); }

	dotPos = name2.Pos(".");
	if (dotPos > 0) { schema2 = name2.SubString(1, dotPos - 1); name2 = name2.SubString(dotPos + 1, name2.Length() - dotPos); }

	String sql =
		"SELECT "
		"  COALESCE(t1.column_name, t2.column_name) AS column_name, "
		"  t1.data_type AS table1_type, "
		"  t2.data_type AS table2_type, "
		"  CASE "
		"    WHEN t1.column_name IS NULL THEN 'only_in_table2' "
		"    WHEN t2.column_name IS NULL THEN 'only_in_table1' "
		"    WHEN t1.data_type <> t2.data_type THEN 'type_mismatch' "
		"    ELSE 'match' "
		"  END AS status "
		"FROM "
		"  (SELECT column_name, data_type FROM information_schema.columns "
		"   WHERE table_schema = '" + schema1 + "' AND table_name = '" + name1 + "') t1 "
		"FULL OUTER JOIN "
		"  (SELECT column_name, data_type FROM information_schema.columns "
		"   WHERE table_schema = '" + schema2 + "' AND table_name = '" + name2 + "') t2 "
		"ON t1.column_name = t2.column_name "
		"ORDER BY 1";

	return RunQueryToJSON(sql);
}
//---------------------------------------------------------------------------

String TPostgreSQLProvider::TraceFkPath(const String &fromTable, const String &toTable, int maxDepth)
{
	return MakeErrorJSON("TraceFkPath not implemented for PostgreSQL");
}
//---------------------------------------------------------------------------

String TPostgreSQLProvider::GetModuleOverview(const String &tablesList)
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
		String tableName = names->Strings[i].Trim();
		if (tableName.IsEmpty()) continue;

		// Parse schema.table
		String schema = "public";
		String tblName = tableName;
		int dotPos = tableName.Pos(".");
		if (dotPos > 0)
		{
			schema = tableName.SubString(1, dotPos - 1);
			tblName = tableName.SubString(dotPos + 1, tableName.Length() - dotPos);
		}

		TJSONObject *tableInfo = new TJSONObject();
		tableInfo->AddPair("name", tableName);

		// Get column count
		String sqlCols = "SELECT COUNT(*) AS cnt FROM information_schema.columns "
			"WHERE table_schema = '" + schema + "' AND table_name = '" + tblName + "'";
		TFDQuery *qry = new TFDQuery(NULL);
		qry->Connection = GetMainConnection();
		try
		{
			qry->SQL->Text = sqlCols;
			qry->Open();
			tableInfo->AddPair("column_count", new TJSONNumber(qry->FieldByName("cnt")->AsInteger));
			qry->Close();

			// Get row count (approx from pg_stat)
			String sqlRows = "SELECT COALESCE(reltuples, 0)::bigint AS cnt "
				"FROM pg_class c JOIN pg_namespace n ON c.relnamespace = n.oid "
				"WHERE n.nspname = '" + schema + "' AND c.relname = '" + tblName + "'";
			qry->SQL->Text = sqlRows;
			qry->Open();
			if (!qry->Eof)
				tableInfo->AddPair("row_count", new TJSONNumber(qry->FieldByName("cnt")->AsLargeInt));
			else
				tableInfo->AddPair("row_count", new TJSONNumber(0));
			qry->Close();

			// Get FK count
			String sqlFks = "SELECT COUNT(*) AS cnt FROM information_schema.table_constraints "
				"WHERE table_schema = '" + schema + "' AND table_name = '" + tblName + "' "
				"AND constraint_type = 'FOREIGN KEY'";
			qry->SQL->Text = sqlFks;
			qry->Open();
			tableInfo->AddPair("fk_count", new TJSONNumber(qry->FieldByName("cnt")->AsInteger));
			qry->Close();
		}
		catch (Exception &E)
		{
			tableInfo->AddPair("error", E.Message);
		}
		delete qry;

		tables->Add(tableInfo);
	}

	delete names;

	result->AddPair("tables", tables);
	String jsonResult = result->ToJSON();
	delete result;

	return jsonResult;
}
//---------------------------------------------------------------------------
