//---------------------------------------------------------------------------
#include <vcl.h>
#pragma hdrstop

#include "CodeGenerator.h"
#include "UcodeUtf8.h"
#include "CodeTemplates.h"
#include "ParamCollector.h"
#include "DataTypeMapper.h"
#include "AlignedBlock.h"
#include <fstream>
#include <set>
#include <algorithm>
//---------------------------------------------------------------------------

CodeGenerator::CodeGenerator(IQueryStorage* storage)
	: m_storage(storage)
{
}
//---------------------------------------------------------------------------

TemplateContext CodeGenerator::BuildTemplateClassContext(
	const String& queryName,
	const std::vector<ParamInfo>& inParams,
	const std::vector<ParamInfo>& outParams)
{
	TemplateContext ctx;
	ctx.Set("QUERY_NAME", queryName);

	// --- IN key field ---
	String inKeyField = "m_iID";
	String inKeyDbName = "ID";
	for (const auto &p : inParams) {
		if (p.IsKey) { inKeyField = u(p.MemberName); inKeyDbName = u(p.DbName); break; }
	}
	ctx.Set("IN_KEY_FIELD_NAME", inKeyField);
	ctx.Set("IN_HAS_EXTRA_KEY", inKeyField != "m_iID");

	// --- OUT key field ---
	String outKeyField = "m_iID";
	String outKeyDbName = "ID";
	bool outHasKey = false;
	for (const auto &p : outParams) {
		if (p.IsKey) {
			outKeyField = u(p.MemberName);
			outKeyDbName = u(p.DbName);
			outHasKey = true;
			break;
		}
	}
	ctx.Set("OUT_KEY_FIELD_NAME", outKeyField);
	ctx.Set("OUT_HAS_EXTRA_KEY", outKeyField != "m_iID");

	// --- PID / TID ---
	String pidField = "", tidField = "";
	for (const auto &p : outParams) { if (p.IsPid) { pidField = u(p.MemberName); break; } }
	for (const auto &p : outParams) { if (p.IsTid) { tidField = u(p.MemberName); break; } }
	ctx.Set("HAS_PID", !pidField.IsEmpty());
	ctx.Set("HAS_TID", !tidField.IsEmpty());
	ctx.Set("PID_FIELD_NAME", pidField);
	ctx.Set("TID_FIELD_NAME", tidField);

	// --- Helper: set common validation variables ---
	auto setValidationVars = [](TemplateContext& pc, const ParamInfo& p) {
		String st = u(p.sqlType);
		bool isStr = (st.Pos("char") > 0 || st.Pos("text") > 0);
		pc.Set("IS_STRING",    isStr);
		pc.Set("IS_NULLABLE",  p.IsNullable);
		pc.Set("MAX_LENGTH",   String(p.MaxLength));
		pc.Set("IS_KEY_FIELD", p.IsKey);
		pc.Set("FIELD_TYPE",   st);
		pc.Set("PRECISION",    String(p.Precision));
	};

	// --- IN_PARAMS list ---
	std::vector<TemplateContext> inList;
	for (const auto &p : inParams) {
		TemplateContext pc;
		pc.Set("CPP_TYPE",      u(p.CppType));
		pc.Set("MEMBER_NAME",   u(p.MemberName));
		pc.Set("DB_NAME",       u(p.DbName));
		pc.Set("DEFAULT_VALUE", u(p.DefaultValue));
		pc.Set("HAS_NULL_VALUE", !p.NullValue.empty());
		if (!p.NullValue.empty())
			pc.Set("NULL_VALUE", u(p.NullValue));
		setValidationVars(pc, p);
		inList.push_back(pc);
	}
	ctx.SetList("IN_PARAMS", inList);

	// --- OUT_PARAMS list ---
	std::vector<TemplateContext> outList;
	for (const auto &p : outParams) {
		TemplateContext pc;
		pc.Set("CPP_TYPE",      u(p.CppType));
		pc.Set("MEMBER_NAME",   u(p.MemberName));
		pc.Set("DB_NAME",       u(p.DbName));
		pc.Set("DEFAULT_VALUE", u(p.DefaultValue));

		// Precompute DISPLAY_EXPR
		pc.Set("DISPLAY_EXPR",
			DataTypeMapper::getWstringBySQLType(
				u(p.sqlType), u(p.MemberName), p.DisplayZero));

		// Precompute FROM_ROW_LINE
		String dbName = (p.MemberName == "m_iID" && !p.IsKey && outKeyField != "m_iID")
						? outKeyDbName : u(p.DbName);
		if (p.IsKey)
			dbName = outKeyDbName;

		String fromRowLine;
		if (p.MemberName == "m_iID" && !outHasKey)
			fromRowLine = "        row.getUniqueID(m.m_iID);";
		else
			fromRowLine = "        row.fill(\"" + dbName + "\", m." +
						  u(p.MemberName) + ");";
		pc.Set("FROM_ROW_LINE", fromRowLine);

		setValidationVars(pc, p);
		outList.push_back(pc);
	}
	ctx.SetList("OUT_PARAMS", outList);

	return ctx;
}
//---------------------------------------------------------------------------

