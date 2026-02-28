# Database MCP — DB Exploration & Query Agent

## Overview

dbmcp is an MCP server for database introspection and ad-hoc SQL execution. It connects to MS SQL Server, PostgreSQL, or Firebird via FireDAC and provides tools for exploring schemas, running queries, analyzing execution plans, profiling data, and comparing query snapshots.

Two instances may be available (configured in MCPHub):
- **dbmcp** (port 8080) — primary database (e.g., DEV)
- **dbmcp_target** (port 8081) — secondary database (e.g., PRODUCTION copy)

## When to Use This Agent

**Use dbmcp when:**
- Exploring database schema (tables, columns, types, indexes, FKs)
- Running ad-hoc SELECT/INSERT/UPDATE/DELETE queries
- Searching for columns or tables by name pattern
- Analyzing execution plans to find missing indexes or slow operations
- Profiling column data (distinct counts, null counts, top values)
- Creating/comparing query snapshots for optimization verification
- Tracing FK paths between tables (join chain discovery)
- Searching SQL object source code (procedures, views, functions, triggers)
- Getting object definitions (stored procedures, views, functions)
- Comparing table schemas side-by-side

**Do NOT use for:**
- Managing SQL queries for code generation (use `merp-helper` instead)
- Generating C++ data access code (use `merp-helper` instead)
- Saving knowledge about business logic (use `project-memory` instead)

## How to Add to a Project

Add to `.mcp.json`:

```json
{
  "mcpServers": {
    "dbmcp": {
      "type": "http",
      "url": "http://localhost:8080/mcp"
    }
  }
}
```

All tools are available as `mcp__dbmcp__<tool_name>`.

---

## Tools Reference (18 tools)

### Schema Exploration

| Tool | Purpose | Key Params |
|------|---------|------------|
| `list_databases` | List all databases on the server | — |
| `list_tables` | List tables with row counts | `schema`, `include_views` |
| `list_objects` | List objects (tables, views, functions, procedures, triggers) | `type`, `schema`, `name_pattern` |
| `get_table_schema` | Full table structure: columns, types, PKs, FKs, indexes, descriptions | `table` |
| `get_table_relations` | All FK relationships (outgoing + incoming) | `table_name` |
| `compare_tables` | Side-by-side schema comparison of two tables | `table1`, `table2` |
| `module_overview` | Summary of multiple tables (columns, row counts, FKs) | `tables` (comma-separated) |
| `trace_fk_path` | Find FK join chains between two tables (BFS) | `from_table`, `to_table`, `max_depth` |

### Data Exploration

| Tool | Purpose | Key Params |
|------|---------|------------|
| `get_table_sample` | Sample rows from a table (TOP N) | `table`, `limit` (max 100) |
| `search_columns` | Search columns across all tables by name pattern | `pattern` (SQL LIKE) |
| `profile_column` | Column profiling: distinct/null counts, min/max, top 10 values | `table`, `column` |

### Query Execution

| Tool | Purpose | Key Params |
|------|---------|------------|
| `execute_query` | Execute SQL (SELECT/INSERT/UPDATE/DELETE) | `sql`, `max_rows` (max 50000) |
| `explain_query` | Get estimated execution plan without running | `sql` |

### SQL Object Introspection

| Tool | Purpose | Key Params |
|------|---------|------------|
| `get_object_definition` | Source code of procedure, function, view, or trigger | `object_name`, `type` |
| `search_object_source` | Grep through all SQL object source code | `pattern` (SQL LIKE) |
| `get_dependencies` | SQL code dependencies (USES / USED_BY) | `object_name`, `direction` |

### Snapshot System

| Tool | Purpose | Key Params |
|------|---------|------------|
| `snapshot_query` | Execute query and save results as named snapshot | `sql`, `name`, `max_rows` |
| `list_snapshots` | List all saved snapshots with metadata | — |
| `compare_snapshots` | Compare two snapshots (row count, checksum, differences) | `name1`, `name2` |
| `delete_snapshot` | Delete a saved snapshot | `name` |

### Statistics

| Tool | Purpose | Key Params |
|------|---------|------------|
| `get_tool_stats` | Usage statistics for MCP tools | `tool_name`, `since` |

---

## Standard Workflows

### Exploring an Unknown Table

```
1. list_tables(schema="dbo")                         — find the table
2. get_table_schema(table="dbo.MyTable")             — columns, types, PKs, FKs, indexes
3. get_table_relations(table_name="dbo.MyTable")     — all FK relationships
4. get_table_sample(table="dbo.MyTable", limit=5)    — see actual data
5. profile_column(table="dbo.MyTable", column="Status") — understand column values
```

