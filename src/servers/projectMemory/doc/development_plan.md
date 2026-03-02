# smart_context — Полный план развития ProjectMemory

## Context

**ProjectMemory** — MCP-сервер в составе MCPHub для накопления знаний о MERP между сессиями Claude Code.

**Текущее состояние** (оценка 6.1/10):
- 188 сущностей, 441 факт, 32 связи (из 500+ реальных FK)
- 43% full coverage, 36% unknown, 94% FK-связей не сохранены
- pm-context агент: 20 вызовов, ~30 сек, не сохраняет найденное
- Поиск по запросам сломан (0/6 релевантных для "документы по продукту")

**Цель**: `prepare_context(task)` — один вызов = полный контекст. Self-improving loop: каждый запуск улучшает базу для следующих.

**Исходники**: `C:\RADProjects\MCPHub\src\servers\projectMemory\`
**Гайд**: `C:\RADProjects\MCPHub\src\servers\NEW_MCP_GUIDE.md`
**Build**: RAD Studio 23.0, MCPHub.cbproj

---

## Архитектура: 3 слоя

```
┌─────────────────────────────────────────────────────┐
│  Layer 3: Claude Code Agent (pm-context)            │
│  - Получает structured brief от PM                  │
│  - Заполняет code gaps (Grep/Read)                  │
│  - Auto-save обратно в PM (bulk_save)               │
│  - ~10 вызовов вместо 20                            │
└────────────────┬────────────────────────────────────┘
                 │ 1 вызов prepare_context(task)
┌────────────────▼────────────────────────────────────┐
│  Layer 2: ProjectMemory (server-side intelligence)  │
│  - Batch FTS по всем 7 stores (один вход)           │
│  - Federation → dbmcp HTTP для schema/FK            │
│  - Cache-through: результаты dbmcp → PM SQLite      │
│  - Gap identification + suggested actions            │
│  - Embedding search (Stage 5)                        │
└────────────────┬────────────────────────────────────┘
                 │ HTTP localhost:PORT
┌────────────────▼────────────────────────────────────┐
│  Layer 1: dbmcp (live database, always fresh)       │
└─────────────────────────────────────────────────────┘
```

---

## Stage 1: Фундамент — batch-операции и метрики

**Цель**: PM отдаёт больше данных за один вызов. Измеряем качество.

### 1.1 Новые таблицы

В `ProjectMemoryModule.cpp` → `OnInitialize()`:

```sql
-- Трекинг неудачных поисков
CREATE TABLE IF NOT EXISTS search_gaps (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  query_text TEXT NOT NULL,
  tool_name TEXT NOT NULL,
  result_count INTEGER DEFAULT 0,
  resolved INTEGER DEFAULT 0,
  resolved_by TEXT,
  created_at TEXT DEFAULT (datetime('now')),
  resolved_at TEXT
);
CREATE INDEX IF NOT EXISTS idx_search_gaps_resolved ON search_gaps(resolved);

-- Обратная связь по контексту
CREATE TABLE IF NOT EXISTS context_feedback (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  task_text TEXT,
  entity TEXT NOT NULL,
  useful INTEGER NOT NULL DEFAULT 1,
  created_at TEXT DEFAULT (datetime('now'))
);
CREATE INDEX IF NOT EXISTS idx_ctx_feedback_entity ON context_feedback(entity);

