//---------------------------------------------------------------------------

#include <vcl.h>
#pragma hdrstop

#include "MSSQLProvider.h"
#include <System.SysUtils.hpp>
#include <vector>
#include <set>
#include <sql.h>
#include <sqlext.h>
#pragma comment(lib, "odbc32.lib")
//---------------------------------------------------------------------------
#pragma package(smart_init)
//---------------------------------------------------------------------------

TMSSQLProvider::TMSSQLProvider(TFDConnection *mainConnection)
	: TDbProviderBase(mainConnection)
{
}
//---------------------------------------------------------------------------

TMSSQLProvider::~TMSSQLProvider()
{
}
//---------------------------------------------------------------------------

String TMSSQLProvider::GetProviderName() const
{
	return "Microsoft SQL Server";
}
//---------------------------------------------------------------------------

String TMSSQLProvider::GetDefaultSchema() const
{
	return "dbo";
}
//---------------------------------------------------------------------------

String TMSSQLProvider::ListTables(const String &schemaFilter, bool includeViews)
{
	String sql;

	if (includeViews)
	{
		// Include both tables and views
		sql =
			"SELECT "
			"  s.name AS [schema], "
			"  o.name AS [table], "
			"  o.type_desc AS [type], "
			"  ISNULL(p.rows, 0) AS [row_count], "
			"  ISNULL(ep.value, '') AS [description] "
			"FROM sys.objects o "
			"INNER JOIN sys.schemas s ON o.schema_id = s.schema_id "
			"LEFT JOIN sys.partitions p ON o.object_id = p.object_id AND p.index_id IN (0,1) "
			"LEFT JOIN sys.extended_properties ep ON ep.major_id = o.object_id "
			"  AND ep.minor_id = 0 AND ep.name = 'MS_Description' "
			"WHERE o.type IN ('U','V') ";
	}
	else
	{
		// Tables only
		sql =
			"SELECT "
			"  s.name AS [schema], "
			"  t.name AS [table], "
			"  t.type_desc AS [type], "
			"  p.rows AS [row_count], "
			"  ISNULL(ep.value, '') AS [description] "
			"FROM sys.tables t "
			"INNER JOIN sys.schemas s ON t.schema_id = s.schema_id "
			"LEFT JOIN sys.partitions p ON t.object_id = p.object_id AND p.index_id IN (0,1) "
			"LEFT JOIN sys.extended_properties ep ON ep.major_id = t.object_id "
			"  AND ep.minor_id = 0 AND ep.name = 'MS_Description' ";
	}

	// Apply schema filter if provided
	if (!schemaFilter.IsEmpty())
	{
		String safeSchema = StringReplace(schemaFilter, "'", "''", TReplaceFlags() << rfReplaceAll);
		if (includeViews)
			sql = sql + "AND s.name = '" + safeSchema + "' ";
		else
			sql = sql + "WHERE s.name = '" + safeSchema + "' ";
	}

	sql = sql + "ORDER BY s.name, " + (includeViews ? "o.name" : "t.name");

	return RunQueryToJSON(sql);
}
//---------------------------------------------------------------------------

String TMSSQLProvider::BuildColumnSchemaSQL()
{
	// This query is used by GetTableSchema with :tableName parameter
	return
		"DECLARE @fullTableName NVARCHAR(256) = :tableName; "
		"DECLARE @schema NVARCHAR(128) = ISNULL(PARSENAME(@fullTableName, 2), 'dbo'); "
		"DECLARE @table NVARCHAR(128) = PARSENAME(@fullTableName, 1); "
		"SELECT "
		"  c.COLUMN_NAME AS [field], "
		"  c.DATA_TYPE AS [type], "
		"  c.CHARACTER_MAXIMUM_LENGTH AS [max_length], "
		"  c.NUMERIC_PRECISION AS [precision], "
		"  c.NUMERIC_SCALE AS [scale], "
		"  c.IS_NULLABLE AS [nullable], "
		"  c.COLUMN_DEFAULT AS [default_value], "
		"  CASE WHEN pk.COLUMN_NAME IS NOT NULL THEN 1 ELSE 0 END AS [is_primary_key], "
		"  CASE WHEN fk.COLUMN_NAME IS NOT NULL THEN 1 ELSE 0 END AS [is_foreign_key], "
		"  CASE WHEN idx.COLUMN_NAME IS NOT NULL THEN 1 ELSE 0 END AS [is_indexed], "
		"  fk.ReferencedTable AS [references_table], "
		"  fk.ReferencedColumn AS [references_column], "
		"  ISNULL(ep.value, '') AS [description] "
		"FROM INFORMATION_SCHEMA.COLUMNS c "
		"LEFT JOIN ( "
		"  SELECT kcu.COLUMN_NAME "
		"  FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS tc "
		"  JOIN INFORMATION_SCHEMA.KEY_COLUMN_USAGE kcu ON tc.CONSTRAINT_NAME = kcu.CONSTRAINT_NAME "
		"  WHERE tc.CONSTRAINT_TYPE = 'PRIMARY KEY' "
		"    AND tc.TABLE_NAME = @table AND tc.TABLE_SCHEMA = @schema "
		") pk ON c.COLUMN_NAME = pk.COLUMN_NAME "
		"LEFT JOIN ( "
		"  SELECT kcu.COLUMN_NAME, "
		"    OBJECT_SCHEMA_NAME(fkc.referenced_object_id) + '.' + OBJECT_NAME(fkc.referenced_object_id) AS ReferencedTable, "
		"    COL_NAME(fkc.referenced_object_id, fkc.referenced_column_id) AS ReferencedColumn "
		"  FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS tc "
		"  JOIN INFORMATION_SCHEMA.KEY_COLUMN_USAGE kcu ON tc.CONSTRAINT_NAME = kcu.CONSTRAINT_NAME "
		"  JOIN sys.foreign_keys fk ON fk.name = tc.CONSTRAINT_NAME "
		"  JOIN sys.foreign_key_columns fkc ON fk.object_id = fkc.constraint_object_id "
		"    AND kcu.COLUMN_NAME = COL_NAME(fkc.parent_object_id, fkc.parent_column_id) "
		"  WHERE tc.CONSTRAINT_TYPE = 'FOREIGN KEY' "
		"    AND tc.TABLE_NAME = @table AND tc.TABLE_SCHEMA = @schema "
		") fk ON c.COLUMN_NAME = fk.COLUMN_NAME "
		"LEFT JOIN ( "
		"  SELECT DISTINCT col.name AS COLUMN_NAME "
		"  FROM sys.indexes i "
		"  JOIN sys.index_columns ic ON i.object_id = ic.object_id AND i.index_id = ic.index_id "
		"  JOIN sys.columns col ON ic.object_id = col.object_id AND ic.column_id = col.column_id "
		"  WHERE OBJECT_NAME(i.object_id) = @table AND OBJECT_SCHEMA_NAME(i.object_id) = @schema "
		") idx ON c.COLUMN_NAME = idx.COLUMN_NAME "
		"LEFT JOIN sys.extended_properties ep ON ep.major_id = OBJECT_ID(@schema + '.' + @table) "
		"  AND ep.minor_id = c.ORDINAL_POSITION AND ep.name = 'MS_Description' "
		"WHERE c.TABLE_NAME = @table AND c.TABLE_SCHEMA = @schema "
		"ORDER BY c.ORDINAL_POSITION";
}
//---------------------------------------------------------------------------

