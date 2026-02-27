# Project Memory MCP Usage

Use the `project-memory` MCP server to accumulate and retrieve knowledge about the project codebase between Claude Code sessions.

## When to Use Project Memory

**ALWAYS use project-memory MCP tools when:**
- Learning about new modules, tables, or business logic
- Discovering form-to-table-to-class mappings
- Understanding business processes or workflows
- Finding glossary terms or domain terminology
- Saving verified SQL queries for reuse
- Documenting code patterns and architectural decisions

**Workflow:**
1. When exploring new code areas, save facts as you learn
2. When finding forms/dialogs, save form mappings
3. When understanding business logic, save processes
4. When encountering domain terms, save to glossary
5. When writing complex queries, save verified queries
6. When identifying patterns, save code patterns

## Available Tools

### Facts (Entity Knowledge)
- `save_fact` - Save knowledge about entities (tables, classes, modules)
  - Types: schema, business_rule, gotcha, code_pattern, statistics, concrete_data, relationship
  - Always include: entity, fact_type, description
  - Optional: evidence (file path, line number), confidence (verified/unverified/uncertain), task_context
- `update_fact` - Update an existing fact by ID. Only provided fields are updated.
  - Required: id (integer)
  - Optional: entity, fact_type, description, evidence, confidence, task_context
  - Use when: correcting inaccurate facts, upgrading confidence, adding evidence
- `delete_fact` - Delete a fact by ID
  - Required: id (integer)
  - Use when: removing outdated or duplicate facts
- `get_facts` - Retrieve facts by entity name and optional type filter
- `search_facts` - Full-text search across all facts (supports Cyrillic with morphology)
  - Required: query
  - Optional: entity_filter (exact match), fact_type_filter (exact match), limit (default 50)
- `list_entities` - List all unique entities with fact counts, types, and last update
  - Optional: fact_type (filter entities by fact type)
  - Returns: entities[], total_entities, total_facts

### Relationships (Directed Entity Links)
- `save_relationship` - Save a directional relationship between two entities. UPSERT by (entity_from, entity_to, rel_type).
  - Required: entity_from, entity_to, rel_type
  - Optional: description, evidence
  - rel_type values: creates, contains, references, depends_on, produces, consumes
- `get_relationships` - Get relationships for an entity
  - Required: entity
  - Optional: direction (outgoing|incoming|both, default: both), rel_type (filter)
  - Returns: relationships[] with direction indicator

### Form Mappings (Template -> Class -> Table)
- `save_form_mapping` - Map template names to classes, files, and database tables
  - Required: template name
  - Optional: class_name, file_path, main_table, sql_file, operations (JSON array)
- `find_form` - Search forms by template, table, class_name, or general query

### Business Processes
- `save_process` - Document multi-step business workflows
  - Required: name, steps (JSON array)
  - Optional: description, synonyms (JSON array), related_entities (JSON array)
- `find_process` - Full-text search across processes (name, description, synonyms, steps)

### Glossary (Domain Terms)
- `save_glossary` - Define project-specific terminology
  - Required: term
  - Optional: synonyms (JSON array), entity, menu_path, context
- `get_glossary` - Look up term (exact match first, then FTS fallback)

### Verified Queries (SQL Knowledge)
- `save_query` - Save tested SQL queries with documentation
  - Required: purpose, sql_text
  - Optional: result_summary, notes
- `get_query` - Search queries by purpose (FTS)

### Code Patterns (Architecture)
- `save_pattern` - Document reusable code structures
  - Required: pattern_name, description
  - Optional: structure (JSON/text), files (JSON array), evidence
- `get_pattern` - Find pattern by exact name or FTS query

### Modules (Entity Grouping)
- `save_module` - Assign entities to a functional module. UPSERT (safe to call repeatedly).
  - Required: module_name (string), entities (JSON array of entity names)
- `get_module` - Get all entities in a module
  - Required: module_name
- `list_modules` - List all modules with entity counts