-- История изменений фактов
CREATE TABLE IF NOT EXISTS fact_history (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  fact_id INTEGER NOT NULL,
  old_description TEXT,
  new_description TEXT,
  changed_at TEXT DEFAULT (datetime('now'))
);
```

### 1.2 Tool: `batch_context(entities[])`

Один вызов = полный контекст для N сущностей.

```
Вход: entities = ["MoveMaterials", "Product", "DocApprove"]
Выход: {
  "MoveMaterials": {
    "coverage": "full",
    "facts": [{id, type, description, confidence}, ...],
    "relationships": [{from, to, type, description}, ...],
    "form": {template, class_name, file_path, main_table},
    "glossary": {term, synonyms, context},
    "status": {coverage, notes, updated_at}
  },
  "Product": { ... },
  ...
}
```

**SQL** (5 запросов вместо N×5):
```sql
SELECT * FROM facts WHERE entity IN (:e1,:e2,:e3) ORDER BY entity, fact_type;
SELECT * FROM relationships WHERE entity_from IN (...) OR entity_to IN (...);
SELECT * FROM form_table_map WHERE main_table IN (...) OR template IN (...);
SELECT * FROM glossary WHERE entity IN (...);
SELECT * FROM entity_status WHERE entity IN (...);
```

**Файл**: `ProjectMemoryTools.h`, ~100 строк

### 1.3 Tool: `identify_gaps(entities[])`

Для каждой сущности: что есть, что отсутствует, что делать.

```
Вход: entities = ["Product"]
Выход: {
  "Product": {
    "coverage": "schema_only",
    "has": ["schema"],
    "missing": ["business_rule", "form_mapping", "gotcha"],
    "completeness": 20,
    "relationship_count": 6,
    "suggested_actions": [
      {"tool": "dbmcp:get_table_relations", "params": {"table": "Product"}, "reason": "51 FK not saved"},
      {"tool": "Grep", "params": {"pattern": "CProductDlg", "glob": "*.h"}, "reason": "find form class"}
    ]
  }
}
```

**Файл**: `ProjectMemoryTools.h`, ~120 строк

### 1.4 Tool: `quality_report(entity?)`

Глобальные метрики или per-entity.

```
Без параметров: {
  total_entities: 188, total_facts: 441, total_relationships: 32,
  coverage: {full: 81, partial: 24, schema_only: 15, unknown: 68, needs_review: 0},
  stale_entities: 12,
  orphan_entities: 45,
  unresolved_gaps: 8,
  confidence: {verified: 380, unverified: 50, uncertain: 11}
}

С entity: {
  entity: "MoveMaterials", coverage: "full",
  facts_by_type: {schema: 3, business_rule: 2, gotcha: 1},
  relationship_count: 6, has_form: true, has_glossary: true,
  last_updated: "2026-02-20", days_since_update: 8,
  completeness_score: 92
}
```

**Side-effect**: auto-marks stale entities (>30 days, coverage IN full/partial) as `needs_review`.

**Файл**: `ProjectMemoryTools.h`, ~150 строк

### 1.5 Auto-logging search gaps

В существующие search-инструменты (`ask_business`, `search_facts`, `get_query`, `find_process`) добавить:
```cpp
if (results.empty()) {
    db->Execute(
        "INSERT INTO search_gaps(query_text, tool_name, result_count) VALUES(:q,:t,0)",
        {{":q", query}, {":t", "ask_business"}});
}
```

Auto-resolve при `save_fact`/`save_query`:
```cpp
db->Execute(
    "UPDATE search_gaps SET resolved=1, resolved_by='auto', resolved_at=datetime('now') "
    "WHERE resolved=0 AND query_text LIKE '%'||:entity||'%'",
    {{":entity", entity}});
```

Tool `get_knowledge_gaps(limit?)`:
```sql
SELECT query_text, tool_name, COUNT(*) as ask_count
FROM search_gaps WHERE resolved = 0
GROUP BY query_text ORDER BY ask_count DESC LIMIT :limit;
```

### 1.6 Fact history в `update_fact`

Перед обновлением — записать old → new:
```cpp
// В обработчике update_fact, перед UPDATE:
auto old = db->Query("SELECT description FROM facts WHERE id = :id", {{":id", id}});
if (!old.empty()) {
    db->Execute("INSERT INTO fact_history(fact_id, old_description, new_description) VALUES(:fid,:old,:new)",
        {{":fid", id}, {":old", old[0]["description"]}, {":new", newDescription}});
}
```

### Верификация Stage 1

```bash
# batch_context
curl tools/call batch_context '{"entities": ["MoveMaterials","Product"]}' → оба entity с facts/rels

# identify_gaps
curl tools/call identify_gaps '{"entities": ["Product"]}' → missing: business_rule, suggested: dbmcp

# quality_report
curl tools/call quality_report '{}' → global metrics
curl tools/call quality_report '{"entity": "MoveMaterials"}' → per-entity

# search_gaps
curl tools/call ask_business '{"question": "несуществующее"}' → 0 results
curl tools/call get_knowledge_gaps '{}' → показывает "несуществующее"
```

---

## Stage 2: Federation — PM вызывает dbmcp

**Цель**: PM автоматически подгружает schema/FK из живой БД когда в PM пусто.

### 2.1 HTTP-клиент для dbmcp

Новый файл: `src/servers/projectMemory/DbMcpFederation.h`

```cpp
#ifndef DbMcpFederationH
#define DbMcpFederationH