String CodeGenerator::GenerateTemplateClassCode(
	const String& queryName,
	const std::vector<ParamInfo>& inParams,
	const std::vector<ParamInfo>& outParams)
{
	auto ctx = BuildTemplateClassContext(queryName, inParams, outParams);
	String out, err;
	if (!TemplateEngine::TryRenderEx(TPL_QUERY_TYPES, ctx, out, err))
		throw Exception("Template error in TPL_QUERY_TYPES: " + err);
	return out;
}
//---------------------------------------------------------------------------

TemplateContext CodeGenerator::GetQueryContext(const String& queryName)
{
	auto qr = m_storage->GetQueryByName(utf8(queryName));
	auto params = m_storage->GetInputParams(qr.queryID);
	auto fields = m_storage->GetOutputFields(qr.queryID);
	std::vector<ParamInfo> inParams, outParams;
	ParamCollector::CollectFromStorage(params, fields, inParams, outParams);
	auto ctx = BuildTemplateClassContext(queryName, inParams, outParams);

	// Extra variables from QueryRecord
	std::string pascalName = ParamCollector::QueryNameToMethodName(qr.queryName);
	ctx.Set("QUERY_NAME_PASCAL", u(pascalName));
	ctx.Set("MODULE_NAME", u(qr.moduleName));
	ctx.Set("TABLE_NAME", u(qr.tableName));
	ctx.Set("DESCRIPTION", u(qr.description));
	ctx.Set("QUERY_TYPE", u(qr.queryType));
	ctx.Set("IN_COUNT", String(static_cast<int>(inParams.size())));
	ctx.Set("OUT_COUNT", String(static_cast<int>(outParams.size())));

	// Dataset mapping variables
	bool hasDs = !qr.dsName.empty() || !qr.dsQueryI.empty();
	ctx.Set("HAS_DS", hasDs);
	ctx.Set("DS_NAME", u(qr.dsName));
	ctx.Set("DS_QUERY_S", u(qr.dsQueryS));
	ctx.Set("DS_QUERY_I", u(qr.dsQueryI));
	ctx.Set("DS_QUERY_U", u(qr.dsQueryU));
	ctx.Set("DS_QUERY_D", u(qr.dsQueryD));
	ctx.Set("DS_QUERY_ONE_S", u(qr.dsQueryOneS));
	// PascalCase variants
	ctx.Set("DS_QUERY_S_PASCAL", qr.dsQueryS.empty() ? String("") :
		u(ParamCollector::QueryNameToMethodName(qr.dsQueryS)));
	ctx.Set("DS_QUERY_I_PASCAL", qr.dsQueryI.empty() ? String("") :
		u(ParamCollector::QueryNameToMethodName(qr.dsQueryI)));
	ctx.Set("DS_QUERY_U_PASCAL", qr.dsQueryU.empty() ? String("") :
		u(ParamCollector::QueryNameToMethodName(qr.dsQueryU)));
	ctx.Set("DS_QUERY_D_PASCAL", qr.dsQueryD.empty() ? String("") :
		u(ParamCollector::QueryNameToMethodName(qr.dsQueryD)));
	ctx.Set("DS_QUERY_ONE_S_PASCAL", qr.dsQueryOneS.empty() ? String("") :
		u(ParamCollector::QueryNameToMethodName(qr.dsQueryOneS)));

	return ctx;
}
//---------------------------------------------------------------------------