### Entity Status (Research Coverage)
- `set_entity_status` - Set the research coverage level for an entity. UPSERT by entity name.
  - Required: entity, coverage (unknown|schema_only|partial|full|needs_review)
  - Optional: notes (what is/isn't covered)
- `get_entity_status` - Get coverage status for one or all entities
  - Optional: entity (if omitted, returns all statuses with fact counts)

### Bulk Operations
- `bulk_save` - Save multiple items in a single atomic transaction
  - Optional: facts (array), glossary (array), form_mappings (array), relationships (array)
  - At least one array must be non-empty
  - Returns: saved counts per type, total count

### Business Search
- `ask_business` - Multi-table search across glossary + processes + facts
  - Use for general questions about domain or business logic

## Usage Examples

```javascript
// After exploring a table
mcp__project-memory__save_fact({
  entity: "Orders",
  fact_type: "schema",
  description: "Table stores purchase orders with OrderNumber, DocDate, Status fields",
  evidence: "dbmcp:get_table_schema(Orders)",
  confidence: "verified"
})

// After finding a form
mcp__project-memory__save_form_mapping({
  template: "PurchaseOrder",
  class_name: "CPurchaseOrderDlg",
  file_path: "PurchaseOrderDlg.cpp",
  main_table: "Orders",
  sql_file: "OrderSQL.h",
  operations: JSON.stringify(["add", "edit", "delete", "view"])
})

// After understanding a workflow
mcp__project-memory__save_process({
  name: "Purchase order processing",
  description: "Creating and approving a purchase order",
  steps: JSON.stringify([
    "1. Create document (Orders)",
    "2. Add line items (OrderLines)",
    "3. Calculate totals",
    "4. Manager approval",
    "5. Send to supplier"
  ]),
  related_entities: JSON.stringify(["Orders", "OrderLines", "Products", "Suppliers"])
})

// Save directed relationship
mcp__project-memory__save_relationship({
  entity_from: "Orders",
  entity_to: "OrderLines",
  rel_type: "contains",
  description: "Order header contains line items",
  evidence: "FK: OrderLines.OrderID -> Orders.OrderID"
})

// Bulk save (atomic transaction)
mcp__project-memory__bulk_save({
  facts: [
    {entity: "NewTable", fact_type: "schema", description: "...", confidence: "verified"},
    {entity: "NewTable", fact_type: "business_rule", description: "..."}
  ],
  glossary: [{term: "Domain term", entity: "NewTable", context: "..."}],
  relationships: [{entity_from: "NewTable", entity_to: "OtherTable", rel_type: "references"}]
})

// Search and retrieve
mcp__project-memory__search_facts({query: "order processing", entity_filter: "Orders", limit: 10})
mcp__project-memory__ask_business({question: "how does order approval work"})
```

## Best Practices

1. **Save early and often** - Don't wait until you fully understand something
2. **Use descriptive entity names** - Use table names, class names, or module names
3. **Include evidence** - Always provide file paths and line numbers when possible
4. **Mark confidence** - Use "verified" for code you've read, "unverified" for assumptions
5. **Save relationships** - Use `save_relationship` for directed links (A creates B, A references B)
6. **Group into modules** - Use `save_module` to organize entities by functional area
7. **Track coverage** - Use `set_entity_status` after research (schema_only -> partial -> full)
8. **Fix, don't duplicate** - Use `update_fact` to correct errors, `delete_fact` to remove outdated
9. **Use filters** - `search_facts(query, entity_filter, fact_type_filter, limit)` for precise results
10. **Use bulk_save** - Save 3+ items atomically after investigation
11. **Update glossary** - Add domain terms as you encounter them in code or discussions
12. **Check gaps** - `list_entities()` + `get_entity_status()` to see what needs more research

## Integration with dbmcp

Use both MCP servers together:
- **project-memory** - For accumulating knowledge over time
- **dbmcp** - For querying database structure and data

Typical workflow:
1. Use `dbmcp` to explore database (list_tables, get_table_schema, search_columns)
2. Use `project-memory` to save what you learn (save_fact, save_relationship, save_form_mapping)
3. Mark entity as researched: `set_entity_status(entity, "full")`
4. Group into module: `save_module("ModuleName", [entities])`
5. Later, retrieve knowledge from `project-memory` instead of re-querying database
6. Check coverage gaps with `list_entities()` and `get_entity_status()`