String TMSSQLProvider::GetTableSchema(const String &table)
{
	return RunQueryWithParam(BuildColumnSchemaSQL(), "tableName", table);
}
//---------------------------------------------------------------------------

String TMSSQLProvider::BuildRelationsSQL()
{
	return
		"DECLARE @fullTableName NVARCHAR(256) = :tableName; "
		"DECLARE @ObjectId INT = OBJECT_ID(@fullTableName); "
		""
		"SELECT "
		"  'referenced_by' AS [direction], "
		"  fk.name AS [constraint_name], "
		"  sch1.name + '.' + t1.name AS [from_table], "
		"  c1.name AS [from_column], "
		"  sch2.name + '.' + t2.name AS [to_table], "
		"  c2.name AS [to_column] "
		"FROM sys.foreign_keys fk "
		"INNER JOIN sys.foreign_key_columns fkc ON fk.object_id = fkc.constraint_object_id "
		"INNER JOIN sys.tables t1 ON fkc.parent_object_id = t1.object_id "
		"INNER JOIN sys.schemas sch1 ON t1.schema_id = sch1.schema_id "
		"INNER JOIN sys.columns c1 ON fkc.parent_object_id = c1.object_id AND fkc.parent_column_id = c1.column_id "
		"INNER JOIN sys.tables t2 ON fkc.referenced_object_id = t2.object_id "
		"INNER JOIN sys.schemas sch2 ON t2.schema_id = sch2.schema_id "
		"INNER JOIN sys.columns c2 ON fkc.referenced_object_id = c2.object_id AND fkc.referenced_column_id = c2.column_id "
		"WHERE fkc.referenced_object_id = @ObjectId "
		""
		"UNION ALL "
		""
		"SELECT "
		"  'references' AS [direction], "
		"  fk.name AS [constraint_name], "
		"  sch1.name + '.' + t1.name AS [from_table], "
		"  c1.name AS [from_column], "
		"  sch2.name + '.' + t2.name AS [to_table], "
		"  c2.name AS [to_column] "
		"FROM sys.foreign_keys fk "
		"INNER JOIN sys.foreign_key_columns fkc ON fk.object_id = fkc.constraint_object_id "
		"INNER JOIN sys.tables t1 ON fkc.parent_object_id = t1.object_id "
		"INNER JOIN sys.schemas sch1 ON t1.schema_id = sch1.schema_id "
		"INNER JOIN sys.columns c1 ON fkc.parent_object_id = c1.object_id AND fkc.parent_column_id = c1.column_id "
		"INNER JOIN sys.tables t2 ON fkc.referenced_object_id = t2.object_id "
		"INNER JOIN sys.schemas sch2 ON t2.schema_id = sch2.schema_id "
		"INNER JOIN sys.columns c2 ON fkc.referenced_object_id = c2.object_id AND fkc.referenced_column_id = c2.column_id "
		"WHERE fkc.parent_object_id = @ObjectId "
		""
		"ORDER BY [direction], [constraint_name]";
}
//---------------------------------------------------------------------------

String TMSSQLProvider::GetTableRelations(const String &table)
{
	return RunQueryWithParam(BuildRelationsSQL(), "tableName", table);
}
//---------------------------------------------------------------------------

String TMSSQLProvider::ListDatabases()
{
	String sql =
		"SELECT "
		"  d.name AS [database_name], "
		"  d.state_desc AS [state], "
		"  d.recovery_model_desc AS [recovery_model], "
		"  CAST(SUM(mf.size) * 8.0 / 1024 AS DECIMAL(12,2)) AS [size_mb] "
		"FROM sys.databases d "
		"LEFT JOIN sys.master_files mf ON d.database_id = mf.database_id "
		"GROUP BY d.name, d.state_desc, d.recovery_model_desc "
		"ORDER BY d.name";

	return RunQueryToJSON(sql);
}
//---------------------------------------------------------------------------

String TMSSQLProvider::GetTableSample(const String &table, int limit)
{
	String schema, tableName;
	ParseTableName(table, schema, tableName);

	String sql = "SELECT TOP " + IntToStr(limit) + " * FROM [" + schema + "].[" + tableName + "]";
	return RunQueryToJSON(sql);
}
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
// Helper: validate SQL identifier (A-Z, 0-9, underscore, dot)
//---------------------------------------------------------------------------
static bool IsValidSqlIdentifier(const String &name)
{
	for (int i = 1; i <= name.Length(); i++)
	{
		wchar_t ch = name[i];
		if (!((ch >= L'A' && ch <= L'Z') || (ch >= L'a' && ch <= L'z') ||
			  (ch >= L'0' && ch <= L'9') || ch == L'_' || ch == L'.'))
			return false;
	}
	return !name.IsEmpty();
}
//---------------------------------------------------------------------------

