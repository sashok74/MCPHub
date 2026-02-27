# MERP Helper — SQL Query & Code Generation Agent

## Overview

merp-helper is an MCP server for managing SQL queries and generating C++ data access code for the MERP system. It connects to MS SQL Server, stores query metadata (input params, output fields, types), and generates strongly-typed C++ structs and data access layers from Mustache-like templates.

Two instances are available:
- **merp-helper** (port 8090) — DEV database (MERP)
- **merp-helper-prd** (port 8091) — PRODUCTION database copy (MERP_PRD)

## When to Use This Agent

**Use merp-helper when:**
- Adding new SQL queries to the MERP system
- Generating C++ data access code (InParam/OutParam structs)
- Browsing existing queries by module or search pattern
- Analyzing SQL to extract parameter metadata
- Generating CRUD SQL from table names
- Creating custom code templates for new patterns
- Verifying SQL syntax before committing
- Setting up dataset fields (DS) for full CRUD callback generation

**Do NOT use for:**
- Running ad-hoc SQL queries against the database (use `dbmcp` instead)
- Exploring table schemas and relationships (use `dbmcp` instead)
- Saving knowledge about business logic (use `project-memory` instead)

## How to Add to a Project

Add to `.mcp.json`:

```json
{
  "mcpServers": {
    "merp-helper": {
      "type": "http",
      "url": "http://localhost:8090/mcp"
    }
  }
}
```

All tools are available as `mcp__merp-helper__<tool_name>`.

---

## Tools Reference (24 tools)

### Query Browse & Management

| Tool | Purpose | Key Params |
|------|---------|------------|
| `list_modules` | List SQL modules (query groups) | — |
| `list_queries` | List queries in a module | `module_id` (int) |
| `search_queries` | Search by name/SQL/description | `pattern` (string) |
| `get_query` | Full query details + params + fields + DS-fields | `query_name` (string) |
| `add_query` | Add new query | `query_name`, `module_id`, `query_type`, `source_sql` |
| `update_query` | Update existing query + DS-fields (auto-snapshots) | `query_name` + fields to change |
| `delete_query` | Delete query (auto-snapshots) | `query_name` |

### Metadata Analysis

| Tool | Purpose | Key Params |
|------|---------|------------|
| `analyze_query` | Extract params/fields from SQL metadata (auto-snapshots) | `query_name`, `save` (bool) |
| `describe_table` | Get table columns, types, PKs | `table_name` |
| `test_sql` | Validate SQL without saving | `sql` (string) |
| `generate_sql` | Generate CRUD SQL from table name | `table_name`, `operation` (SELECT/INSERT/UPDATE/DELETE) |

### Code Generation

| Tool | Purpose | Key Params |
|------|---------|------------|
| `generate_query_code` | Generate InParam/OutParam/TypeConversion structs | `query_name` |
| `generate_module_code` | Generate complete `{ModuleName}SQL.h` header | `module_id` |
| `generate_all_files` | Generate all project files to directory | `output_dir` |
| `generate_example` | Generate usage example code | `query_name`, `example_type` (data_access/grid/dataset) |
| `render_template` | Render any template against a query | `query_name` + `template_name` or `template_text` |

### Template Management

| Tool | Purpose | Key Params |
|------|---------|------------|
| `list_templates` | List built-in + custom templates | `category` (optional) |
| `get_template` | Get template content by name | `name` |
| `save_template` | Save custom Mustache template | `name`, `template_text` |
| `get_template_vars` | Reference of all template variables | — |

### Configuration

| Tool | Purpose | Key Params |
|------|---------|------------|
| `get_output_dir` | Get configured output directory for generated files | — |

### Safety & Naming

| Tool | Purpose | Key Params |
|------|---------|------------|
| `get_naming_convention` | Show naming rules and workflow | — |
| `list_query_snapshots` | List auto-saved snapshots | `query_name` (optional) |
| `restore_query_snapshot` | Restore query to snapshot state | `snapshot_id` |

---

## Standard Workflow

### Adding a New Query

```
1. search_queries(pattern="entity_name")     — check if similar exists
2. add_query(query_name, module_id, query_type, source_sql)  — create
3. analyze_query(query_name)                  — extract params/fields from metadata
4. get_query(query_name)                      — verify results
5. generate_query_code(query_name)            — generate C++ structs
6. generate_module_code(module_id)            — generate module header
```

### Dataset Setup

```
1. get_query(query_name)                      — check current DS-field values
2. Find or create related queries: _i (INSERT), _u (UPDATE), _d (DELETE), _one_s (reload)
3. update_query(query_name, ds_query_i="...", ds_query_u="...", ds_query_d="...", ds_query_one_s="...")
4. generate_example(query_name, "dataset")    — generate full callback code
```

