//---------------------------------------------------------------------------
// LocalMcpDbInit.h — Schema initialization for Project Memory database
//---------------------------------------------------------------------------

#ifndef LocalMcpDbInitH
#define LocalMcpDbInitH

#include "LocalMcpDb.h"

//---------------------------------------------------------------------------
// InitializeProjectMemorySchema — Creates all tables and FTS indexes
//---------------------------------------------------------------------------
inline void InitializeProjectMemorySchema(LocalMcpDb *db)
{
	// Facts table
	db->Exec(
		"CREATE TABLE IF NOT EXISTS facts ("
		"  id INTEGER PRIMARY KEY AUTOINCREMENT,"
		"  entity TEXT NOT NULL,"
		"  fact_type TEXT NOT NULL,"
		"  description TEXT NOT NULL,"
		"  evidence TEXT,"
		"  confidence TEXT DEFAULT 'unverified',"
		"  task_context TEXT,"
		"  created_at TEXT DEFAULT (datetime('now')),"
		"  updated_at TEXT DEFAULT (datetime('now'))"
		");"
		"CREATE INDEX IF NOT EXISTS idx_facts_entity ON facts(entity);"
		"CREATE INDEX IF NOT EXISTS idx_facts_type ON facts(fact_type);"
	);

	// Facts FTS
	db->Exec(
		"CREATE VIRTUAL TABLE IF NOT EXISTS facts_fts USING fts5("
		"  entity, description, evidence, task_context,"
		"  content='facts', content_rowid='id'"
		");"
	);

	// Facts FTS triggers
	db->Exec(
		"CREATE TRIGGER IF NOT EXISTS facts_ai AFTER INSERT ON facts BEGIN "
		"  INSERT INTO facts_fts(rowid, entity, description, evidence, task_context) "
		"  VALUES (new.id, new.entity, new.description, new.evidence, new.task_context); "
		"END;"
	);
	db->Exec(
		"CREATE TRIGGER IF NOT EXISTS facts_ad AFTER DELETE ON facts BEGIN "
		"  INSERT INTO facts_fts(facts_fts, rowid, entity, description, evidence, task_context) "
		"  VALUES ('delete', old.id, old.entity, old.description, old.evidence, old.task_context); "
		"END;"
	);
	db->Exec(
		"CREATE TRIGGER IF NOT EXISTS facts_au AFTER UPDATE ON facts BEGIN "
		"  INSERT INTO facts_fts(facts_fts, rowid, entity, description, evidence, task_context) "
		"  VALUES ('delete', old.id, old.entity, old.description, old.evidence, old.task_context); "
		"  INSERT INTO facts_fts(rowid, entity, description, evidence, task_context) "
		"  VALUES (new.id, new.entity, new.description, new.evidence, new.task_context); "
		"END;"
	);

	// Form table map
	db->Exec(
		"CREATE TABLE IF NOT EXISTS form_table_map ("
		"  id INTEGER PRIMARY KEY AUTOINCREMENT,"
		"  template TEXT UNIQUE NOT NULL,"
		"  class_name TEXT,"
		"  file_path TEXT,"
		"  main_table TEXT,"
		"  sql_file TEXT,"
		"  operations TEXT,"
		"  created_at TEXT DEFAULT (datetime('now')),"
		"  updated_at TEXT DEFAULT (datetime('now'))"
		");"
		"CREATE INDEX IF NOT EXISTS idx_form_template ON form_table_map(template);"
		"CREATE INDEX IF NOT EXISTS idx_form_table ON form_table_map(main_table);"
	);

	// Business processes
	db->Exec(
		"CREATE TABLE IF NOT EXISTS business_processes ("
		"  id INTEGER PRIMARY KEY AUTOINCREMENT,"
		"  name TEXT NOT NULL,"
		"  description TEXT,"
		"  steps TEXT,"
		"  synonyms TEXT,"
		"  related_entities TEXT,"
		"  created_at TEXT DEFAULT (datetime('now')),"
		"  updated_at TEXT DEFAULT (datetime('now'))"
		");"
		"CREATE INDEX IF NOT EXISTS idx_bp_name ON business_processes(name);"
	);

	// Processes FTS
	db->Exec(
		"CREATE VIRTUAL TABLE IF NOT EXISTS processes_fts USING fts5("
		"  name, description, synonyms, steps,"
		"  content='business_processes', content_rowid='id'"
		");"
	);
	db->Exec(
		"CREATE TRIGGER IF NOT EXISTS bp_ai AFTER INSERT ON business_processes BEGIN "
		"  INSERT INTO processes_fts(rowid, name, description, synonyms, steps) "
		"  VALUES (new.id, new.name, new.description, new.synonyms, new.steps); "
		"END;"
	);
	db->Exec(
		"CREATE TRIGGER IF NOT EXISTS bp_ad AFTER DELETE ON business_processes BEGIN "
		"  INSERT INTO processes_fts(processes_fts, rowid, name, description, synonyms, steps) "
		"  VALUES ('delete', old.id, old.name, old.description, old.synonyms, old.steps); "
		"END;"
	);
	db->Exec(
		"CREATE TRIGGER IF NOT EXISTS bp_au AFTER UPDATE ON business_processes BEGIN "
		"  INSERT INTO processes_fts(processes_fts, rowid, name, description, synonyms, steps) "
		"  VALUES ('delete', old.id, old.name, old.description, old.synonyms, old.steps); "
		"  INSERT INTO processes_fts(rowid, name, description, synonyms, steps) "
		"  VALUES (new.id, new.name, new.description, new.synonyms, new.steps); "
		"END;"
	);

	// Glossary
	db->Exec(
		"CREATE TABLE IF NOT EXISTS glossary ("
		"  id INTEGER PRIMARY KEY AUTOINCREMENT,"
		"  term TEXT UNIQUE NOT NULL,"
		"  synonyms TEXT,"
		"  entity TEXT,"
		"  menu_path TEXT,"
		"  context TEXT,"
		"  created_at TEXT DEFAULT (datetime('now')),"
		"  updated_at TEXT DEFAULT (datetime('now'))"
		");"
		"CREATE INDEX IF NOT EXISTS idx_glossary_term ON glossary(term);"
	);

	// Glossary FTS
	db->Exec(
		"CREATE VIRTUAL TABLE IF NOT EXISTS glossary_fts USING fts5("
		"  term, synonyms, context,"
		"  content='glossary', content_rowid='id'"
		");"
	);
	db->Exec(
		"CREATE TRIGGER IF NOT EXISTS gloss_ai AFTER INSERT ON glossary BEGIN "
		"  INSERT INTO glossary_fts(rowid, term, synonyms, context) "
		"  VALUES (new.id, new.term, new.synonyms, new.context); "
		"END;"
	);
	db->Exec(
		"CREATE TRIGGER IF NOT EXISTS gloss_ad AFTER DELETE ON glossary BEGIN "
		"  INSERT INTO glossary_fts(glossary_fts, rowid, term, synonyms, context) "
		"  VALUES ('delete', old.id, old.term, old.synonyms, old.context); "
		"END;"
	);
	db->Exec(
		"CREATE TRIGGER IF NOT EXISTS gloss_au AFTER UPDATE ON glossary BEGIN "
		"  INSERT INTO glossary_fts(glossary_fts, rowid, term, synonyms, context) "
		"  VALUES ('delete', old.id, old.term, old.synonyms, old.context); "
		"  INSERT INTO glossary_fts(rowid, term, synonyms, context) "
		"  VALUES (new.id, new.term, new.synonyms, new.context); "
		"END;"
	);

	// Verified queries
	db->Exec(
		"CREATE TABLE IF NOT EXISTS verified_queries ("
		"  id INTEGER PRIMARY KEY AUTOINCREMENT,"
		"  purpose TEXT NOT NULL,"
		"  sql_text TEXT NOT NULL,"
		"  result_summary TEXT,"
		"  notes TEXT,"
		"  created_at TEXT DEFAULT (datetime('now')),"
		"  updated_at TEXT DEFAULT (datetime('now'))"
		");"
	);

	// Queries FTS
	db->Exec(
		"CREATE VIRTUAL TABLE IF NOT EXISTS queries_fts USING fts5("
		"  purpose, sql_text, notes,"
		"  content='verified_queries', content_rowid='id'"
		");"
	);
	db->Exec(
		"CREATE TRIGGER IF NOT EXISTS vq_ai AFTER INSERT ON verified_queries BEGIN "
		"  INSERT INTO queries_fts(rowid, purpose, sql_text, notes) "
		"  VALUES (new.id, new.purpose, new.sql_text, new.notes); "
		"END;"
	);
	db->Exec(
		"CREATE TRIGGER IF NOT EXISTS vq_ad AFTER DELETE ON verified_queries BEGIN "
		"  INSERT INTO queries_fts(queries_fts, rowid, purpose, sql_text, notes) "
		"  VALUES ('delete', old.id, old.purpose, old.sql_text, old.notes); "
		"END;"
	);
	db->Exec(
		"CREATE TRIGGER IF NOT EXISTS vq_au AFTER UPDATE ON verified_queries BEGIN "
		"  INSERT INTO queries_fts(queries_fts, rowid, purpose, sql_text, notes) "
		"  VALUES ('delete', old.id, old.purpose, old.sql_text, old.notes); "
		"  INSERT INTO queries_fts(rowid, purpose, sql_text, notes) "
		"  VALUES (new.id, new.purpose, new.sql_text, new.notes); "
		"END;"
	);

	// Code patterns
	db->Exec(
		"CREATE TABLE IF NOT EXISTS code_patterns ("
		"  id INTEGER PRIMARY KEY AUTOINCREMENT,"
		"  pattern_name TEXT UNIQUE NOT NULL,"
		"  description TEXT NOT NULL,"
		"  structure TEXT,"
		"  files TEXT,"
		"  evidence TEXT,"
		"  created_at TEXT DEFAULT (datetime('now')),"
		"  updated_at TEXT DEFAULT (datetime('now'))"
		");"
		"CREATE INDEX IF NOT EXISTS idx_pattern_name ON code_patterns(pattern_name);"
	);

	// Patterns FTS
	db->Exec(
		"CREATE VIRTUAL TABLE IF NOT EXISTS patterns_fts USING fts5("
		"  pattern_name, description, structure,"
		"  content='code_patterns', content_rowid='id'"
		");"
	);
	db->Exec(
		"CREATE TRIGGER IF NOT EXISTS cp_ai AFTER INSERT ON code_patterns BEGIN "
		"  INSERT INTO patterns_fts(rowid, pattern_name, description, structure) "
		"  VALUES (new.id, new.pattern_name, new.description, new.structure); "
		"END;"
	);
	db->Exec(
		"CREATE TRIGGER IF NOT EXISTS cp_ad AFTER DELETE ON code_patterns BEGIN "
		"  INSERT INTO patterns_fts(patterns_fts, rowid, pattern_name, description, structure) "
		"  VALUES ('delete', old.id, old.pattern_name, old.description, old.structure); "
		"END;"
	);
	db->Exec(
		"CREATE TRIGGER IF NOT EXISTS cp_au AFTER UPDATE ON code_patterns BEGIN "
		"  INSERT INTO patterns_fts(patterns_fts, rowid, pattern_name, description, structure) "
		"  VALUES ('delete', old.id, old.pattern_name, old.description, old.structure); "
		"  INSERT INTO patterns_fts(rowid, pattern_name, description, structure) "
		"  VALUES (new.id, new.pattern_name, new.description, new.structure); "
		"END;"
	);

	// Relationships
	db->Exec(
		"CREATE TABLE IF NOT EXISTS relationships ("
		"  id INTEGER PRIMARY KEY AUTOINCREMENT,"
		"  entity_from TEXT NOT NULL,"
		"  entity_to TEXT NOT NULL,"
		"  rel_type TEXT NOT NULL,"
		"  description TEXT,"
		"  evidence TEXT,"
		"  created_at TEXT DEFAULT (datetime('now')),"
		"  UNIQUE(entity_from, entity_to, rel_type)"
		");"
		"CREATE INDEX IF NOT EXISTS idx_rel_from ON relationships(entity_from);"
		"CREATE INDEX IF NOT EXISTS idx_rel_to ON relationships(entity_to);"
		"CREATE INDEX IF NOT EXISTS idx_rel_type ON relationships(rel_type);"
	);

	// Entity modules
	db->Exec(
		"CREATE TABLE IF NOT EXISTS entity_modules ("
		"  id INTEGER PRIMARY KEY AUTOINCREMENT,"
		"  module_name TEXT NOT NULL,"
		"  entity TEXT NOT NULL,"
		"  created_at TEXT DEFAULT (datetime('now')),"
		"  UNIQUE(module_name, entity)"
		");"
		"CREATE INDEX IF NOT EXISTS idx_em_module ON entity_modules(module_name);"
		"CREATE INDEX IF NOT EXISTS idx_em_entity ON entity_modules(entity);"
	);

	// Entity status
	db->Exec(
		"CREATE TABLE IF NOT EXISTS entity_status ("
		"  id INTEGER PRIMARY KEY AUTOINCREMENT,"
		"  entity TEXT UNIQUE NOT NULL,"
		"  coverage TEXT NOT NULL,"
		"  notes TEXT,"
		"  updated_at TEXT DEFAULT (datetime('now'))"
		");"
		"CREATE INDEX IF NOT EXISTS idx_es_entity ON entity_status(entity);"
		"CREATE INDEX IF NOT EXISTS idx_es_coverage ON entity_status(coverage);"
	);

	// Tool usage statistics
	db->Exec(
		"CREATE TABLE IF NOT EXISTS tool_usage ("
		"  id INTEGER PRIMARY KEY AUTOINCREMENT,"
		"  tool_name TEXT NOT NULL,"
		"  success INTEGER DEFAULT 1,"
		"  error_message TEXT,"
		"  called_at TEXT DEFAULT (datetime('now'))"
		");"
		"CREATE INDEX IF NOT EXISTS idx_tu_tool ON tool_usage(tool_name);"
		"CREATE INDEX IF NOT EXISTS idx_tu_called ON tool_usage(called_at);"
	);

	// Feature requests
	db->Exec(
		"CREATE TABLE IF NOT EXISTS feature_requests ("
		"  id INTEGER PRIMARY KEY AUTOINCREMENT,"
		"  title TEXT NOT NULL,"
		"  description TEXT NOT NULL,"
		"  use_case TEXT,"
		"  priority TEXT DEFAULT 'normal',"
		"  status TEXT DEFAULT 'new',"
		"  created_at TEXT DEFAULT (datetime('now')),"
		"  updated_at TEXT DEFAULT (datetime('now'))"
		");"
		"CREATE INDEX IF NOT EXISTS idx_fr_status ON feature_requests(status);"
		"CREATE INDEX IF NOT EXISTS idx_fr_priority ON feature_requests(priority);"
	);

	// Completed tasks
	db->Exec(
		"CREATE TABLE IF NOT EXISTS completed_tasks ("
		"  id INTEGER PRIMARY KEY AUTOINCREMENT,"
		"  title TEXT NOT NULL,"
		"  issue_number TEXT,"
		"  author TEXT,"
		"  description TEXT NOT NULL,"
		"  created_at TEXT DEFAULT (datetime('now'))"
		");"
		"CREATE INDEX IF NOT EXISTS idx_ct_author ON completed_tasks(author);"
		"CREATE INDEX IF NOT EXISTS idx_ct_created ON completed_tasks(created_at);"
	);

	// Completed tasks FTS
	db->Exec(
		"CREATE VIRTUAL TABLE IF NOT EXISTS tasks_fts USING fts5("
		"  title, description, content='completed_tasks', content_rowid='id'"
		");"
	);
	db->Exec(
		"CREATE TRIGGER IF NOT EXISTS ct_ai AFTER INSERT ON completed_tasks BEGIN "
		"  INSERT INTO tasks_fts(rowid, title, description) "
		"  VALUES (new.id, new.title, new.description); "
		"END;"
	);
	db->Exec(
		"CREATE TRIGGER IF NOT EXISTS ct_ad AFTER DELETE ON completed_tasks BEGIN "
		"  INSERT INTO tasks_fts(tasks_fts, rowid, title, description) "
		"  VALUES ('delete', old.id, old.title, old.description); "
		"END;"
	);
	db->Exec(
		"CREATE TRIGGER IF NOT EXISTS ct_au AFTER UPDATE ON completed_tasks BEGIN "
		"  INSERT INTO tasks_fts(tasks_fts, rowid, title, description) "
		"  VALUES ('delete', old.id, old.title, old.description); "
		"  INSERT INTO tasks_fts(rowid, title, description) "
		"  VALUES (new.id, new.title, new.description); "
		"END;"
	);

	// UI routes — navigation route storage (UPSERT by form_type + scenario)
	db->Exec(
		"CREATE TABLE IF NOT EXISTS ui_routes ("
		"  id INTEGER PRIMARY KEY AUTOINCREMENT,"
		"  form_type TEXT NOT NULL,"
		"  scenario TEXT NOT NULL DEFAULT 'default',"
		"  description TEXT,"
		"  steps TEXT NOT NULL,"
		"  controls TEXT,"
		"  grid_rects TEXT,"
		"  behavior TEXT,"
		"  code_context TEXT,"
		"  verified TEXT,"
		"  created_at TEXT DEFAULT (datetime('now')),"
		"  updated_at TEXT DEFAULT (datetime('now')),"
		"  UNIQUE(form_type, scenario)"
		");"
		"CREATE INDEX IF NOT EXISTS idx_uir_form_type ON ui_routes(form_type);"
		"CREATE INDEX IF NOT EXISTS idx_uir_scenario ON ui_routes(scenario);"
	);

	// UI routes FTS
	db->Exec(
		"CREATE VIRTUAL TABLE IF NOT EXISTS ui_routes_fts USING fts5("
		"  form_type, scenario, description, controls,"
		"  content='ui_routes', content_rowid='id'"
		");"
	);
	db->Exec(
		"CREATE TRIGGER IF NOT EXISTS uir_ai AFTER INSERT ON ui_routes BEGIN "
		"  INSERT INTO ui_routes_fts(rowid, form_type, scenario, description, controls) "
		"  VALUES (new.id, new.form_type, new.scenario, new.description, new.controls); "
		"END;"
	);
	db->Exec(
		"CREATE TRIGGER IF NOT EXISTS uir_ad AFTER DELETE ON ui_routes BEGIN "
		"  INSERT INTO ui_routes_fts(ui_routes_fts, rowid, form_type, scenario, description, controls) "
		"  VALUES ('delete', old.id, old.form_type, old.scenario, old.description, old.controls); "
		"END;"
	);
	db->Exec(
		"CREATE TRIGGER IF NOT EXISTS uir_au AFTER UPDATE ON ui_routes BEGIN "
		"  INSERT INTO ui_routes_fts(ui_routes_fts, rowid, form_type, scenario, description, controls) "
		"  VALUES ('delete', old.id, old.form_type, old.scenario, old.description, old.controls); "
		"  INSERT INTO ui_routes_fts(rowid, form_type, scenario, description, controls) "
		"  VALUES (new.id, new.form_type, new.scenario, new.description, new.controls); "
		"END;"
	);

	// Search gaps — track unsuccessful searches for knowledge gap detection
	db->Exec(
		"CREATE TABLE IF NOT EXISTS search_gaps ("
		"  id INTEGER PRIMARY KEY AUTOINCREMENT,"
		"  query_text TEXT NOT NULL,"
		"  tool_name TEXT NOT NULL,"
		"  result_count INTEGER DEFAULT 0,"
		"  resolved INTEGER DEFAULT 0,"
		"  resolved_by TEXT,"
		"  created_at TEXT DEFAULT (datetime('now')),"
		"  resolved_at TEXT"
		");"
		"CREATE INDEX IF NOT EXISTS idx_sg_resolved ON search_gaps(resolved);"
	);

	// Context feedback — track which entities were useful/useless in context
	db->Exec(
		"CREATE TABLE IF NOT EXISTS context_feedback ("
		"  id INTEGER PRIMARY KEY AUTOINCREMENT,"
		"  task_text TEXT,"
		"  entity TEXT NOT NULL,"
		"  useful INTEGER NOT NULL DEFAULT 1,"
		"  created_at TEXT DEFAULT (datetime('now'))"
		");"
		"CREATE INDEX IF NOT EXISTS idx_cf_entity ON context_feedback(entity);"
	);

	// Fact history — track changes to facts for audit trail
	db->Exec(
		"CREATE TABLE IF NOT EXISTS fact_history ("
		"  id INTEGER PRIMARY KEY AUTOINCREMENT,"
		"  fact_id INTEGER NOT NULL,"
		"  old_description TEXT,"
		"  new_description TEXT,"
		"  changed_at TEXT DEFAULT (datetime('now'))"
		");"
	);

	// UI research — code investigation results (UPSERT by form_class)
	db->Exec(
		"CREATE TABLE IF NOT EXISTS ui_research ("
		"  id INTEGER PRIMARY KEY AUTOINCREMENT,"
		"  form_class TEXT NOT NULL UNIQUE,"
		"  form_type TEXT,"
		"  file_path TEXT,"
		"  message_map TEXT,"
		"  handlers TEXT,"
		"  validations TEXT,"
		"  ribbon_logic TEXT,"
		"  modals TEXT,"
		"  notes TEXT,"
		"  created_at TEXT DEFAULT (datetime('now')),"
		"  updated_at TEXT DEFAULT (datetime('now'))"
		");"
		"CREATE INDEX IF NOT EXISTS idx_uires_class ON ui_research(form_class);"
		"CREATE INDEX IF NOT EXISTS idx_uires_type ON ui_research(form_type);"
	);

	// UI research FTS
	db->Exec(
		"CREATE VIRTUAL TABLE IF NOT EXISTS ui_research_fts USING fts5("
		"  form_class, form_type, handlers, validations, notes,"
		"  content='ui_research', content_rowid='id'"
		");"
	);
	db->Exec(
		"CREATE TRIGGER IF NOT EXISTS uires_ai AFTER INSERT ON ui_research BEGIN "
		"  INSERT INTO ui_research_fts(rowid, form_class, form_type, handlers, validations, notes) "
		"  VALUES (new.id, new.form_class, new.form_type, new.handlers, new.validations, new.notes); "
		"END;"
	);
	db->Exec(
		"CREATE TRIGGER IF NOT EXISTS uires_ad AFTER DELETE ON ui_research BEGIN "
		"  INSERT INTO ui_research_fts(ui_research_fts, rowid, form_class, form_type, handlers, validations, notes) "
		"  VALUES ('delete', old.id, old.form_class, old.form_type, old.handlers, old.validations, old.notes); "
		"END;"
	);
	db->Exec(
		"CREATE TRIGGER IF NOT EXISTS uires_au AFTER UPDATE ON ui_research BEGIN "
		"  INSERT INTO ui_research_fts(ui_research_fts, rowid, form_class, form_type, handlers, validations, notes) "
		"  VALUES ('delete', old.id, old.form_class, old.form_type, old.handlers, old.validations, old.notes); "
		"  INSERT INTO ui_research_fts(rowid, form_class, form_type, handlers, validations, notes) "
		"  VALUES (new.id, new.form_class, new.form_type, new.handlers, new.validations, new.notes); "
		"END;"
	);
}

#endif