String TMSSQLProvider::ListObjects(const String &objType, const String &schemaFilter,
	const String &namePattern)
{
	String typeFilter = objType.IsEmpty() ? "ALL" : objType.UpperCase();
	String pattern = namePattern.IsEmpty() ? "%" : namePattern;

	String sql =
		"SELECT "
		"  s.name AS [schema], "
		"  o.name AS [name], "
		"  CASE o.type "
		"    WHEN 'U'  THEN 'TABLE' "
		"    WHEN 'V'  THEN 'VIEW' "
		"    WHEN 'P'  THEN 'PROCEDURE' "
		"    WHEN 'FN' THEN 'FUNCTION' "
		"    WHEN 'IF' THEN 'FUNCTION' "
		"    WHEN 'TF' THEN 'FUNCTION' "
		"    WHEN 'TR' THEN 'TRIGGER' "
		"    ELSE o.type_desc "
		"  END AS [type] "
		"FROM sys.objects o "
		"INNER JOIN sys.schemas s ON o.schema_id = s.schema_id "
		"WHERE o.type IN ('U','V','P','FN','IF','TF','TR') "
		"  AND (:typeFilter = 'ALL' OR "
		"       (:typeFilter = 'TABLE' AND o.type = 'U') OR "
		"       (:typeFilter = 'VIEW' AND o.type = 'V') OR "
		"       (:typeFilter = 'PROCEDURE' AND o.type = 'P') OR "
		"       (:typeFilter = 'FUNCTION' AND o.type IN ('FN','IF','TF')) OR "
		"       (:typeFilter = 'TRIGGER' AND o.type = 'TR')) "
		"  AND (:schemaFilter = '' OR s.name = :schemaFilter) "
		"  AND o.name LIKE :namePattern "
		"ORDER BY [type], s.name, o.name";

	TStringList *params = new TStringList();
	params->Add("typeFilter=" + typeFilter);
	params->Add("schemaFilter=" + schemaFilter);
	params->Add("namePattern=" + pattern);
	String result = RunQueryToJSON(sql, params);
	delete params;
	return result;
}
//---------------------------------------------------------------------------

String TMSSQLProvider::GetObjectDefinition(const String &objName, const String &objType)
{
	String sql =
		"SELECT "
		"  s.name AS [schema], "
		"  o.name AS [name], "
		"  o.type_desc AS [type], "
		"  m.definition "
		"FROM sys.sql_modules m "
		"INNER JOIN sys.objects o ON m.object_id = o.object_id "
		"INNER JOIN sys.schemas s ON o.schema_id = s.schema_id "
		"WHERE o.object_id = OBJECT_ID(:objectName)";

	return RunQueryWithParam(sql, "objectName", objName);
}
//---------------------------------------------------------------------------

String TMSSQLProvider::GetDependencies(const String &objName, const String &direction)
{
	String dir = direction.IsEmpty() ? "BOTH" : direction.UpperCase();
	String sql;

	if (dir == "USES")
	{
		sql =
			"SELECT DISTINCT "
			"  'uses' AS [direction], "
			"  OBJECT_SCHEMA_NAME(d.referenced_id) AS [schema], "
			"  OBJECT_NAME(d.referenced_id) AS [name], "
			"  o.type_desc AS [type] "
			"FROM sys.sql_expression_dependencies d "
			"INNER JOIN sys.objects o ON d.referenced_id = o.object_id "
			"WHERE d.referencing_id = OBJECT_ID(:objectName) "
			"ORDER BY [type], [schema], [name]";
	}
	else if (dir == "USED_BY")
	{
		sql =
			"SELECT DISTINCT "
			"  'used_by' AS [direction], "
			"  OBJECT_SCHEMA_NAME(d.referencing_id) AS [schema], "
			"  OBJECT_NAME(d.referencing_id) AS [name], "
			"  o.type_desc AS [type] "
			"FROM sys.sql_expression_dependencies d "
			"INNER JOIN sys.objects o ON d.referencing_id = o.object_id "
			"WHERE d.referenced_id = OBJECT_ID(:objectName) "
			"ORDER BY [type], [schema], [name]";
	}
	else // BOTH
	{
		sql =
			"SELECT DISTINCT "
			"  'uses' AS [direction], "
			"  OBJECT_SCHEMA_NAME(d.referenced_id) AS [schema], "
			"  OBJECT_NAME(d.referenced_id) AS [name], "
			"  o.type_desc AS [type] "
			"FROM sys.sql_expression_dependencies d "
			"INNER JOIN sys.objects o ON d.referenced_id = o.object_id "
			"WHERE d.referencing_id = OBJECT_ID(:objectName) "
			""
			"UNION ALL "
			""
			"SELECT DISTINCT "
			"  'used_by' AS [direction], "
			"  OBJECT_SCHEMA_NAME(d.referencing_id) AS [schema], "
			"  OBJECT_NAME(d.referencing_id) AS [name], "
			"  o.type_desc AS [type] "
			"FROM sys.sql_expression_dependencies d "
			"INNER JOIN sys.objects o ON d.referencing_id = o.object_id "
			"WHERE d.referenced_id = OBJECT_ID(:objectName) "
			"ORDER BY [direction], [type], [schema], [name]";
	}

	return RunQueryWithParam(sql, "objectName", objName);
}
//---------------------------------------------------------------------------

String TMSSQLProvider::SearchColumns(const String &pattern)
{
	String sql =
		"SELECT "
		"  c.TABLE_SCHEMA AS [schema], "
		"  c.TABLE_NAME AS [table_name], "
		"  c.COLUMN_NAME AS [column_name], "
		"  c.DATA_TYPE AS [data_type], "
		"  c.CHARACTER_MAXIMUM_LENGTH AS [max_length], "
		"  c.IS_NULLABLE AS [nullable] "
		"FROM INFORMATION_SCHEMA.COLUMNS c "
		"WHERE c.COLUMN_NAME LIKE :pattern "
		"ORDER BY c.TABLE_SCHEMA, c.TABLE_NAME, c.COLUMN_NAME";

	return RunQueryWithParam(sql, "pattern", "%" + pattern + "%");
}
//---------------------------------------------------------------------------