### Naming Convention

Format: `<group>_<entity>_<sub>_<operation_suffix>`

| Suffix | Type | Example |
|--------|------|---------|
| `_s` | SELECT | `doc_approve_doc_list_s` |
| `_i` | INSERT | `catalogs_apr_i` |
| `_u` | UPDATE | `production_move_material_u` |
| `_d` | DELETE | `catalogs_apr_d` |

### Hungarian Prefixes for Member Names

| Prefix | C++ Type |
|--------|----------|
| `m_i` | int |
| `m_s` | std::string / std::wstring |
| `m_f` | float / double |
| `m_b` | bool |
| `m_dt` | TDateTime / COleDateTime |

---

## Template System

The code generator uses a Mustache-like template engine. Templates use `{{VARIABLE}}` for substitution and `{{#SECTION}}...{{/SECTION}}` for loops/conditionals.

### Template Syntax

| Syntax | Description |
|--------|-------------|
| `{{VARIABLE}}` | Substitute a string value |
| `{{#SECTION}}...{{/SECTION}}` | Loop over array OR render if flag is true |
| `{{^SECTION}}...{{/SECTION}}` | Inverted — render if empty/false |
| `{{!comment}}` | Comment (ignored) |

### Template Variables

#### Global Variables

| Variable | Example | Description |
|----------|---------|-------------|
| `{{QUERY_NAME}}` | `doc_approve_doc_list_s` | Query identifier (snake_case) |
| `{{QUERY_NAME_PASCAL}}` | `DocApproveDocListS` | PascalCase version (for type aliases, DB:: namespace) |
| `{{MODULE_NAME}}` | `DocApprove` | Module name from DB |
| `{{TABLE_NAME}}` | `dbo.DocApproveDocList` | Target table name |
| `{{DESCRIPTION}}` | `Список документов...` | Query description |
| `{{QUERY_TYPE}}` | `SELECT` | Query type |
| `{{IN_COUNT}}` | `3` | Input parameter count |
| `{{OUT_COUNT}}` | `30` | Output field count |

#### Parameter Variables (inside `{{#IN_PARAMS}}` / `{{#OUT_PARAMS}}`)

| Variable | Description |
|----------|-------------|
| `{{CPP_TYPE}}` | C++ type (`int`, `std::string`, `float`) |
| `{{MEMBER_NAME}}` | Struct member (`m_iProductID`) |
| `{{DB_NAME}}` | Database column name (`ProductID`) |
| `{{DEFAULT_VALUE}}` | Default value (`0`, `L""`) |
| `{{HAS_NULL_VALUE}}` | Flag: parameter is nullable |
| `{{NULL_VALUE}}` | Null sentinel (`GLOBAL_INVALID_ID`) |
| `{{DISPLAY_EXPR}}` | *(OUT only)* Display expression |
| `{{FROM_ROW_LINE}}` | *(OUT only)* C++ row extraction code |
| `{{IS_STRING}}` | Flag: string type (nvarchar, varchar, text) |
| `{{IS_NULLABLE}}` | Flag: column allows NULL |
| `{{MAX_LENGTH}}` | Max length for strings (0 if N/A) |
| `{{IS_KEY_FIELD}}` | Flag: primary key column |
| `{{FIELD_TYPE}}` | SQL type: nvarchar, int, datetime, etc. |
| `{{PRECISION}}` | Numeric precision (0 if N/A) |

#### Key Field Variables

| Variable | Description |
|----------|-------------|
| `{{IN_KEY_FIELD_NAME}}` / `{{OUT_KEY_FIELD_NAME}}` | PK member name |
| `{{IN_HAS_EXTRA_KEY}}` / `{{OUT_HAS_EXTRA_KEY}}` | Flag: has extra key beyond m_iID |
| `{{HAS_PID}}` / `{{PID_FIELD_NAME}}` | Parent ID field |
| `{{HAS_TID}}` / `{{TID_FIELD_NAME}}` | Tree ID field |

#### Dataset Template Variables