### Finding Where Data Lives

```
1. search_columns(pattern="%Email%")                 — find columns by name
2. get_table_schema(table="dbo.Contacts")            — check the matching table
3. execute_query(sql="SELECT TOP 10 Email, ... FROM dbo.Contacts")
```

### Understanding Table Relationships

```
1. get_table_relations(table_name="dbo.Orders")      — direct FK links
2. trace_fk_path(from="dbo.Orders", to="dbo.Products") — discover join chain
3. module_overview(tables="dbo.Orders,dbo.OrderItems,dbo.Products") — summary view
```

### Investigating SQL Objects

```
1. list_objects(type="PROCEDURE", name_pattern="%Order%")  — find procedures
2. get_object_definition(object_name="dbo.sp_CreateOrder") — get source code
3. search_object_source(pattern="%OrderTotal%")            — grep across all objects
4. get_dependencies(object_name="dbo.sp_CreateOrder", direction="USES") — what it references
```

### Optimizing a Query

```
1. explain_query(sql="SELECT ...")                   — get execution plan
   → Analyze: Scan vs Seek, estimated rows, cost, missing indexes

2. snapshot_query(sql="SELECT ...", name="before")   — save baseline results
3. (optimize the query — add indexes, rewrite JOINs, etc.)
4. explain_query(sql="SELECT ... optimized")         — verify plan improved
5. snapshot_query(sql="SELECT ... optimized", name="after") — save new results
6. compare_snapshots(name1="before", name2="after")  — verify data equivalence
7. delete_snapshot(name="before")                    — cleanup
8. delete_snapshot(name="after")
```

### Comparing Two Tables

```
1. compare_tables(table1="dbo.Orders", table2="dbo.OrdersArchive")
   → Common columns, type differences, columns only in one table
```

---

## Execution Plan Analysis

When using `explain_query`, look for these key metrics:

### Operation Types (best to worst)

1. **Clustered Index Seek** — point lookup on clustered index
2. **Index Seek** — point lookup on non-clustered index
3. **Index Scan** — full index scan
4. **Clustered Index Scan** — full table scan
5. **Table Scan** — full heap scan

### Key Metrics

| Metric | Good | Bad |
|--------|------|-----|
| PhysicalOp | Index Seek | Table Scan, Index Scan |
| EstimateRows | Small number | Large number |
| EstimatedTotalSubtreeCost | < 1 | > 10 |
| EstimateIO | Low | High |

### Common Anti-Patterns

- Functions in WHERE (`YEAR(col)`, `UPPER(col)`) prevent index usage
- Implicit conversions (type mismatch between column and parameter)
- Missing indexes (SQL Server will suggest them in the plan)
- Non-SARGable predicates (`LIKE '%value'`, `col1 + col2 = 5`)

---

## Snapshot System Details

Snapshots are stored in the module's local SQLite database. Each snapshot saves:
- Snapshot name (unique identifier)
- SQL query text
- Row count
- Checksum of result data
- Execution time
- Creation timestamp
- Full result data (up to max_rows, default 1000, max 10000)

**Comparison** checks: row count, checksum match, column structure, and row-level differences.

---

## Multi-Database Support

dbmcp supports three database engines via FireDAC:
- **MS SQL Server** — full support including execution plans, object definitions
- **PostgreSQL** — full support
- **Firebird** — full support

The SQL dialect depends on the configured provider. Adjust queries accordingly:
- MSSQL: `TOP N`, `NOLOCK`, `WITH (INDEX(...))`, `sp_describe_first_result_set`
- PostgreSQL: `LIMIT N`, `EXPLAIN ANALYZE`
- Firebird: `FIRST N`, `ROWS N`

---

## Integration with Other Agents

| Need | Agent | Example |
|------|-------|---------|
| Manage SQL queries for C++ code gen | `merp-helper` | `add_query`, `analyze_query`, `generate_query_code` |
| Save knowledge about DB findings | `project-memory` | `save_fact`, `save_relationship` |
| Gather context before a task | `pm-context` | Auto-context agent |
| Build C++ project | `/build` | Compile after changes |

### Typical End-to-End Workflow

1. **Explore**: Use `dbmcp:get_table_schema` to understand the table
2. **Query**: Use `dbmcp:execute_query` to test SQL
3. **Optimize**: Use `dbmcp:explain_query` + snapshot workflow
4. **Register**: Use `merp-helper:add_query` to create a managed query
5. **Analyze**: Use `merp-helper:analyze_query` to extract metadata
6. **Generate**: Use `merp-helper:generate_query_code` for C++ structs
7. **Remember**: Use `project-memory:save_fact` to document findings