String TMSSQLProvider::SearchObjectSource(const String &pattern)
{
	// Strip '%' from pattern to get clean substring for CHARINDEX context
	String patternClean = StringReplace(pattern, "%", "", TReplaceFlags() << rfReplaceAll);
	String patternLike = "%" + pattern + "%";

	String sql =
		"SELECT "
		"  s.name AS [schema], "
		"  o.name AS [object_name], "
		"  o.type_desc AS [type], "
		"  CASE WHEN CHARINDEX(:patternClean, m.definition) > 50 "
		"    THEN SUBSTRING(m.definition, CHARINDEX(:patternClean, m.definition) - 50, LEN(:patternClean) + 100) "
		"    ELSE LEFT(m.definition, LEN(:patternClean) + 100) "
		"  END AS [context] "
		"FROM sys.sql_modules m "
		"INNER JOIN sys.objects o ON m.object_id = o.object_id "
		"INNER JOIN sys.schemas s ON o.schema_id = s.schema_id "
		"WHERE m.definition LIKE :pattern "
		"ORDER BY o.type_desc, s.name, o.name";

	TStringList *params = new TStringList();
	params->Add("patternClean=" + patternClean);
	params->Add("pattern=" + patternLike);
	String result = RunQueryToJSON(sql, params);
	delete params;
	return result;
}
//---------------------------------------------------------------------------

String TMSSQLProvider::ProfileColumn(const String &table, const String &column)
{
	// Validate identifiers to prevent SQL injection
	if (!IsValidSqlIdentifier(table))
		return MakeErrorJSON("Invalid table name. Only A-Z, 0-9, underscore, dot allowed.");
	if (!IsValidSqlIdentifier(column))
		return MakeErrorJSON("Invalid column name. Only A-Z, 0-9, underscore, dot allowed.");

	// Stats query
	String sqlStats =
		"SELECT "
		"  COUNT(*) AS [total_rows], "
		"  COUNT(DISTINCT [" + column + "]) AS [distinct_count], "
		"  SUM(CASE WHEN [" + column + "] IS NULL THEN 1 ELSE 0 END) AS [null_count], "
		"  MIN(CAST([" + column + "] AS NVARCHAR(MAX))) AS [min_value], "
		"  MAX(CAST([" + column + "] AS NVARCHAR(MAX))) AS [max_value] "
		"FROM " + table;

	String statsResult = RunQueryToJSON(sqlStats);

	// Top 10 frequent values
	String sqlTop =
		"SELECT TOP 10 "
		"  CAST([" + column + "] AS NVARCHAR(MAX)) AS [value], "
		"  COUNT(*) AS [count] "
		"FROM " + table + " "
		"WHERE [" + column + "] IS NOT NULL "
		"GROUP BY [" + column + "] "
		"ORDER BY COUNT(*) DESC";

	String topResult = RunQueryToJSON(sqlTop);

	// Combine results
	TJSONObject *resp = new TJSONObject();
	TJSONValue *statsVal = TJSONObject::ParseJSONValue(statsResult);
	TJSONValue *topVal = TJSONObject::ParseJSONValue(topResult);
	if (statsVal)
		resp->AddPair("statistics", statsVal);
	else
		resp->AddPair("statistics_error", statsResult);
	if (topVal)
		resp->AddPair("top_values", topVal);
	else
		resp->AddPair("top_values_error", topResult);

	String result = resp->ToJSON();
	delete resp;
	return result;
}
//---------------------------------------------------------------------------

static String GetOdbcDiag(SQLSMALLINT handleType, SQLHANDLE handle)
{
	SQLWCHAR state[6], msg[1024];
	SQLINTEGER nativeErr;
	SQLSMALLINT msgLen;
	if (SQLGetDiagRecW(handleType, handle, 1, state, &nativeErr,
		msg, 1024, &msgLen) == SQL_SUCCESS)
		return String(state, 5) + ": " + String(msg, msgLen);
	return "";
}
//---------------------------------------------------------------------------

String TMSSQLProvider::GetShowplanXml(const String &sql)
{
	// Build ODBC connection string from cached connection params
	String server, database, user, password, port, osAuthent;
	FConnParamsLock->Acquire();
	try {
		server   = FConnParams->Values["Server"];
		database = FConnParams->Values["Database"];
		user     = FConnParams->Values["User_Name"];
		password = FConnParams->Values["Password"];
		port     = FConnParams->Values["Port"];
		osAuthent = FConnParams->Values["OSAuthent"];
	} __finally { FConnParamsLock->Release(); }

	String connStr = "DRIVER={ODBC Driver 17 for SQL Server};"
		"SERVER=" + server + (port.IsEmpty() ? "" : "," + port) + ";"
		"DATABASE=" + database + ";";

	if (osAuthent.UpperCase() == "YES" || (user.IsEmpty() && password.IsEmpty()))
		connStr += "Trusted_Connection=yes;";
	else
		connStr += "UID=" + user + ";PWD=" + password + ";";

	SQLHENV hEnv = SQL_NULL_HANDLE;
	SQLHDBC hDbc = SQL_NULL_HANDLE;
	SQLHSTMT hStmt = SQL_NULL_HANDLE;
	String xmlResult;

	try {
		// Allocate environment + connection
		SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &hEnv);
		SQLSetEnvAttr(hEnv, SQL_ATTR_ODBC_VERSION, (SQLPOINTER)SQL_OV_ODBC3, 0);
		SQLAllocHandle(SQL_HANDLE_DBC, hEnv, &hDbc);

		// Connect
		SQLWCHAR outConnStr[1024];
		SQLSMALLINT outLen;
		SQLRETURN rc = SQLDriverConnectW(hDbc, NULL,
			(SQLWCHAR*)connStr.c_str(), SQL_NTS,
			outConnStr, 1024, &outLen, SQL_DRIVER_NOPROMPT);
		if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO)
			throw Exception("ODBC connect failed: " + GetOdbcDiag(SQL_HANDLE_DBC, hDbc));

		// SET SHOWPLAN_XML ON
		SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &hStmt);
		rc = SQLExecDirectW(hStmt, (SQLWCHAR*)L"SET SHOWPLAN_XML ON", SQL_NTS);
		if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO)
			throw Exception("SET SHOWPLAN_XML ON failed: " + GetOdbcDiag(SQL_HANDLE_STMT, hStmt));
		SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
		hStmt = SQL_NULL_HANDLE;

		// Execute user's SQL — SQL Server returns XML plan instead of data
		SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &hStmt);
		rc = SQLExecDirectW(hStmt, (SQLWCHAR*)String(sql).c_str(), SQL_NTS);
		if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO)
			throw Exception("SHOWPLAN exec failed (rc=" + IntToStr((int)rc) + "): "
				+ GetOdbcDiag(SQL_HANDLE_STMT, hStmt));

		// Fetch first row (ODBC requires SQLFetch before SQLGetData)
		rc = SQLFetch(hStmt);
		if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO)
			throw Exception("No plan row from SHOWPLAN (rc=" + IntToStr((int)rc) + "): "
				+ GetOdbcDiag(SQL_HANDLE_STMT, hStmt));

		// Read XML result (may be large, read in chunks)
		SQLWCHAR buf[8192];
		SQLLEN indicator;
		while (true) {
			rc = SQLGetData(hStmt, 1, SQL_C_WCHAR, buf, sizeof(buf), &indicator);
			if (rc == SQL_NO_DATA) break;
			if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) break;
			if (indicator == SQL_NULL_DATA) break;

			SQLLEN chars;
			if (rc == SQL_SUCCESS) {
				// All remaining data fit in buffer
				chars = indicator / sizeof(SQLWCHAR);
			} else {
				// Buffer full, more data to come
				chars = sizeof(buf) / sizeof(SQLWCHAR) - 1;
			}
			xmlResult += String(buf, (int)chars);
			if (rc == SQL_SUCCESS) break;
		}

		// SET SHOWPLAN_XML OFF
		SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
		hStmt = SQL_NULL_HANDLE;
		SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &hStmt);
		SQLExecDirectW(hStmt, (SQLWCHAR*)L"SET SHOWPLAN_XML OFF", SQL_NTS);
	}
	__finally {
		if (hStmt) SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
		if (hDbc) { SQLDisconnect(hDbc); SQLFreeHandle(SQL_HANDLE_DBC, hDbc); }
		if (hEnv) SQLFreeHandle(SQL_HANDLE_ENV, hEnv);
	}

	return xmlResult;
}
//---------------------------------------------------------------------------