| Variable | Example | Description |
|----------|---------|-------------|
| `{{#HAS_DS}}` | section | True if DS-fields are configured |
| `{{DS_NAME}}` | `m_dsDocList` | Dataset variable name |
| `{{DS_QUERY_S}}` | `doc_list_s` | SELECT query name (dataset source) |
| `{{DS_QUERY_I}}` | `doc_list_i` | INSERT query name |
| `{{DS_QUERY_U}}` | `doc_list_u` | UPDATE query name |
| `{{DS_QUERY_D}}` | `doc_list_d` | DELETE query name |
| `{{DS_QUERY_ONE_S}}` | `doc_list_one_s` | Reload query name |
| `{{DS_QUERY_I_PASCAL}}` | `DocListI` | PascalCase INSERT |
| `{{DS_QUERY_U_PASCAL}}` | `DocListU` | PascalCase UPDATE |
| `{{DS_QUERY_D_PASCAL}}` | `DocListD` | PascalCase DELETE |
| `{{DS_QUERY_ONE_S_PASCAL}}` | `DocListOneS` | PascalCase reload |
| `{{DS_QUERY_S_PASCAL}}` | `DocListS` | PascalCase SELECT |

#### Loop Control (auto-injected)

`{{_INDEX}}`, `{{_FIRST}}`, `{{_LAST}}`

#### Syntax

`{{#SECTION}}...{{/SECTION}}` — loop/conditional, `{{^SECTION}}...{{/SECTION}}` — inverted

### Built-in Templates

| Name | Category | Description |
|------|----------|-------------|
| `query_types` | code_gen | InParam/OutParam/TypeConversion structs |
| `sql_enum` | code_gen | SQLQueries.h — enum + StringToQuerys() |
| `assign_fields` | code_gen | AssignCommonFields.h |
| `module_header` | code_gen | Per-module {Name}SQL.h |
| `data_access` | code_gen | DataAccess.cpp |
| `file_sql` | code_gen | FileSQLQueryStorage.cpp |
| `example_select` | example | Usage: SELECT query |
| `example_other` | example | Usage: INSERT/UPDATE/DELETE |
| `example_grid` | example | Grid initialization |
| `example_dataset` | example | Dataset with callbacks |

### Custom Template Example

```
save_template(
  name="my_mapper",
  template_text="// Mapper for {{QUERY_NAME}}\n{{#OUT_PARAMS}}\nresult.{{MEMBER_NAME}} = row[\"{{DB_NAME}}\"];\n{{/OUT_PARAMS}}",
  description="Simple row mapper",
  category="custom"
)

render_template(query_name="product_list_s", template_name="my_mapper")
```

### Ad-hoc Template Rendering

```
render_template(
  query_name="product_list_s",
  template_text="struct {{QUERY_NAME}}_Key {\n{{#IN_PARAMS}}  {{CPP_TYPE}} {{MEMBER_NAME}};\n{{/IN_PARAMS}}};"
)
```

---

## Dataset Fields (DS)

Queries can be linked for dataset generation via `update_query`:

| Parameter | Description | Example |
|-----------|-------------|---------|
| `ds_name` | Dataset variable name | `m_dsDocList` |
| `ds_query_i` | INSERT query name | `doc_approve_doc_list_i` |
| `ds_query_u` | UPDATE query name | `doc_approve_doc_list_u` |
| `ds_query_d` | DELETE query name | `doc_approve_doc_list_d` |
| `ds_query_one_s` | Reload query name | `doc_approve_doc_list_one_s` |

After setting DS-fields, `generate_example(query_name, "dataset")` produces complete callback code with OnOpen, Save, Delete, Reload.

---

## Safety: Auto-Snapshots

The following operations automatically create a snapshot before making changes:
- `update_query` — saves current SQL, description, etc.
- `delete_query` — saves full query state before deletion
- `analyze_query(save=true)` — saves current params/fields before overwriting

**Recovery:**
```
list_query_snapshots(query_name="my_query")  — find snapshot ID
restore_query_snapshot(snapshot_id=42)         — restore full state
```

---

## Integration with Other Agents

| Need | Agent | Example |
|------|-------|---------|
| Explore DB schema | `dbmcp` | `get_table_schema`, `search_columns`, `get_table_relations` |
| Save knowledge | `project-memory` | `save_fact`, `save_relationship` |
| Gather context | `pm-context` | Auto-context before tasks |
| Build C++ project | `/build` | Compile after code generation |

### Typical End-to-End Workflow

1. **Explore**: Use `dbmcp` to understand the table (`get_table_schema`)
2. **Generate SQL**: Use `merp-helper:generate_sql` to create CRUD SQL from table
3. **Add query**: Use `merp-helper:add_query` to register it
4. **Analyze**: Use `merp-helper:analyze_query` to extract metadata
5. **Generate code**: Use `merp-helper:generate_query_code` for C++ structs
6. **Generate module**: Use `merp-helper:generate_module_code` for full header
7. **Save knowledge**: Use `project-memory:save_fact` to document what was done
