//---------------------------------------------------------------------------
// ProjectMemoryTools.h — MCP Tools for Project Memory
//
// 40 tools for knowledge management:
//   Facts, Forms, Processes, Glossary, Queries, Patterns,
//   Relationships, Modules, Entity Status, Stats, Feature Requests,
//   Completed Tasks, UI Routes, UI Research,
//   Batch Context, Gaps Analysis, Quality Report, Knowledge Gaps,
//   Context Feedback, Prepare Context
//---------------------------------------------------------------------------

#ifndef ProjectMemoryToolsH
#define ProjectMemoryToolsH

#include "ToolDefs.h"
#include "LocalMcpDb.h"
#include "DbMcpFederation.h"
#include <sstream>
#include <set>
#include <map>
#include <algorithm>

namespace Mcp { namespace Tools {

using json = nlohmann::json;
using Params = LocalMcpDb::Params;

//---------------------------------------------------------------------------
// Helper: truncate string at UTF-8 boundary (never splits multi-byte chars)
//---------------------------------------------------------------------------
inline std::string TruncateUtf8(const std::string &s, size_t maxBytes,
	const std::string &suffix = "...")
{
	if (s.size() <= maxBytes) return s;
	size_t pos = maxBytes;
	// Walk back to find a valid UTF-8 character boundary
	while (pos > 0 && (static_cast<unsigned char>(s[pos]) & 0xC0) == 0x80)
		--pos;
	return s.substr(0, pos) + suffix;
}

//---------------------------------------------------------------------------
// Helper: get string from json, or default
//---------------------------------------------------------------------------
inline std::string GetStr(const json &args, const std::string &key,
	const std::string &defaultVal = "")
{
	if (args.is_null() || !args.contains(key))
		return defaultVal;
	const auto &val = args[key];
	if (val.is_null())
		return defaultVal;
	if (val.is_string())
		return val.get<std::string>();
	return val.dump();
}

inline int GetInt(const json &args, const std::string &key, int defaultVal = 0)
{
	if (args.is_null() || !args.contains(key))
		return defaultVal;
	const auto &val = args[key];
	if (val.is_null())
		return defaultVal;
	if (val.is_number_integer())
		return val.get<int>();
	if (val.is_string()) {
		try { return std::stoi(val.get<std::string>()); }
		catch (...) { return defaultVal; }
	}
	return defaultVal;
}

inline bool TryGetInt64(const json &args, const std::string &key, long long &out)
{
	if (args.is_null() || !args.contains(key))
		return false;
	const auto &val = args[key];
	if (val.is_null())
		return false;
	if (val.is_number_integer() || val.is_number_unsigned()) {
		out = val.get<long long>();
		return true;
	}
	if (val.is_number_float()) {
		out = static_cast<long long>(val.get<double>());
		return true;
	}
	if (val.is_string()) {
		try {
			out = std::stoll(val.get<std::string>());
			return true;
		} catch (...) {
			return false;
		}
	}
	return false;
}

//---------------------------------------------------------------------------
// Helper: get array from json - handles both arrays and JSON strings
//---------------------------------------------------------------------------
inline json GetArray(const json &args, const std::string &key)
{
	if (args.is_null() || !args.contains(key))
		return json::array();
	const auto &val = args[key];
	if (val.is_null())
		return json::array();
	if (val.is_array())
		return val;
	if (val.is_string()) {
		try {
			json parsed = json::parse(val.get<std::string>());
			if (parsed.is_array())
				return parsed;
		} catch (...) {}
	}
	return json::array();
}

//---------------------------------------------------------------------------
// Helper: add FTS wildcard for prefix search
//---------------------------------------------------------------------------
inline std::string AddWildcard(const std::string &query)
{
	if (query.empty()) return query;
	if (query.back() != '*')
		return query + "*";
	return query;
}

//---------------------------------------------------------------------------
// Helper: generate LIKE pattern from query (first 5 UTF-8 chars for morphology)
//---------------------------------------------------------------------------
inline std::string MakeLikePattern(const std::string &query)
{
	// Take first 5 UTF-8 characters, not bytes
	int charCount = 0;
	size_t pos = 0;
	while (pos < query.length() && charCount < 5) {
		unsigned char c = static_cast<unsigned char>(query[pos]);
		if (c < 0x80) pos += 1;
		else if (c < 0xE0) pos += 2;
		else if (c < 0xF0) pos += 3;
		else pos += 4;
		charCount++;
	}
	if (pos > query.length()) pos = query.length();
	std::string base = query.substr(0, pos);
	return "%" + base + "%";
}

//---------------------------------------------------------------------------
// Helper: split query into words (min 3 bytes to skip noise)
//---------------------------------------------------------------------------
inline std::vector<std::string> SplitWords(const std::string &query)
{
	std::istringstream iss(query);
	std::string word;
	std::vector<std::string> words;
	while (iss >> word) {
		if (word.length() >= 3) // skip very short tokens
			words.push_back(word);
	}
	return words;
}

//---------------------------------------------------------------------------
// Helper: build FTS OR query from multi-word input
// "документы модуль продукция" -> "документы* OR модуль* OR продукция*"
//---------------------------------------------------------------------------
inline std::string MakeFtsOrQuery(const std::string &query)
{
	auto words = SplitWords(query);
	if (words.empty())
		return AddWildcard(query);
	if (words.size() == 1)
		return AddWildcard(words[0]);
	std::string result;
	for (const auto &w : words) {
		if (!result.empty()) result += " OR ";
		result += AddWildcard(w);
	}
	return result;
}

//---------------------------------------------------------------------------
// Helper: basic UTF-8 lowercase (ASCII + Cyrillic А-Я + Ё)
//---------------------------------------------------------------------------
inline std::string Utf8Lower(const std::string &s)
{
	std::string result;
	result.reserve(s.size());
	size_t i = 0;
	while (i < s.size()) {
		unsigned char c = static_cast<unsigned char>(s[i]);
		if (c < 0x80) {
			result += static_cast<char>(std::tolower(c));
			i += 1;
		} else if (c == 0xD0 && i + 1 < s.size()) {
			unsigned char c2 = static_cast<unsigned char>(s[i+1]);
			if (c2 >= 0x90 && c2 <= 0x9F) {
				// А-П -> а-п
				result += static_cast<char>(0xD0);
				result += static_cast<char>(c2 + 0x20);
				i += 2;
			} else if (c2 >= 0xA0 && c2 <= 0xAF) {
				// Р-Я -> р-я
				result += static_cast<char>(0xD1);
				result += static_cast<char>(c2 - 0x20);
				i += 2;
			} else if (c2 == 0x81) {
				// Ё -> ё
				result += static_cast<char>(0xD1);
				result += static_cast<char>(0x91);
				i += 2;
			} else {
				result += s[i]; result += s[i+1];
				i += 2;
			}
		} else if (c < 0xE0) {
			result += s[i];
			if (i + 1 < s.size()) result += s[i+1];
			i += 2;
		} else if (c < 0xF0) {
			for (int k = 0; k < 3 && i + k < s.size(); k++)
				result += s[i+k];
			i += 3;
		} else {
			for (int k = 0; k < 4 && i + k < s.size(); k++)
				result += s[i+k];
			i += 4;
		}
	}
	return result;
}

//---------------------------------------------------------------------------
// Helper: auto-detect stop words from FTS5 vocab tables (cached)
// Differentiated thresholds based on table size:
//   >100 docs: 30%  (large table — frequent terms are truly generic)
//   30-100:    50%  (medium — raise bar to keep domain terms)
//   <30:       skip (too few docs — every term looks frequent)
//---------------------------------------------------------------------------
inline const std::set<std::string>& GetStopWords(LocalMcpDb *db)
{
	static std::set<std::string> stopWords;
	static bool initialized = false;
	if (initialized) return stopWords;
	initialized = true;

	try {
		db->Execute("CREATE VIRTUAL TABLE IF NOT EXISTS facts_vocab "
			"USING fts5vocab(facts_fts, 'row')", {});
		db->Execute("CREATE VIRTUAL TABLE IF NOT EXISTS processes_vocab "
			"USING fts5vocab(processes_fts, 'row')", {});
		db->Execute("CREATE VIRTUAL TABLE IF NOT EXISTS glossary_vocab "
			"USING fts5vocab(glossary_fts, 'row')", {});

		auto fc = db->Query("SELECT COUNT(*) as cnt FROM facts", {});
		auto pc = db->Query("SELECT COUNT(*) as cnt FROM business_processes", {});
		auto gc = db->Query("SELECT COUNT(*) as cnt FROM glossary", {});

		long long totalF = fc.size() > 0 ? fc[0].value("cnt", (long long)0) : 0;
		long long totalP = pc.size() > 0 ? pc[0].value("cnt", (long long)0) : 0;
		long long totalG = gc.size() > 0 ? gc[0].value("cnt", (long long)0) : 0;

		// Differentiated thresholds by table size
		auto getThreshold = [](long long total) -> double {
			if (total > 100) return 0.3;  // large: 30%
			if (total >= 30)  return 0.5;  // medium: 50%
			return -1.0;                   // small: skip
		};

		double thF = getThreshold(totalF);
		double thP = getThreshold(totalP);
		double thG = getThreshold(totalG);

		if (thF > 0 && totalF > 10) {
			long long minDocs = static_cast<long long>(totalF * thF);
			auto rows = db->Query(
				"SELECT term FROM facts_vocab WHERE doc > " +
				std::to_string(minDocs), {});
			for (auto &r : rows)
				stopWords.insert(r.value("term", std::string()));
		}
		if (thP > 0 && totalP > 5) {
			long long minDocs = static_cast<long long>(totalP * thP);
			auto rows = db->Query(
				"SELECT term FROM processes_vocab WHERE doc > " +
				std::to_string(minDocs), {});
			for (auto &r : rows)
				stopWords.insert(r.value("term", std::string()));
		}
		if (thG > 0 && totalG > 5) {
			long long minDocs = static_cast<long long>(totalG * thG);
			auto rows = db->Query(
				"SELECT term FROM glossary_vocab WHERE doc > " +
				std::to_string(minDocs), {});
			for (auto &r : rows)
				stopWords.insert(r.value("term", std::string()));
		}
	} catch (...) {
		// If vocab creation fails, proceed without stop words
	}
	return stopWords;
}

//---------------------------------------------------------------------------
// Helper: split words + filter stop words (for broad search)
// If ALL words are stop words, returns original words (never empty)
//---------------------------------------------------------------------------
inline std::vector<std::string> SplitWordsFiltered(
	const std::string &query, LocalMcpDb *db)
{
	auto allWords = SplitWords(query);
	if (allWords.empty()) return allWords;

	const auto &stops = GetStopWords(db);
	if (stops.empty()) return allWords;

	std::vector<std::string> filtered;
	for (const auto &w : allWords) {
		if (stops.find(Utf8Lower(w)) == stops.end())
			filtered.push_back(w);
	}
	return filtered.empty() ? allWords : filtered;
}

//---------------------------------------------------------------------------
// Helper: escape single quotes for dynamic SQL IN clauses
//---------------------------------------------------------------------------
inline std::string EscapeSqlString(const std::string &s)
{
	std::string result;
	result.reserve(s.size());
	for (char c : s) {
		if (c == '\'') result += "''";
		else result += c;
	}
	return result;
}

//---------------------------------------------------------------------------
// Helper: two-phase ranked search — FTS with BM25 first, LIKE supplement
// Returns up to 'limit' results, FTS results ranked by relevance
//---------------------------------------------------------------------------
inline json RankedSearch(LocalMcpDb *db, Params &p,
	const std::string &ftsSql, const std::string &likeSql,
	int limit, const std::string &idField = "id")
{
	json results = json::array();
	std::set<long long> seenIds;

	// Phase 1: FTS with BM25 ranking (most relevant first)
	try {
		auto ftsRows = db->Query(ftsSql, p);
		for (auto &row : ftsRows) {
			long long id = row.value(idField, (long long)0);
			if (seenIds.insert(id).second) {
				results.push_back(row);
				if ((int)results.size() >= limit) return results;
			}
		}
	} catch (...) {}

	// Phase 2: LIKE supplement (fills remaining slots, no duplicates)
	if ((int)results.size() < limit) {
		try {
			auto likeRows = db->Query(likeSql, p);
			for (auto &row : likeRows) {
				long long id = row.value(idField, (long long)0);
				if (seenIds.insert(id).second) {
					results.push_back(row);
					if ((int)results.size() >= limit) return results;
				}
			}
		} catch (...) {}
	}
	return results;
}

//---------------------------------------------------------------------------
// FederateEntity — fetch schema/FK from dbmcp and cache in SQLite
// Returns true if new data was fetched, false if skipped or error
//---------------------------------------------------------------------------
inline bool FederateEntity(LocalMcpDb *db, Federation::DbMcpClient *fedClient,
	const std::string &entity, std::string *outError = nullptr)
{
	if (!fedClient || !db || entity.empty())
		return false;

	// Skip if schema fact already cached
	try {
		LocalMcpDb::Params chk;
		chk["entity"] = entity;
		json existing = db->Query(
			"SELECT id FROM facts WHERE entity = :entity AND fact_type = 'schema' LIMIT 1", chk);
		if (!existing.empty())
			return false;
	} catch (...) { return false; }

	bool fetched = false;

	// --- Fetch table schema ---
	std::string fedError;
	try {
		json schema = fedClient->GetTableSchema(entity);
		if (!schema.is_null() && schema.contains("rows") && schema["rows"].is_array()
			&& schema.value("rowCount", 0) > 0)
		{
			std::ostringstream desc;
			desc << "Table " << entity << " columns:\n";
			for (const auto &col : schema["rows"]) {
				std::string field = col.value("field", std::string());
				std::string type = col.value("type", std::string());
				desc << "  " << field << " " << type;
				if (col.contains("is_primary_key") && !col["is_primary_key"].is_null()
					&& col["is_primary_key"].is_number() && col["is_primary_key"].get<int>() == 1)
					desc << " [PK]";
				if (col.contains("nullable") && col["nullable"].is_string()
					&& col["nullable"].get<std::string>() == "NO")
					desc << " NOT NULL";
				if (col.contains("references_table") && col["references_table"].is_string()) {
					std::string refTable = col["references_table"].get<std::string>();
					if (!refTable.empty()) {
						if (refTable.substr(0, 4) == "dbo.")
							refTable = refTable.substr(4);
						desc << " -> " << refTable;
					}
				}
				desc << "\n";
			}

			LocalMcpDb::Params ip;
			ip["entity"] = entity;
			ip["fact_type"] = std::string("schema");
			ip["description"] = desc.str();
			ip["evidence"] = std::string("auto:dbmcp_federation");
			ip["confidence"] = std::string("verified");
			db->Execute(
				"INSERT INTO facts (entity, fact_type, description, evidence, confidence) "
				"VALUES (:entity, :fact_type, :description, :evidence, :confidence)", ip);
			fetched = true;
		}
	} catch (const std::exception &e) { fedError = e.what(); }
	  catch (...) { fedError = "unknown"; }

	// --- Fetch table relations ---
	try {
		json rels = fedClient->GetTableRelations(entity);
		if (!rels.is_null() && rels.contains("rows") && rels["rows"].is_array()
			&& rels.value("rowCount", 0) > 0)
		{
			for (const auto &fk : rels["rows"]) {
				std::string fromTable = fk.value("from_table", std::string());
				std::string toTable = fk.value("to_table", std::string());
				std::string fromCol = fk.value("from_column", std::string());
				std::string toCol = fk.value("to_column", std::string());
				std::string constraint = fk.value("constraint_name", std::string());

				// Strip dbo. prefix
				if (fromTable.substr(0, 4) == "dbo.")
					fromTable = fromTable.substr(4);
				if (toTable.substr(0, 4) == "dbo.")
					toTable = toTable.substr(4);

				if (fromTable.empty() || toTable.empty())
					continue;

				std::string desc = fromCol + " -> " + toTable + "." + toCol;
				if (!constraint.empty())
					desc += " (" + constraint + ")";

				LocalMcpDb::Params rp;
				rp["entity_from"] = fromTable;
				rp["entity_to"] = toTable;
				rp["rel_type"] = std::string("references");
				rp["description"] = desc;
				rp["evidence"] = std::string("auto:dbmcp_federation");
				db->Execute(
					"INSERT OR REPLACE INTO relationships "
					"(entity_from, entity_to, rel_type, description, evidence) "
					"VALUES (:entity_from, :entity_to, :rel_type, :description, :evidence)", rp);
			}
			fetched = true;
		}
	} catch (...) {}

	// Update entity status to schema_only
	if (fetched) {
		try {
			LocalMcpDb::Params sp;
			sp["entity"] = entity;
			sp["coverage"] = std::string("schema_only");
			sp["notes"] = std::string("Auto-federated from dbmcp");
			db->Execute(
				"INSERT OR REPLACE INTO entity_status (entity, coverage, notes) "
				"VALUES (:entity, :coverage, :notes)", sp);
		} catch (...) {}
	}

	if (outError && !fedError.empty())
		*outError = fedError;

	return fetched;
}

//---------------------------------------------------------------------------
// GetProjectMemoryTools — Returns all 40 tools
//---------------------------------------------------------------------------
inline ToolList GetProjectMemoryTools(LocalMcpDb *db, Federation::DbMcpClient *fedClient = nullptr)
{
	ToolList tools;

	//=======================================================================
	// 1. save_fact
	//=======================================================================
	tools.push_back({
		"save_fact",
		"Save a fact about an entity in the MERP codebase. "
		"Types: schema, business_rule, gotcha, code_pattern, statistics, concrete_data, relationship.",
		TMcpToolSchema()
			.AddString("entity", "Entity name (e.g. table, class, module)", true)
			.AddString("fact_type", "Fact type: schema|business_rule|gotcha|code_pattern|statistics|concrete_data|relationship", true)
			.AddString("description", "The fact description", true)
			.AddString("evidence", "Evidence or source for this fact")
			.AddString("confidence", "Confidence level: verified|unverified|uncertain")
			.AddString("task_context", "Task context when this fact was discovered"),
		[db](const json &args, TMcpToolContext &ctx) -> TMcpToolResult {
			std::string entity = GetStr(args, "entity");
			std::string factType = GetStr(args, "fact_type");
			std::string description = GetStr(args, "description");

			if (entity.empty() || factType.empty() || description.empty())
				return TMcpToolResult::Error("Missing required parameters: entity, fact_type, description");

			std::string confidence = GetStr(args, "confidence", "unverified");

			Params p;
			p["entity"] = entity;
			p["fact_type"] = factType;
			p["description"] = description;
			p["evidence"] = GetStr(args, "evidence");
			p["confidence"] = confidence;
			p["task_context"] = GetStr(args, "task_context");

			try {
				db->Execute(
					"INSERT INTO facts (entity, fact_type, description, evidence, confidence, task_context) "
					"VALUES (:entity, :fact_type, :description, :evidence, :confidence, :task_context)", p);
				long long id = db->LastInsertRowId();

				// Auto-resolve search gaps mentioning this entity
				try {
					Params rp;
					rp["ent"] = "%" + entity + "%";
					db->Execute(
						"UPDATE search_gaps SET resolved=1, resolved_by='save_fact', resolved_at=datetime('now') "
						"WHERE resolved=0 AND query_text LIKE :ent", rp);
				} catch (...) {}

				json resp;
				resp["success"] = true;
				resp["id"] = id;
				resp["message"] = "Fact saved for entity: " + entity;
				return TMcpToolResult::Success(resp);
			} catch (const std::exception &e) {
				return TMcpToolResult::Error(e.what());
			}
		}
	});

	//=======================================================================
	// 2. get_facts
	//=======================================================================
	tools.push_back({
		"get_facts",
		"Get all facts about a specific entity. Entity is a name of a DB table, C++ class, module, or concept (e.g. 'MoveMaterials', 'Product', 'ConstructionObject'). Returns schema info, business rules, gotchas, code patterns, statistics.",
		TMcpToolSchema()
			.AddString("entity", "Entity name to look up", true)
			.AddString("fact_type", "Optional filter by fact type"),
		[db](const json &args, TMcpToolContext &ctx) -> TMcpToolResult {
			std::string entity = GetStr(args, "entity");
			if (entity.empty())
				return TMcpToolResult::Error("Missing required parameter: entity");

			std::string factType = GetStr(args, "fact_type");

			Params p;
			p["entity"] = entity;

			std::string sql = "SELECT * FROM facts WHERE entity = :entity";
			if (!factType.empty()) {
				sql += " AND fact_type = :fact_type";
				p["fact_type"] = factType;
			}
			sql += " ORDER BY updated_at DESC";

			try {
				json rows = db->Query(sql, p);
				json resp;
				resp["facts"] = rows;
				resp["count"] = rows.size();
				return TMcpToolResult::Success(resp);
			} catch (const std::exception &e) {
				return TMcpToolResult::Error(e.what());
			}
		}
	});

	//=======================================================================
	// 3. search_facts
	//=======================================================================
	tools.push_back({
		"search_facts",
		"Full-text search across all facts (description, entity, evidence fields). Supports FTS5 with LIKE fallback. Use for precise fact lookup. For broad multi-topic search use ask_business instead.",
		TMcpToolSchema()
			.AddString("query", "Search query", true)
			.AddString("entity_filter", "Filter by entity name (exact match)")
			.AddString("fact_type_filter", "Filter by fact_type (exact match)")
			.AddInteger("limit", "Max results to return (default: 50)"),
		[db](const json &args, TMcpToolContext &ctx) -> TMcpToolResult {
			std::string query = GetStr(args, "query");
			if (query.empty())
				return TMcpToolResult::Error("Missing required parameter: query");

			std::string entityFilter = GetStr(args, "entity_filter");
			std::string factTypeFilter = GetStr(args, "fact_type_filter");
			int limit = GetInt(args, "limit", 50);

			std::string ftsQuery = AddWildcard(query);
			std::string likePattern = MakeLikePattern(query);

			Params p;
			p["query"] = ftsQuery;
			p["like_pattern"] = likePattern;

			std::string sql =
				"SELECT * FROM facts WHERE id IN ("
				"SELECT f.id FROM facts f INNER JOIN facts_fts fts ON f.id = fts.rowid WHERE facts_fts MATCH :query "
				"UNION "
				"SELECT f.id FROM facts f WHERE f.description LIKE :like_pattern OR f.entity LIKE :like_pattern OR f.evidence LIKE :like_pattern"
				")";

			if (!entityFilter.empty()) {
				sql += " AND entity = :entity_filter";
				p["entity_filter"] = entityFilter;
			}
			if (!factTypeFilter.empty()) {
				sql += " AND fact_type = :fact_type_filter";
				p["fact_type_filter"] = factTypeFilter;
			}
			sql += " ORDER BY updated_at DESC LIMIT :limit";
			p["limit"] = std::to_string(limit);

			try {
				json rows = db->Query(sql, p);

				// Auto-log search gap if no results
				if (rows.empty()) {
					try {
						Params gp;
						gp["qt"] = query;
						gp["tn"] = "search_facts";
						db->Execute("INSERT INTO search_gaps(query_text, tool_name, result_count) VALUES(:qt,:tn,0)", gp);
					} catch (...) {}
				}

				json resp;
				resp["facts"] = rows;
				resp["count"] = rows.size();
				resp["limit"] = limit;
				resp["filters_applied"] = json{
					{"entity_filter", entityFilter},
					{"fact_type_filter", factTypeFilter}
				};
				return TMcpToolResult::Success(resp);
			} catch (const std::exception &e) {
				return TMcpToolResult::Error(e.what());
			}
		}
	});

	//=======================================================================
	// 4. save_form_mapping
	//=======================================================================
	tools.push_back({
		"save_form_mapping",
		"Save a mapping: template name -> class, file, table, sql_file, operations. UPSERT by template.",
		TMcpToolSchema()
			.AddString("template", "Template name (e.g. 'IncomingOrder')", true)
			.AddString("class_name", "C++ class name")
			.AddString("file_path", "Source file path")
			.AddString("main_table", "Main database table")
			.AddString("sql_file", "SQL header file")
			.AddString("operations", "JSON array of supported operations"),
		[db](const json &args, TMcpToolContext &ctx) -> TMcpToolResult {
			std::string tmpl = GetStr(args, "template");
			if (tmpl.empty())
				return TMcpToolResult::Error("Missing required parameter: template");

			Params p;
			p["template"] = tmpl;
			p["class_name"] = GetStr(args, "class_name");
			p["file_path"] = GetStr(args, "file_path");
			p["main_table"] = GetStr(args, "main_table");
			p["sql_file"] = GetStr(args, "sql_file");
			p["operations"] = GetStr(args, "operations");

			try {
				db->Execute(
					"INSERT OR REPLACE INTO form_table_map "
					"(template, class_name, file_path, main_table, sql_file, operations) "
					"VALUES (:template, :class_name, :file_path, :main_table, :sql_file, :operations)", p);

				json resp;
				resp["success"] = true;
				resp["message"] = "Form mapping saved for template: " + tmpl;
				return TMcpToolResult::Success(resp);
			} catch (const std::exception &e) {
				return TMcpToolResult::Error(e.what());
			}
		}
	});

	//=======================================================================
	// 5. find_form
	//=======================================================================
	tools.push_back({
		"find_form",
		"Find form mappings by template, table, class_name, or general query.",
		TMcpToolSchema()
			.AddString("template", "Exact template name")
			.AddString("table", "Search by main_table (LIKE)")
			.AddString("class_name", "Search by class_name (LIKE)")
			.AddString("query", "General search across all fields (LIKE)"),
		[db](const json &args, TMcpToolContext &ctx) -> TMcpToolResult {
			std::string tmpl = GetStr(args, "template");
			std::string table = GetStr(args, "table");
			std::string className = GetStr(args, "class_name");
			std::string query = GetStr(args, "query");

			std::string sql;
			Params p;

			if (!tmpl.empty()) {
				sql = "SELECT * FROM form_table_map WHERE template = :template";
				p["template"] = tmpl;
			} else if (!table.empty()) {
				sql = "SELECT * FROM form_table_map WHERE main_table LIKE :table";
				p["table"] = "%" + table + "%";
			} else if (!className.empty()) {
				sql = "SELECT * FROM form_table_map WHERE class_name LIKE :class_name";
				p["class_name"] = "%" + className + "%";
			} else if (!query.empty()) {
				sql = "SELECT * FROM form_table_map WHERE "
					  "template LIKE :q OR class_name LIKE :q OR file_path LIKE :q "
					  "OR main_table LIKE :q OR sql_file LIKE :q";
				p["q"] = "%" + query + "%";
			} else {
				return TMcpToolResult::Error("At least one parameter required: template, table, class_name, or query");
			}

			try {
				json rows = db->Query(sql, p);
				json resp;
				resp["forms"] = rows;
				resp["count"] = rows.size();
				return TMcpToolResult::Success(resp);
			} catch (const std::exception &e) {
				return TMcpToolResult::Error(e.what());
			}
		}
	});

	//=======================================================================
	// 6. save_process
	//=======================================================================
	tools.push_back({
		"save_process",
		"Save a business process description with steps and related entities. Steps must be a JSON array of strings, each describing one step (e.g. [\"1. Create document\", \"2. Fill header fields\", \"3. Add lines\"]).",
		TMcpToolSchema()
			.AddString("name", "Process name", true)
			.AddString("description", "Process description")
			.AddString("steps", "JSON array of process steps", true)
			.AddString("synonyms", "JSON array of synonyms")
			.AddString("related_entities", "JSON array of related entities"),
		[db](const json &args, TMcpToolContext &ctx) -> TMcpToolResult {
			std::string name = GetStr(args, "name");
			std::string steps = GetStr(args, "steps");
			if (name.empty() || steps.empty())
				return TMcpToolResult::Error("Missing required parameters: name, steps");

			Params p;
			p["name"] = name;
			p["description"] = GetStr(args, "description");
			p["steps"] = steps;
			p["synonyms"] = GetStr(args, "synonyms");
			p["related_entities"] = GetStr(args, "related_entities");

			try {
				db->Execute(
					"INSERT INTO business_processes (name, description, steps, synonyms, related_entities) "
					"VALUES (:name, :description, :steps, :synonyms, :related_entities)", p);
				long long id = db->LastInsertRowId();

				json resp;
				resp["success"] = true;
				resp["id"] = id;
				resp["message"] = "Process saved: " + name;
				return TMcpToolResult::Success(resp);
			} catch (const std::exception &e) {
				return TMcpToolResult::Error(e.what());
			}
		}
	});

	//=======================================================================
	// 7. find_process
	//=======================================================================
	tools.push_back({
		"find_process",
		"Full-text search across business processes (name, description, synonyms, steps). Returns step-by-step workflows with related entities. Use when you need to understand HOW something works in the system.",
		TMcpToolSchema()
			.AddString("query", "Search query", true),
		[db](const json &args, TMcpToolContext &ctx) -> TMcpToolResult {
			std::string query = GetStr(args, "query");
			if (query.empty())
				return TMcpToolResult::Error("Missing required parameter: query");

			std::string ftsQuery = AddWildcard(query);
			std::string likePattern = MakeLikePattern(query);

			Params p;
			p["query"] = ftsQuery;
			p["like_pattern"] = likePattern;

			try {
				json rows = db->Query(
					"SELECT bp.* FROM business_processes bp INNER JOIN processes_fts fts ON bp.id = fts.rowid WHERE processes_fts MATCH :query "
					"UNION "
					"SELECT bp.* FROM business_processes bp WHERE bp.name LIKE :like_pattern OR bp.description LIKE :like_pattern "
					"ORDER BY id", p);

				// Auto-log search gap if no results
				if (rows.empty()) {
					try {
						Params gp;
						gp["qt"] = query;
						gp["tn"] = "find_process";
						db->Execute("INSERT INTO search_gaps(query_text, tool_name, result_count) VALUES(:qt,:tn,0)", gp);
					} catch (...) {}
				}

				json resp;
				resp["processes"] = rows;
				resp["count"] = rows.size();
				return TMcpToolResult::Success(resp);
			} catch (const std::exception &e) {
				return TMcpToolResult::Error(e.what());
			}
		}
	});

	//=======================================================================
	// 8. save_glossary
	//=======================================================================
	tools.push_back({
		"save_glossary",
		"Save a glossary term with synonyms and context. UPSERT by term.",
		TMcpToolSchema()
			.AddString("term", "The term", true)
			.AddString("synonyms", "JSON array of synonyms")
			.AddString("entity", "Related entity")
			.AddString("menu_path", "Menu path in MERP UI")
			.AddString("context", "Usage context"),
		[db](const json &args, TMcpToolContext &ctx) -> TMcpToolResult {
			std::string term = GetStr(args, "term");
			if (term.empty())
				return TMcpToolResult::Error("Missing required parameter: term");

			Params p;
			p["term"] = term;
			p["synonyms"] = GetStr(args, "synonyms");
			p["entity"] = GetStr(args, "entity");
			p["menu_path"] = GetStr(args, "menu_path");
			p["context"] = GetStr(args, "context");

			try {
				db->Execute(
					"INSERT OR REPLACE INTO glossary (term, synonyms, entity, menu_path, context) "
					"VALUES (:term, :synonyms, :entity, :menu_path, :context)", p);

				json resp;
				resp["success"] = true;
				resp["message"] = "Glossary term saved: " + term;
				return TMcpToolResult::Success(resp);
			} catch (const std::exception &e) {
				return TMcpToolResult::Error(e.what());
			}
		}
	});

	//=======================================================================
	// 9. get_glossary
	//=======================================================================
	tools.push_back({
		"get_glossary",
		"Look up a glossary term. Tries exact match first, then FTS fallback.",
		TMcpToolSchema()
			.AddString("term", "Term to look up", true),
		[db](const json &args, TMcpToolContext &ctx) -> TMcpToolResult {
			std::string term = GetStr(args, "term");
			if (term.empty())
				return TMcpToolResult::Error("Missing required parameter: term");

			Params p;
			p["term"] = term;

			try {
				// Try exact match first
				json rows = db->Query("SELECT * FROM glossary WHERE term = :term", p);

				if (rows.empty()) {
					// FTS fallback
					std::string searchTerm = AddWildcard(term);
					Params ftsParams;
					ftsParams["term"] = searchTerm;
					rows = db->Query(
						"SELECT g.* FROM glossary g "
						"INNER JOIN glossary_fts fts ON g.id = fts.rowid "
						"WHERE glossary_fts MATCH :term "
						"ORDER BY rank", ftsParams);
				}

				json resp;
				resp["glossary"] = rows;
				resp["count"] = rows.size();
				return TMcpToolResult::Success(resp);
			} catch (const std::exception &e) {
				return TMcpToolResult::Error(e.what());
			}
		}
	});

	//=======================================================================
	// 10. save_query
	//=======================================================================
	tools.push_back({
		"save_query",
		"Save a tested SQL query with its purpose and notes. Only save queries that have been executed successfully. Retrievable later via get_query by purpose keyword search.",
		TMcpToolSchema()
			.AddString("purpose", "What this query does", true)
			.AddString("sql_text", "The SQL query text", true)
			.AddString("result_summary", "Summary of expected results")
			.AddString("notes", "Additional notes"),
		[db](const json &args, TMcpToolContext &ctx) -> TMcpToolResult {
			std::string purpose = GetStr(args, "purpose");
			std::string sqlText = GetStr(args, "sql_text");
			if (purpose.empty() || sqlText.empty())
				return TMcpToolResult::Error("Missing required parameters: purpose, sql_text");

			Params p;
			p["purpose"] = purpose;
			p["sql_text"] = sqlText;
			p["result_summary"] = GetStr(args, "result_summary");
			p["notes"] = GetStr(args, "notes");

			try {
				db->Execute(
					"INSERT INTO verified_queries (purpose, sql_text, result_summary, notes) "
					"VALUES (:purpose, :sql_text, :result_summary, :notes)", p);
				long long id = db->LastInsertRowId();

				// Auto-resolve search gaps mentioning this purpose
				try {
					Params rp;
					rp["purp"] = "%" + purpose + "%";
					db->Execute(
						"UPDATE search_gaps SET resolved=1, resolved_by='save_query', resolved_at=datetime('now') "
						"WHERE resolved=0 AND query_text LIKE :purp", rp);
				} catch (...) {}

				json resp;
				resp["success"] = true;
				resp["id"] = id;
				resp["message"] = "Query saved: " + purpose;
				return TMcpToolResult::Success(resp);
			} catch (const std::exception &e) {
				return TMcpToolResult::Error(e.what());
			}
		}
	});

	//=======================================================================
	// 11. get_query
	//=======================================================================
	tools.push_back({
		"get_query",
		"Search previously tested and verified SQL queries by purpose. Use to find ready-to-use SQL instead of writing from scratch. Returns sql_text, notes, and result_summary.",
		TMcpToolSchema()
			.AddString("purpose", "Search query for purpose", true),
		[db](const json &args, TMcpToolContext &ctx) -> TMcpToolResult {
			std::string purpose = GetStr(args, "purpose");
			if (purpose.empty())
				return TMcpToolResult::Error("Missing required parameter: purpose");

			std::string ftsQuery = AddWildcard(purpose);
			std::string likePattern = MakeLikePattern(purpose);

			Params p;
			p["purpose"] = ftsQuery;
			p["like_pattern"] = likePattern;

			try {
				json rows = db->Query(
					"SELECT vq.* FROM verified_queries vq INNER JOIN queries_fts fts ON vq.id = fts.rowid WHERE queries_fts MATCH :purpose "
					"UNION "
					"SELECT vq.* FROM verified_queries vq WHERE vq.purpose LIKE :like_pattern "
					"ORDER BY id", p);

				// Auto-log search gap if no results
				if (rows.empty()) {
					try {
						Params gp;
						gp["qt"] = purpose;
						gp["tn"] = "get_query";
						db->Execute("INSERT INTO search_gaps(query_text, tool_name, result_count) VALUES(:qt,:tn,0)", gp);
					} catch (...) {}
				}

				json resp;
				resp["queries"] = rows;
				resp["count"] = rows.size();
				return TMcpToolResult::Success(resp);
			} catch (const std::exception &e) {
				return TMcpToolResult::Error(e.what());
			}
		}
	});

	//=======================================================================
	// 12. save_pattern
	//=======================================================================
	tools.push_back({
		"save_pattern",
		"Save a code pattern with description and structure. UPSERT by pattern_name.",
		TMcpToolSchema()
			.AddString("pattern_name", "Pattern name (e.g. 'BaseCrudDialog')", true)
			.AddString("description", "Pattern description", true)
			.AddString("structure", "Pattern structure (JSON or text)")
			.AddString("files", "JSON array of related files")
			.AddString("evidence", "Evidence or examples"),
		[db](const json &args, TMcpToolContext &ctx) -> TMcpToolResult {
			std::string patternName = GetStr(args, "pattern_name");
			std::string description = GetStr(args, "description");
			if (patternName.empty() || description.empty())
				return TMcpToolResult::Error("Missing required parameters: pattern_name, description");

			Params p;
			p["pattern_name"] = patternName;
			p["description"] = description;
			p["structure"] = GetStr(args, "structure");
			p["files"] = GetStr(args, "files");
			p["evidence"] = GetStr(args, "evidence");

			try {
				db->Execute(
					"INSERT OR REPLACE INTO code_patterns "
					"(pattern_name, description, structure, files, evidence) "
					"VALUES (:pattern_name, :description, :structure, :files, :evidence)", p);

				json resp;
				resp["success"] = true;
				resp["message"] = "Pattern saved: " + patternName;
				return TMcpToolResult::Success(resp);
			} catch (const std::exception &e) {
				return TMcpToolResult::Error(e.what());
			}
		}
	});

	//=======================================================================
	// 13. get_pattern
	//=======================================================================
	tools.push_back({
		"get_pattern",
		"Get a code pattern by exact name or full-text search.",
		TMcpToolSchema()
			.AddString("pattern_name", "Exact pattern name")
			.AddString("query", "Full-text search query"),
		[db](const json &args, TMcpToolContext &ctx) -> TMcpToolResult {
			std::string patternName = GetStr(args, "pattern_name");
			std::string query = GetStr(args, "query");

			if (patternName.empty() && query.empty())
				return TMcpToolResult::Error("At least one parameter required: pattern_name or query");

			try {
				json rows;

				if (!patternName.empty()) {
					Params p;
					p["pattern_name"] = patternName;
					rows = db->Query(
						"SELECT * FROM code_patterns WHERE pattern_name = :pattern_name", p);

					if (rows.empty() && !query.empty()) {
						std::string ftsQuery = AddWildcard(query);
						std::string likePattern = MakeLikePattern(query);
						Params p2;
						p2["query"] = ftsQuery;
						p2["like_pattern"] = likePattern;
						rows = db->Query(
							"SELECT cp.* FROM code_patterns cp INNER JOIN patterns_fts fts ON cp.id = fts.rowid WHERE patterns_fts MATCH :query "
							"UNION "
							"SELECT cp.* FROM code_patterns cp WHERE cp.description LIKE :like_pattern "
							"ORDER BY id", p2);
					}
				} else {
					std::string ftsQuery = AddWildcard(query);
					std::string likePattern = MakeLikePattern(query);
					Params p;
					p["query"] = ftsQuery;
					p["like_pattern"] = likePattern;
					rows = db->Query(
						"SELECT cp.* FROM code_patterns cp INNER JOIN patterns_fts fts ON cp.id = fts.rowid WHERE patterns_fts MATCH :query "
						"UNION "
						"SELECT cp.* FROM code_patterns cp WHERE cp.description LIKE :like_pattern "
						"ORDER BY id", p);
				}

				json resp;
				resp["patterns"] = rows;
				resp["count"] = rows.size();
				return TMcpToolResult::Success(resp);
			} catch (const std::exception &e) {
				return TMcpToolResult::Error(e.what());
			}
		}
	});

	//=======================================================================
	// 14. ask_business
	//=======================================================================
	tools.push_back({
		"ask_business",
		"Broad search across ALL 7 knowledge stores: glossary, processes, facts, forms, "
		"verified queries, code patterns, relationships. "
		"Two modes: 'compact' (default) returns entity-centric knowledge cards with "
		"coverage level, module, form mapping, fact counts by type, relationship graph. "
		"'full' returns raw matching records from all tables. "
		"Compact mode is token-efficient and best for navigation -- shows WHAT is known "
		"about each entity and WHERE to drill deeper. Full mode for deep content retrieval.",
		TMcpToolSchema()
			.AddString("question", "Business question to search for", true)
			.AddString("mode", "Response mode: compact (entity cards, default) or full (raw records)"),
		[db](const json &args, TMcpToolContext &ctx) -> TMcpToolResult {
			std::string question = GetStr(args, "question");
			if (question.empty())
				return TMcpToolResult::Error("Missing required parameter: question");

			std::string mode = GetStr(args, "mode", "compact");

			// Filter stop words for more relevant results
			auto words = SplitWordsFiltered(question, db);
			if (words.empty()) words.push_back(question);
			if (words.size() > 5) words.resize(5);

			// Build FTS OR query from filtered words
			std::string ftsQuery;
			for (const auto &w : words) {
				if (!ftsQuery.empty()) ftsQuery += " OR ";
				ftsQuery += AddWildcard(w);
			}

			Params p;
			p["query"] = ftsQuery;

			// Build per-word LIKE clauses for each table type
			std::string glossaryLike, processLike, factLike,
						formLike, relLike, queryLike, patternLike;
			for (size_t i = 0; i < words.size(); i++) {
				std::string pn = "like_w" + std::to_string(i);
				p[pn] = MakeLikePattern(words[i]);
				std::string sep = (i > 0) ? " OR " : "";
				glossaryLike += sep + "g.term LIKE :" + pn + " OR g.context LIKE :" + pn;
				processLike += sep + "bp.name LIKE :" + pn + " OR bp.description LIKE :" + pn;
				factLike += sep + "f.description LIKE :" + pn + " OR f.entity LIKE :" + pn;
				formLike += sep + "fm.template LIKE :" + pn + " OR fm.class_name LIKE :" + pn
					+ " OR fm.main_table LIKE :" + pn;
				relLike += sep + "r.entity_from LIKE :" + pn + " OR r.entity_to LIKE :" + pn;
				queryLike += sep + "vq.purpose LIKE :" + pn + " OR vq.notes LIKE :" + pn;
				patternLike += sep + "cp.pattern_name LIKE :" + pn + " OR cp.description LIKE :" + pn;
			}

			try {
				// === Search all 7 data sources ===
				json glossaryMatches = RankedSearch(db, p,
					"SELECT g.* FROM glossary g INNER JOIN glossary_fts fts ON g.id = fts.rowid "
					"WHERE glossary_fts MATCH :query ORDER BY fts.rank LIMIT 10",
					"SELECT g.* FROM glossary g WHERE " + glossaryLike + " LIMIT 10", 10);

				json processMatches = RankedSearch(db, p,
					"SELECT bp.* FROM business_processes bp INNER JOIN processes_fts fts ON bp.id = fts.rowid "
					"WHERE processes_fts MATCH :query ORDER BY fts.rank LIMIT 10",
					"SELECT bp.* FROM business_processes bp WHERE " + processLike + " LIMIT 10", 10);

				json factMatches = RankedSearch(db, p,
					"SELECT f.* FROM facts f INNER JOIN facts_fts fts ON f.id = fts.rowid "
					"WHERE facts_fts MATCH :query ORDER BY fts.rank LIMIT 20",
					"SELECT f.* FROM facts f WHERE " + factLike + " LIMIT 20", 20);

				json queryMatches = RankedSearch(db, p,
					"SELECT vq.id, vq.purpose, vq.notes, vq.created_at FROM verified_queries vq "
					"INNER JOIN queries_fts fts ON vq.id = fts.rowid "
					"WHERE queries_fts MATCH :query ORDER BY fts.rank LIMIT 5",
					"SELECT vq.id, vq.purpose, vq.notes, vq.created_at FROM verified_queries vq WHERE "
					+ queryLike + " LIMIT 5", 5);

				json patternMatches = RankedSearch(db, p,
					"SELECT cp.id, cp.pattern_name, cp.description FROM code_patterns cp "
					"INNER JOIN patterns_fts fts ON cp.id = fts.rowid "
					"WHERE patterns_fts MATCH :query ORDER BY fts.rank LIMIT 3",
					"SELECT cp.id, cp.pattern_name, cp.description FROM code_patterns cp WHERE "
					+ patternLike + " LIMIT 3", 3);

				json formMatches = json::array();
				try { formMatches = db->Query(
					"SELECT * FROM form_table_map fm WHERE " + formLike + " LIMIT 10", p);
				} catch (...) {}

				json relMatches = json::array();
				try { relMatches = db->Query(
					"SELECT * FROM relationships r WHERE " + relLike + " LIMIT 15", p);
				} catch (...) {}

				int totalMatches = (int)(glossaryMatches.size() + processMatches.size()
					+ factMatches.size() + queryMatches.size() + patternMatches.size()
					+ formMatches.size() + relMatches.size());

				// Auto-log search gap if no results across all stores
				if (totalMatches == 0) {
					try {
						Params gp;
						gp["qt"] = question;
						gp["tn"] = "ask_business";
						db->Execute("INSERT INTO search_gaps(query_text, tool_name, result_count) VALUES(:qt,:tn,0)", gp);
					} catch (...) {}
				}

				// === Full mode: return raw results from all tables ===
				if (mode == "full") {
					json resp;
					resp["mode"] = "full";
					resp["glossary_matches"] = glossaryMatches;
					resp["process_matches"] = processMatches;
					resp["fact_matches"] = factMatches;
					resp["query_matches"] = queryMatches;
					resp["pattern_matches"] = patternMatches;
					resp["form_matches"] = formMatches;
					resp["relationship_matches"] = relMatches;
					resp["total_matches"] = totalMatches;
					return TMcpToolResult::Success(resp);
				}

				// === Compact mode: entity-centric knowledge cards ===

				// 1. Extract entities with relevance scores
				std::map<std::string, int> entScores;
				std::map<std::string, std::set<std::string>> entSources;

				for (const auto &r : factMatches) {
					std::string e = r.value("entity", std::string());
					if (!e.empty()) { entScores[e] += 2; entSources[e].insert("facts"); }
				}
				for (const auto &r : glossaryMatches) {
					std::string e = r.value("entity", std::string());
					if (!e.empty()) { entScores[e] += 3; entSources[e].insert("glossary"); }
				}
				for (const auto &r : formMatches) {
					std::string mt = r.value("main_table", std::string());
					if (!mt.empty()) { entScores[mt] += 3; entSources[mt].insert("forms"); }
				}
				// Deduplicate process-related entities (flat +1 per entity)
				std::set<std::string> procEnts;
				for (const auto &r : processMatches) {
					std::string re = r.value("related_entities", std::string());
					try {
						json arr = json::parse(re);
						if (arr.is_array())
							for (const auto &item : arr)
								if (item.is_string()) procEnts.insert(item.get<std::string>());
					} catch (...) {}
				}
				for (const auto &e : procEnts) { entScores[e] += 1; entSources[e].insert("processes"); }

				// Deduplicate relationship entities (flat +1 per entity)
				std::set<std::string> relEnts;
				for (const auto &r : relMatches) {
					std::string ef = r.value("entity_from", std::string());
					std::string et = r.value("entity_to", std::string());
					if (!ef.empty()) relEnts.insert(ef);
					if (!et.empty()) relEnts.insert(et);
				}
				for (const auto &e : relEnts) { entScores[e] += 1; entSources[e].insert("relationships"); }

				// 2. Rank entities by score, take top 15
				std::vector<std::pair<std::string, int>> ranked(entScores.begin(), entScores.end());
				std::sort(ranked.begin(), ranked.end(),
					[](const std::pair<std::string,int> &a, const std::pair<std::string,int> &b) {
						return a.second > b.second;
					});
				if (ranked.size() > 15) ranked.resize(15);

				// 3. Build compact summaries for non-entity results
				json procSummary = json::array();
				for (const auto &r : processMatches) {
					json ps;
					ps["id"] = r.value("id", (long long)0);
					ps["name"] = r.value("name", std::string());
					int reCount = 0;
					try {
						json arr = json::parse(r.value("related_entities", std::string()));
						if (arr.is_array()) reCount = (int)arr.size();
					} catch (...) {}
					ps["entity_count"] = reCount;
					procSummary.push_back(ps);
				}
				json querySummary = json::array();
				for (const auto &r : queryMatches) {
					querySummary.push_back(json{
						{"id", r.value("id", (long long)0)},
						{"purpose", r.value("purpose", std::string())}
					});
				}
				json patternSummary = json::array();
				for (const auto &r : patternMatches) {
					std::string desc = r.value("description", std::string());
					// Truncate to ~150 UTF-8 characters for compact mode
					{
						int charCount = 0;
						size_t pos = 0;
						while (pos < desc.length() && charCount < 150) {
							unsigned char c = static_cast<unsigned char>(desc[pos]);
							if (c < 0x80) pos += 1;
							else if (c < 0xE0) pos += 2;
							else if (c < 0xF0) pos += 3;
							else pos += 4;
							charCount++;
						}
						if (pos < desc.length()) {
							desc = desc.substr(0, pos) + "...";
						}
					}
					patternSummary.push_back(json{
						{"pattern_name", r.value("pattern_name", std::string())},
						{"description", desc}
					});
				}

				// 4. Entity enrichment via batch queries
				json entityCards = json::array();
				if (!ranked.empty()) {
					std::string inC = "(";
					for (size_t i = 0; i < ranked.size(); i++) {
						if (i > 0) inC += ",";
						inC += "'" + EscapeSqlString(ranked[i].first) + "'";
					}
					inC += ")";

					auto safeQ = [db](const std::string &sql) -> json {
						try { return db->Query(sql, {}); }
						catch (...) { return json::array(); }
					};

					json stRows = safeQ("SELECT entity, coverage FROM entity_status WHERE entity IN " + inC);
					json moRows = safeQ("SELECT entity, module_name FROM entity_modules WHERE entity IN " + inC);
					json fcRows = safeQ("SELECT entity, fact_type, COUNT(*) as cnt FROM facts WHERE entity IN "
						+ inC + " GROUP BY entity, fact_type");
					json rcRows = safeQ(
						"SELECT entity_from as entity, 'out' as dir, COUNT(*) as cnt "
						"FROM relationships WHERE entity_from IN " + inC + " GROUP BY entity_from "
						"UNION ALL "
						"SELECT entity_to as entity, 'in' as dir, COUNT(*) as cnt "
						"FROM relationships WHERE entity_to IN " + inC + " GROUP BY entity_to");
					json rsRows = safeQ(
						"SELECT entity_from, entity_to, rel_type FROM relationships "
						"WHERE entity_from IN " + inC + " OR entity_to IN " + inC + " LIMIT 60");
					json frRows = safeQ("SELECT template, class_name, file_path, main_table "
						"FROM form_table_map WHERE main_table IN " + inC);
					json glRows = safeQ("SELECT entity, term, menu_path FROM glossary WHERE entity IN " + inC);

					// Build lookup maps
					std::map<std::string, std::string> covMap, modMap;
					std::map<std::string, json> ftMap, frmMap, glsMap;
					std::map<std::string, int> relOut, relIn;
					std::map<std::string, std::vector<std::string>> relSOut, relSIn;

					for (const auto &r : stRows)
						covMap[r.value("entity", std::string())] = r.value("coverage", std::string());
					for (const auto &r : moRows)
						modMap[r.value("entity", std::string())] = r.value("module_name", std::string());
					for (const auto &r : fcRows) {
						std::string e = r.value("entity", std::string());
						std::string ft = r.value("fact_type", std::string());
						long long cnt = r.value("cnt", (long long)0);
						if (!e.empty() && !ft.empty()) ftMap[e][ft] = cnt;
					}
					for (const auto &r : rcRows) {
						std::string e = r.value("entity", std::string());
						std::string d = r.value("dir", std::string());
						int cnt = (int)r.value("cnt", (long long)0);
						if (d == "out") relOut[e] = cnt; else relIn[e] = cnt;
					}
					for (const auto &r : rsRows) {
						std::string ef = r.value("entity_from", std::string());
						std::string et = r.value("entity_to", std::string());
						std::string rt = r.value("rel_type", std::string());
						if (relSOut[ef].size() < 5)
							relSOut[ef].push_back(et + " (" + rt + ")");
						if (relSIn[et].size() < 5)
							relSIn[et].push_back(ef + " (" + rt + ")");
					}
					for (const auto &r : frRows) {
						std::string mt = r.value("main_table", std::string());
						frmMap[mt] = json{
							{"class_name", r.value("class_name", std::string())},
							{"file_path", r.value("file_path", std::string())},
							{"template", r.value("template", std::string())}
						};
					}
					for (const auto &r : glRows) {
						std::string e = r.value("entity", std::string());
						glsMap[e] = json{
							{"term", r.value("term", std::string())},
							{"menu_path", r.value("menu_path", std::string())}
						};
					}

					// 5. Assemble entity cards
					for (size_t i = 0; i < ranked.size(); i++) {
						const std::string &ent = ranked[i].first;
						json card;
						card["entity"] = ent;
						card["relevance"] = ranked[i].second;

						// Sources this entity was found in
						json foundIn = json::array();
						auto sit = entSources.find(ent);
						if (sit != entSources.end())
							for (const auto &s : sit->second) foundIn.push_back(s);
						card["found_in"] = foundIn;

						// Coverage level
						auto ci = covMap.find(ent);
						card["coverage"] = (ci != covMap.end()) ? ci->second : "unknown";

						// Module
						auto mi = modMap.find(ent);
						if (mi != modMap.end()) card["module"] = mi->second;

						// Glossary term
						auto gi = glsMap.find(ent);
						if (gi != glsMap.end()) card["glossary"] = gi->second;

						// Form mapping
						auto fi = frmMap.find(ent);
						if (fi != frmMap.end()) card["form"] = fi->second;

						// Fact counts by type
						auto fti = ftMap.find(ent);
						if (fti != ftMap.end()) {
							card["facts"] = fti->second;
							int total = 0;
							for (auto it = fti->second.begin(); it != fti->second.end(); ++it)
								if (it.value().is_number()) total += it.value().get<int>();
							card["facts"]["_total"] = total;
						}

						// Relationships with samples
						int ro = 0, ri2 = 0;
						auto roi = relOut.find(ent); if (roi != relOut.end()) ro = roi->second;
						auto rii = relIn.find(ent); if (rii != relIn.end()) ri2 = rii->second;
						if (ro > 0 || ri2 > 0) {
							json rel;
							rel["outgoing"] = ro;
							rel["incoming"] = ri2;
							auto rsoi = relSOut.find(ent);
							if (rsoi != relSOut.end()) rel["sample_out"] = json(rsoi->second);
							auto rsii = relSIn.find(ent);
							if (rsii != relSIn.end()) rel["sample_in"] = json(rsii->second);
							card["relationships"] = rel;
						}

						entityCards.push_back(card);
					}
				}

				json resp;
				resp["mode"] = "compact";
				resp["entity_cards"] = entityCards;
				resp["matching_processes"] = procSummary;
				resp["matching_queries"] = querySummary;
				resp["matching_patterns"] = patternSummary;
				resp["total_entities"] = (int)entityCards.size();
				resp["total_matches"] = totalMatches;
				return TMcpToolResult::Success(resp);
			} catch (const std::exception &e) {
				return TMcpToolResult::Error(e.what());
			}
		}
	});

	//=======================================================================
	// 15. update_fact
	//=======================================================================
	tools.push_back({
		"update_fact",
		"Update an existing fact by ID. Only provided fields are updated.",
		TMcpToolSchema()
			.AddInteger("id", "Fact ID to update", true)
			.AddString("entity", "New entity name")
			.AddString("fact_type", "New fact type")
			.AddString("description", "New description")
			.AddString("evidence", "New evidence")
			.AddString("confidence", "New confidence level")
			.AddString("task_context", "New task context"),
		[db](const json &args, TMcpToolContext &ctx) -> TMcpToolResult {
			long long id = 0;
			if (!TryGetInt64(args, "id", id))
				return TMcpToolResult::Error("Missing or invalid parameter: id");

			std::vector<std::string> fields = {"entity", "fact_type", "description", "evidence", "confidence", "task_context"};
			std::string setClauses;
			Params p;
			p["id"] = std::to_string(id);

			for (const auto &fname : fields) {
				if (args.contains(fname) && !args[fname].is_null()) {
					if (!setClauses.empty()) setClauses += ", ";
					setClauses += fname + " = :" + fname;
					p[fname] = GetStr(args, fname);
				}
			}

			if (setClauses.empty())
				return TMcpToolResult::Error("No fields provided to update");

			setClauses += ", updated_at = datetime('now')";
			std::string sql = "UPDATE facts SET " + setClauses + " WHERE id = :id";

			try {
				// Save fact history before updating
				if (p.count("description")) {
					try {
						Params hp;
						hp["fid"] = std::to_string(id);
						hp["new_desc"] = p["description"];
						json oldRow = db->Query("SELECT description FROM facts WHERE id = :fid", hp);
						if (!oldRow.empty()) {
							hp["old_desc"] = oldRow[0].value("description", std::string());
							db->Execute(
								"INSERT INTO fact_history(fact_id, old_description, new_description) "
								"VALUES(:fid, :old_desc, :new_desc)", hp);
						}
					} catch (...) {}
				}

				int affected = db->Execute(sql, p);

				json resp;
				if (affected > 0) {
					resp["success"] = true;
					resp["id"] = id;
					resp["message"] = "Fact updated";
				} else {
					resp["success"] = false;
					resp["message"] = "Fact not found with id: " + std::to_string(id);
				}
				return TMcpToolResult::Success(resp);
			} catch (const std::exception &e) {
				return TMcpToolResult::Error(e.what());
			}
		}
	});

	//=======================================================================
	// 16. delete_fact
	//=======================================================================
	tools.push_back({
		"delete_fact",
		"Delete an outdated or incorrect fact by ID. Use get_facts or search_facts first to find the fact ID.",
		TMcpToolSchema()
			.AddInteger("id", "Fact ID to delete", true),
		[db](const json &args, TMcpToolContext &ctx) -> TMcpToolResult {
			long long id = 0;
			if (!TryGetInt64(args, "id", id))
				return TMcpToolResult::Error("Missing or invalid parameter: id");

			Params p;
			p["id"] = std::to_string(id);

			try {
				int affected = db->Execute("DELETE FROM facts WHERE id = :id", p);

				json resp;
				if (affected > 0) {
					resp["success"] = true;
					resp["id"] = id;
					resp["message"] = "Fact deleted";
				} else {
					resp["success"] = false;
					resp["message"] = "Fact not found with id: " + std::to_string(id);
				}
				return TMcpToolResult::Success(resp);
			} catch (const std::exception &e) {
				return TMcpToolResult::Error(e.what());
			}
		}
	});

	//=======================================================================
	// 17. list_entities
	//=======================================================================
	tools.push_back({
		"list_entities",
		"List all known entities with fact counts and types. Useful for understanding knowledge coverage.",
		TMcpToolSchema()
			.AddString("fact_type", "Optional filter by fact type (e.g. 'schema', 'business_rule')"),
		[db](const json &args, TMcpToolContext &ctx) -> TMcpToolResult {
			std::string factType = GetStr(args, "fact_type");

			try {
				json rows;
				json totalRows;

				if (!factType.empty()) {
					Params p;
					p["fact_type"] = factType;
					rows = db->Query(
						"SELECT entity, COUNT(*) as fact_count, "
						"GROUP_CONCAT(DISTINCT fact_type) as fact_types, "
						"MAX(updated_at) as last_updated "
						"FROM facts WHERE fact_type = :fact_type "
						"GROUP BY entity ORDER BY entity", p);
					totalRows = db->Query(
						"SELECT COUNT(*) as total FROM facts WHERE fact_type = :fact_type", p);
				} else {
					rows = db->Query(
						"SELECT entity, COUNT(*) as fact_count, "
						"GROUP_CONCAT(DISTINCT fact_type) as fact_types, "
						"MAX(updated_at) as last_updated "
						"FROM facts GROUP BY entity ORDER BY entity", {});
					totalRows = db->Query("SELECT COUNT(*) as total FROM facts", {});
				}

				int totalFacts = 0;
				if (!totalRows.empty() && totalRows[0].contains("total"))
					totalFacts = totalRows[0]["total"].get<int>();

				json resp;
				resp["entities"] = rows;
				resp["total_entities"] = rows.size();
				resp["total_facts"] = totalFacts;
				return TMcpToolResult::Success(resp);
			} catch (const std::exception &e) {
				return TMcpToolResult::Error(e.what());
			}
		}
	});

	//=======================================================================
	// 18. save_relationship
	//=======================================================================
	tools.push_back({
		"save_relationship",
		"Save a directional relationship between two entities. UPSERT by (entity_from, entity_to, rel_type).",
		TMcpToolSchema()
			.AddString("entity_from", "Source entity name", true)
			.AddString("entity_to", "Target entity name", true)
			.AddString("rel_type", "Relationship type: creates|contains|references|depends_on|produces|consumes", true)
			.AddString("description", "Relationship description")
			.AddString("evidence", "Evidence (file:line, SQL, etc.)"),
		[db](const json &args, TMcpToolContext &ctx) -> TMcpToolResult {
			std::string entityFrom = GetStr(args, "entity_from");
			std::string entityTo = GetStr(args, "entity_to");
			std::string relType = GetStr(args, "rel_type");

			if (entityFrom.empty() || entityTo.empty() || relType.empty())
				return TMcpToolResult::Error("Missing required parameters: entity_from, entity_to, rel_type");

			Params p;
			p["entity_from"] = entityFrom;
			p["entity_to"] = entityTo;
			p["rel_type"] = relType;
			p["description"] = GetStr(args, "description");
			p["evidence"] = GetStr(args, "evidence");

			try {
				db->Execute(
					"INSERT OR REPLACE INTO relationships (entity_from, entity_to, rel_type, description, evidence) "
					"VALUES (:entity_from, :entity_to, :rel_type, :description, :evidence)", p);
				long long id = db->LastInsertRowId();

				json resp;
				resp["success"] = true;
				resp["id"] = id;
				resp["message"] = "Relationship saved: " + entityFrom + " -[" + relType + "]-> " + entityTo;
				return TMcpToolResult::Success(resp);
			} catch (const std::exception &e) {
				return TMcpToolResult::Error(e.what());
			}
		}
	});

	//=======================================================================
	// 19. get_relationships
	//=======================================================================
	tools.push_back({
		"get_relationships",
		"Get relationships for an entity. Returns both outgoing and incoming by default.",
		TMcpToolSchema()
			.AddString("entity", "Entity name to find relationships for", true)
			.AddString("direction", "Filter: outgoing|incoming|both (default: both)")
			.AddString("rel_type", "Filter by relationship type"),
		[db](const json &args, TMcpToolContext &ctx) -> TMcpToolResult {
			std::string entity = GetStr(args, "entity");
			if (entity.empty())
				return TMcpToolResult::Error("Missing required parameter: entity");

			std::string direction = GetStr(args, "direction", "both");
			std::string relType = GetStr(args, "rel_type");

			try {
				std::string sql;
				Params p;
				p["entity"] = entity;

				if (direction == "outgoing") {
					sql = "SELECT *, 'outgoing' as direction FROM relationships WHERE entity_from = :entity";
					if (!relType.empty()) {
						sql += " AND rel_type = :rel_type";
						p["rel_type"] = relType;
					}
				} else if (direction == "incoming") {
					sql = "SELECT *, 'incoming' as direction FROM relationships WHERE entity_to = :entity";
					if (!relType.empty()) {
						sql += " AND rel_type = :rel_type";
						p["rel_type"] = relType;
					}
				} else {
					if (!relType.empty()) {
						p["rel_type"] = relType;
						sql = "SELECT *, 'outgoing' as direction FROM relationships WHERE entity_from = :entity AND rel_type = :rel_type "
							  "UNION ALL "
							  "SELECT *, 'incoming' as direction FROM relationships WHERE entity_to = :entity AND rel_type = :rel_type";
					} else {
						sql = "SELECT *, 'outgoing' as direction FROM relationships WHERE entity_from = :entity "
							  "UNION ALL "
							  "SELECT *, 'incoming' as direction FROM relationships WHERE entity_to = :entity";
					}
				}

				json rows = db->Query(sql, p);

				json resp;
				resp["relationships"] = rows;
				resp["count"] = rows.size();
				return TMcpToolResult::Success(resp);
			} catch (const std::exception &e) {
				return TMcpToolResult::Error(e.what());
			}
		}
	});

	//=======================================================================
	// 20. bulk_save
	//=======================================================================
	tools.push_back({
		"bulk_save",
		"Save multiple items in a single atomic transaction. Accepts arrays of: facts, glossary terms, form_mappings, relationships. "
		"Use after investigation sessions to save 3+ items at once. All-or-nothing: if any item fails, nothing is saved.",
		TMcpToolSchema()
			.AddString("facts", "Array of fact objects with entity, fact_type, description, evidence, confidence, task_context")
			.AddString("glossary", "Array of glossary objects with term, synonyms, entity, menu_path, context")
			.AddString("form_mappings", "Array of form_mapping objects with template, class_name, file_path, main_table, sql_file, operations")
			.AddString("relationships", "Array of relationship objects with entity_from, entity_to, rel_type, description, evidence"),
		[db](const json &args, TMcpToolContext &ctx) -> TMcpToolResult {
			int factCount = 0, glossaryCount = 0, formCount = 0, relCount = 0;

			try {
				db->BeginTransaction();

				// Save facts
				json factsArr = GetArray(args, "facts");
				if (!factsArr.empty()) {
					for (const auto &item : factsArr) {
						std::string entity = GetStr(item, "entity");
						std::string factType = GetStr(item, "fact_type");
						std::string description = GetStr(item, "description");
						if (entity.empty() || factType.empty() || description.empty()) continue;

						Params p;
						p["entity"] = entity;
						p["fact_type"] = factType;
						p["description"] = description;
						p["evidence"] = GetStr(item, "evidence");
						p["confidence"] = GetStr(item, "confidence", "unverified");
						p["task_context"] = GetStr(item, "task_context");

						db->Execute(
							"INSERT INTO facts (entity, fact_type, description, evidence, confidence, task_context) "
							"VALUES (:entity, :fact_type, :description, :evidence, :confidence, :task_context)", p);
						factCount++;
					}
				}

				// Save glossary
				json glossaryArr = GetArray(args, "glossary");
				if (!glossaryArr.empty()) {
					for (const auto &item : glossaryArr) {
						std::string term = GetStr(item, "term");
						if (term.empty()) continue;

						Params p;
						p["term"] = term;
						p["synonyms"] = GetStr(item, "synonyms");
						p["entity"] = GetStr(item, "entity");
						p["menu_path"] = GetStr(item, "menu_path");
						p["context"] = GetStr(item, "context");

						db->Execute(
							"INSERT OR REPLACE INTO glossary (term, synonyms, entity, menu_path, context) "
							"VALUES (:term, :synonyms, :entity, :menu_path, :context)", p);
						glossaryCount++;
					}
				}

				// Save form_mappings
				json formArr = GetArray(args, "form_mappings");
				if (!formArr.empty()) {
					for (const auto &item : formArr) {
						std::string tmpl = GetStr(item, "template");
						if (tmpl.empty()) continue;

						Params p;
						p["template"] = tmpl;
						p["class_name"] = GetStr(item, "class_name");
						p["file_path"] = GetStr(item, "file_path");
						p["main_table"] = GetStr(item, "main_table");
						p["sql_file"] = GetStr(item, "sql_file");
						p["operations"] = GetStr(item, "operations");

						db->Execute(
							"INSERT OR REPLACE INTO form_table_map "
							"(template, class_name, file_path, main_table, sql_file, operations) "
							"VALUES (:template, :class_name, :file_path, :main_table, :sql_file, :operations)", p);
						formCount++;
					}
				}

				// Save relationships
				json relArr = GetArray(args, "relationships");
				if (!relArr.empty()) {
					for (const auto &item : relArr) {
						std::string entityFrom = GetStr(item, "entity_from");
						std::string entityTo = GetStr(item, "entity_to");
						std::string relType = GetStr(item, "rel_type");
						if (entityFrom.empty() || entityTo.empty() || relType.empty()) continue;

						Params p;
						p["entity_from"] = entityFrom;
						p["entity_to"] = entityTo;
						p["rel_type"] = relType;
						p["description"] = GetStr(item, "description");
						p["evidence"] = GetStr(item, "evidence");

						db->Execute(
							"INSERT OR REPLACE INTO relationships (entity_from, entity_to, rel_type, description, evidence) "
							"VALUES (:entity_from, :entity_to, :rel_type, :description, :evidence)", p);
						relCount++;
					}
				}

				db->Commit();

				int total = factCount + glossaryCount + formCount + relCount;
				if (total == 0)
					return TMcpToolResult::Error("No valid items provided. At least one array must contain valid items.");

				json resp;
				resp["success"] = true;
				resp["saved"] = json{
					{"facts", factCount},
					{"glossary", glossaryCount},
					{"form_mappings", formCount},
					{"relationships", relCount}
				};
				resp["total"] = total;
				resp["message"] = "Saved " + std::to_string(total) + " items";
				return TMcpToolResult::Success(resp);
			} catch (const std::exception &e) {
				try { db->Rollback(); } catch (...) {}
				return TMcpToolResult::Error(e.what());
			}
		}
	});

	//=======================================================================
	// 21. save_module
	//=======================================================================
	tools.push_back({
		"save_module",
		"Assign entities to a module (functional group). UPSERT - safe to call repeatedly.",
		TMcpToolSchema()
			.AddString("module_name", "Module name (e.g. 'Production', 'Warehouse', 'Projects')", true)
			.AddString("entities", "Array of entity names to add to this module", true),
		[db](const json &args, TMcpToolContext &ctx) -> TMcpToolResult {
			std::string moduleName = GetStr(args, "module_name");
			if (moduleName.empty())
				return TMcpToolResult::Error("Missing required parameter: module_name");

			json entitiesArr = GetArray(args, "entities");
			if (entitiesArr.empty())
				return TMcpToolResult::Error("Missing required parameter: entities (non-empty array)");

			try {
				int added = 0;
				for (const auto &ent : entitiesArr) {
					std::string entity = ent.is_string() ? ent.get<std::string>() : "";
					if (entity.empty()) continue;

					Params p;
					p["module_name"] = moduleName;
					p["entity"] = entity;
					db->Execute(
						"INSERT OR IGNORE INTO entity_modules (module_name, entity) "
						"VALUES (:module_name, :entity)", p);
					added++;
				}

				json resp;
				resp["success"] = true;
				resp["module"] = moduleName;
				resp["entities_added"] = added;
				return TMcpToolResult::Success(resp);
			} catch (const std::exception &e) {
				return TMcpToolResult::Error(e.what());
			}
		}
	});

	//=======================================================================
	// 22. get_module
	//=======================================================================
	tools.push_back({
		"get_module",
		"Get all entities in a module (functional group). Module examples: 'Production', 'Warehouse', 'Projects', 'Finance'. Returns list of entity names belonging to the module.",
		TMcpToolSchema()
			.AddString("module_name", "Module name", true),
		[db](const json &args, TMcpToolContext &ctx) -> TMcpToolResult {
			std::string moduleName = GetStr(args, "module_name");
			if (moduleName.empty())
				return TMcpToolResult::Error("Missing required parameter: module_name");

			Params p;
			p["module_name"] = moduleName;

			try {
				json rows = db->Query(
					"SELECT entity FROM entity_modules WHERE module_name = :module_name ORDER BY entity", p);

				json entities = json::array();
				for (const auto &row : rows) {
					if (row.contains("entity"))
						entities.push_back(row["entity"]);
				}

				json resp;
				resp["module"] = moduleName;
				resp["entities"] = entities;
				resp["count"] = entities.size();
				return TMcpToolResult::Success(resp);
			} catch (const std::exception &e) {
				return TMcpToolResult::Error(e.what());
			}
		}
	});

	//=======================================================================
	// 23. list_modules
	//=======================================================================
	tools.push_back({
		"list_modules",
		"List all modules with entity counts.",
		TMcpToolSchema(),
		[db](const json &args, TMcpToolContext &ctx) -> TMcpToolResult {
			try {
				json rows = db->Query(
					"SELECT module_name, COUNT(*) as entity_count "
					"FROM entity_modules GROUP BY module_name ORDER BY module_name", {});

				json resp;
				resp["modules"] = rows;
				resp["count"] = rows.size();
				return TMcpToolResult::Success(resp);
			} catch (const std::exception &e) {
				return TMcpToolResult::Error(e.what());
			}
		}
	});

	//=======================================================================
	// 24. set_entity_status
	//=======================================================================
	tools.push_back({
		"set_entity_status",
		"Set the research coverage level for an entity. UPSERT by entity name. "
		"Levels: unknown (not studied), schema_only (DB schema explored), partial (some code/logic known), full (completely documented), needs_review (may be outdated).",
		TMcpToolSchema()
			.AddString("entity", "Entity name", true)
			.AddString("coverage", "Coverage level: unknown|schema_only|partial|full|needs_review", true)
			.AddString("notes", "Notes about what is/isn't covered"),
		[db](const json &args, TMcpToolContext &ctx) -> TMcpToolResult {
			std::string entity = GetStr(args, "entity");
			std::string coverage = GetStr(args, "coverage");
			if (entity.empty() || coverage.empty())
				return TMcpToolResult::Error("Missing required parameters: entity, coverage");

			Params p;
			p["entity"] = entity;
			p["coverage"] = coverage;
			p["notes"] = GetStr(args, "notes");

			try {
				db->Execute(
					"INSERT OR REPLACE INTO entity_status (entity, coverage, notes, updated_at) "
					"VALUES (:entity, :coverage, :notes, datetime('now'))", p);

				json resp;
				resp["success"] = true;
				resp["entity"] = entity;
				resp["coverage"] = coverage;
				resp["message"] = "Entity status set: " + entity + " = " + coverage;
				return TMcpToolResult::Success(resp);
			} catch (const std::exception &e) {
				return TMcpToolResult::Error(e.what());
			}
		}
	});

	//=======================================================================
	// 25. get_entity_status
	//=======================================================================
	tools.push_back({
		"get_entity_status",
		"Get research coverage status for one or all entities. Coverage levels: unknown, schema_only, partial, full, needs_review. Omit entity to get all statuses -- useful to find knowledge gaps.",
		TMcpToolSchema()
			.AddString("entity", "Entity name. If omitted, returns all statuses."),
		[db](const json &args, TMcpToolContext &ctx) -> TMcpToolResult {
			std::string entity = GetStr(args, "entity");

			try {
				json rows;
				if (!entity.empty()) {
					Params p;
					p["entity"] = entity;
					rows = db->Query(
						"SELECT es.*, "
						"(SELECT COUNT(*) FROM facts WHERE facts.entity = es.entity) as fact_count "
						"FROM entity_status es WHERE es.entity = :entity", p);
				} else {
					rows = db->Query(
						"SELECT es.*, "
						"(SELECT COUNT(*) FROM facts WHERE facts.entity = es.entity) as fact_count "
						"FROM entity_status es ORDER BY es.coverage, es.entity", {});
				}

				json resp;
				resp["statuses"] = rows;
				resp["count"] = rows.size();
				return TMcpToolResult::Success(resp);
			} catch (const std::exception &e) {
				return TMcpToolResult::Error(e.what());
			}
		}
	});

	//=======================================================================
	// 26. get_tool_stats
	//=======================================================================
	tools.push_back({
		"get_tool_stats",
		"Get usage statistics for MCP tools. Shows call counts, success rates, and last used times.",
		TMcpToolSchema()
			.AddString("tool_name", "Optional: filter by specific tool name")
			.AddString("since", "Optional: only count calls since this datetime (ISO format)"),
		[db](const json &args, TMcpToolContext &ctx) -> TMcpToolResult {
			std::string toolName = GetStr(args, "tool_name");
			std::string since = GetStr(args, "since");

			try {
				std::string sql =
					"SELECT tool_name, "
					"COUNT(*) as call_count, "
					"SUM(CASE WHEN success = 1 THEN 1 ELSE 0 END) as success_count, "
					"SUM(CASE WHEN success = 0 THEN 1 ELSE 0 END) as error_count, "
					"MAX(called_at) as last_called "
					"FROM tool_usage";

				Params p;
				std::string whereClause;

				if (!toolName.empty()) {
					whereClause = " WHERE tool_name = :tool_name";
					p["tool_name"] = toolName;
				}
				if (!since.empty()) {
					if (whereClause.empty())
						whereClause = " WHERE called_at >= :since";
					else
						whereClause += " AND called_at >= :since";
					p["since"] = since;
				}

				sql += whereClause + " GROUP BY tool_name ORDER BY call_count DESC";

				json rows = db->Query(sql, p);

				int totalCalls = 0;
				for (const auto &row : rows) {
					if (row.contains("call_count"))
						totalCalls += row["call_count"].get<int>();
				}

				json resp;
				resp["stats"] = rows;
				resp["total_calls"] = totalCalls;
				resp["period"] = since.empty() ? "all_time" : "since " + since;
				return TMcpToolResult::Success(resp);
			} catch (const std::exception &e) {
				return TMcpToolResult::Error(e.what());
			}
		}
	});

	//=======================================================================
	// 27. submit_feature_request
	//=======================================================================
	tools.push_back({
		"submit_feature_request",
		"Submit a feature request or suggest additional functionality for the ProjectMemory MCP server.",
		TMcpToolSchema()
			.AddString("title", "Short title for the feature request", true)
			.AddString("description", "Detailed description of what is needed", true)
			.AddString("use_case", "Specific use case or scenario that prompted this request")
			.AddString("priority", "Suggested priority: low|normal|high|critical"),
		[db](const json &args, TMcpToolContext &ctx) -> TMcpToolResult {
			std::string title = GetStr(args, "title");
			std::string description = GetStr(args, "description");
			if (title.empty() || description.empty())
				return TMcpToolResult::Error("Missing required parameters: title, description");

			Params p;
			p["title"] = title;
			p["description"] = description;
			p["use_case"] = GetStr(args, "use_case");
			p["priority"] = GetStr(args, "priority", "normal");

			try {
				db->Execute(
					"INSERT INTO feature_requests (title, description, use_case, priority) "
					"VALUES (:title, :description, :use_case, :priority)", p);
				long long id = db->LastInsertRowId();

				json resp;
				resp["success"] = true;
				resp["id"] = id;
				resp["message"] = "Feature request submitted: " + title;
				return TMcpToolResult::Success(resp);
			} catch (const std::exception &e) {
				return TMcpToolResult::Error(e.what());
			}
		}
	});

	//=======================================================================
	// 28. get_feature_requests
	//=======================================================================
	tools.push_back({
		"get_feature_requests",
		"View submitted feature requests. Useful for reviewing what additional functionality has been requested.",
		TMcpToolSchema()
			.AddString("status", "Filter by status: new|approved|implemented|rejected (default: all)"),
		[db](const json &args, TMcpToolContext &ctx) -> TMcpToolResult {
			std::string status = GetStr(args, "status");

			try {
				json rows;
				if (!status.empty()) {
					Params p;
					p["status"] = status;
					rows = db->Query(
						"SELECT * FROM feature_requests WHERE status = :status ORDER BY created_at DESC", p);
				} else {
					rows = db->Query(
						"SELECT * FROM feature_requests ORDER BY created_at DESC", {});
				}

				json resp;
				resp["requests"] = rows;
				resp["count"] = rows.size();
				return TMcpToolResult::Success(resp);
			} catch (const std::exception &e) {
				return TMcpToolResult::Error(e.what());
			}
		}
	});

	//=======================================================================
	// 29. save_task
	//=======================================================================
	tools.push_back({
		"save_task",
		"Save a completed task record. Use to track what was done between sessions. "
		"Author field is useful for review scenarios (recording work done by others).",
		TMcpToolSchema()
			.AddString("title", "Short task description", true)
			.AddString("description", "Detailed description of what was done", true)
			.AddString("issue_number", "Issue number if applicable (e.g. '#42', 'MERP-123')")
			.AddString("author", "Who performed the task (for review scenarios)"),
		[db](const json &args, TMcpToolContext &ctx) -> TMcpToolResult {
			std::string title = GetStr(args, "title");
			std::string description = GetStr(args, "description");
			if (title.empty() || description.empty())
				return TMcpToolResult::Error("Missing required parameters: title, description");

			Params p;
			p["title"] = title;
			p["description"] = description;
			p["issue_number"] = GetStr(args, "issue_number");
			p["author"] = GetStr(args, "author");

			try {
				db->Execute(
					"INSERT INTO completed_tasks (title, issue_number, author, description) "
					"VALUES (:title, :issue_number, :author, :description)", p);
				long long id = db->LastInsertRowId();

				json resp;
				resp["success"] = true;
				resp["id"] = id;
				resp["message"] = "Task saved: " + title;
				return TMcpToolResult::Success(resp);
			} catch (const std::exception &e) {
				return TMcpToolResult::Error(e.what());
			}
		}
	});

	//=======================================================================
	// 30. search_tasks
	//=======================================================================
	tools.push_back({
		"search_tasks",
		"Search completed tasks. Supports full-text search (FTS5 + LIKE fallback) "
		"with filters by author and date range. Omit query to list all tasks with filters.",
		TMcpToolSchema()
			.AddString("query", "Search query (FTS across title + description)")
			.AddString("author", "Filter by author (exact match)")
			.AddString("date_from", "Start date filter (YYYY-MM-DD)")
			.AddString("date_to", "End date filter (YYYY-MM-DD)")
			.AddInteger("limit", "Max results (default: 50)"),
		[db](const json &args, TMcpToolContext &ctx) -> TMcpToolResult {
			std::string query = GetStr(args, "query");
			std::string author = GetStr(args, "author");
			std::string dateFrom = GetStr(args, "date_from");
			std::string dateTo = GetStr(args, "date_to");
			int limit = GetInt(args, "limit", 50);

			try {
				std::string sql;
				Params p;

				if (!query.empty()) {
					// FTS + LIKE fallback
					std::string ftsQuery = AddWildcard(query);
					std::string likePattern = MakeLikePattern(query);
					p["query"] = ftsQuery;
					p["like_pattern"] = likePattern;

					sql = "SELECT * FROM completed_tasks WHERE id IN ("
						"SELECT ct.id FROM completed_tasks ct "
						"INNER JOIN tasks_fts fts ON ct.id = fts.rowid "
						"WHERE tasks_fts MATCH :query "
						"UNION "
						"SELECT ct.id FROM completed_tasks ct "
						"WHERE ct.title LIKE :like_pattern OR ct.description LIKE :like_pattern"
						")";
				} else {
					sql = "SELECT * FROM completed_tasks WHERE 1=1";
				}

				if (!author.empty()) {
					sql += " AND author = :author";
					p["author"] = author;
				}
				if (!dateFrom.empty()) {
					sql += " AND created_at >= :date_from";
					p["date_from"] = dateFrom;
				}
				if (!dateTo.empty()) {
					sql += " AND created_at <= :date_to || ' 23:59:59'";
					p["date_to"] = dateTo;
				}

				sql += " ORDER BY created_at DESC LIMIT :limit";
				p["limit"] = std::to_string(limit);

				json rows = db->Query(sql, p);

				json resp;
				resp["tasks"] = rows;
				resp["count"] = rows.size();
				resp["filters"] = json{
					{"query", query},
					{"author", author},
					{"date_from", dateFrom},
					{"date_to", dateTo}
				};
				return TMcpToolResult::Success(resp);
			} catch (const std::exception &e) {
				return TMcpToolResult::Error(e.what());
			}
		}
	});

	//=======================================================================
	// 31. save_route
	//=======================================================================
	tools.push_back({
		"save_route",
		"Save a UI navigation route. UPSERT by (form_type, scenario). "
		"Stores step-by-step navigation instructions, control mappings, grid rectangles, "
		"behavior descriptions, and code context for a specific form+scenario combination.",
		TMcpToolSchema()
			.AddString("form_type", "Form type for ui_open_document (e.g. 'DocApproveProject')", true)
			.AddString("scenario", "Scenario name (e.g. 'list', 'search', 'remarks')", true)
			.AddString("steps", "JSON array of step objects", true)
			.AddString("controls", "JSON: controlId -> {type, label, items?}")
			.AddString("grid_rects", "JSON: gridId -> {top, bottom, left, right}")
			.AddString("behavior", "JSON: event -> effect description")
			.AddString("code_context", "JSON: class, file, handlers summary")
			.AddString("description", "Human-readable route description")
			.AddString("verified", "ISO date of last verification (e.g. '2026-02-22')"),
		[db](const json &args, TMcpToolContext &ctx) -> TMcpToolResult {
			std::string formType = GetStr(args, "form_type");
			std::string scenario = GetStr(args, "scenario");
			std::string steps = GetStr(args, "steps");

			if (formType.empty() || scenario.empty() || steps.empty())
				return TMcpToolResult::Error("Missing required parameters: form_type, scenario, steps");

			Params p;
			p["form_type"] = formType;
			p["scenario"] = scenario;
			p["steps"] = steps;
			p["controls"] = GetStr(args, "controls");
			p["grid_rects"] = GetStr(args, "grid_rects");
			p["behavior"] = GetStr(args, "behavior");
			p["code_context"] = GetStr(args, "code_context");
			p["description"] = GetStr(args, "description");
			p["verified"] = GetStr(args, "verified");

			try {
				db->Execute(
					"INSERT OR REPLACE INTO ui_routes "
					"(form_type, scenario, description, steps, controls, grid_rects, "
					"behavior, code_context, verified, updated_at) "
					"VALUES (:form_type, :scenario, :description, :steps, :controls, :grid_rects, "
					":behavior, :code_context, :verified, datetime('now'))", p);
				long long id = db->LastInsertRowId();

				json resp;
				resp["success"] = true;
				resp["id"] = id;
				resp["message"] = "Route saved: " + formType + "/" + scenario;
				return TMcpToolResult::Success(resp);
			} catch (const std::exception &e) {
				return TMcpToolResult::Error(e.what());
			}
		}
	});

	//=======================================================================
	// 32. get_route
	//=======================================================================
	tools.push_back({
		"get_route",
		"Get UI navigation routes. Search by form_type (exact), optional scenario filter, "
		"or full-text query. Returns step-by-step navigation instructions with control mappings.",
		TMcpToolSchema()
			.AddString("form_type", "Exact form type (e.g. 'DocApproveProject')")
			.AddString("scenario", "Filter by scenario (e.g. 'list', 'search')")
			.AddString("query", "Full-text search across routes"),
		[db](const json &args, TMcpToolContext &ctx) -> TMcpToolResult {
			std::string formType = GetStr(args, "form_type");
			std::string scenario = GetStr(args, "scenario");
			std::string query = GetStr(args, "query");

			if (formType.empty() && query.empty())
				return TMcpToolResult::Error("At least one parameter required: form_type or query");

			try {
				json rows;

				if (!formType.empty()) {
					// Exact match by form_type, optional scenario filter
					Params p;
					p["form_type"] = formType;

					std::string sql = "SELECT * FROM ui_routes WHERE form_type = :form_type";
					if (!scenario.empty()) {
						sql += " AND scenario = :scenario";
						p["scenario"] = scenario;
					}
					sql += " ORDER BY scenario";
					rows = db->Query(sql, p);
				} else {
					// Full-text search
					std::string ftsQuery = MakeFtsOrQuery(query);
					std::string likePattern = MakeLikePattern(query);

					Params p;
					p["query"] = ftsQuery;
					p["like_pattern"] = likePattern;

					rows = RankedSearch(db, p,
						"SELECT ur.* FROM ui_routes ur "
						"INNER JOIN ui_routes_fts fts ON ur.id = fts.rowid "
						"WHERE ui_routes_fts MATCH :query ORDER BY fts.rank LIMIT 10",
						"SELECT ur.* FROM ui_routes ur WHERE "
						"ur.form_type LIKE :like_pattern OR ur.description LIKE :like_pattern "
						"OR ur.controls LIKE :like_pattern LIMIT 10", 10);
				}

				json resp;
				resp["routes"] = rows;
				resp["count"] = rows.size();
				return TMcpToolResult::Success(resp);
			} catch (const std::exception &e) {
				return TMcpToolResult::Error(e.what());
			}
		}
	});

	//=======================================================================
	// 33. save_ui_research
	//=======================================================================
	tools.push_back({
		"save_ui_research",
		"Save UI code investigation results for a form class. UPSERT by form_class. "
		"Stores extracted message maps, handler descriptions, validations, ribbon logic, and modal dialogs.",
		TMcpToolSchema()
			.AddString("form_class", "C++ class name (e.g. 'CMERP_ViewFormDocApprove')", true)
			.AddString("form_type", "Form type for ui_open_document (e.g. 'DocApproveProject')")
			.AddString("file_path", "Source file path (e.g. 'MERP_ViewFormDocApprove.cpp')")
			.AddString("message_map", "JSON: {\"ON_CBN_SELCHANGE(17587)\": \"OnDocTypeChanged\"}")
			.AddString("handlers", "JSON: {\"OnDocTypeChanged\": \"Reloads tree and grid\"}")
			.AddString("validations", "JSON array: [\"if project not selected -> MessageBox(...)\"]")
			.AddString("ribbon_logic", "JSON: {\"OnUpdateEditDoc\": \"enabled if grid row selected\"}")
			.AddString("modals", "JSON array: [\"CDocVersionDlg - add version dialog\"]")
			.AddString("notes", "Additional notes"),
		[db](const json &args, TMcpToolContext &ctx) -> TMcpToolResult {
			std::string formClass = GetStr(args, "form_class");
			if (formClass.empty())
				return TMcpToolResult::Error("Missing required parameter: form_class");

			Params p;
			p["form_class"] = formClass;
			p["form_type"] = GetStr(args, "form_type");
			p["file_path"] = GetStr(args, "file_path");
			p["message_map"] = GetStr(args, "message_map");
			p["handlers"] = GetStr(args, "handlers");
			p["validations"] = GetStr(args, "validations");
			p["ribbon_logic"] = GetStr(args, "ribbon_logic");
			p["modals"] = GetStr(args, "modals");
			p["notes"] = GetStr(args, "notes");

			try {
				db->Execute(
					"INSERT OR REPLACE INTO ui_research "
					"(form_class, form_type, file_path, message_map, handlers, "
					"validations, ribbon_logic, modals, notes, updated_at) "
					"VALUES (:form_class, :form_type, :file_path, :message_map, :handlers, "
					":validations, :ribbon_logic, :modals, :notes, datetime('now'))", p);
				long long id = db->LastInsertRowId();

				json resp;
				resp["success"] = true;
				resp["id"] = id;
				resp["message"] = "UI research saved: " + formClass;
				return TMcpToolResult::Success(resp);
			} catch (const std::exception &e) {
				return TMcpToolResult::Error(e.what());
			}
		}
	});

	//=======================================================================
	// 34. get_ui_research
	//=======================================================================
	tools.push_back({
		"get_ui_research",
		"Get UI code investigation results. Search by form_class (exact), form_type (exact), "
		"or full-text query. Returns message maps, handlers, validations, ribbon logic.",
		TMcpToolSchema()
			.AddString("form_class", "Exact C++ class name")
			.AddString("form_type", "Exact form type (e.g. 'DocApproveProject')")
			.AddString("query", "Full-text search across research data"),
		[db](const json &args, TMcpToolContext &ctx) -> TMcpToolResult {
			std::string formClass = GetStr(args, "form_class");
			std::string formType = GetStr(args, "form_type");
			std::string query = GetStr(args, "query");

			if (formClass.empty() && formType.empty() && query.empty())
				return TMcpToolResult::Error("At least one parameter required: form_class, form_type, or query");

			try {
				json rows;

				if (!formClass.empty()) {
					Params p;
					p["form_class"] = formClass;
					rows = db->Query(
						"SELECT * FROM ui_research WHERE form_class = :form_class", p);
				} else if (!formType.empty()) {
					Params p;
					p["form_type"] = formType;
					rows = db->Query(
						"SELECT * FROM ui_research WHERE form_type = :form_type", p);
				} else {
					// Full-text search
					std::string ftsQuery = MakeFtsOrQuery(query);
					std::string likePattern = MakeLikePattern(query);

					Params p;
					p["query"] = ftsQuery;
					p["like_pattern"] = likePattern;

					rows = RankedSearch(db, p,
						"SELECT ur.* FROM ui_research ur "
						"INNER JOIN ui_research_fts fts ON ur.id = fts.rowid "
						"WHERE ui_research_fts MATCH :query ORDER BY fts.rank LIMIT 10",
						"SELECT ur.* FROM ui_research ur WHERE "
						"ur.form_class LIKE :like_pattern OR ur.form_type LIKE :like_pattern "
						"OR ur.handlers LIKE :like_pattern OR ur.notes LIKE :like_pattern LIMIT 10", 10);
				}

				json resp;
				resp["research"] = rows;
				resp["count"] = rows.size();
				return TMcpToolResult::Success(resp);
			} catch (const std::exception &e) {
				return TMcpToolResult::Error(e.what());
			}
		}
	});

	//=======================================================================
	// 35. batch_context — full context for N entities in one call
	//=======================================================================
	tools.push_back({
		"batch_context",
		"Get full context for multiple entities in one call. Returns facts, relationships, "
		"form mappings, glossary entries, and coverage status for each entity. "
		"Much more efficient than calling get_facts + get_relationships + find_form separately.",
		TMcpToolSchema()
			.AddString("entities", "JSON array of entity names (e.g. [\"MoveMaterials\",\"Product\"])", true),
		[db](const json &args, TMcpToolContext &ctx) -> TMcpToolResult {
			json entArr = GetArray(args, "entities");
			if (entArr.empty())
				return TMcpToolResult::Error("Missing or empty parameter: entities (expected JSON array)");

			std::vector<std::string> entities;
			for (const auto &e : entArr) {
				if (e.is_string() && !e.get<std::string>().empty())
					entities.push_back(e.get<std::string>());
			}
			if (entities.empty())
				return TMcpToolResult::Error("No valid entity names provided");
			if (entities.size() > 20)
				entities.resize(20);

			try {
				// Build IN clause
				std::string inC = "(";
				for (size_t i = 0; i < entities.size(); i++) {
					if (i > 0) inC += ",";
					inC += "'" + EscapeSqlString(entities[i]) + "'";
				}
				inC += ")";

				auto safeQ = [db](const std::string &sql) -> json {
					try { return db->Query(sql, {}); }
					catch (...) { return json::array(); }
				};

				// Batch queries
				json allFacts = safeQ("SELECT * FROM facts WHERE entity IN " + inC + " ORDER BY entity, fact_type");
				json allRels = safeQ(
					"SELECT * FROM relationships WHERE entity_from IN " + inC + " OR entity_to IN " + inC);
				json allForms = safeQ("SELECT * FROM form_table_map WHERE main_table IN " + inC
					+ " OR template IN " + inC);
				json allGloss = safeQ("SELECT * FROM glossary WHERE entity IN " + inC);
				json allStatus = safeQ("SELECT * FROM entity_status WHERE entity IN " + inC);
				json allModules = safeQ("SELECT * FROM entity_modules WHERE entity IN " + inC);

				// Group by entity
				std::map<std::string, json> factsByEnt, relsByEnt, glossByEnt;
				std::map<std::string, json> formByEnt; // arrays of form mappings
				std::map<std::string, std::string> statusByEnt, moduleByEnt;

				for (const auto &f : allFacts) {
					std::string e = f.value("entity", std::string());
					if (!e.empty()) factsByEnt[e].push_back(f);
				}
				for (const auto &r : allRels) {
					std::string ef = r.value("entity_from", std::string());
					std::string et = r.value("entity_to", std::string());
					if (!ef.empty()) relsByEnt[ef].push_back(r);
					if (!et.empty() && et != ef) relsByEnt[et].push_back(r);
				}
				for (const auto &f : allForms) {
					std::string mt = f.value("main_table", std::string());
					std::string tp = f.value("template", std::string());
					json compact = json{
						{"template", f.value("template", std::string())},
						{"class_name", f.value("class_name", std::string())},
						{"file_path", f.value("file_path", std::string())},
						{"main_table", f.value("main_table", std::string())}
					};
					if (!mt.empty()) formByEnt[mt].push_back(compact);
					if (!tp.empty() && tp != mt) formByEnt[tp].push_back(compact);
				}
				for (const auto &g : allGloss) {
					std::string e = g.value("entity", std::string());
					if (!e.empty()) glossByEnt[e].push_back(g);
				}
				for (const auto &s : allStatus) {
					std::string e = s.value("entity", std::string());
					statusByEnt[e] = s.value("coverage", std::string("unknown"));
				}
				for (const auto &m : allModules) {
					std::string e = m.value("entity", std::string());
					moduleByEnt[e] = m.value("module_name", std::string());
				}

				// Build response per entity
				json result = json::object();
				for (const auto &ent : entities) {
					json entry;
					entry["coverage"] = statusByEnt.count(ent) ? statusByEnt[ent] : "unknown";
					if (moduleByEnt.count(ent)) entry["module"] = moduleByEnt[ent];

					// Facts (compact: id, type, description, confidence)
					json facts = json::array();
					if (factsByEnt.count(ent)) {
						for (const auto &f : factsByEnt[ent]) {
							facts.push_back(json{
								{"id", f.value("id", (long long)0)},
								{"type", f.value("fact_type", std::string())},
								{"description", f.value("description", std::string())},
								{"confidence", f.value("confidence", std::string())}
							});
						}
					}
					entry["facts"] = facts;
					entry["fact_count"] = (int)facts.size();

					// Relationships
					json rels = json::array();
					if (relsByEnt.count(ent)) {
						for (const auto &r : relsByEnt[ent]) {
							rels.push_back(json{
								{"from", r.value("entity_from", std::string())},
								{"to", r.value("entity_to", std::string())},
								{"type", r.value("rel_type", std::string())},
								{"description", r.value("description", std::string())}
							});
						}
					}
					entry["relationships"] = rels;

					// Form mappings (array)
					if (formByEnt.count(ent) && !formByEnt[ent].empty()) {
						entry["forms"] = formByEnt[ent];
					}

					// Glossary (array — compact)
					if (glossByEnt.count(ent) && !glossByEnt[ent].empty()) {
						json glossArr = json::array();
						for (const auto &g : glossByEnt[ent]) {
							glossArr.push_back(json{
								{"term", g.value("term", std::string())},
								{"synonyms", g.value("synonyms", std::string())},
								{"context", g.value("context", std::string())}
							});
						}
						entry["glossary"] = glossArr;
					}

					result[ent] = entry;
				}

				return TMcpToolResult::Success(json{
					{"entities", result},
					{"count", (int)entities.size()}
				});
			} catch (const std::exception &e) {
				return TMcpToolResult::Error(e.what());
			}
		}
	});

	//=======================================================================
	// 36. identify_gaps — what's missing for each entity
	//=======================================================================
	tools.push_back({
		"identify_gaps",
		"Identify knowledge gaps for entities. For each entity: what fact types exist, what's missing, "
		"completeness score (0-100), and suggested actions to fill gaps. "
		"Use to prioritize knowledge collection efforts.",
		TMcpToolSchema()
			.AddString("entities", "JSON array of entity names", true),
		[db, fedClient](const json &args, TMcpToolContext &ctx) -> TMcpToolResult {
			json entArr = GetArray(args, "entities");
			if (entArr.empty())
				return TMcpToolResult::Error("Missing or empty parameter: entities");

			std::vector<std::string> entities;
			for (const auto &e : entArr) {
				if (e.is_string() && !e.get<std::string>().empty())
					entities.push_back(e.get<std::string>());
			}
			if (entities.empty())
				return TMcpToolResult::Error("No valid entity names provided");
			if (entities.size() > 20)
				entities.resize(20);

			try {
				std::string inC = "(";
				for (size_t i = 0; i < entities.size(); i++) {
					if (i > 0) inC += ",";
					inC += "'" + EscapeSqlString(entities[i]) + "'";
				}
				inC += ")";

				auto safeQ = [db](const std::string &sql) -> json {
					try { return db->Query(sql, {}); }
					catch (...) { return json::array(); }
				};

				// Batch queries
				json factTypes = safeQ("SELECT entity, fact_type, COUNT(*) as cnt FROM facts WHERE entity IN "
					+ inC + " GROUP BY entity, fact_type");
				json relCounts = safeQ(
					"SELECT e, SUM(cnt) as total FROM ("
					"  SELECT entity_from as e, COUNT(*) as cnt FROM relationships WHERE entity_from IN " + inC + " GROUP BY entity_from "
					"  UNION ALL "
					"  SELECT entity_to as e, COUNT(*) as cnt FROM relationships WHERE entity_to IN " + inC + " GROUP BY entity_to"
					") GROUP BY e");
				json statuses = safeQ("SELECT entity, coverage FROM entity_status WHERE entity IN " + inC);
				json forms = safeQ("SELECT main_table FROM form_table_map WHERE main_table IN " + inC);
				json glossary = safeQ("SELECT entity FROM glossary WHERE entity IN " + inC);

				// Build lookup maps
				std::map<std::string, std::set<std::string>> hasTypes;
				std::map<std::string, int> relCountMap;
				std::map<std::string, std::string> coverageMap;
				std::set<std::string> hasForm, hasGloss;

				for (const auto &r : factTypes) {
					std::string e = r.value("entity", std::string());
					std::string ft = r.value("fact_type", std::string());
					if (!e.empty() && !ft.empty()) hasTypes[e].insert(ft);
				}
				for (const auto &r : relCounts) {
					std::string e = r.value("e", std::string());
					relCountMap[e] = (int)r.value("total", (long long)0);
				}
				for (const auto &r : statuses)
					coverageMap[r.value("entity", std::string())] = r.value("coverage", std::string());
				for (const auto &r : forms)
					hasForm.insert(r.value("main_table", std::string()));
				for (const auto &r : glossary)
					hasGloss.insert(r.value("entity", std::string()));

				// Federation: auto-fetch schema/FK from dbmcp for unknown entities
				json fedLog = json::array();
				if (fedClient) {
					int fedCount = 0;
					for (const auto &ent : entities) {
						if (fedCount >= 3) break;
						std::string cov = coverageMap.count(ent) ? coverageMap[ent] : "unknown";
						bool hasSchema = hasTypes.count(ent) && hasTypes[ent].count("schema");
						if (cov == "unknown" && !hasSchema) {
							std::string fedErr;
							bool ok = FederateEntity(db, fedClient, ent, &fedErr);
							if (ok) {
								fedLog.push_back(json{{"entity", ent}, {"status", "fetched"}});
								hasTypes[ent].insert("schema");
								coverageMap[ent] = "schema_only";
								// Re-check relationship count
								try {
									LocalMcpDb::Params rp;
									rp["ent"] = ent;
									json rc = db->Query(
										"SELECT COUNT(*) as cnt FROM relationships "
										"WHERE entity_from = :ent OR entity_to = :ent", rp);
									if (!rc.empty())
										relCountMap[ent] = (int)rc[0].value("cnt", (long long)0);
								} catch (...) {}
								fedCount++;
							}
						}
					}
				}

				// All desirable fact types
				std::vector<std::string> allFactTypes = {
					"schema", "business_rule", "gotcha", "code_pattern", "statistics", "relationship"
				};

				json result = json::object();
				for (const auto &ent : entities) {
					json entry;

					std::string cov = coverageMap.count(ent) ? coverageMap[ent] : "unknown";
					entry["coverage"] = cov;

					// What exists
					json has = json::array();
					auto htIt = hasTypes.find(ent);
					if (htIt != hasTypes.end())
						for (const auto &t : htIt->second) has.push_back(t);
					if (hasForm.count(ent)) has.push_back("form_mapping");
					if (hasGloss.count(ent)) has.push_back("glossary");
					if (relCountMap.count(ent) && relCountMap[ent] > 0) has.push_back("relationships");
					entry["has"] = has;

					// What's missing
					json missing = json::array();
					std::set<std::string> hasSet;
					if (htIt != hasTypes.end()) hasSet = htIt->second;
					for (const auto &ft : allFactTypes) {
						if (hasSet.find(ft) == hasSet.end()) missing.push_back(ft);
					}
					if (!hasForm.count(ent)) missing.push_back("form_mapping");
					if (!hasGloss.count(ent)) missing.push_back("glossary");
					entry["missing"] = missing;

					// Relationship count
					int rc = relCountMap.count(ent) ? relCountMap[ent] : 0;
					entry["relationship_count"] = rc;

					// Completeness score (0-100)
					int maxItems = (int)allFactTypes.size() + 2 + 1; // fact types + form + glossary + relationships
					int hasCount = (int)has.size();
					int completeness = (int)(100.0 * hasCount / maxItems);
					entry["completeness"] = completeness;

					// Suggested actions
					json actions = json::array();
					if (hasSet.find("schema") == hasSet.end()) {
						actions.push_back(json{
							{"tool", "dbmcp:get_table_schema"},
							{"params", json{{"table", ent}}},
							{"reason", "Schema not documented"}
						});
					}
					if (rc == 0) {
						actions.push_back(json{
							{"tool", "dbmcp:get_table_relations"},
							{"params", json{{"table_name", ent}}},
							{"reason", "No relationships saved - FK info missing"}
						});
					}
					if (!hasForm.count(ent)) {
						actions.push_back(json{
							{"tool", "Grep"},
							{"params", json{{"pattern", "C" + ent + "Dlg"}, {"glob", "*.h"}}},
							{"reason", "Find form/dialog class"}
						});
					}
					if (hasSet.find("business_rule") == hasSet.end()) {
						actions.push_back(json{
							{"tool", "Grep"},
							{"params", json{{"pattern", ent}, {"glob", "*.cpp"}}},
							{"reason", "Find business rules in code"}
						});
					}
					entry["suggested_actions"] = actions;

					result[ent] = entry;
				}

				json resp = {
					{"gaps", result},
					{"count", (int)entities.size()}
				};
				if (!fedLog.empty())
					resp["federation_log"] = fedLog;
				return TMcpToolResult::Success(resp);
			} catch (const std::exception &e) {
				return TMcpToolResult::Error(e.what());
			}
		}
	});

	//=======================================================================
	// 37. quality_report — global or per-entity knowledge metrics
	//=======================================================================
	tools.push_back({
		"quality_report",
		"Get knowledge quality metrics. Without parameters: global stats (entity count, fact count, "
		"coverage distribution, stale entities, confidence breakdown). With entity: per-entity detailed report. "
		"Set auto_stale=false to skip auto-marking entities not updated in 30+ days as needs_review.",
		TMcpToolSchema()
			.AddString("entity", "Optional: get report for specific entity")
			.AddBoolean("auto_stale", "Auto-mark stale entities as needs_review (default: true in global mode)"),
		[db](const json &args, TMcpToolContext &ctx) -> TMcpToolResult {
			std::string entity = GetStr(args, "entity");
			bool autoStale = true;  // auto-stale in global mode by default
			if (args.contains("auto_stale") && !args["auto_stale"].is_null())
				autoStale = args["auto_stale"].get<bool>();

			try {
				if (!entity.empty()) {
					// Per-entity report
					Params p;
					p["entity"] = entity;

					json facts = db->Query("SELECT fact_type, COUNT(*) as cnt FROM facts WHERE entity = :entity GROUP BY fact_type", p);
					json relOut = db->Query("SELECT COUNT(*) as cnt FROM relationships WHERE entity_from = :entity", p);
					json relIn = db->Query("SELECT COUNT(*) as cnt FROM relationships WHERE entity_to = :entity", p);
					json status = db->Query("SELECT coverage, notes, updated_at FROM entity_status WHERE entity = :entity", p);
					json form = db->Query("SELECT template FROM form_table_map WHERE main_table = :entity", p);
					json gloss = db->Query("SELECT term FROM glossary WHERE entity = :entity", p);
					json lastFact = db->Query("SELECT MAX(updated_at) as last_upd FROM facts WHERE entity = :entity", p);

					json resp;
					resp["entity"] = entity;

					// Coverage
					std::string cov = "unknown";
					if (!status.empty()) {
						cov = status[0].value("coverage", std::string("unknown"));
						resp["notes"] = status[0].value("notes", std::string());
						resp["status_updated"] = status[0].value("updated_at", std::string());
					}
					resp["coverage"] = cov;

					// Facts by type
					json factsByType;
					int totalFacts = 0;
					for (const auto &r : facts) {
						std::string ft = r.value("fact_type", std::string());
						int cnt = (int)r.value("cnt", (long long)0);
						factsByType[ft] = cnt;
						totalFacts += cnt;
					}
					resp["facts_by_type"] = factsByType;
					resp["total_facts"] = totalFacts;

					// Relationships
					int ro = relOut.empty() ? 0 : (int)relOut[0].value("cnt", (long long)0);
					int ri = relIn.empty() ? 0 : (int)relIn[0].value("cnt", (long long)0);
					resp["relationships"] = json{{"outgoing", ro}, {"incoming", ri}};

					resp["has_form"] = !form.empty();
					resp["has_glossary"] = !gloss.empty();

					// Last updated
					std::string lastUpd;
					if (!lastFact.empty())
						lastUpd = lastFact[0].value("last_upd", std::string());
					resp["last_updated"] = lastUpd;

					// Completeness
					int items = 0;
					if (totalFacts > 0) items++;
					if (ro + ri > 0) items++;
					if (!form.empty()) items++;
					if (!gloss.empty()) items++;
					std::set<std::string> ftSet;
					for (const auto &r : facts) ftSet.insert(r.value("fact_type", std::string()));
					items += (int)ftSet.size();
					int maxItems = 9; // 6 fact types + form + glossary + relationships
					resp["completeness_score"] = (int)(100.0 * items / maxItems);

					return TMcpToolResult::Success(resp);
				}

				// Global report
				auto safeQ = [db](const std::string &sql) -> json {
					try { return db->Query(sql, {}); }
					catch (...) { return json::array(); }
				};

				json entCount = safeQ("SELECT COUNT(DISTINCT entity) as cnt FROM facts");
				json factCount = safeQ("SELECT COUNT(*) as cnt FROM facts");
				json relCount = safeQ("SELECT COUNT(*) as cnt FROM relationships");
				json covDist = safeQ("SELECT coverage, COUNT(*) as cnt FROM entity_status GROUP BY coverage");
				json confDist = safeQ("SELECT confidence, COUNT(*) as cnt FROM facts GROUP BY confidence");
				json orphans = safeQ(
					"SELECT COUNT(DISTINCT f.entity) as cnt FROM facts f "
					"LEFT JOIN entity_status es ON f.entity = es.entity "
					"WHERE es.entity IS NULL");
				json gaps = safeQ("SELECT COUNT(*) as cnt FROM search_gaps WHERE resolved = 0");

				// Auto-mark stale entities (>30 days since last update, coverage IN full/partial)
				int staleCount = 0;
				if (autoStale) {
					try {
						staleCount = db->Execute(
							"UPDATE entity_status SET coverage = 'needs_review', "
							"  notes = COALESCE(notes,'') || ' | Auto-stale: ' || datetime('now'), "
							"  updated_at = datetime('now') "
							"WHERE entity IN ("
							"  SELECT entity FROM facts GROUP BY entity "
							"  HAVING CAST(julianday('now') - julianday(MAX(updated_at)) AS INTEGER) > 30"
							") AND coverage IN ('full', 'partial')", {});
					} catch (...) {}
				}

				// Coverage map
				json covMap;
				for (const auto &r : covDist) {
					std::string c = r.value("coverage", std::string("unknown"));
					covMap[c] = r.value("cnt", (long long)0);
				}

				// Confidence map
				json confMap;
				for (const auto &r : confDist) {
					std::string c = r.value("confidence", std::string("unverified"));
					confMap[c] = r.value("cnt", (long long)0);
				}

				json resp;
				resp["total_entities"] = entCount.empty() ? 0 : entCount[0].value("cnt", (long long)0);
				resp["total_facts"] = factCount.empty() ? 0 : factCount[0].value("cnt", (long long)0);
				resp["total_relationships"] = relCount.empty() ? 0 : relCount[0].value("cnt", (long long)0);
				resp["coverage_distribution"] = covMap;
				resp["confidence_distribution"] = confMap;
				resp["orphan_entities"] = orphans.empty() ? 0 : orphans[0].value("cnt", (long long)0);
				resp["unresolved_gaps"] = gaps.empty() ? 0 : gaps[0].value("cnt", (long long)0);
				resp["stale_auto_marked"] = staleCount;

				return TMcpToolResult::Success(resp);
			} catch (const std::exception &e) {
				return TMcpToolResult::Error(e.what());
			}
		}
	});

	//=======================================================================
	// 38. get_knowledge_gaps — unresolved search gaps
	//=======================================================================
	tools.push_back({
		"get_knowledge_gaps",
		"Get unresolved knowledge gaps - queries that returned zero results. "
		"Shows what information users searched for but couldn't find. "
		"Use to prioritize knowledge collection.",
		TMcpToolSchema()
			.AddInteger("limit", "Max gaps to return (default 20)"),
		[db](const json &args, TMcpToolContext &ctx) -> TMcpToolResult {
			int limit = GetInt(args, "limit", 20);
			if (limit < 1) limit = 1;
			if (limit > 100) limit = 100;

			try {
				Params p;
				p["limit"] = std::to_string(limit);

				json rows = db->Query(
					"SELECT query_text, tool_name, COUNT(*) as ask_count, "
					"  MIN(created_at) as first_asked, MAX(created_at) as last_asked "
					"FROM search_gaps WHERE resolved = 0 "
					"GROUP BY query_text, tool_name "
					"ORDER BY ask_count DESC, last_asked DESC LIMIT :limit", p);

				json totalRow = db->Query("SELECT COUNT(*) as cnt FROM search_gaps WHERE resolved = 0", {});
				int total = totalRow.empty() ? 0 : (int)totalRow[0].value("cnt", (long long)0);

				return TMcpToolResult::Success(json{
					{"gaps", rows},
					{"count", (int)rows.size()},
					{"total_unresolved", total}
				});
			} catch (const std::exception &e) {
				return TMcpToolResult::Error(e.what());
			}
		}
	});

	//=======================================================================
	// 39. context_feedback — track useful/useless entities
	//=======================================================================
	tools.push_back({
		"context_feedback",
		"Provide feedback on which entities were useful or useless for a task. "
		"Used to improve future context relevance via boosting/penalizing.",
		TMcpToolSchema()
			.AddString("task_text", "Description of the task")
			.AddString("useful_entities", "JSON array of entity names that were useful")
			.AddString("useless_entities", "JSON array of entity names that were not useful"),
		[db](const json &args, TMcpToolContext &ctx) -> TMcpToolResult {
			std::string taskText = GetStr(args, "task_text");
			json useful = GetArray(args, "useful_entities");
			json useless = GetArray(args, "useless_entities");

			if (useful.empty() && useless.empty())
				return TMcpToolResult::Error("Provide at least useful_entities or useless_entities");

			try {
				db->BeginTransaction();
				int count = 0;
				Params p;
				p["task"] = taskText;

				for (const auto &e : useful) {
					if (!e.is_string()) continue;
					p["entity"] = e.get<std::string>();
					db->Execute(
						"INSERT INTO context_feedback(task_text, entity, useful) VALUES(:task, :entity, 1)", p);
					count++;
				}
				for (const auto &e : useless) {
					if (!e.is_string()) continue;
					p["entity"] = e.get<std::string>();
					db->Execute(
						"INSERT INTO context_feedback(task_text, entity, useful) VALUES(:task, :entity, 0)", p);
					count++;
				}
				db->Commit();

				return TMcpToolResult::Success(json{
					{"success", true},
					{"saved", count},
					{"message", "Saved " + std::to_string(count) + " feedback entries"}
				});
			} catch (const std::exception &e) {
				try { db->Rollback(); } catch (...) {}
				return TMcpToolResult::Error(e.what());
			}
		}
	});

	//=======================================================================
	// 40. prepare_context — one call = full context for a task
	//=======================================================================
	tools.push_back({
		"prepare_context",
		"One call = full task context. Extracts keywords from task description, expands synonyms, "
		"searches ALL knowledge stores (FTS + LIKE), ranks entities by relevance, enriches top entities "
		"with facts/relationships/forms, identifies gaps with suggested actions. "
		"Returns structured brief ready for the agent to use.",
		TMcpToolSchema()
			.AddString("task_description", "Description of the task to prepare context for", true)
			.AddInteger("max_entities", "Max entities to return (default 10)")
			.AddBoolean("include_code_search", "Include suggested code search actions in gaps (default true)"),
		[db, fedClient](const json &args, TMcpToolContext &ctx) -> TMcpToolResult {
			std::string task = GetStr(args, "task_description");
			if (task.empty())
				return TMcpToolResult::Error("Missing required parameter: task_description");

			int maxEnt = GetInt(args, "max_entities", 10);
			if (maxEnt < 1) maxEnt = 1;
			if (maxEnt > 30) maxEnt = 30;

			try {
				// === STEP 1: Keyword extraction + synonym expansion ===
				auto words = SplitWordsFiltered(task, db);
				if (words.empty()) words.push_back(task);
				if (words.size() > 7) words.resize(7);

				// Synonym expansion from glossary
				std::vector<std::string> expandedWords = words;
				for (const auto &w : words) {
					std::string wLike = "%" + w + "%";
					Params sp;
					sp["wlike"] = wLike;
					try {
						json synRows = db->Query(
							"SELECT synonyms FROM glossary WHERE term LIKE :wlike OR synonyms LIKE :wlike LIMIT 3", sp);
						for (const auto &sr : synRows) {
							std::string syns = sr.value("synonyms", std::string());
							try {
								json synArr = json::parse(syns);
								if (synArr.is_array()) {
									for (const auto &syn : synArr) {
										if (syn.is_string()) {
											std::string s = syn.get<std::string>();
											if (s.length() >= 3 && expandedWords.size() < 15)
												expandedWords.push_back(s);
										}
									}
								}
							} catch (...) {}
						}
					} catch (...) {}
				}

				// Build FTS query from expanded words
				std::string ftsQuery;
				for (const auto &w : expandedWords) {
					if (!ftsQuery.empty()) ftsQuery += " OR ";
					ftsQuery += AddWildcard(w);
				}

				Params p;
				p["query"] = ftsQuery;

				// Build per-word LIKE patterns
				std::string factLike, glossLike, procLike, formLike, relLike, queryLike, patternLike, taskLike;
				for (size_t i = 0; i < words.size() && i < 5; i++) {
					std::string pn = "lw" + std::to_string(i);
					p[pn] = MakeLikePattern(words[i]);
					std::string sep = (i > 0) ? " OR " : "";
					factLike += sep + "f.description LIKE :" + pn + " OR f.entity LIKE :" + pn;
					glossLike += sep + "g.term LIKE :" + pn + " OR g.context LIKE :" + pn;
					procLike += sep + "bp.name LIKE :" + pn + " OR bp.description LIKE :" + pn;
					formLike += sep + "fm.template LIKE :" + pn + " OR fm.class_name LIKE :" + pn
						+ " OR fm.main_table LIKE :" + pn;
					relLike += sep + "r.entity_from LIKE :" + pn + " OR r.entity_to LIKE :" + pn;
					queryLike += sep + "vq.purpose LIKE :" + pn + " OR vq.notes LIKE :" + pn;
					patternLike += sep + "cp.pattern_name LIKE :" + pn + " OR cp.description LIKE :" + pn;
					taskLike += sep + "ct.title LIKE :" + pn + " OR ct.description LIKE :" + pn;
				}

				// === STEP 2: Parallel FTS+LIKE search across all stores ===
				json factMatches = RankedSearch(db, p,
					"SELECT f.id, f.entity, f.fact_type, f.description, f.confidence "
					"FROM facts f INNER JOIN facts_fts fts ON f.id = fts.rowid "
					"WHERE facts_fts MATCH :query ORDER BY fts.rank LIMIT 20",
					"SELECT f.id, f.entity, f.fact_type, f.description, f.confidence "
					"FROM facts f WHERE " + factLike + " LIMIT 20", 20);

				json glossMatches = RankedSearch(db, p,
					"SELECT g.* FROM glossary g INNER JOIN glossary_fts fts ON g.id = fts.rowid "
					"WHERE glossary_fts MATCH :query ORDER BY fts.rank LIMIT 5",
					"SELECT g.* FROM glossary g WHERE " + glossLike + " LIMIT 5", 5);

				json procMatches = RankedSearch(db, p,
					"SELECT bp.id, bp.name, bp.related_entities FROM business_processes bp "
					"INNER JOIN processes_fts fts ON bp.id = fts.rowid "
					"WHERE processes_fts MATCH :query ORDER BY fts.rank LIMIT 5",
					"SELECT bp.id, bp.name, bp.related_entities FROM business_processes bp WHERE "
					+ procLike + " LIMIT 5", 5);

				json queryMatches = RankedSearch(db, p,
					"SELECT vq.id, vq.purpose, vq.sql_text, vq.notes FROM verified_queries vq "
					"INNER JOIN queries_fts fts ON vq.id = fts.rowid "
					"WHERE queries_fts MATCH :query ORDER BY fts.rank LIMIT 5",
					"SELECT vq.id, vq.purpose, vq.sql_text, vq.notes FROM verified_queries vq WHERE "
					+ queryLike + " LIMIT 5", 5);

				json patternMatches = RankedSearch(db, p,
					"SELECT cp.id, cp.pattern_name, cp.description FROM code_patterns cp "
					"INNER JOIN patterns_fts fts ON cp.id = fts.rowid "
					"WHERE patterns_fts MATCH :query ORDER BY fts.rank LIMIT 3",
					"SELECT cp.id, cp.pattern_name, cp.description FROM code_patterns cp WHERE "
					+ patternLike + " LIMIT 3", 3);

				json formMatches = json::array();
				try { formMatches = db->Query(
					"SELECT * FROM form_table_map fm WHERE " + formLike + " LIMIT 5", p);
				} catch (...) {}

				json relMatches = json::array();
				try { relMatches = db->Query(
					"SELECT * FROM relationships r WHERE " + relLike + " LIMIT 15", p);
				} catch (...) {}

				json taskMatches = json::array();
				try {
					taskMatches = RankedSearch(db, p,
						"SELECT ct.id, ct.title, ct.created_at FROM completed_tasks ct "
						"INNER JOIN tasks_fts fts ON ct.id = fts.rowid "
						"WHERE tasks_fts MATCH :query ORDER BY fts.rank LIMIT 5",
						"SELECT ct.id, ct.title, ct.created_at FROM completed_tasks ct WHERE "
						+ taskLike + " LIMIT 5", 5);
				} catch (...) {}

				// === STEP 3: Entity extraction + ranking ===
				std::map<std::string, int> entScores;
				std::map<std::string, std::set<std::string>> entSources;

				// Facts: +2 per match
				for (const auto &r : factMatches) {
					std::string e = r.value("entity", std::string());
					if (!e.empty()) { entScores[e] += 2; entSources[e].insert("facts"); }
				}
				// Glossary: +3 (high-value match)
				for (const auto &r : glossMatches) {
					std::string e = r.value("entity", std::string());
					if (!e.empty()) { entScores[e] += 3; entSources[e].insert("glossary"); }
				}
				// Forms: +3
				for (const auto &r : formMatches) {
					std::string mt = r.value("main_table", std::string());
					if (!mt.empty()) { entScores[mt] += 3; entSources[mt].insert("forms"); }
				}
				// Processes: +1 per related entity
				for (const auto &r : procMatches) {
					try {
						json arr = json::parse(r.value("related_entities", std::string()));
						if (arr.is_array())
							for (const auto &item : arr)
								if (item.is_string()) {
									std::string e = item.get<std::string>();
									entScores[e] += 1;
									entSources[e].insert("processes");
								}
					} catch (...) {}
				}
				// Relationships: +1 per entity
				for (const auto &r : relMatches) {
					std::string ef = r.value("entity_from", std::string());
					std::string et = r.value("entity_to", std::string());
					if (!ef.empty()) { entScores[ef] += 1; entSources[ef].insert("relationships"); }
					if (!et.empty()) { entScores[et] += 1; entSources[et].insert("relationships"); }
				}

				// Apply feedback boosting
				if (!entScores.empty()) {
					try {
						json fb = db->Query(
							"SELECT entity, "
							"  SUM(CASE WHEN useful=1 THEN 1 ELSE 0 END) as pos, "
							"  SUM(CASE WHEN useful=0 THEN 1 ELSE 0 END) as neg "
							"FROM context_feedback GROUP BY entity HAVING COUNT(*) >= 3", {});
						for (const auto &r : fb) {
							std::string e = r.value("entity", std::string());
							int pos = (int)r.value("pos", (long long)0);
							int neg = (int)r.value("neg", (long long)0);
							if (entScores.count(e) && (pos + neg) > 0) {
								double ratio = (double)pos / (pos + neg);
								// Boost useful entities, penalize useless
								double factor = 0.5 + 0.5 * ratio; // range: 0.5 - 1.0
								entScores[e] = (int)(entScores[e] * factor);
								if (entScores[e] < 1) entScores[e] = 1;
							}
						}
					} catch (...) {}
				}

				// Rank and take top N
				std::vector<std::pair<std::string, int>> ranked(entScores.begin(), entScores.end());
				std::sort(ranked.begin(), ranked.end(),
					[](const std::pair<std::string,int> &a, const std::pair<std::string,int> &b) {
						return a.second > b.second;
					});
				if ((int)ranked.size() > maxEnt) ranked.resize(maxEnt);

				// === STEP 4: Batch enrichment ===
				json entityBriefs = json::array();
				if (!ranked.empty()) {
					std::string inC = "(";
					for (size_t i = 0; i < ranked.size(); i++) {
						if (i > 0) inC += ",";
						inC += "'" + EscapeSqlString(ranked[i].first) + "'";
					}
					inC += ")";

					auto safeQ = [db](const std::string &sql) -> json {
						try { return db->Query(sql, {}); }
						catch (...) { return json::array(); }
					};

					json stRows = safeQ("SELECT entity, coverage FROM entity_status WHERE entity IN " + inC);
					json fcRows = safeQ("SELECT entity, fact_type, COUNT(*) as cnt FROM facts WHERE entity IN "
						+ inC + " GROUP BY entity, fact_type");
					json rcRows = safeQ(
						"SELECT entity_from as entity, COUNT(*) as cnt "
						"FROM relationships WHERE entity_from IN " + inC + " GROUP BY entity_from "
						"UNION ALL "
						"SELECT entity_to as entity, COUNT(*) as cnt "
						"FROM relationships WHERE entity_to IN " + inC + " GROUP BY entity_to");
					json frRows = safeQ("SELECT template, class_name, file_path, main_table "
						"FROM form_table_map WHERE main_table IN " + inC);

					// Build lookup maps
					std::map<std::string, std::string> covMap;
					std::map<std::string, json> ftMap;
					std::map<std::string, int> relMap;
					std::map<std::string, json> frmMap;

					for (const auto &r : stRows)
						covMap[r.value("entity", std::string())] = r.value("coverage", std::string());
					for (const auto &r : fcRows) {
						std::string e = r.value("entity", std::string());
						std::string ft = r.value("fact_type", std::string());
						ftMap[e][ft] = r.value("cnt", (long long)0);
					}
					for (const auto &r : rcRows) {
						std::string e = r.value("entity", std::string());
						relMap[e] += (int)r.value("cnt", (long long)0);
					}
					for (const auto &r : frRows) {
						std::string mt = r.value("main_table", std::string());
						frmMap[mt] = json{
							{"class_name", r.value("class_name", std::string())},
							{"file_path", r.value("file_path", std::string())}
						};
					}

					for (const auto &re : ranked) {
						const std::string &ent = re.first;
						json brief;
						brief["name"] = ent;
						brief["relevance"] = re.second;
						brief["coverage"] = covMap.count(ent) ? covMap[ent] : "unknown";

						if (ftMap.count(ent)) {
							brief["fact_counts"] = ftMap[ent];
							int total = 0;
							for (auto it = ftMap[ent].begin(); it != ftMap[ent].end(); ++it)
								if (it.value().is_number()) total += it.value().get<int>();
							brief["total_facts"] = total;
						} else {
							brief["total_facts"] = 0;
						}

						brief["rel_count"] = relMap.count(ent) ? relMap[ent] : 0;
						if (frmMap.count(ent)) brief["form"] = frmMap[ent];

						// Completeness %
						int items = 0;
						if (ftMap.count(ent)) items += (int)ftMap[ent].size();
						if (relMap.count(ent) && relMap[ent] > 0) items++;
						if (frmMap.count(ent)) items++;
						brief["completeness"] = (int)(100.0 * items / 9);

						entityBriefs.push_back(brief);
					}
				}

				// === STEP 4.5: Federation — auto-fetch from dbmcp ===
				json fedLog = json::array();
				int dbmcpCalls = 0;
				if (fedClient) {
					int fedCount = 0;
					for (size_t bi = 0; bi < entityBriefs.size() && fedCount < 3; bi++) {
						auto &eb = entityBriefs[bi];
						std::string ent = eb.value("name", std::string());
						std::string cov = eb.value("coverage", std::string("unknown"));
						bool hasSchema = eb.contains("fact_counts") && eb["fact_counts"].contains("schema");

						if (cov == "unknown" && !hasSchema) {
							std::string fedErr;
							bool ok = FederateEntity(db, fedClient, ent, &fedErr);
							if (ok) {
								dbmcpCalls += 2; // schema + relations calls
								json logEntry = {{"entity", ent}, {"status", "fetched"}};
								if (!fedErr.empty()) logEntry["schema_error"] = fedErr;
								fedLog.push_back(logEntry);
								fedCount++;

								// Re-enrich this entity's brief from SQLite
								LocalMcpDb::Params rp;
								rp["ent"] = ent;
								eb["coverage"] = "schema_only";
								try {
									json fc = db->Query(
										"SELECT fact_type, COUNT(*) as cnt FROM facts "
										"WHERE entity = :ent GROUP BY fact_type", rp);
									json ftCounts;
									int total = 0;
									for (const auto &r : fc) {
										std::string ft = r.value("fact_type", std::string());
										int cnt = (int)r.value("cnt", (long long)0);
										ftCounts[ft] = cnt;
										total += cnt;
									}
									eb["fact_counts"] = ftCounts;
									eb["total_facts"] = total;
								} catch (...) {}
								try {
									json rc = db->Query(
										"SELECT COUNT(*) as cnt FROM relationships "
										"WHERE entity_from = :ent OR entity_to = :ent", rp);
									if (!rc.empty())
										eb["rel_count"] = (int)rc[0].value("cnt", (long long)0);
								} catch (...) {}
								// Recalculate completeness
								int items = 0;
								if (eb.contains("fact_counts")) items += (int)eb["fact_counts"].size();
								if (eb.value("rel_count", 0) > 0) items++;
								if (eb.contains("form")) items++;
								eb["completeness"] = (int)(100.0 * items / 9);
							}
						}
					}
				}

				// === STEP 5: Gap identification ===
				json gaps = json::array();
				for (const auto &eb : entityBriefs) {
					std::string ent = eb.value("name", std::string());
					std::string cov = eb.value("coverage", std::string("unknown"));
					int comp = eb.value("completeness", 0);

					if (comp < 50 || cov == "unknown" || cov == "schema_only") {
						json gap;
						gap["entity"] = ent;
						gap["coverage"] = cov;
						gap["completeness"] = comp;

						json actions = json::array();
						bool hasSchema = false;
						if (eb.contains("fact_counts") && eb["fact_counts"].contains("schema"))
							hasSchema = true;

						if (!hasSchema) {
							actions.push_back(json{
								{"tool", "dbmcp:get_table_schema"},
								{"params", json{{"table", ent}}},
								{"reason", "Schema not in PM"}
							});
						}
						int rc = eb.value("rel_count", 0);
						if (rc == 0) {
							actions.push_back(json{
								{"tool", "dbmcp:get_table_relations"},
								{"params", json{{"table_name", ent}}},
								{"reason", "No FK relationships saved"}
							});
						}
						if (!eb.contains("form")) {
							actions.push_back(json{
								{"tool", "Grep"},
								{"params", json{{"pattern", ent}, {"glob", "*.h"}}},
								{"reason", "Find form/dialog class"}
							});
						}
						gap["suggested_actions"] = actions;
						gaps.push_back(gap);
					}
				}

				// === STEP 6: Compact summaries for non-entity results ===
				// Relevant facts (top 15 from FTS)
				json relevantFacts = json::array();
				for (size_t i = 0; i < factMatches.size() && i < 15; i++) {
					const auto &f = factMatches[i];
					relevantFacts.push_back(json{
						{"id", f.value("id", (long long)0)},
						{"entity", f.value("entity", std::string())},
						{"type", f.value("fact_type", std::string())},
						{"description", f.value("description", std::string())},
						{"confidence", f.value("confidence", std::string())}
					});
				}

				// Process summaries
				json procSummary = json::array();
				for (const auto &r : procMatches) {
					procSummary.push_back(json{
						{"id", r.value("id", (long long)0)},
						{"name", r.value("name", std::string())}
					});
				}

				// Query summaries (with SQL preview)
				json querySummary = json::array();
				for (const auto &r : queryMatches) {
					std::string sql = r.value("sql_text", std::string());
					// Truncate SQL to ~200 chars
					if (sql.size() > 200) sql = TruncateUtf8(sql, 200);
					querySummary.push_back(json{
						{"id", r.value("id", (long long)0)},
						{"purpose", r.value("purpose", std::string())},
						{"sql_preview", sql}
					});
				}

				// Pattern summaries
				json patternSummary = json::array();
				for (const auto &r : patternMatches) {
					std::string desc = r.value("description", std::string());
					if (desc.size() > 150) desc = TruncateUtf8(desc, 150);
					patternSummary.push_back(json{
						{"name", r.value("pattern_name", std::string())},
						{"description", desc}
					});
				}

				// Similar tasks
				json taskSummary = json::array();
				for (const auto &r : taskMatches) {
					taskSummary.push_back(json{
						{"id", r.value("id", (long long)0)},
						{"title", r.value("title", std::string())},
						{"date", r.value("created_at", std::string())}
					});
				}

				// Relationship pairs
				json relPairs = json::array();
				for (const auto &r : relMatches) {
					relPairs.push_back(json{
						{"from", r.value("entity_from", std::string())},
						{"to", r.value("entity_to", std::string())},
						{"type", r.value("rel_type", std::string())}
					});
				}

				// === STEP 7: Build response ===
				json resp;
				resp["task_summary"] = task;
				resp["keywords"] = json(words);
				resp["entities"] = entityBriefs;
				resp["relevant_facts"] = relevantFacts;
				resp["relationships"] = relPairs;
				resp["forms"] = formMatches;
				resp["processes"] = procSummary;
				resp["queries"] = querySummary;
				resp["patterns"] = patternSummary;
				resp["similar_tasks"] = taskSummary;
				resp["gaps"] = gaps;
				if (!fedLog.empty())
					resp["federation_log"] = fedLog;
				int pmQueries = 5 + 2; // 5 RankedSearch (FTS+LIKE each = 2 queries × 5 = 10, counted as 5) + 2 direct queries (forms, rels)
				if (!taskMatches.empty() || !taskLike.empty()) pmQueries++; // tasks search
				if (!ranked.empty()) pmQueries += 4; // enrichment: status + facts + rels + forms
				resp["stats"] = json{
					{"pm_queries", pmQueries},
					{"dbmcp_calls", dbmcpCalls},
					{"entities_found", (int)ranked.size()},
					{"gaps_found", (int)gaps.size()},
					{"keywords_used", (int)words.size()},
					{"synonyms_expanded", (int)(expandedWords.size() - words.size())}
				};

				return TMcpToolResult::Success(resp);
			} catch (const std::exception &e) {
				return TMcpToolResult::Error(e.what());
			}
		}
	});

	return tools;
}

}} // namespace Mcp::Tools

#endif