String TMSSQLProvider::ExplainQuery(const String &sql)
{
	if (sql.Trim().IsEmpty())
		return MakeErrorJSON("Empty SQL query");

	// Approach: Get execution plan XML via raw ODBC (bypasses FireDAC's
	// metadata layer that crashes on SHOWPLAN_XML result sets), then
	// parse the XML via FireDAC + XQuery into a structured tree.
	TFDConnection *conn = NULL;
	TFDQuery *qry = NULL;
	String result;
	try
	{
		// Phase 1: Get raw XML plan via ODBC
		String xmlPlan = GetShowplanXml(sql.Trim());
		if (xmlPlan.IsEmpty())
			return MakeErrorJSON("No execution plan returned");

		// Phase 2: Parse XML via FireDAC + XQuery
		conn = CreateWorkerConnection();
		qry = new TFDQuery(NULL);
		qry->Connection = conn;

		// Escape XML for SQL string literal
		String escapedXml = StringReplace(xmlPlan, "'", "''",
			TReplaceFlags() << rfReplaceAll);

		qry->SQL->Text =
			"DECLARE @plan XML = N'" + escapedXml + "'; "
			";WITH XMLNAMESPACES "
			"(DEFAULT 'http://schemas.microsoft.com/sqlserver/2004/07/showplan') "
			"SELECT "
			"  r.value('@NodeId', 'int') AS NodeId, "
			"  r.value('@PhysicalOp', 'nvarchar(128)') AS PhysicalOp, "
			"  r.value('@LogicalOp', 'nvarchar(128)') AS LogicalOp, "
			"  r.value('@EstimateRows', 'float') AS EstimateRows, "
			"  r.value('@EstimateCPU', 'float') AS EstimateCPU, "
			"  r.value('@EstimateIO', 'float') AS EstimateIO, "
			"  r.value('@AvgRowSize', 'int') AS AvgRowSize, "
			"  r.value('@EstimatedTotalSubtreeCost', 'float') AS TotalCost, "
			"  r.value('@Parallel', 'bit') AS IsParallel "
			"FROM @plan.nodes('//RelOp') AS t(r)";
		qry->Open();

		// Build text tree and JSON from XQuery results.
		// .nodes() returns in document order (DFS). We compute depth
		// by tracking a NodeId stack: in DFS traversal NodeIds increase,
		// so we pop the stack until the top is less than the current NodeId.
		String planText;
		TJSONArray *planRows = new TJSONArray();
		int rowCount = 0;
		std::vector<int> nodeStack;

		while (!qry->Eof)
		{
			int nodeId = qry->FieldByName("NodeId")->IsNull
				? 0 : qry->FieldByName("NodeId")->AsInteger;
			String physOp = qry->FieldByName("PhysicalOp")->IsNull
				? "" : qry->FieldByName("PhysicalOp")->AsString;
			String logOp = qry->FieldByName("LogicalOp")->IsNull
				? "" : qry->FieldByName("LogicalOp")->AsString;
			double estRows = qry->FieldByName("EstimateRows")->IsNull
				? 0.0 : qry->FieldByName("EstimateRows")->AsFloat;
			double estIO = qry->FieldByName("EstimateIO")->IsNull
				? 0.0 : qry->FieldByName("EstimateIO")->AsFloat;
			double estCPU = qry->FieldByName("EstimateCPU")->IsNull
				? 0.0 : qry->FieldByName("EstimateCPU")->AsFloat;
			int avgRowSize = qry->FieldByName("AvgRowSize")->IsNull
				? 0 : qry->FieldByName("AvgRowSize")->AsInteger;
			double totalCost = qry->FieldByName("TotalCost")->IsNull
				? 0.0 : qry->FieldByName("TotalCost")->AsFloat;
			bool parallel = !qry->FieldByName("IsParallel")->IsNull
				&& qry->FieldByName("IsParallel")->AsBoolean;

			// Compute depth from NodeId stack
			while (!nodeStack.empty() && nodeStack.back() >= nodeId)
				nodeStack.pop_back();
			int depth = static_cast<int>(nodeStack.size());
			nodeStack.push_back(nodeId);

			// Build text tree line
			String indent = StringOfChar(' ', depth * 3);
			planText += indent + "|--" + physOp;
			if (!logOp.IsEmpty() && logOp != physOp)
				planText += " (" + logOp + ")";
			planText += "\n";

			planText += indent + "   ";
			planText += "Rows: " + FormatFloat("0.#", estRows);
			planText += "  Cost: " + FormatFloat("0.######", totalCost);
			if (estIO > 0)
				planText += "  IO: " + FormatFloat("0.######", estIO);
			if (estCPU > 0)
				planText += "  CPU: " + FormatFloat("0.######", estCPU);
			if (avgRowSize > 0)
				planText += "  RowSize: " + IntToStr(avgRowSize);
			if (parallel)
				planText += "  [PARALLEL]";
			planText += "\n";

			// Build JSON row
			TJSONObject *jsonRow = new TJSONObject();
			jsonRow->AddPair("nodeId", new TJSONNumber(nodeId));
			jsonRow->AddPair("depth", new TJSONNumber(depth));
			jsonRow->AddPair("physicalOp", physOp);
			jsonRow->AddPair("logicalOp", logOp);
			jsonRow->AddPair("estimateRows", new TJSONNumber(estRows));
			jsonRow->AddPair("estimateIO", new TJSONNumber(estIO));
			jsonRow->AddPair("estimateCPU", new TJSONNumber(estCPU));
			jsonRow->AddPair("avgRowSize", new TJSONNumber(avgRowSize));
			jsonRow->AddPair("totalCost", new TJSONNumber(totalCost));
			jsonRow->AddPair("parallel",
				parallel ? static_cast<TJSONValue*>(new TJSONTrue())
						 : static_cast<TJSONValue*>(new TJSONFalse()));
			planRows->AddElement(jsonRow);
			rowCount++;

			qry->Next();
		}
		qry->Close();

		TJSONObject *resp = new TJSONObject();
		if (rowCount > 0)
		{
			resp->AddPair("plan_text", planText);
			resp->AddPair("plan_rows", planRows);
			resp->AddPair("row_count", new TJSONNumber(rowCount));
			resp->AddPair("format", "tree");
		}
		else
		{
			delete planRows;
			resp->AddPair("plan_text", "");
			resp->AddPair("row_count", new TJSONNumber(0));
			resp->AddPair("message",
				"No RelOp nodes found in the execution plan XML.");
		}
		result = resp->ToJSON();
		delete resp;
	}
	catch (Exception &E)
	{
		result = MakeErrorJSON(E.Message);
	}
	catch (...)
	{
		result = MakeErrorJSON("Unknown exception");
	}

	try { delete qry; } catch (...) {}
	try { if (conn) { conn->Connected = false; delete conn; } } catch (...) {}
	return result;
}
//---------------------------------------------------------------------------