#include <System.Net.HttpClient.hpp>  // RAD Studio THTTPClient
#include <json.hpp>
#include <string>

namespace Federation {

class DbMcpClient {
    std::string FBaseUrl;  // "http://localhost:PORT"
    int FRequestId = 0;

public:
    DbMcpClient(const std::string& baseUrl) : FBaseUrl(baseUrl) {}

    // JSON-RPC call to dbmcp
    nlohmann::json CallTool(const std::string& toolName, const nlohmann::json& args);

    // High-level methods
    nlohmann::json GetTableSchema(const std::string& table);
    nlohmann::json GetTableRelations(const std::string& table);
    nlohmann::json GetTableSample(const std::string& table, int limit = 3);
};

} // namespace Federation
#endif
```

Реализация `CallTool`: формирует JSON-RPC 2.0 request, POST на `FBaseUrl + "/mcp"`, парсит response.

**~100 строк C++** (header-only или с отдельным .cpp)

### 2.2 Конфигурация federation

В `ProjectMemoryModule::GetConfigFields()` добавить:
```cpp
{"dbmcp_url", "dbMCP URL (for federation)", ConfigFieldType::String, {}, "http://localhost:8765", false},
```

В `OnInitialize()`:
```cpp
std::string dbmcpUrl = FConfig.value("dbmcp_url", "");
if (!dbmcpUrl.empty()) {
    FDbMcpClient = std::make_unique<Federation::DbMcpClient>(dbmcpUrl);
}
```

### 2.3 Cache-through в tools

В `identify_gaps` и будущем `prepare_context`: если сущность unknown и есть federation client:

```cpp
if (coverage == "unknown" && FDbMcpClient) {
    // Fetch schema
    auto schema = FDbMcpClient->GetTableSchema(entity);
    if (!schema.is_null()) {
        db->Execute("INSERT INTO facts(entity, fact_type, description, confidence) VALUES(:e,'schema',:d,'verified')",
            {{":e", entity}, {":d", schema.dump()}});
    }
    // Fetch relations
    auto rels = FDbMcpClient->GetTableRelations(entity);
    for (auto& rel : rels) {
        db->Execute("INSERT OR REPLACE INTO relationships(entity_from, entity_to, rel_type, description) ...");
    }
    db->Execute("INSERT OR REPLACE INTO entity_status(entity, coverage, notes) VALUES(:e,'schema_only','auto-imported from dbmcp')",
        {{":e", entity}});
}
```

**Лимит**: максимум 3 federation-вызова за один prepare_context (чтобы не тормозить).

### Верификация Stage 2

```bash
# Federation: PM запрашивает dbmcp
curl tools/call identify_gaps '{"entities": ["SomeUnknownTable"]}'
→ PM вызывает dbmcp → сохраняет schema → возвращает gaps с новым coverage=schema_only

# Cache: повторный вызов НЕ идёт в dbmcp
curl tools/call identify_gaps '{"entities": ["SomeUnknownTable"]}'
→ берёт из SQLite, 0 HTTP calls
```

---

## Stage 3: `prepare_context` — главный инструмент

**Цель**: Один вызов = полный контекст для задачи.

### 3.1 Tool: `prepare_context(task_description, max_entities?, include_schema?)`

**Алгоритм (server-side)**:

```
1. KEYWORD EXTRACTION
   - Tokenize task_description → words
   - Remove stop words (existing StopWordFilter)
   - Keep top 7 meaningful words
   - For each word: check glossary → expand with synonyms
   - Build FTS query: "word1* OR synonym1* OR word2* OR ..."

2. PARALLEL FTS SEARCH (all 7 stores, one pass each)
   - facts_fts MATCH query → top 20
   - processes_fts MATCH query → top 5
   - glossary_fts MATCH query → top 5
   - queries_fts MATCH query → top 5
   - patterns_fts MATCH query → top 3
   - form_table_map LIKE → top 5
   - relationships LIKE → top 15
   - completed_tasks FTS MATCH → top 5

3. ENTITY EXTRACTION
   - Collect unique entity names from all results
   - Rank by frequency (appears in more stores = higher rank)
   - Take top max_entities (default 10)

4. BATCH ENRICHMENT (reuse batch_context logic)
   - For top entities: facts, relationships, forms, glossary, status

