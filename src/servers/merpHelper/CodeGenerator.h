//---------------------------------------------------------------------------
#pragma once

#include "QueryDataTypes.h"
#include "IQueryStorage.h"
#include "TemplateEngine.h"
#include <string>
#include <vector>
#include <map>
#include <System.Classes.hpp>

class CodeGenerator {
public:
	explicit CodeGenerator(IQueryStorage* storage);

	// Single-query struct generation (InParam/OutParam/TypeConversion)
	String GenerateTemplateClassCode(const String& queryName,
		const std::vector<ParamInfo>& inParams,
		const std::vector<ParamInfo>& outParams);

	// Render arbitrary template for a query (looked up by name in storage)
	String RenderQueryTemplate(const String& queryName, const String& tpl);

	// Build context for a query (looked up by name in storage)
	TemplateContext GetQueryContext(const String& queryName);

	// Build context from raw params (static, no storage needed)
	static TemplateContext BuildTemplateClassContext(const String& queryName,
		const std::vector<ParamInfo>& inParams,
		const std::vector<ParamInfo>& outParams);

	// File generators — return content as String
	String GenerateSQLQueriesHeader();
	String GenerateAssignCommonFields();
	String GenerateDataAccess();
	String GenerateFileSql(bool pg = false);
	String GenerateModuleStructs(int moduleID, const String& moduleName);

	// Generate all files at once into a directory
	void GenerateAll(const String& outputDir);

	// Utilities
	static String Render(const char* tpl, const std::map<String, String>& vars);
	static bool WriteFileBOM(const String& path, const String& content);

private:
	IQueryStorage* m_storage;

	String BuildSqlEntries(const std::vector<QueryRecord>& records, bool usePgSql);
	static void BOM_UTF_8(std::ofstream& outFile);
};
//---------------------------------------------------------------------------