String TMSSQLProvider::CompareTables(const String &table1, const String &table2)
{
	String sql =
		"DECLARE @t1 NVARCHAR(256) = :table1; "
		"DECLARE @t2 NVARCHAR(256) = :table2; "
		"DECLARE @s1 NVARCHAR(128) = ISNULL(PARSENAME(@t1, 2), 'dbo'); "
		"DECLARE @n1 NVARCHAR(128) = PARSENAME(@t1, 1); "
		"DECLARE @s2 NVARCHAR(128) = ISNULL(PARSENAME(@t2, 2), 'dbo'); "
		"DECLARE @n2 NVARCHAR(128) = PARSENAME(@t2, 1); "
		"SELECT "
		"  COALESCE(c1.COLUMN_NAME, c2.COLUMN_NAME) AS [column_name], "
		"  c1.DATA_TYPE AS [table1_type], "
		"  c1.CHARACTER_MAXIMUM_LENGTH AS [table1_max_length], "
		"  c2.DATA_TYPE AS [table2_type], "
		"  c2.CHARACTER_MAXIMUM_LENGTH AS [table2_max_length], "
		"  CASE "
		"    WHEN c1.COLUMN_NAME IS NULL THEN 'only_in_table2' "
		"    WHEN c2.COLUMN_NAME IS NULL THEN 'only_in_table1' "
		"    WHEN c1.DATA_TYPE <> c2.DATA_TYPE OR "
		"         ISNULL(c1.CHARACTER_MAXIMUM_LENGTH,0) <> ISNULL(c2.CHARACTER_MAXIMUM_LENGTH,0) "
		"      THEN 'type_mismatch' "
		"    ELSE 'match' "
		"  END AS [status] "
		"FROM (SELECT * FROM INFORMATION_SCHEMA.COLUMNS "
		"      WHERE TABLE_NAME = @n1 AND TABLE_SCHEMA = @s1) c1 "
		"FULL OUTER JOIN (SELECT * FROM INFORMATION_SCHEMA.COLUMNS "
		"      WHERE TABLE_NAME = @n2 AND TABLE_SCHEMA = @s2) c2 "
		"  ON c1.COLUMN_NAME = c2.COLUMN_NAME "
		"ORDER BY [status], [column_name]";

	TStringList *params = new TStringList();
	params->Add("table1=" + table1);
	params->Add("table2=" + table2);
	String result = RunQueryToJSON(sql, params);
	delete params;
	return result;
}
//---------------------------------------------------------------------------

