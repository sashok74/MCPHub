//---------------------------------------------------------------------------
// McpQueryAnalyzer.h — SQL metadata extraction (replaces SqlQueryProcessor)
// Returns vectors instead of TMemTableEh, no EhLib dependency.
//---------------------------------------------------------------------------

#ifndef McpQueryAnalyzerH
#define McpQueryAnalyzerH

#include "QueryDataTypes.h"
#include <FireDAC.Comp.Client.hpp>
#include <string>
#include <vector>
#include <memory>

//---------------------------------------------------------------------------

class McpQueryAnalyzer {
public:
	explicit McpQueryAnalyzer(TFDConnection* conn);

	// Extract input params via sp_describe_undeclared_parameters
	std::vector<ParamRecord> AnalyzeInputParams(const std::string& sql);

	// Extract output fields via sp_describe_first_result_set (+ fallback)
	std::vector<FieldRecord> AnalyzeOutputFields(const std::string& sql);

	// Generate CRUD SQL from table name (via INFORMATION_SCHEMA)
	std::string GenerateSQL(const std::string& tableName, const std::string& operation);

private:
	TFDConnection* FConn;

	// :param -> @param with _dub handling for duplicates
	std::string CorrectSqlSyntax(const std::string& sql);

	// Escape single quotes for dynamic SQL
	std::string EscapeSql(const std::string& sql);

	// Extract schema.table from INSERT/UPDATE SQL
	std::pair<std::string, std::string> ExtractTableSchemaAndName(const std::string& sql);

	// INFORMATION_SCHEMA checks
	bool GetIsNullable(const std::string& schema, const std::string& table, const std::string& column);
	bool GetIsKeyField(const std::string& schema, const std::string& table, const std::string& column);

	// Output fallback — execute with empty params and read Fields
	std::vector<FieldRecord> FetchOutputFieldsFallback(const std::string& originalSql, const std::string& correctedSql);
};

//---------------------------------------------------------------------------
#endif