String CodeGenerator::RenderQueryTemplate(const String& queryName,
										  const String& tpl)
{
	auto ctx = GetQueryContext(queryName);
	return TemplateEngine::RenderEx(AnsiString(tpl).c_str(), ctx);
}
//---------------------------------------------------------------------------

String CodeGenerator::GenerateSQLQueriesHeader()
{
	auto queryNames = m_storage->GetAllQueryNames();

	String enumItems, stringToQuerys;
	for (const auto& qn : queryNames) {
		String name = u(qn);
		String enumName = name;
		std::replace(enumName.begin(), enumName.end(), L' ', L'_');
		enumItems      += "    " + enumName + ",\n";
		stringToQuerys += "    if (name == L\"" + name
						+ "\") return Querys::" + enumName + ";\n";
	}

	return Render(TPL_SQL_QUERIES_H, {
		{"{ENUM_ITEMS}",       enumItems},
		{"{STRING_TO_QUERYS}", stringToQuerys},
	});
}
//---------------------------------------------------------------------------

String CodeGenerator::GenerateAssignCommonFields()
{
	auto fieldPairs = m_storage->GetDistinctFieldNamesAndTypes();

	std::set<String> fields;
	for (const auto& [name, type] : fieldPairs) {
		String field = DataTypeMapper::getCppPrefixBySqlType(u(type)) + u(name);
		fields.insert(field);
	}

	TCodeBlock codp;
	for (const String &f : fields) {
		std::vector<String> line { "X(" + f + ")", '\\' };
		codp.push_back(line);
	}
	String block = AlignedBlock(codp);
	block.Delete(block.Length(), 3);

	return Render(TPL_ASSIGN_COMMON_FIELDS, {
		{"{FIELDS_LIST}", block},
	});
}
//---------------------------------------------------------------------------

String CodeGenerator::GenerateDataAccess()
{
	auto modules = m_storage->GetModules();
	auto allQueries = m_storage->GetAllQueriesWithModules();

	String moduleIncludes;
	for (const auto& mod : modules) {
		moduleIncludes += "#include \"" + u(mod.moduleName) + "SQL.h\"\n";
	}

	String explicitInst;
	for (const auto& qr : allQueries) {
		if (qr.queryName.empty()) continue;
		String qEnum = u(qr.queryName);
		String qType = u(qr.queryType).UpperCase();

		if (qType == "SELECT") {
			explicitInst += "template void DBSelect<Querys::" + qEnum
				+ ">(const InParam<Querys::" + qEnum + ">&, "
				+ "std::vector<OutParam<Querys::" + qEnum + ">>&);\n";
		} else {
			explicitInst += "template OutParam<Querys::" + qEnum
				+ "> DBExecute<Querys::" + qEnum
				+ ">(const InParam<Querys::" + qEnum + ">&);\n";
		}
	}

	return Render(TPL_DATA_ACCESS_CPP, {
		{"{MODULE_INCLUDES}",        moduleIncludes},
		{"{EXPLICIT_INSTANTIATION}", explicitInst},
	});
}
//---------------------------------------------------------------------------

static String TrailingSpace(const String &line)
{
	if (!line.IsEmpty() && line[line.Length()] != ' ')
		return " ";
	return "";
}