String TMSSQLProvider::TraceFkPath(const String &fromTable, const String &toTable, int maxDepth)
{
	if (maxDepth < 1) maxDepth = 1;
	if (maxDepth > 10) maxDepth = 10;

	// Step 1: Load all FK edges
	String edgeSql =
		"SELECT "
		"  OBJECT_SCHEMA_NAME(fk.parent_object_id) + '.' + "
		"    OBJECT_NAME(fk.parent_object_id) AS [from_table], "
		"  OBJECT_SCHEMA_NAME(fk.referenced_object_id) + '.' + "
		"    OBJECT_NAME(fk.referenced_object_id) AS [to_table], "
		"  fk.name AS [fk_name], "
		"  COL_NAME(fkc.parent_object_id, fkc.parent_column_id) AS [from_column], "
		"  COL_NAME(fkc.referenced_object_id, fkc.referenced_column_id) AS [to_column] "
		"FROM sys.foreign_keys fk "
		"INNER JOIN sys.foreign_key_columns fkc ON fk.object_id = fkc.constraint_object_id";

	String edgeJson = RunQueryToJSON(edgeSql);
	TJSONObject *edgeResult = static_cast<TJSONObject*>(TJSONObject::ParseJSONValue(edgeJson));
	if (!edgeResult || edgeResult->GetValue("error"))
	{
		if (edgeResult) delete edgeResult;
		return edgeJson; // propagate error
	}

	TJSONArray *edgeRows = static_cast<TJSONArray*>(edgeResult->GetValue("rows"));
	if (!edgeRows)
	{
		delete edgeResult;
		return MakeErrorJSON("No FK data found");
	}

	// Resolve from/to using OBJECT_ID-like approach: normalize names
	String resolveSQL =
		"SELECT "
		"  OBJECT_SCHEMA_NAME(OBJECT_ID(:fromTable)) + '.' + "
		"    OBJECT_NAME(OBJECT_ID(:fromTable)) AS [resolved_from], "
		"  OBJECT_SCHEMA_NAME(OBJECT_ID(:toTable)) + '.' + "
		"    OBJECT_NAME(OBJECT_ID(:toTable)) AS [resolved_to]";

	TStringList *resolveParams = new TStringList();
	resolveParams->Add("fromTable=" + fromTable);
	resolveParams->Add("toTable=" + toTable);
	String resolveJson = RunQueryToJSON(resolveSQL, resolveParams);
	delete resolveParams;

	TJSONObject *resolveResult = static_cast<TJSONObject*>(TJSONObject::ParseJSONValue(resolveJson));
	String resolvedFrom, resolvedTo;
	if (resolveResult)
	{
		TJSONArray *rRows = static_cast<TJSONArray*>(resolveResult->GetValue("rows"));
		if (rRows && rRows->Count > 0)
		{
			TJSONObject *rRow = static_cast<TJSONObject*>(rRows->Items[0]);
			TJSONValue *rf = rRow->GetValue("resolved_from");
			TJSONValue *rt = rRow->GetValue("resolved_to");
			if (rf && !rf->Null) resolvedFrom = rf->Value();
			if (rt && !rt->Null) resolvedTo = rt->Value();
		}
		delete resolveResult;
	}

	if (resolvedFrom.IsEmpty())
		resolvedFrom = fromTable;
	if (resolvedTo.IsEmpty())
		resolvedTo = toTable;

	// Build adjacency list (bidirectional since FK can be traversed either way)
	struct FkEdge {
		String from_table;
		String to_table;
		String fk_name;
		String from_column;
		String to_column;
	};

	std::vector<FkEdge> edges;
	for (int i = 0; i < edgeRows->Count; i++)
	{
		TJSONObject *row = static_cast<TJSONObject*>(edgeRows->Items[i]);
		FkEdge e;
		e.from_table = row->GetValue("from_table")->Value();
		e.to_table = row->GetValue("to_table")->Value();
		e.fk_name = row->GetValue("fk_name")->Value();
		e.from_column = row->GetValue("from_column")->Value();
		e.to_column = row->GetValue("to_column")->Value();
		edges.push_back(e);
	}

	// BFS
	struct BfsNode {
		String table;
		std::vector<int> pathEdges; // indices into edges
		std::vector<bool> pathDirection; // true = forward, false = reversed
	};

	std::vector<BfsNode> queue;
	std::set<String> visited;
	TJSONArray *paths = new TJSONArray();

	BfsNode start;
	start.table = resolvedFrom;
	queue.push_back(start);
	visited.insert(resolvedFrom.UpperCase());

	int head = 0;
	while (head < (int)queue.size())
	{
		BfsNode current = queue[head];
		head++;

		if ((int)current.pathEdges.size() > maxDepth)
			continue;

		// Check if we reached the target
		if (current.table.UpperCase() == resolvedTo.UpperCase() &&
			current.pathEdges.size() > 0)
		{
			// Build path description
			TJSONArray *steps = new TJSONArray();
			for (int i = 0; i < (int)current.pathEdges.size(); i++)
			{
				FkEdge &e = edges[current.pathEdges[i]];
				TJSONObject *step = new TJSONObject();
				step->AddPair("fk_name", e.fk_name);
				if (current.pathDirection[i])
				{
					step->AddPair("from_table", e.from_table);
					step->AddPair("from_column", e.from_column);
					step->AddPair("to_table", e.to_table);
					step->AddPair("to_column", e.to_column);
					step->AddPair("direction", "forward");
				}
				else
				{
					step->AddPair("from_table", e.to_table);
					step->AddPair("from_column", e.to_column);
					step->AddPair("to_table", e.from_table);
					step->AddPair("to_column", e.from_column);
					step->AddPair("direction", "reverse");
				}
				steps->AddElement(step);
			}
			TJSONObject *pathObj = new TJSONObject();
			pathObj->AddPair("depth", new TJSONNumber((int)current.pathEdges.size()));
			pathObj->AddPair("steps", steps);
			paths->AddElement(pathObj);
			continue; // don't expand further from target
		}

		if ((int)current.pathEdges.size() >= maxDepth)
			continue;

		// Expand neighbors
		for (int i = 0; i < (int)edges.size(); i++)
		{
			String neighbor;
			bool forward;
			if (edges[i].from_table.UpperCase() == current.table.UpperCase())
			{
				neighbor = edges[i].to_table;
				forward = true;
			}
			else if (edges[i].to_table.UpperCase() == current.table.UpperCase())
			{
				neighbor = edges[i].from_table;
				forward = false;
			}
			else
				continue;

			// Allow revisiting the target even if visited (to find multiple paths)
			if (neighbor.UpperCase() != resolvedTo.UpperCase() &&
				visited.count(neighbor.UpperCase()) > 0)
				continue;

			BfsNode next;
			next.table = neighbor;
			next.pathEdges = current.pathEdges;
			next.pathEdges.push_back(i);
			next.pathDirection = current.pathDirection;
			next.pathDirection.push_back(forward);
			queue.push_back(next);

			if (neighbor.UpperCase() != resolvedTo.UpperCase())
				visited.insert(neighbor.UpperCase());
		}
	}

	TJSONObject *resp = new TJSONObject();
	resp->AddPair("from_table", resolvedFrom);
	resp->AddPair("to_table", resolvedTo);
	resp->AddPair("paths_found", new TJSONNumber(paths->Count));
	resp->AddPair("paths", paths);
	String result = resp->ToJSON();
	delete resp;
	delete edgeResult;
	return result;
}
//---------------------------------------------------------------------------