5. FEDERATION (if include_schema != false)
   - For entities with coverage IN ('unknown') AND has_schema = false:
     - dbmcp:get_table_schema → save as fact
     - dbmcp:get_table_relations → save as relationships
     - LIMIT: max 3 federation calls

6. GAP IDENTIFICATION (reuse identify_gaps logic)
   - For each entity: what's missing, what tool to use

7. BUILD RESPONSE
   {
     task_summary: "краткое описание задачи",
     entities: [{name, coverage, fact_counts, rel_count, form?, completeness%}],
     relevant_facts: [{id, entity, type, description, confidence}],  // top 15 most relevant
     relationships: [{from, to, type}],
     forms: [{template, class_name, file_path}],
     processes: [{name, steps_count}],
     queries: [{purpose, sql_preview}],
     patterns: [{name, description_preview}],
     similar_tasks: [{title, date, description_preview}],
     gaps: [{entity, missing[], suggested_action, suggested_tool, suggested_params}],
     federation_log: [{entity, action, result}],
     stats: {pm_queries: N, dbmcp_calls: N, entities_enriched: N, gaps_found: N, duration_ms: N}
   }
```

**Файл**: `ProjectMemoryTools.h`, ~400 строк (biggest single tool)

### 3.2 Synonym expansion

В шаге 1 `prepare_context`:
```sql
SELECT synonyms FROM glossary
WHERE term LIKE :word OR synonyms LIKE '%' || :word || '%'
LIMIT 3;
```

Парсить JSON-массив synonyms → добавить в FTS query.

### 3.3 `context_feedback(task_text, useful_entities[], useless_entities[])`

Агент или main LLM вызывает после завершения задачи:
```
context_feedback({
  task_text: "исправить ошибку при перемещении",
  useful_entities: ["MoveMaterials", "CMoveMaterialDlg"],
  useless_entities: ["Storage"]
})
```

Сохраняет в `context_feedback` таблицу. Используется в Stage 5 для boosting.

### Верификация Stage 3

```bash
# Основной тест
curl tools/call prepare_context '{"task_description": "исправить ошибку при перемещении материалов"}'
→ entities: MoveMaterials (full), Storage (partial), ...
→ facts: schema, business_rules для MoveMaterials
→ forms: CMoveMaterialDlg
→ similar_tasks: прошлые баг-фиксы
→ gaps: ["Product: missing business_rule, suggested: Grep CProductDlg"]

# Synonym expansion
curl tools/call prepare_context '{"task_description": "документы по продукту"}'
→ Должен найти через synonym "продукт" → "Product" → "Module"
→ DocApprove facts, MoveMaterials via UniqueEntity path

# Federation test
curl tools/call prepare_context '{"task_description": "проблема с таблицей NewUnknownTable"}'
→ federation_log: [{entity: "NewUnknownTable", action: "get_table_schema", result: "saved"}]
→ При повторном вызове: 0 federation calls
```

---

## Stage 4: Agent upgrade — smart pm-context

**Цель**: Обновить агента Claude Code для использования prepare_context.

### 4.1 Обновить `C:\Monarch\merp\.claude\agents\pm-context.md`

Новый алгоритм:
```
Step 1: prepare_context(task_description)                    [1 вызов → 80% контекста]
  PM внутри делает: FTS + enrichment + federation + gaps
  → Получаем structured brief

Step 2: search_tasks(bug_keywords)                           [0-1 вызов, если баг]
  Только если в task_description есть bug-сигналы
  И если prepare_context не вернул similar_tasks

Step 3: Fill code gaps (по suggested_actions из prepare_context)  [3-8 вызовов]
  Для каждого gap.suggested_tool:
    - "Grep" → Grep(gap.suggested_params.pattern, gap.suggested_params.glob)
    - "Read" → Read(file, offset, limit)
    - "dbmcp:get_object_definition" → dbmcp call
  PM уже подсказал ЧТО и ГДЕ — агент не гадает

Step 4: Auto-save findings                                   [1-2 вызова]
  bulk_save(
    facts=[новые schema/code_pattern факты из Grep/Read],
    relationships=[найденные связи],
    form_mappings=[найденные формы]
  )
  → Self-improving: следующий запуск найдёт это в PM

Step 5: Format and return brief                              [0 вызовов]
  Структурированный бриф (тот же формат что сейчас)