String CodeGenerator::BuildSqlEntries(const std::vector<QueryRecord>& records, bool usePgSql)
{
	String result;
	bool first = true;
	for (const auto& rec : records) {
		String modu = u(rec.moduleName);
		String name = u(rec.queryName);
		String desc = u(rec.description);
		String SQL  = usePgSql ? u(rec.pgSQL) : u(rec.sourceSQL);

		if (!first) result += ",\n";

		result += "\t// " + modu + "\n";
		result += "\t// " + desc + "\n";
		result += "\t{ Querys::" + name + ",\n";

		auto lines = SplitSQLByLines(SQL);
		String l = "L";
		for (String line : lines) {
			line = StringReplace(line, "\"", "\\\"", TReplaceFlags() << rfReplaceAll);
			result += "\t " + l + "\"" + line + TrailingSpace(line) + "\\n\"\n";
			l = "";
		}
		result += "\t}";
		first = false;
	}
	return result;
}
//---------------------------------------------------------------------------

String CodeGenerator::GenerateFileSql(bool pg)
{
	auto records = m_storage->GetAllQueriesWithModules();

	String mssqlEntries = BuildSqlEntries(records, false);
	String pgEntries    = BuildSqlEntries(records, true);

	return Render(TPL_FILE_SQL_STORAGE, {
		{"{MSSQL_ENTRIES}", mssqlEntries},
		{"{PG_ENTRIES}",    pgEntries},
	});
}
//---------------------------------------------------------------------------

String CodeGenerator::GenerateModuleStructs(int moduleID, const String& moduleName)
{
	auto queries = m_storage->GetQueriesByModule(moduleID);

	String structCode;
	String dbAliases;
	for (const auto& qr : queries) {
		if (qr.queryName.empty()) continue;
		auto params = m_storage->GetInputParams(qr.queryID);
		auto fields = m_storage->GetOutputFields(qr.queryID);
		std::vector<ParamInfo> inParams, outParams;
		ParamCollector::CollectFromStorage(params, fields, inParams, outParams);
		structCode += GenerateTemplateClassCode(u(qr.queryName), inParams, outParams) + "\n";

		String methodName = u(ParamCollector::QueryNameToMethodName(qr.queryName));
		String queryEnum = u(qr.queryName);
		String comment = qr.description.empty() ? "" : " ///<" + u(qr.description);
		dbAliases += "    using " + methodName + " = QTypes<Querys::" + queryEnum + ">;" + comment + "\n";
	}

	String filename = moduleName + "SQL.h";
	return Render(TPL_MODULE_STRUCTS_H, {
		{"{FILENAME}",    filename},
		{"{STRUCT_CODE}", structCode},
		{"{DB_ALIASES}",  dbAliases},
	});
}
//---------------------------------------------------------------------------

void CodeGenerator::GenerateAll(const String& outputDir)
{
	WriteFileBOM(outputDir + "\\FileSQLQueryStorage.cpp", GenerateFileSql());
	WriteFileBOM(outputDir + "\\SQLQueries.h", GenerateSQLQueriesHeader());
	WriteFileBOM(outputDir + "\\AssignCommonFields.h", GenerateAssignCommonFields());
	WriteFileBOM(outputDir + "\\DataAccess.cpp", GenerateDataAccess());

	auto modules = m_storage->GetModules();
	for (const auto& mod : modules) {
		String filename = u(mod.moduleName) + "SQL.h";
		WriteFileBOM(outputDir + "\\" + filename,
			GenerateModuleStructs(mod.moduleID, u(mod.moduleName)));
	}
}
//---------------------------------------------------------------------------

String CodeGenerator::Render(const char* tpl, const std::map<String, String>& vars)
{
	String result = tpl;
	for (const auto& [key, val] : vars)
		result = StringReplace(result, key, val, TReplaceFlags() << rfReplaceAll);
	return result;
}
//---------------------------------------------------------------------------

void CodeGenerator::BOM_UTF_8(std::ofstream &outFile)
{
	if (outFile.is_open()) {
		outFile << static_cast<char>(0xEF);
		outFile << static_cast<char>(0xBB);
		outFile << static_cast<char>(0xBF);
	}
}
//---------------------------------------------------------------------------

bool CodeGenerator::WriteFileBOM(const String& path, const String& content)
{
	std::ofstream f(path.c_str(), std::ios::binary);
	if (!f.is_open())
		return false;
	BOM_UTF_8(f);
	f << utf8(content);
	f.close();
	return true;
}
//---------------------------------------------------------------------------
