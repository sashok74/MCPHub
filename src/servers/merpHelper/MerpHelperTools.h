//---------------------------------------------------------------------------
// MerpHelperTools.h — MCP tool definitions for MerpHelper module
//---------------------------------------------------------------------------

#ifndef MerpHelperToolsH
#define MerpHelperToolsH

#include "ToolDefs.h"
#include "LocalMcpDb.h"
#include "McpQueryAnalyzer.h"
#include "IQueryStorage.h"
#include "CodeGenerator.h"
#include "CodeTemplates.h"
#include "ParamCollector.h"
#include "DataTypeMapper.h"
#include "AlignedBlock.h"
#include "UcodeUtf8.h"
#include <string>
#include <algorithm>
#include <set>

namespace Mcp { namespace Tools {

using json = nlohmann::json;

// ── Helpers ──────────────────────────────────────────────────────────────

// snake_case → PascalCase: "doc_approve_doc_list_s" → "DocApproveDocListS"
inline std::string SnakeToPascal(const std::string& snake)
{
	std::string result;
	bool capitalize = true;
	for (char c : snake) {
		if (c == '_') { capitalize = true; continue; }
		if (capitalize) { result += (char)std::toupper((unsigned char)c); capitalize = false; }
		else result += c;
	}
	return result;
}

inline std::string MhGetStr(const json& args, const std::string& key,
	const std::string& def = "")
{
	if (!args.contains(key) || args[key].is_null()) return def;
	if (args[key].is_string()) return args[key].get<std::string>();
	return args[key].dump();
}

inline int MhGetInt(const json& args, const std::string& key, int def = 0)
{
	if (!args.contains(key) || args[key].is_null()) return def;
	if (args[key].is_number_integer()) return args[key].get<int>();
	if (args[key].is_string()) {
		try { return std::stoi(args[key].get<std::string>()); }
		catch (...) { return def; }
	}
	return def;
}

inline bool MhGetBool(const json& args, const std::string& key, bool def = false)
{
	if (!args.contains(key) || args[key].is_null()) return def;
	if (args[key].is_boolean()) return args[key].get<bool>();
	if (args[key].is_string()) {
		std::string v = args[key].get<std::string>();
		return v == "true" || v == "1";
	}
	return def;
}

// Convert ParamRecord -> json
inline json ParamRecordToJson(const ParamRecord& p)
{
	return json{
		{"paramName", p.paramName}, {"paramType", p.paramType},
		{"direction", p.direction}, {"isNullable", p.isNullable},
		{"isKeyField", p.isKeyField}, {"maxLength", p.maxLength},
		{"precision", p.precision}, {"scale", p.scale},
		{"defaultValue", p.defaultValue}, {"nullValue", p.nullValue},
		{"paramOrder", p.paramOrder}
	};
}

// Convert FieldRecord -> json
inline json FieldRecordToJson(const FieldRecord& f)
{
	return json{
		{"fieldName", f.fieldName}, {"fieldType", f.fieldType},
		{"treeIdent", f.treeIdent}, {"isNullable", f.isNullable},
		{"isKeyField", f.isKeyField}, {"displayZero", f.displayZero},
		{"maxLength", f.maxLength}, {"precision", f.precision},
		{"scale", f.scale}, {"fieldOrder", f.fieldOrder},
		{"defaultValue", f.defaultValue}
	};
}

// Convert QueryRecord -> json
inline json QueryRecordToJson(const QueryRecord& q)
{
	return json{
		{"queryID", q.queryID}, {"queryName", q.queryName},
		{"queryType", q.queryType}, {"description", q.description},
		{"sourceSQL", q.sourceSQL}, {"pgSQL", q.pgSQL},
		{"moduleID", q.moduleID}, {"moduleName", q.moduleName},
		{"tableName", q.tableName},
		{"dsName", q.dsName}, {"dsQueryI", q.dsQueryI},
		{"dsQueryU", q.dsQueryU}, {"dsQueryD", q.dsQueryD},
		{"dsQueryOneS", q.dsQueryOneS}
	};
}

// ── Snapshot helper ──────────────────────────────────────────────────────

// Creates a snapshot of the current query state before destructive operations.
// Returns snapshot ID or 0 if snapshot failed (non-fatal).
inline int MhCreateSnapshot(LocalMcpDb* localDb, IQueryStorage* storage,
	const std::string& queryName, const std::string& operation)
{
	if (!localDb || !storage) return 0;
	try {
		auto qr = storage->GetQueryByName(queryName);
		if (qr.queryID == 0) return 0;

		auto params = storage->GetInputParams(qr.queryID);
		auto fields = storage->GetOutputFields(qr.queryID);

		json snap;
		snap["query"] = QueryRecordToJson(qr);

		json jParams = json::array();
		for (const auto& p : params)
			jParams.push_back(ParamRecordToJson(p));
		snap["params"] = jParams;

		json jFields = json::array();
		for (const auto& f : fields)
			jFields.push_back(FieldRecordToJson(f));
		snap["fields"] = jFields;

		localDb->Execute(
			"INSERT INTO query_snapshots (query_name, operation, data_json) "
			"VALUES (:qname, :op, :data)",
			{{"qname", queryName}, {"op", operation}, {"data", snap.dump()}});

		// Return the new snapshot ID
		auto rows = localDb->Query(
			"SELECT id FROM query_snapshots WHERE query_name = :qname "
			"ORDER BY id DESC LIMIT 1",
			{{"qname", queryName}});
		if (!rows.empty() && rows[0].contains("id"))
			return rows[0]["id"].get<int>();
	} catch (...) {
		// Snapshot failure must not block the operation
	}
	return 0;
}

// ── Built-in template registry ───────────────────────────────────────────

struct BuiltinTemplate {
	std::string name;
	std::string description;
	std::string category;
	const char* text;
};

inline const std::vector<BuiltinTemplate>& GetBuiltinTemplates()
{
	static std::vector<BuiltinTemplate> templates = {
		{"query_types", "InParam/OutParam/TypeConversion structs", "code_gen", TPL_QUERY_TYPES},
		{"sql_enum", "SQLQueries.h - enum + StringToQuerys", "code_gen", TPL_SQL_QUERIES_H},
		{"assign_fields", "AssignCommonFields.h", "code_gen", TPL_ASSIGN_COMMON_FIELDS},
		{"module_header", "Per-module {Name}SQL.h", "code_gen", TPL_MODULE_STRUCTS_H},
		{"data_access", "DataAccess.cpp template", "code_gen", TPL_DATA_ACCESS_CPP},
		{"file_sql", "FileSQLQueryStorage.cpp template", "code_gen", TPL_FILE_SQL_STORAGE},
		{"example_select", "Usage example: SELECT query", "example", TPL_EXAMPLE_DATA_SELECT},
		{"example_other", "Usage example: INSERT/UPDATE/DELETE", "example", TPL_EXAMPLE_DATA_OTHER},
		{"example_grid", "Grid initialization example", "example", TPL_EXAMPLE_GRID},
		{"example_dataset", "Dataset with callbacks example", "example", TPL_EXAMPLE_DATASET},
	};
	return templates;
}

// ── Tool list ────────────────────────────────────────────────────────────

inline ToolList GetMerpHelperTools(
	IQueryStorage* storage,
	McpQueryAnalyzer* analyzer,
	CodeGenerator* codeGen,
	LocalMcpDb* localDb,
	TFDConnection* conn,
	const nlohmann::json* config = nullptr)
{
	return {

	// ═══════════════════════════════════════════════════════════════════════
	// 1. list_modules
	// ═══════════════════════════════════════════════════════════════════════
	{
		"list_modules",
		"List SQL modules (query groups). Each module contains related SQL queries.",
		TMcpToolSchema(),
		[storage](const json& args, TMcpToolContext&) -> TMcpToolResult
		{
			try {
				auto modules = storage->GetModules();
				json arr = json::array();
				for (const auto& m : modules)
					arr.push_back(json{{"moduleID", m.moduleID}, {"moduleName", m.moduleName}});
				return TMcpToolResult::Success(json{{"modules", arr}, {"count", arr.size()}});
			} catch (const Exception& e) {
				return TMcpToolResult::Error(utf8(e.Message));
			}
		}
	},

	// ═══════════════════════════════════════════════════════════════════════
	// 2. list_queries
	// ═══════════════════════════════════════════════════════════════════════
	{
		"list_queries",
		"List queries in a module.",
		TMcpToolSchema()
			.AddInteger("module_id", "Module ID", true),
		[storage](const json& args, TMcpToolContext&) -> TMcpToolResult
		{
			try {
				int moduleId = MhGetInt(args, "module_id");
				auto queries = storage->GetQueriesByModule(moduleId);
				json arr = json::array();
				for (const auto& q : queries) {
					arr.push_back(json{
						{"queryID", q.queryID}, {"queryName", q.queryName},
						{"queryType", q.queryType}, {"description", q.description},
						{"tableName", q.tableName}
					});
				}
				return TMcpToolResult::Success(json{{"queries", arr}, {"count", arr.size()}});
			} catch (const Exception& e) {
				return TMcpToolResult::Error(utf8(e.Message));
			}
		}
	},

	// ═══════════════════════════════════════════════════════════════════════
	// 3. search_queries
	// ═══════════════════════════════════════════════════════════════════════
	{
		"search_queries",
		"Search existing queries by name or SQL body. Use this BEFORE adding a new query "
		"to check if a similar one already exists. "
		"Naming convention: <entity>_<sub>_<op> where op is _s (SELECT), _i (INSERT), "
		"_u (UPDATE), _d (DELETE). Examples: doc_approve_doc_list_s, catalogs_apr_i.",
		TMcpToolSchema()
			.AddString("pattern", "Search pattern (matched against QueryName, SourceSQL, Description)", true),
		[conn](const json& args, TMcpToolContext&) -> TMcpToolResult
		{
			try {
				std::string pattern = MhGetStr(args, "pattern");
				if (pattern.empty())
					return TMcpToolResult::Error("pattern is required");

				auto q = std::make_unique<TFDQuery>(nullptr);
				q->Connection = conn;
				q->SQL->Text =
					"SELECT q.QueryID, q.QueryName, q.QueryType, q.Description, "
					"e.StrVal AS ModuleName "
					"FROM dbo.SqlQueries q "
					"LEFT JOIN dbo.Enumeration e ON e.IntVal = q.ModuleID AND e.EntityName = 'SQLModule' "
					"WHERE q.IsActive = 1 AND ("
					"q.QueryName LIKE :Pattern OR q.SourceSQL LIKE :Pattern OR q.Description LIKE :Pattern) "
					"ORDER BY q.QueryName";
				UnicodeString pat = u("%" + pattern + "%");
				q->ParamByName("Pattern")->AsString = pat;
				q->Open();

				json arr = json::array();
				while (!q->Eof) {
					arr.push_back(json{
						{"queryID", q->FieldByName("QueryID")->AsInteger},
						{"queryName", utf8(q->FieldByName("QueryName")->AsString)},
						{"queryType", utf8(q->FieldByName("QueryType")->AsString)},
						{"moduleName", utf8(q->FieldByName("ModuleName")->AsString)},
						{"description", utf8(q->FieldByName("Description")->AsString)}
					});
					q->Next();
				}
				return TMcpToolResult::Success(json{{"results", arr}, {"count", arr.size()}});
			} catch (const Exception& e) {
				return TMcpToolResult::Error(utf8(e.Message));
			}
		}
	},

	// ═══════════════════════════════════════════════════════════════════════
	// 4. get_query
	// ═══════════════════════════════════════════════════════════════════════
	{
		"get_query",
		"Get full query details with input parameters and output fields.",
		TMcpToolSchema()
			.AddString("query_name", "Query name", true),
		[storage](const json& args, TMcpToolContext&) -> TMcpToolResult
		{
			try {
				std::string name = MhGetStr(args, "query_name");
				if (name.empty())
					return TMcpToolResult::Error("query_name is required");

				auto qr = storage->GetQueryByName(name);
				if (qr.queryID == 0)
					return TMcpToolResult::Error("Query not found: " + name);

				auto params = storage->GetInputParams(qr.queryID);
				auto fields = storage->GetOutputFields(qr.queryID);

				json jParams = json::array();
				for (const auto& p : params)
					jParams.push_back(ParamRecordToJson(p));

				json jFields = json::array();
				for (const auto& f : fields)
					jFields.push_back(FieldRecordToJson(f));

				return TMcpToolResult::Success(json{
					{"query", QueryRecordToJson(qr)},
					{"params", jParams},
					{"fields", jFields}
				});
			} catch (const Exception& e) {
				return TMcpToolResult::Error(utf8(e.Message));
			}
		}
	},

	// ═══════════════════════════════════════════════════════════════════════
	// 5. add_query
	// ═══════════════════════════════════════════════════════════════════════
	{
		"add_query",
		"Add a new SQL query. Naming: <group>_<entity>_<operation>. "
		"After adding, use analyze_query to extract parameters from metadata.",
		TMcpToolSchema()
			.AddString("query_name", "Query name (snake_case)", true)
			.AddInteger("module_id", "Module ID", true)
			.AddEnum("query_type", "Query type", {"SELECT", "INSERT", "UPDATE", "DELETE"}, true)
			.AddString("source_sql", "MS SQL query text", true)
			.AddString("description", "Query description")
			.AddString("table_name", "Target table name")
			.AddString("pg_sql", "PostgreSQL version of the query"),
		[storage](const json& args, TMcpToolContext&) -> TMcpToolResult
		{
			try {
				std::string name = MhGetStr(args, "query_name");
				if (name.empty())
					return TMcpToolResult::Error("query_name is required");

				if (storage->QueryExists(name))
					return TMcpToolResult::Error("Query already exists: " + name);

				QueryRecord qr;
				qr.queryID = 0; // new
				qr.queryName = name;
				qr.moduleID = MhGetInt(args, "module_id");
				qr.queryType = MhGetStr(args, "query_type");
				qr.sourceSQL = MhGetStr(args, "source_sql");
				qr.description = MhGetStr(args, "description");
				qr.tableName = MhGetStr(args, "table_name");
				qr.pgSQL = MhGetStr(args, "pg_sql");
				qr.isActive = true;

				int newID = storage->SaveQuery(qr);
				return TMcpToolResult::Success(json{
					{"queryID", newID},
					{"message", "Query added successfully. Use analyze_query to extract parameters."}
				});
			} catch (const Exception& e) {
				return TMcpToolResult::Error(utf8(e.Message));
			}
		}
	},

	// ═══════════════════════════════════════════════════════════════════════
	// 6. update_query
	// ═══════════════════════════════════════════════════════════════════════
	{
		"update_query",
		"Update an existing query. Only specified fields are changed. "
		"Automatically creates a snapshot of the current state before updating.",
		TMcpToolSchema()
			.AddString("query_name", "Query name to update", true)
			.AddString("source_sql", "New MS SQL text")
			.AddString("pg_sql", "New PostgreSQL text")
			.AddString("description", "New description")
			.AddString("table_name", "New table name")
			.AddInteger("module_id", "New module ID")
			.AddString("query_type", "New query type")
			.AddString("ds_name", "Dataset variable name (e.g. m_dsMyQuery)")
			.AddString("ds_query_i", "INSERT query name for dataset")
			.AddString("ds_query_u", "UPDATE query name for dataset")
			.AddString("ds_query_d", "DELETE query name for dataset")
			.AddString("ds_query_one_s", "Single-record SELECT query name for dataset reload"),
		[storage, localDb](const json& args, TMcpToolContext&) -> TMcpToolResult
		{
			try {
				std::string name = MhGetStr(args, "query_name");
				if (name.empty())
					return TMcpToolResult::Error("query_name is required");

				auto qr = storage->GetQueryByName(name);
				if (qr.queryID == 0)
					return TMcpToolResult::Error("Query not found: " + name);

				// Auto-snapshot before update
				int snapId = MhCreateSnapshot(localDb, storage, name, "update_query");

				// Update only provided fields
				if (args.contains("source_sql") && !args["source_sql"].is_null())
					qr.sourceSQL = MhGetStr(args, "source_sql");
				if (args.contains("pg_sql") && !args["pg_sql"].is_null())
					qr.pgSQL = MhGetStr(args, "pg_sql");
				if (args.contains("description") && !args["description"].is_null())
					qr.description = MhGetStr(args, "description");
				if (args.contains("table_name") && !args["table_name"].is_null())
					qr.tableName = MhGetStr(args, "table_name");
				if (args.contains("module_id") && !args["module_id"].is_null())
					qr.moduleID = MhGetInt(args, "module_id");
				if (args.contains("query_type") && !args["query_type"].is_null())
					qr.queryType = MhGetStr(args, "query_type");
				if (args.contains("ds_name") && !args["ds_name"].is_null())
					qr.dsName = MhGetStr(args, "ds_name");
				if (args.contains("ds_query_i") && !args["ds_query_i"].is_null())
					qr.dsQueryI = MhGetStr(args, "ds_query_i");
				if (args.contains("ds_query_u") && !args["ds_query_u"].is_null())
					qr.dsQueryU = MhGetStr(args, "ds_query_u");
				if (args.contains("ds_query_d") && !args["ds_query_d"].is_null())
					qr.dsQueryD = MhGetStr(args, "ds_query_d");
				if (args.contains("ds_query_one_s") && !args["ds_query_one_s"].is_null())
					qr.dsQueryOneS = MhGetStr(args, "ds_query_one_s");

				storage->SaveQuery(qr);
				json result = json{{"status", "updated"}, {"queryID", qr.queryID}};
				if (snapId > 0)
					result["snapshot_id"] = snapId;
				return TMcpToolResult::Success(result);
			} catch (const Exception& e) {
				return TMcpToolResult::Error(utf8(e.Message));
			}
		}
	},

	// ═══════════════════════════════════════════════════════════════════════
	// 7. delete_query
	// ═══════════════════════════════════════════════════════════════════════
	{
		"delete_query",
		"Delete a query and its params/fields. "
		"Automatically creates a snapshot before deletion for recovery via restore_query_snapshot.",
		TMcpToolSchema()
			.AddString("query_name", "Query name to delete", true),
		[storage, localDb](const json& args, TMcpToolContext&) -> TMcpToolResult
		{
			try {
				std::string name = MhGetStr(args, "query_name");
				if (name.empty())
					return TMcpToolResult::Error("query_name is required");

				auto qr = storage->GetQueryByName(name);
				if (qr.queryID == 0)
					return TMcpToolResult::Error("Query not found: " + name);

				// Auto-snapshot before delete
				int snapId = MhCreateSnapshot(localDb, storage, name, "delete_query");

				storage->DeleteQuery(qr.queryID);
				json result = json{{"status", "deleted"}, {"queryID", qr.queryID}};
				if (snapId > 0)
					result["snapshot_id"] = snapId;
				return TMcpToolResult::Success(result);
			} catch (const Exception& e) {
				return TMcpToolResult::Error(utf8(e.Message));
			}
		}
	},

	// ═══════════════════════════════════════════════════════════════════════
	// 8. analyze_query
	// ═══════════════════════════════════════════════════════════════════════
	{
		"analyze_query",
		"Analyze a query's SQL text to extract input parameters and output fields from "
		"database metadata. Uses sp_describe_undeclared_parameters and "
		"sp_describe_first_result_set. Sets: data types, C++ types, Hungarian prefixes, "
		"default values, nullable flags, primary key flags. "
		"After analysis, the query is ready for code generation. "
		"When save=true, automatically creates a snapshot of current params/fields before overwriting.",
		TMcpToolSchema()
			.AddString("query_name", "Query name to analyze", true)
			.AddBoolean("save", "Auto-save results to SqlQueryParams/SqlQueryFields (default: true)"),
		[storage, analyzer, localDb](const json& args, TMcpToolContext&) -> TMcpToolResult
		{
			try {
				std::string name = MhGetStr(args, "query_name");
				if (name.empty())
					return TMcpToolResult::Error("query_name is required");
				bool save = MhGetBool(args, "save", true);

				auto qr = storage->GetQueryByName(name);
				if (qr.queryID == 0)
					return TMcpToolResult::Error("Query not found: " + name);

				auto params = analyzer->AnalyzeInputParams(qr.sourceSQL);
				auto fields = analyzer->AnalyzeOutputFields(qr.sourceSQL);

				// Enrich with C++ type info via DataTypeMapper
				for (auto& p : params) {
					UnicodeString sqlType = u(p.paramType);
					p.defaultValue = utf8(DataTypeMapper::getDefaultValueByCppType(
						DataTypeMapper::getCppTypeBySqlType(sqlType)));
				}
				for (auto& f : fields) {
					UnicodeString sqlType = u(f.fieldType);
					f.defaultValue = utf8(DataTypeMapper::getDefaultValueByCppType(
						DataTypeMapper::getCppTypeBySqlType(sqlType)));
				}

				int snapId = 0;
				if (save) {
					// Auto-snapshot before overwriting params/fields
					snapId = MhCreateSnapshot(localDb, storage, name, "analyze_query");
					storage->SaveInputParams(qr.queryID, params);
					storage->SaveOutputFields(qr.queryID, fields);
				}

				json jParams = json::array();
				for (const auto& p : params)
					jParams.push_back(ParamRecordToJson(p));

				json jFields = json::array();
				for (const auto& f : fields)
					jFields.push_back(FieldRecordToJson(f));

				json result = json{
					{"params", jParams},
					{"fields", jFields},
					{"saved", save},
					{"message", save ? "Parameters saved to database" : "Analysis complete (not saved)"}
				};
				if (snapId > 0)
					result["snapshot_id"] = snapId;
				return TMcpToolResult::Success(result);
			} catch (const Exception& e) {
				return TMcpToolResult::Error(utf8(e.Message));
			}
		}
	},

	// ═══════════════════════════════════════════════════════════════════════
	// 9. generate_query_code
	// ═══════════════════════════════════════════════════════════════════════
	{
		"generate_query_code",
		"Generate InParam/OutParam/TypeConversion C++ structs for a single query.",
		TMcpToolSchema()
			.AddString("query_name", "Query name", true),
		[storage, codeGen](const json& args, TMcpToolContext&) -> TMcpToolResult
		{
			try {
				std::string name = MhGetStr(args, "query_name");
				if (name.empty())
					return TMcpToolResult::Error("query_name is required");

				auto qr = storage->GetQueryByName(name);
				if (qr.queryID == 0)
					return TMcpToolResult::Error("Query not found: " + name);

				auto params = storage->GetInputParams(qr.queryID);
				auto fields = storage->GetOutputFields(qr.queryID);
				std::vector<ParamInfo> inParams, outParams;
				ParamCollector::CollectFromStorage(params, fields, inParams, outParams);

				String code = codeGen->GenerateTemplateClassCode(
					u(name), inParams, outParams);

				return TMcpToolResult::Success(json{
					{"code", utf8(code)},
					{"queryName", name}
				});
			} catch (const Exception& e) {
				return TMcpToolResult::Error(utf8(e.Message));
			}
		}
	},

	// ═══════════════════════════════════════════════════════════════════════
	// 10. generate_module_code
	// ═══════════════════════════════════════════════════════════════════════
	{
		"generate_module_code",
		"Generate complete {ModuleName}SQL.h header for a module.",
		TMcpToolSchema()
			.AddInteger("module_id", "Module ID", true),
		[storage, codeGen](const json& args, TMcpToolContext&) -> TMcpToolResult
		{
			try {
				int moduleId = MhGetInt(args, "module_id");

				// Find module name
				auto modules = storage->GetModules();
				std::string moduleName;
				for (const auto& m : modules) {
					if (m.moduleID == moduleId) {
						moduleName = m.moduleName;
						break;
					}
				}
				if (moduleName.empty())
					return TMcpToolResult::Error("Module not found: " + std::to_string(moduleId));

				String code = codeGen->GenerateModuleStructs(moduleId, u(moduleName));
				return TMcpToolResult::Success(json{
					{"code", utf8(code)},
					{"moduleName", moduleName},
					{"fileName", moduleName + "SQL.h"}
				});
			} catch (const Exception& e) {
				return TMcpToolResult::Error(utf8(e.Message));
			}
		}
	},

	// ═══════════════════════════════════════════════════════════════════════
	// 11. generate_all_files
	// ═══════════════════════════════════════════════════════════════════════
	{
		"generate_all_files",
		"Generate all project files (SQLQueries.h, AssignCommonFields.h, DataAccess.cpp, "
		"FileSQLQueryStorage.cpp, per-module headers) to output directory.",
		TMcpToolSchema()
			.AddString("output_dir", "Directory to write generated files", true),
		[codeGen](const json& args, TMcpToolContext&) -> TMcpToolResult
		{
			try {
				std::string dir = MhGetStr(args, "output_dir");
				if (dir.empty())
					return TMcpToolResult::Error("output_dir is required");

				codeGen->GenerateAll(u(dir));
				return TMcpToolResult::Success(json{
					{"status", "generated"},
					{"output_dir", dir},
					{"message", "All files generated successfully"}
				});
			} catch (const Exception& e) {
				return TMcpToolResult::Error(utf8(e.Message));
			}
		}
	},

	// ═══════════════════════════════════════════════════════════════════════
	// 12. generate_example
	// ═══════════════════════════════════════════════════════════════════════
	{
		"generate_example",
		"Generate usage example code for a query (data_access, grid, or dataset).",
		TMcpToolSchema()
			.AddString("query_name", "Query name", true)
			.AddEnum("example_type", "Example type", {"data_access", "grid", "dataset"}),
		[storage, codeGen](const json& args, TMcpToolContext&) -> TMcpToolResult
		{
			try {
				std::string name = MhGetStr(args, "query_name");
				if (name.empty())
					return TMcpToolResult::Error("query_name is required");
				std::string exType = MhGetStr(args, "example_type", "data_access");

				auto qr = storage->GetQueryByName(name);
				if (qr.queryID == 0)
					return TMcpToolResult::Error("Query not found: " + name);

				auto params = storage->GetInputParams(qr.queryID);
				auto fields = storage->GetOutputFields(qr.queryID);
				SqlInfo sInfo = ParamCollector::BuildSqlInfo(qr, params, fields);

				// Build substitution vars
				std::string methodName = ParamCollector::QueryNameToMethodName(name);
				std::string typeName = "T" + methodName;
				std::map<String, String> vars;
				vars["{METHOD_NAME}"] = u(methodName);
				vars["{TYPE_NAME}"] = u(typeName);
				vars["{QUERY_NAME}"] = u(name);
				vars["{IN_TYPE}"] = u(typeName + "::in inPrm");
				vars["{OUT_TYPE}"] = u(typeName + "::out outPrm");
				vars["{CON_TYPE}"] = u(typeName + "::cont outCont");

				// Build IN_PARAMS_BLOCK
				String inBlock;
				for (const auto& p : sInfo.inParams)
					inBlock += "inPrm." + u(p.MemberName) + " = /* value */;\n";
				vars["{IN_PARAMS_BLOCK}"] = inBlock;

				// Build OUT_FIELDS
				String outFields;
				for (const auto& p : sInfo.outParams)
					outFields += "///    " + u(p.CppType) + " " + u(p.MemberName) + ";\n";
				vars["{OUT_FIELDS}"] = outFields;

				const char* tpl;
				if (exType == "grid") {
					tpl = TPL_EXAMPLE_GRID;
					String gridName = u("m_grid" + methodName);
					vars["{GRID_NAME}"] = gridName;
					vars["{ADAPTER_NAME}"] = u("m_adapter" + methodName);
					vars["{DATASET_NAME}"] = u("m_ds" + methodName);
					vars["{SOURCE_NAME}"] = u("m_src" + methodName);

					// Build COLUMNS_BLOCK
					String cols;
					for (const auto& p : sInfo.outParams) {
						cols += "        {L\"" + u(p.DbName) + "\", L\"" + u(p.DbName) +
								"\", 100},\n";
					}
					vars["{COLUMNS_BLOCK}"] = cols;
				}
				else if (exType == "dataset") {
					tpl = TPL_EXAMPLE_DATASET;

					String datasetName = qr.dsName.empty()
						? u("m_ds" + methodName) : u(qr.dsName);
					vars["{DATASET_NAME}"] = datasetName;

					// Key field from output params
					String keyField = "m_iID";
					for (const auto& p : sInfo.outParams) {
						if (p.IsKey) { keyField = u(p.MemberName); break; }
					}
					vars["{KEY_FIELD}"] = keyField;

					// Helper lambda to build SqlInfo for a related query
					auto getRelatedInfo = [&](const std::string& relName) -> SqlInfo {
						SqlInfo si;
						if (relName.empty()) return si;
						auto rq = storage->GetQueryByName(relName);
						if (rq.queryID == 0) return si;
						auto rp = storage->GetInputParams(rq.queryID);
						auto rf = storage->GetOutputFields(rq.queryID);
						return ParamCollector::BuildSqlInfo(rq, rp, rf);
					};

					SqlInfo iInfo = getRelatedInfo(qr.dsQueryI);
					SqlInfo uInfo = getRelatedInfo(qr.dsQueryU);
					SqlInfo dInfo = getRelatedInfo(qr.dsQueryD);
					SqlInfo oInfo = getRelatedInfo(qr.dsQueryOneS);

					// --- OnOpen callback ---
					String onOpenBlock;
					if (!sInfo.queryName.empty()) {
						TCodeBlock codp;
						for (const auto& p : sInfo.inParams) {
							std::vector<String> line {
								"        inPrm." + u(p.MemberName),
								"= " + u(p.DefaultValue) + ";"
							};
							codp.push_back(line);
						}
						auto v = vars;
						v["{IN_PARAMS_BLOCK}"] = codp.empty() ? String("") : AlignedBlock(codp);
						onOpenBlock = CodeGenerator::Render(TPL_CALLBACK_OPEN, v);
					}

					// --- Save callback (INSERT/UPDATE) ---
					String saveBlock;
					if (!iInfo.queryName.empty() || !uInfo.queryName.empty()) {
						saveBlock = CodeGenerator::Render(TPL_CALLBACK_SAVE_HEADER, vars);
						if (!iInfo.queryName.empty()) {
							auto v = vars;
							v["{I_IN_TYPE}"]     = u(iInfo.inType);
							v["{I_OUT_TYPE}"]    = u(iInfo.outType);
							v["{I_METHOD_NAME}"] = u(iInfo.methodName);
							saveBlock += CodeGenerator::Render(TPL_CALLBACK_SAVE_INSERT, v);
						}
						if (!uInfo.queryName.empty()) {
							if (!iInfo.queryName.empty())
								saveBlock += "       else\n";
							auto v = vars;
							v["{U_IN_TYPE}"]     = u(uInfo.inType);
							v["{U_OUT_TYPE}"]    = u(uInfo.outType);
							v["{U_METHOD_NAME}"] = u(uInfo.methodName);
							saveBlock += CodeGenerator::Render(TPL_CALLBACK_SAVE_UPDATE, v);
						}
						if (!oInfo.queryName.empty())
							saveBlock += CodeGenerator::Render(TPL_CALLBACK_SAVE_RELOAD, vars);
						saveBlock += CodeGenerator::Render(TPL_CALLBACK_SAVE_FOOTER, vars);
					}

					// --- Delete callback ---
					String deleteBlock;
					if (!dInfo.queryName.empty()) {
						String extraKeys;
						for (const auto& p : dInfo.inParams) {
							if (p.IsKey && p.MemberName != "m_iID")
								extraKeys += "        inPrm." + u(p.MemberName)
									+ " = rec." + u(p.MemberName) + ";\n";
						}
						auto v = vars;
						v["{D_IN_TYPE}"]          = u(dInfo.inType);
						v["{D_OUT_TYPE}"]         = u(dInfo.outType);
						v["{D_METHOD_NAME}"]      = u(dInfo.methodName);
						v["{D_EXTRA_KEY_FIELDS}"] = extraKeys;
						deleteBlock = CodeGenerator::Render(TPL_CALLBACK_DELETE, v);
					}

					// --- Reload callback ---
					String reloadBlock;
					if (!oInfo.queryName.empty()) {
						String extraKeys;
						for (const auto& p : oInfo.inParams) {
							if (p.IsKey && p.MemberName != "m_iID")
								extraKeys += "        inPrm." + u(p.MemberName) + " = id;\n";
						}
						auto v = vars;
						v["{O_IN_TYPE}"]          = u(oInfo.inType);
						v["{O_OUT_TYPE}"]         = u(oInfo.outType);
						v["{O_METHOD_NAME}"]      = u(oInfo.methodName);
						v["{O_TYPE_NAME}"]        = u(oInfo.TypeName);
						v["{O_EXTRA_KEY_FIELDS}"] = extraKeys;
						reloadBlock = CodeGenerator::Render(TPL_CALLBACK_RELOAD, v);
					}

					vars["{ON_OPEN_BLOCK}"] = onOpenBlock;
					vars["{SAVE_BLOCK}"]    = saveBlock;
					vars["{DELETE_BLOCK}"]  = deleteBlock;
					vars["{RELOAD_BLOCK}"]  = reloadBlock;
				}
				else {
					// data_access
					std::string qType = qr.queryType;
					std::transform(qType.begin(), qType.end(), qType.begin(), ::toupper);
					tpl = (qType == "SELECT") ? TPL_EXAMPLE_DATA_SELECT : TPL_EXAMPLE_DATA_OTHER;
				}

				String result = CodeGenerator::Render(tpl, vars);
				return TMcpToolResult::Success(json{
					{"code", utf8(result)},
					{"example_type", exType},
					{"queryName", name}
				});
			} catch (const Exception& e) {
				return TMcpToolResult::Error(utf8(e.Message));
			}
		}
	},

	// ═══════════════════════════════════════════════════════════════════════
	// 13. render_template
	// ═══════════════════════════════════════════════════════════════════════
	{
		"render_template",
		"Render a template using a query's context (QUERY_NAME, IN_PARAMS, OUT_PARAMS, etc.). "
		"Use template_name for saved templates or template_text for ad-hoc rendering.",
		TMcpToolSchema()
			.AddString("query_name", "Query name for context", true)
			.AddString("template_name", "Name of a saved/built-in template")
			.AddString("template_text", "Ad-hoc template text"),
		[codeGen, localDb](const json& args, TMcpToolContext&) -> TMcpToolResult
		{
			try {
				std::string name = MhGetStr(args, "query_name");
				if (name.empty())
					return TMcpToolResult::Error("query_name is required");

				std::string tplName = MhGetStr(args, "template_name");
				std::string tplText = MhGetStr(args, "template_text");

				if (tplName.empty() && tplText.empty())
					return TMcpToolResult::Error("Either template_name or template_text is required");

				// Resolve template text
				std::string resolvedTpl;
				if (!tplText.empty()) {
					resolvedTpl = tplText;
				} else {
					// Check built-in first
					bool found = false;
					for (const auto& bt : GetBuiltinTemplates()) {
						if (bt.name == tplName) {
							resolvedTpl = bt.text;
							found = true;
							break;
						}
					}
					// Then check custom in SQLite
					if (!found && localDb) {
						auto rows = localDb->Query(
							"SELECT template_text FROM code_templates WHERE name = :name",
							{{"name", tplName}});
						if (!rows.empty() && rows[0].contains("template_text"))
							resolvedTpl = rows[0]["template_text"].get<std::string>();
					}
					if (resolvedTpl.empty())
						return TMcpToolResult::Error("Template not found: " + tplName);
				}

				String result = codeGen->RenderQueryTemplate(u(name), u(resolvedTpl));
				return TMcpToolResult::Success(json{
					{"rendered", utf8(result)},
					{"queryName", name}
				});
			} catch (const Exception& e) {
				return TMcpToolResult::Error(utf8(e.Message));
			}
		}
	},

	// ═══════════════════════════════════════════════════════════════════════
	// 14. generate_sql
	// ═══════════════════════════════════════════════════════════════════════
	{
		"generate_sql",
		"Generate CRUD SQL from a table name using INFORMATION_SCHEMA metadata.",
		TMcpToolSchema()
			.AddString("table_name", "Table name (e.g. dbo.MyTable)", true)
			.AddEnum("operation", "SQL operation", {"SELECT", "INSERT", "UPDATE", "DELETE"}, true),
		[analyzer](const json& args, TMcpToolContext&) -> TMcpToolResult
		{
			try {
				std::string tableName = MhGetStr(args, "table_name");
				std::string operation = MhGetStr(args, "operation");
				if (tableName.empty())
					return TMcpToolResult::Error("table_name is required");
				if (operation.empty())
					return TMcpToolResult::Error("operation is required");

				std::string sql = analyzer->GenerateSQL(tableName, operation);
				if (sql.empty())
					return TMcpToolResult::Error("Could not generate SQL for: " + tableName);

				return TMcpToolResult::Success(json{
					{"sql", sql},
					{"table_name", tableName},
					{"operation", operation}
				});
			} catch (const Exception& e) {
				return TMcpToolResult::Error(utf8(e.Message));
			}
		}
	},

	// ═══════════════════════════════════════════════════════════════════════
	// 15. list_templates
	// ═══════════════════════════════════════════════════════════════════════
	{
		"list_templates",
		"List available templates (built-in + custom from SQLite).",
		TMcpToolSchema()
			.AddString("category", "Filter by category"),
		[localDb](const json& args, TMcpToolContext&) -> TMcpToolResult
		{
			try {
				std::string category = MhGetStr(args, "category");
				json arr = json::array();

				// Built-in templates
				for (const auto& bt : GetBuiltinTemplates()) {
					if (!category.empty() && bt.category != category) continue;
					arr.push_back(json{
						{"name", bt.name}, {"description", bt.description},
						{"category", bt.category}, {"is_builtin", true}
					});
				}

				// Custom templates from SQLite
				if (localDb) {
					std::string sql = "SELECT name, description, category FROM code_templates";
					nlohmann::json params = nlohmann::json::object();
					if (!category.empty()) {
						sql += " WHERE category = :cat";
						params["cat"] = category;
					}
					sql += " ORDER BY name";
					auto rows = localDb->Query(sql, params);
					for (const auto& row : rows) {
						arr.push_back(json{
							{"name", row.value("name", "")},
							{"description", row.value("description", "")},
							{"category", row.value("category", "custom")},
							{"is_builtin", false}
						});
					}
				}

				return TMcpToolResult::Success(json{{"templates", arr}, {"count", arr.size()}});
			} catch (const std::exception& e) {
				return TMcpToolResult::Error(e.what());
			}
		}
	},

	// ═══════════════════════════════════════════════════════════════════════
	// 16. get_template
	// ═══════════════════════════════════════════════════════════════════════
	{
		"get_template",
		"Get template content by name.",
		TMcpToolSchema()
			.AddString("name", "Template name", true),
		[localDb](const json& args, TMcpToolContext&) -> TMcpToolResult
		{
			try {
				std::string name = MhGetStr(args, "name");
				if (name.empty())
					return TMcpToolResult::Error("name is required");

				// Check built-in first
				for (const auto& bt : GetBuiltinTemplates()) {
					if (bt.name == name) {
						return TMcpToolResult::Success(json{
							{"name", bt.name}, {"description", bt.description},
							{"template_text", std::string(bt.text)},
							{"category", bt.category}, {"is_builtin", true}
						});
					}
				}

				// Check custom
				if (localDb) {
					auto rows = localDb->Query(
						"SELECT name, description, template_text, category "
						"FROM code_templates WHERE name = :name",
						{{"name", name}});
					if (!rows.empty()) {
						return TMcpToolResult::Success(json{
							{"name", rows[0].value("name", "")},
							{"description", rows[0].value("description", "")},
							{"template_text", rows[0].value("template_text", "")},
							{"category", rows[0].value("category", "custom")},
							{"is_builtin", false}
						});
					}
				}

				return TMcpToolResult::Error("Template not found: " + name);
			} catch (const std::exception& e) {
				return TMcpToolResult::Error(e.what());
			}
		}
	},

	// ═══════════════════════════════════════════════════════════════════════
	// 17. save_template
	// ═══════════════════════════════════════════════════════════════════════
	{
		"save_template",
		"Save a custom Mustache-like template. Available context variables: "
		"{{QUERY_NAME}}, {{#IN_PARAMS}}...{{/IN_PARAMS}}, {{#OUT_PARAMS}}...{{/OUT_PARAMS}}, "
		"{{CPP_TYPE}}, {{MEMBER_NAME}}, {{DB_NAME}}, {{DEFAULT_VALUE}}, "
		"{{HAS_NULL_VALUE}}, {{NULL_VALUE}}, {{DISPLAY_EXPR}}, {{FROM_ROW_LINE}}, "
		"{{IN_KEY_FIELD_NAME}}, {{OUT_KEY_FIELD_NAME}}, {{HAS_PID}}, {{HAS_TID}}. "
		"Dataset variables: {{#HAS_DS}}, {{DS_NAME}}, "
		"{{DS_QUERY_I}}, {{DS_QUERY_I_PASCAL}}, {{DS_QUERY_U}}, {{DS_QUERY_U_PASCAL}}, "
		"{{DS_QUERY_D}}, {{DS_QUERY_D_PASCAL}}, {{DS_QUERY_ONE_S}}, {{DS_QUERY_ONE_S_PASCAL}}.",
		TMcpToolSchema()
			.AddString("name", "Template name", true)
			.AddString("template_text", "Template content", true)
			.AddString("description", "Template description")
			.AddString("category", "Template category (default: custom)"),
		[localDb](const json& args, TMcpToolContext&) -> TMcpToolResult
		{
			try {
				if (!localDb)
					return TMcpToolResult::Error("Local database not configured");

				std::string name = MhGetStr(args, "name");
				std::string text = MhGetStr(args, "template_text");
				if (name.empty() || text.empty())
					return TMcpToolResult::Error("name and template_text are required");

				// Check not overwriting built-in
				for (const auto& bt : GetBuiltinTemplates()) {
					if (bt.name == name)
						return TMcpToolResult::Error("Cannot overwrite built-in template: " + name);
				}

				std::string desc = MhGetStr(args, "description");
				std::string cat = MhGetStr(args, "category", "custom");

				localDb->Execute(
					"INSERT INTO code_templates (name, template_text, description, category, updated_at) "
					"VALUES (:name, :text, :desc, :cat, datetime('now')) "
					"ON CONFLICT(name) DO UPDATE SET "
					"template_text = excluded.template_text, description = excluded.description, "
					"category = excluded.category, updated_at = datetime('now')",
					{{"name", name}, {"text", text}, {"desc", desc}, {"cat", cat}});

				return TMcpToolResult::Success(json{{"status", "saved"}, {"name", name}});
			} catch (const std::exception& e) {
				return TMcpToolResult::Error(e.what());
			}
		}
	},

	// ═══════════════════════════════════════════════════════════════════════
	// 18. get_naming_convention
	// ═══════════════════════════════════════════════════════════════════════
	{
		"get_naming_convention",
		"Show naming rules and workflow instructions for working with MERP queries.",
		TMcpToolSchema(),
		[](const json& args, TMcpToolContext&) -> TMcpToolResult
		{
			json result;
			result["naming_rules"] = {
				{"query_name_format", "<group>_<entity>_<sub>_<operation_suffix>"},
				{"operation_suffixes", {
					{"_s", "SELECT"}, {"_i", "INSERT"},
					{"_u", "UPDATE"}, {"_d", "DELETE"}
				}},
				{"examples", {
					"doc_approve_doc_list_s", "catalogs_apr_i",
					"doc_approve_project_s", "production_move_material_u"
				}},
				{"hungarian_prefixes", {
					{"m_i", "int"}, {"m_s", "std::string / std::wstring"},
					{"m_f", "float / double"}, {"m_b", "bool"},
					{"m_dt", "TDateTime / COleDateTime"}
				}}
			};
			result["workflow"] = {
				"1. search_queries - check if a similar query already exists",
				"2. add_query - create the query with SQL text",
				"3. analyze_query - extract input/output parameters from metadata",
				"4. get_query - verify the extracted parameters",
				"5. generate_query_code - generate C++ structs",
				"6. generate_module_code - generate full module header",
				"7. generate_all_files - generate all project files to output directory"
			};
			result["query_types"] = {
				{"SELECT", "Read data - returns multiple rows via DBSelect"},
				{"INSERT", "Create record - returns new ID via DBExecute"},
				{"UPDATE", "Modify record - returns result via DBExecute"},
				{"DELETE", "Remove record - returns result via DBExecute"}
			};
			return TMcpToolResult::Success(result);
		}
	},

	// ═══════════════════════════════════════════════════════════════════════
	// 19. describe_table
	// ═══════════════════════════════════════════════════════════════════════
	{
		"describe_table",
		"Get table structure: columns, data types, max length, nullable, primary key flags. "
		"Use this before writing SQL to understand what columns are available.",
		TMcpToolSchema()
			.AddString("table_name", "Table name (e.g. dbo.MyTable or just MyTable)", true),
		[conn](const json& args, TMcpToolContext&) -> TMcpToolResult
		{
			try {
				std::string fullName = MhGetStr(args, "table_name");
				if (fullName.empty())
					return TMcpToolResult::Error("table_name is required");

				std::string schema, table;
				size_t dot = fullName.find('.');
				if (dot != std::string::npos) {
					schema = fullName.substr(0, dot);
					table = fullName.substr(dot + 1);
				} else {
					schema = "dbo";
					table = fullName;
				}

				// Columns from INFORMATION_SCHEMA
				auto qCols = std::make_unique<TFDQuery>(nullptr);
				qCols->Connection = conn;
				qCols->SQL->Text =
					"SELECT c.COLUMN_NAME, c.DATA_TYPE, c.CHARACTER_MAXIMUM_LENGTH, "
					"c.NUMERIC_PRECISION, c.NUMERIC_SCALE, c.IS_NULLABLE, "
					"c.COLUMN_DEFAULT, c.ORDINAL_POSITION "
					"FROM INFORMATION_SCHEMA.COLUMNS c "
					"WHERE c.TABLE_SCHEMA = :Schema AND c.TABLE_NAME = :TableName "
					"ORDER BY c.ORDINAL_POSITION";
				qCols->ParamByName("Schema")->AsString = u(schema);
				qCols->ParamByName("TableName")->AsString = u(table);
				qCols->Open();

				if (qCols->Eof)
					return TMcpToolResult::Error("Table not found: " + schema + "." + table);

				// PK columns
				auto qPK = std::make_unique<TFDQuery>(nullptr);
				qPK->Connection = conn;
				qPK->SQL->Text =
					"SELECT K.COLUMN_NAME "
					"FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS TC "
					"JOIN INFORMATION_SCHEMA.KEY_COLUMN_USAGE K "
					"ON TC.CONSTRAINT_NAME = K.CONSTRAINT_NAME "
					"WHERE TC.TABLE_SCHEMA = :Schema AND TC.TABLE_NAME = :TableName "
					"AND TC.CONSTRAINT_TYPE = 'PRIMARY KEY'";
				qPK->ParamByName("Schema")->AsString = u(schema);
				qPK->ParamByName("TableName")->AsString = u(table);
				qPK->Open();

				std::set<std::string> pkSet;
				while (!qPK->Eof) {
					pkSet.insert(utf8(qPK->FieldByName("COLUMN_NAME")->AsString));
					qPK->Next();
				}

				json columns = json::array();
				while (!qCols->Eof) {
					std::string colName = utf8(qCols->FieldByName("COLUMN_NAME")->AsString);
					json col;
					col["name"] = colName;
					col["type"] = utf8(qCols->FieldByName("DATA_TYPE")->AsString);
					col["nullable"] = (utf8(qCols->FieldByName("IS_NULLABLE")->AsString) == "YES");
					col["is_pk"] = (pkSet.count(colName) > 0);
					col["ordinal"] = qCols->FieldByName("ORDINAL_POSITION")->AsInteger;

					if (!qCols->FieldByName("CHARACTER_MAXIMUM_LENGTH")->IsNull)
						col["max_length"] = qCols->FieldByName("CHARACTER_MAXIMUM_LENGTH")->AsInteger;
					if (!qCols->FieldByName("NUMERIC_PRECISION")->IsNull)
						col["precision"] = qCols->FieldByName("NUMERIC_PRECISION")->AsInteger;
					if (!qCols->FieldByName("NUMERIC_SCALE")->IsNull)
						col["scale"] = qCols->FieldByName("NUMERIC_SCALE")->AsInteger;
					if (!qCols->FieldByName("COLUMN_DEFAULT")->IsNull)
						col["default"] = utf8(qCols->FieldByName("COLUMN_DEFAULT")->AsString);

					columns.push_back(col);
					qCols->Next();
				}

				json pk = json::array();
				for (const auto& k : pkSet) pk.push_back(k);

				return TMcpToolResult::Success(json{
					{"table", schema + "." + table},
					{"columns", columns},
					{"primary_key", pk},
					{"column_count", columns.size()}
				});
			} catch (const Exception& e) {
				return TMcpToolResult::Error(utf8(e.Message));
			}
		}
	},

	// ═══════════════════════════════════════════════════════════════════════
	// 20. test_sql
	// ═══════════════════════════════════════════════════════════════════════
	{
		"test_sql",
		"Validate SQL without saving. Checks that the query parses correctly and extracts "
		"input parameters and output fields from metadata. Use this to verify SQL before add_query.",
		TMcpToolSchema()
			.AddString("sql", "SQL query text to validate", true),
		[analyzer](const json& args, TMcpToolContext&) -> TMcpToolResult
		{
			try {
				std::string sql = MhGetStr(args, "sql");
				if (sql.empty())
					return TMcpToolResult::Error("sql is required");

				json result;
				result["sql"] = sql;

				// Try input params
				try {
					auto params = analyzer->AnalyzeInputParams(sql);
					json jParams = json::array();
					for (const auto& p : params) {
						jParams.push_back(json{
							{"name", p.paramName}, {"type", p.paramType},
							{"nullable", p.isNullable}, {"is_key", p.isKeyField}
						});
					}
					result["input_params"] = jParams;
					result["input_count"] = jParams.size();
				} catch (const Exception& e) {
					result["input_error"] = utf8(e.Message);
				}

				// Try output fields
				try {
					auto fields = analyzer->AnalyzeOutputFields(sql);
					json jFields = json::array();
					for (const auto& f : fields) {
						jFields.push_back(json{
							{"name", f.fieldName}, {"type", f.fieldType},
							{"nullable", f.isNullable}, {"max_length", f.maxLength}
						});
					}
					result["output_fields"] = jFields;
					result["output_count"] = jFields.size();
				} catch (const Exception& e) {
					result["output_error"] = utf8(e.Message);
				}

				bool valid = result.contains("input_params") && result.contains("output_fields");
				result["valid"] = valid;
				result["message"] = valid
					? "SQL is valid. Ready to use with add_query + analyze_query."
					: "SQL has issues. Check input_error / output_error fields.";

				return TMcpToolResult::Success(result);
			} catch (const Exception& e) {
				return TMcpToolResult::Error(utf8(e.Message));
			}
		}
	},

	// ═══════════════════════════════════════════════════════════════════════
	// 21. list_query_snapshots
	// ═══════════════════════════════════════════════════════════════════════
	{
		"list_query_snapshots",
		"List saved snapshots of query state. Snapshots are created automatically before "
		"update_query, analyze_query (save=true), and delete_query operations.",
		TMcpToolSchema()
			.AddString("query_name", "Filter by query name (optional)"),
		[localDb](const json& args, TMcpToolContext&) -> TMcpToolResult
		{
			try {
				if (!localDb)
					return TMcpToolResult::Error("Local database not configured");

				std::string qname = MhGetStr(args, "query_name");

				std::string sql =
					"SELECT id, query_name, operation, created_at, "
					"json_extract(data_json, '$.query.sourceSQL') AS source_sql, "
					"json_array_length(json_extract(data_json, '$.params')) AS param_count, "
					"json_array_length(json_extract(data_json, '$.fields')) AS field_count "
					"FROM query_snapshots";
				nlohmann::json params = nlohmann::json::object();
				if (!qname.empty()) {
					sql += " WHERE query_name = :qname";
					params["qname"] = qname;
				}
				sql += " ORDER BY id DESC LIMIT 50";

				auto rows = localDb->Query(sql, params);

				json arr = json::array();
				for (const auto& row : rows) {
					json snap;
					snap["id"] = row.value("id", 0);
					snap["query_name"] = row.value("query_name", "");
					snap["operation"] = row.value("operation", "");
					snap["created_at"] = row.value("created_at", "");
					snap["source_sql_preview"] = row.value("source_sql", "");
					snap["param_count"] = row.value("param_count", 0);
					snap["field_count"] = row.value("field_count", 0);
					arr.push_back(snap);
				}
				return TMcpToolResult::Success(json{{"snapshots", arr}, {"count", arr.size()}});
			} catch (const std::exception& e) {
				return TMcpToolResult::Error(e.what());
			}
		}
	},

	// ═══════════════════════════════════════════════════════════════════════
	// 22. restore_query_snapshot
	// ═══════════════════════════════════════════════════════════════════════
	{
		"restore_query_snapshot",
		"Restore a query to a previously saved snapshot state. "
		"Restores query fields (SQL, description, etc.), input params, and output fields. "
		"Creates a snapshot of the CURRENT state before restoring (so you can undo the restore).",
		TMcpToolSchema()
			.AddInteger("snapshot_id", "Snapshot ID from list_query_snapshots", true),
		[storage, localDb](const json& args, TMcpToolContext&) -> TMcpToolResult
		{
			try {
				if (!localDb)
					return TMcpToolResult::Error("Local database not configured");

				int snapId = MhGetInt(args, "snapshot_id");
				if (snapId <= 0)
					return TMcpToolResult::Error("snapshot_id is required");

				// Load snapshot
				auto rows = localDb->Query(
					"SELECT query_name, data_json FROM query_snapshots WHERE id = :id",
					{{"id", std::to_string(snapId)}});

				if (rows.empty())
					return TMcpToolResult::Error("Snapshot not found: " + std::to_string(snapId));

				std::string queryName = rows[0].value("query_name", "");
				std::string dataStr = rows[0].value("data_json", "");
				json data = json::parse(dataStr);

				// Snapshot current state before restoring
				MhCreateSnapshot(localDb, storage, queryName, "restore_query_snapshot");

				// Restore query record
				auto qr = storage->GetQueryByName(queryName);
				bool exists = (qr.queryID != 0);

				json snapQuery = data["query"];
				if (exists) {
					// Update existing query
					qr.sourceSQL = snapQuery.value("sourceSQL", qr.sourceSQL);
					qr.pgSQL = snapQuery.value("pgSQL", qr.pgSQL);
					qr.description = snapQuery.value("description", qr.description);
					qr.tableName = snapQuery.value("tableName", qr.tableName);
					qr.queryType = snapQuery.value("queryType", qr.queryType);
					qr.moduleID = snapQuery.value("moduleID", qr.moduleID);
					storage->SaveQuery(qr);
				} else {
					// Re-create deleted query
					qr.queryID = 0;
					qr.queryName = queryName;
					qr.sourceSQL = snapQuery.value("sourceSQL", "");
					qr.pgSQL = snapQuery.value("pgSQL", "");
					qr.description = snapQuery.value("description", "");
					qr.tableName = snapQuery.value("tableName", "");
					qr.queryType = snapQuery.value("queryType", "SELECT");
					qr.moduleID = snapQuery.value("moduleID", 0);
					qr.isActive = true;
					qr.queryID = storage->SaveQuery(qr);
				}

				// Restore params
				if (data.contains("params") && data["params"].is_array()) {
					std::vector<ParamRecord> params;
					for (const auto& jp : data["params"]) {
						ParamRecord p;
						p.paramName = jp.value("paramName", "");
						p.paramType = jp.value("paramType", "nvarchar");
						p.direction = jp.value("direction", "Input");
						p.isNullable = jp.value("isNullable", false);
						p.isKeyField = jp.value("isKeyField", false);
						p.maxLength = jp.value("maxLength", 0);
						p.precision = jp.value("precision", 0);
						p.scale = jp.value("scale", 0);
						p.defaultValue = jp.value("defaultValue", "");
						p.nullValue = jp.value("nullValue", "");
						p.paramOrder = jp.value("paramOrder", 0);
						if (p.direction.empty()) p.direction = "Input";
						params.push_back(p);
					}
					storage->SaveInputParams(qr.queryID, params);
				}

				// Restore fields
				if (data.contains("fields") && data["fields"].is_array()) {
					std::vector<FieldRecord> fields;
					for (const auto& jf : data["fields"]) {
						FieldRecord f;
						f.fieldName = jf.value("fieldName", "");
						f.fieldType = jf.value("fieldType", "");
						f.treeIdent = jf.value("treeIdent", "");
						f.isNullable = jf.value("isNullable", false);
						f.isKeyField = jf.value("isKeyField", false);
						f.displayZero = jf.value("displayZero", false);
						f.maxLength = jf.value("maxLength", 0);
						f.precision = jf.value("precision", 0);
						f.scale = jf.value("scale", 0);
						f.fieldOrder = jf.value("fieldOrder", 0);
						f.defaultValue = jf.value("defaultValue", "");
						fields.push_back(f);
					}
					storage->SaveOutputFields(qr.queryID, fields);
				}

				return TMcpToolResult::Success(json{
					{"status", "restored"},
					{"query_name", queryName},
					{"queryID", qr.queryID},
					{"from_snapshot", snapId},
					{"message", exists
						? "Query restored to snapshot state"
						: "Deleted query re-created from snapshot"}
				});
			} catch (const std::exception& e) {
				return TMcpToolResult::Error(e.what());
			} catch (const Exception& e) {
				return TMcpToolResult::Error(utf8(e.Message));
			}
		}
	},

	// ═══════════════════════════════════════════════════════════════════════
	// 23. get_output_dir
	// ═══════════════════════════════════════════════════════════════════════
	{
		"get_output_dir",
		"Get the configured output directory where generated files are written. "
		"Returns empty string if not configured.",
		TMcpToolSchema(),
		[config](const json& /*args*/, TMcpToolContext&) -> TMcpToolResult
		{
			std::string dir;
			if (config && config->contains("output_dir") &&
				(*config)["output_dir"].is_string())
			{
				dir = (*config)["output_dir"].get<std::string>();
			}
			return TMcpToolResult::Success(json{
				{"output_dir", dir},
				{"configured", !dir.empty()}
			});
		}
	},

	// ═══════════════════════════════════════════════════════════════════════
	// 24. get_template_vars
	// ═══════════════════════════════════════════════════════════════════════
	{
		"get_template_vars",
		"Get reference of all template variables available for render_template / save_template.",
		TMcpToolSchema(),
		[](const json& /*args*/, TMcpToolContext&) -> TMcpToolResult
		{
			json result;
			result["global"] = {
				{"QUERY_NAME",       "Query name (snake_case), e.g. doc_list_s"},
				{"QUERY_NAME_PASCAL","PascalCase query name, e.g. DocListS"},
				{"MODULE_NAME",      "Module name, e.g. DocApprove"},
				{"TABLE_NAME",       "Target table, e.g. dbo.DocList"},
				{"DESCRIPTION",      "Query description text"},
				{"QUERY_TYPE",       "SELECT | INSERT | UPDATE | DELETE"},
				{"IN_COUNT",         "Input parameter count"},
				{"OUT_COUNT",        "Output field count"}
			};
			result["params_(inside_IN_PARAMS/OUT_PARAMS)"] = {
				{"CPP_TYPE",       "C++ type: int, std::string, float"},
				{"MEMBER_NAME",    "Struct member: m_iProductID"},
				{"DB_NAME",        "DB column name: ProductID"},
				{"DEFAULT_VALUE",  "Default: 0, L\"\""},
				{"HAS_NULL_VALUE", "Flag: nullable parameter"},
				{"NULL_VALUE",     "Null sentinel: GLOBAL_INVALID_ID"},
				{"DISPLAY_EXPR",   "(OUT only) Display expression"},
				{"FROM_ROW_LINE",  "(OUT only) row.fill() code line"}
			};
			result["key_fields"] = {
				{"IN_KEY_FIELD_NAME / OUT_KEY_FIELD_NAME",   "PK member name"},
				{"IN_HAS_EXTRA_KEY / OUT_HAS_EXTRA_KEY",     "Flag: PK differs from m_iID"},
				{"HAS_PID / PID_FIELD_NAME",                 "Parent ID field"},
				{"HAS_TID / TID_FIELD_NAME",                 "Tree ID field"}
			};
			result["dataset"] = {
				{"HAS_DS",               "Section flag: DS-fields configured"},
				{"DS_NAME",              "Dataset variable, e.g. m_dsDocList"},
				{"DS_QUERY_S",           "SELECT query name"},
				{"DS_QUERY_I",           "INSERT query name"},
				{"DS_QUERY_U",           "UPDATE query name"},
				{"DS_QUERY_D",           "DELETE query name"},
				{"DS_QUERY_ONE_S",       "Reload query name"},
				{"DS_QUERY_S_PASCAL",    "PascalCase SELECT"},
				{"DS_QUERY_I_PASCAL",    "PascalCase INSERT"},
				{"DS_QUERY_U_PASCAL",    "PascalCase UPDATE"},
				{"DS_QUERY_D_PASCAL",    "PascalCase DELETE"},
				{"DS_QUERY_ONE_S_PASCAL","PascalCase reload"}
			};
			result["loop_control"] = {
				{"_INDEX", "0-based index inside IN_PARAMS/OUT_PARAMS"},
				{"_FIRST", "true on first iteration"},
				{"_LAST",  "true on last iteration"}
			};
			result["syntax"] = {
				{"{{VAR}}",                        "Substitute value"},
				{"{{#SECTION}}...{{/SECTION}}",    "Loop array / conditional (true)"},
				{"{{^SECTION}}...{{/SECTION}}",    "Inverted (false/empty)"},
				{"{{!comment}}",                   "Comment (ignored)"}
			};
			return TMcpToolResult::Success(result);
		}
	}

	}; // end ToolList
}

}} // namespace Mcp::Tools
#endif