ИТОГО: 6-12 вызовов, контекст полнее, персистентный
```

### 4.2 Добавить auto-save

Текущий pm-context **не сохраняет** (правило #2: "NEVER save anything").
Новый — сохраняет через `bulk_save` после заполнения gaps.

### 4.3 Feedback loop

Main LLM после завершения задачи вызывает:
```
context_feedback(task_text=..., useful_entities=[...], useless_entities=[...])
```

### Верификация Stage 4

```
# Запустить pm-context agent на задачу
Task(subagent_type="pm-context", prompt="добавить валидацию в DocApprove")

# Проверить:
1. Агент вызвал prepare_context (1 вызов, не 7+)
2. Получил structured brief с entities, facts, gaps
3. Заполнил gaps через Grep/Read
4. Вызвал bulk_save (знания сохранены)
5. Brief полнее чем со старым алгоритмом
6. Общее число вызовов < 15

# Self-improving:
# Повторный запуск на похожую задачу → меньше gaps, быстрее
```

---

## Stage 5: Embeddings — семантический поиск

**Цель**: Найти релевантное даже при разном языке/терминологии.

### 5.1 Embedding Service

Отдельный MCP-модуль в MCPHub (или standalone процесс):

```
src/servers/embeddingService/
  EmbeddingModule.h
  EmbeddingModule.cpp
  EmbeddingTools.h
```

Tools:
- `embed_text(text) → float[384]`
- `embed_batch(texts[]) → float[384][]`
- `similar_search(query_text, table, limit) → [{id, text, score}]`

Backend: ONNX Runtime + all-MiniLM-L6-v2 (384 dims, ~80MB).
Или: HTTP proxy к внешнему API (Anthropic/OpenAI).

Config: `{"model_path": "...", "backend": "onnx|api", "api_url": "...", "api_key": "..."}`

### 5.2 Таблица embeddings в ProjectMemory

```sql
-- Используем sqlite-vec если доступен, иначе blob storage
CREATE TABLE IF NOT EXISTS entity_embeddings (
  entity TEXT PRIMARY KEY,
  embedding BLOB NOT NULL,        -- 384 floats = 1536 bytes
  source_text TEXT,                -- текст из которого сделан embedding
  updated_at TEXT DEFAULT (datetime('now'))
);

CREATE TABLE IF NOT EXISTS fact_embeddings (
  fact_id INTEGER PRIMARY KEY,
  embedding BLOB NOT NULL,
  updated_at TEXT DEFAULT (datetime('now'))
);
```

### 5.3 Auto-embedding при save

В `save_fact`, `save_glossary`, `save_process`:
```cpp
if (FEmbeddingClient) {
    auto vec = FEmbeddingClient->Embed(description);
    db->Execute("INSERT OR REPLACE INTO fact_embeddings(fact_id, embedding) VALUES(:id, :vec)",
        {{":id", factId}, {":vec", vecToBlob(vec)}});
}
```

### 5.4 Hybrid search в `prepare_context`

Шаг 1 дополняется:
```
1a. FTS5 keyword search → results_fts (BM25 score)
1b. Synonym expansion → добавить в FTS
1c. Vector search → results_vec (cosine score)
    embed(task_description) → query_vec
    SELECT entity, cosine_similarity(embedding, query_vec) as score
    FROM entity_embeddings WHERE score > 0.7
    ORDER BY score DESC LIMIT 10
1d. Reciprocal Rank Fusion:
    final_score = 1/(k + rank_fts) + 1/(k + rank_vec)  // k=60
    → merged, deduplicated, ranked
```

### 5.5 Dedup detection

При `save_fact`:
```
embed(new_fact_description) → new_vec
SELECT f.id, f.description, cosine_similarity(fe.embedding, new_vec) as sim
FROM facts f JOIN fact_embeddings fe ON f.id = fe.fact_id
WHERE f.entity = :entity AND sim > 0.92
```
Если найден дубликат → warning в response: `"duplicate_warning": "Similar to fact #123 (92% similarity)"`

### Верификация Stage 5

```bash
# Embedding service
curl tools/call embed_text '{"text": "перемещение материалов"}' → [0.12, -0.34, ...]