String TMSSQLProvider::GetModuleOverview(const String &schemaName)
{
	// Note: schemaName is actually a comma-separated list of table names for this tool
	// Validate and build IN list
	if (schemaName.Trim().IsEmpty())
		return MakeErrorJSON("Empty tables list");

	TStringList *tableList = new TStringList();
	tableList->Delimiter = L',';
	tableList->StrictDelimiter = true;
	tableList->DelimitedText = schemaName;

	String inList;
	for (int i = 0; i < tableList->Count; i++)
	{
		String name = tableList->Strings[i].Trim();
		if (name.IsEmpty()) continue;
		if (!IsValidSqlIdentifier(name))
		{
			delete tableList;
			return MakeErrorJSON("Invalid table name: " + name + ". Only A-Z, 0-9, underscore, dot allowed.");
		}
		if (!inList.IsEmpty()) inList = inList + ", ";
		inList = inList + "N'" + name + "'";
	}
	delete tableList;

	if (inList.IsEmpty())
		return MakeErrorJSON("No valid table names provided");

	// Query: columns + types + PK + row count for each table
	String sql =
		"SELECT "
		"  c.TABLE_SCHEMA AS [schema], "
		"  c.TABLE_NAME AS [table_name], "
		"  c.COLUMN_NAME AS [column_name], "
		"  c.DATA_TYPE AS [data_type], "
		"  c.CHARACTER_MAXIMUM_LENGTH AS [max_length], "
		"  c.IS_NULLABLE AS [nullable], "
		"  CASE WHEN pk.COLUMN_NAME IS NOT NULL THEN 1 ELSE 0 END AS [is_pk], "
		"  CASE WHEN fkcol.COLUMN_NAME IS NOT NULL THEN 1 ELSE 0 END AS [is_fk], "
		"  fkcol.RefTable AS [fk_references] "
		"FROM INFORMATION_SCHEMA.COLUMNS c "
		"LEFT JOIN ( "
		"  SELECT kcu.TABLE_SCHEMA, kcu.TABLE_NAME, kcu.COLUMN_NAME "
		"  FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS tc "
		"  JOIN INFORMATION_SCHEMA.KEY_COLUMN_USAGE kcu "
		"    ON tc.CONSTRAINT_NAME = kcu.CONSTRAINT_NAME "
		"    AND tc.TABLE_SCHEMA = kcu.TABLE_SCHEMA "
		"  WHERE tc.CONSTRAINT_TYPE = 'PRIMARY KEY' "
		") pk ON c.TABLE_SCHEMA = pk.TABLE_SCHEMA "
		"  AND c.TABLE_NAME = pk.TABLE_NAME "
		"  AND c.COLUMN_NAME = pk.COLUMN_NAME "
		"LEFT JOIN ( "
		"  SELECT kcu.TABLE_SCHEMA, kcu.TABLE_NAME, kcu.COLUMN_NAME, "
		"    OBJECT_SCHEMA_NAME(fkc.referenced_object_id) + '.' + "
		"      OBJECT_NAME(fkc.referenced_object_id) AS RefTable "
		"  FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS tc "
		"  JOIN INFORMATION_SCHEMA.KEY_COLUMN_USAGE kcu "
		"    ON tc.CONSTRAINT_NAME = kcu.CONSTRAINT_NAME "
		"    AND tc.TABLE_SCHEMA = kcu.TABLE_SCHEMA "
		"  JOIN sys.foreign_keys fk ON fk.name = tc.CONSTRAINT_NAME "
		"  JOIN sys.foreign_key_columns fkc ON fk.object_id = fkc.constraint_object_id "
		"    AND kcu.COLUMN_NAME = COL_NAME(fkc.parent_object_id, fkc.parent_column_id) "
		"  WHERE tc.CONSTRAINT_TYPE = 'FOREIGN KEY' "
		") fkcol ON c.TABLE_SCHEMA = fkcol.TABLE_SCHEMA "
		"  AND c.TABLE_NAME = fkcol.TABLE_NAME "
		"  AND c.COLUMN_NAME = fkcol.COLUMN_NAME "
		"WHERE c.TABLE_NAME IN (" + inList + ") "
		"   OR c.TABLE_SCHEMA + '.' + c.TABLE_NAME IN (" + inList + ") "
		"ORDER BY c.TABLE_SCHEMA, c.TABLE_NAME, c.ORDINAL_POSITION";

	String columnsResult = RunQueryToJSON(sql);

	// Row counts
	String countSql =
		"SELECT "
		"  s.name AS [schema], "
		"  t.name AS [table_name], "
		"  p.rows AS [row_count] "
		"FROM sys.tables t "
		"INNER JOIN sys.schemas s ON t.schema_id = s.schema_id "
		"LEFT JOIN sys.partitions p ON t.object_id = p.object_id AND p.index_id IN (0,1) "
		"WHERE t.name IN (" + inList + ") "
		"   OR s.name + '.' + t.name IN (" + inList + ")";

	String countsResult = RunQueryToJSON(countSql);

	TJSONObject *resp = new TJSONObject();
	TJSONValue *colVal = TJSONObject::ParseJSONValue(columnsResult);
	TJSONValue *cntVal = TJSONObject::ParseJSONValue(countsResult);
	if (colVal)
		resp->AddPair("columns", colVal);
	else
		resp->AddPair("columns_error", columnsResult);
	if (cntVal)
		resp->AddPair("row_counts", cntVal);
	else
		resp->AddPair("row_counts_error", countsResult);

	String result = resp->ToJSON();
	delete resp;
	return result;
}

//---------------------------------------------------------------------------