# Cross-lingual search
curl tools/call prepare_context '{"task_description": "перемещение материалов"}'
→ Через FTS: находит "MoveMaterials" (через synonym)
→ Через Vector: ТОЖЕ находит "MoveMaterials" (семантическая близость)
→ Score выше за счёт double-match

# Dedup
curl tools/call save_fact '{"entity": "MoveMaterials", "fact_type": "schema", "description": "Таблица перемещений..."}'
→ "duplicate_warning": "Similar to fact #45 (94% similarity)"
```

---

## Stage 6: Continuous Improvement — автоматизация

### 6.1 Relevance boosting из context_feedback

В `prepare_context` шаг 3 (entity ranking):
```sql
SELECT entity, SUM(useful) as useful_count, COUNT(*) - SUM(useful) as useless_count,
       CAST(SUM(useful) AS REAL) / COUNT(*) as usefulness_ratio
FROM context_feedback
GROUP BY entity;
```

`final_entity_score = base_score * (0.5 + 0.5 * usefulness_ratio)`

### 6.2 Scheduled staleness check

`quality_report` без параметров автоматически:
```sql
UPDATE entity_status SET coverage = 'needs_review',
  notes = notes || ' | Auto-stale: ' || datetime('now'),
  updated_at = datetime('now')
WHERE entity IN (
  SELECT entity FROM facts GROUP BY entity
  HAVING CAST(julianday('now') - julianday(MAX(updated_at)) AS INTEGER) > 30
) AND coverage IN ('full', 'partial');
```

### 6.3 Skill: `/pm-import-relations [entity]`

Claude Code skill (не требует C++):
```
1. dbmcp:get_table_relations(entity)
2. Для каждого FK → pm:save_relationship(from, to, "references")
3. pm:set_entity_status(entity, "schema_only")
4. Отчёт: "Imported N relationships for entity"
```

Файл: `C:\Monarch\merp\.claude\commands\pm-import-relations.md`

### 6.4 Skill: `/pm-verify [entity]`

Проверка актуальности:
```
1. pm:get_facts(entity, type="schema")
2. dbmcp:get_table_schema(entity)
3. Сравнить: columns в PM vs columns в dbmcp
4. Отчёт: added/removed/changed columns
5. Если diff → pm:set_entity_status(entity, "needs_review")
```

Файл: `C:\Monarch\merp\.claude\commands\pm-verify.md`

---

## Сводка по этапам

| Stage | Что делаем | Новые tools | C++ строк | Зависимости |
|-------|-----------|-------------|-----------|-------------|
| **1** | Batch ops + метрики | batch_context, identify_gaps, quality_report, get_knowledge_gaps | ~500 | Нет |
| **2** | Federation PM→dbmcp | (внутренний HTTP client) | ~250 | THTTPClient (RAD) |
| **3** | prepare_context | prepare_context, context_feedback | ~500 | Stage 1+2 |
| **4** | Agent upgrade | — (md файл) | 0 | Stage 3 |
| **5** | Embeddings | embed_text, similar_search + hybrid search | ~400 | ONNX/API |
| **6** | Automation | — (skills + boosting) | ~50 | Stage 3+5 |

**Итого**: ~1700 строк C++ (stages 1-3-5) + agent md + 2 skills

## Файлы для изменения

| Файл | Stage | Изменения |
|------|-------|-----------|
| `src/servers/projectMemory/ProjectMemoryTools.h` | 1,3 | batch_context, identify_gaps, quality_report, get_knowledge_gaps, prepare_context, context_feedback (~1000 строк) |
| `src/servers/projectMemory/ProjectMemoryModule.h` | 2 | Добавить FDbMcpClient member |
| `src/servers/projectMemory/ProjectMemoryModule.cpp` | 1,2 | Новые таблицы в OnInitialize, federation config, init DbMcpClient |
| `src/servers/projectMemory/DbMcpFederation.h` | 2 | Новый файл: HTTP client для dbmcp (~150 строк) |
| `src/servers/embeddingService/` | 5 | Новый MCP модуль (3 файла) |
| `src/modules/RegisterModules.cpp` | 5 | Регистрация EmbeddingModule |
| `MCPHub.cbproj` | 2,5 | Include paths, CppCompile entries |
| `.claude/agents/pm-context.md` | 4 | Новый алгоритм агента |
| `.claude/commands/pm-import-relations.md` | 6 | Новый skill |
| `.claude/commands/pm-verify.md` | 6 | Новый skill |
