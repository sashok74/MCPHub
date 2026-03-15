# MCPHub

MCPHub is a Windows desktop application that hosts multiple [MCP (Model Context Protocol)](https://modelcontextprotocol.io/) servers in a single process. Each MCP module runs its own HTTP server on a dedicated port, allowing AI assistants like Claude Code to connect to all of them simultaneously.

Built with C++Builder (VCL, Win64).

## Download

Pre-built binaries are available on the [Releases](../../releases) page. Download the latest `MCPHub-vX.X.X-win64.zip`, unpack anywhere, and run `MCPHub.exe`. No installation required.

> **Note:** MCPHub is a standalone EXE with no external DLL dependencies. It requires Windows 10/11 (x64). Database client libraries (e.g., ODBC drivers for SQL Server) must be installed separately if not already present on your system.

## MCP Modules

### Database MCP (`db_mcp`)

Database introspection and ad-hoc SQL execution. Connects to **MS SQL Server**, **PostgreSQL**, or **Firebird** via FireDAC.

**Key capabilities:**
- Schema exploration (tables, columns, indexes, FKs)
- Ad-hoc SQL query execution
- Column search across all tables
- Execution plan analysis
- Column profiling (distinct/null counts, top values)
- FK path tracing between tables
- SQL object introspection (procedures, views, triggers)
- Query snapshot system for optimization verification

You can run multiple instances pointed at different databases (e.g., DEV and PRODUCTION).

### Project Memory (`project_memory`)

SQLite-backed knowledge base that accumulates project knowledge between AI sessions.

**Key capabilities:**
- Entity facts with types (schema, business_rule, gotcha, code_pattern)
- Directed relationships between entities (creates, contains, references)
- Form/class/table mappings
- Business process workflows
- Domain glossary with synonym support
- Verified SQL queries
- Code patterns
- Module grouping and research coverage tracking
- Full-text search with Cyrillic morphology support
- Bulk operations for atomic saves

### SQL Query Helper (`merp_helper`)

SQL query management and C++ code generation from database metadata.

**Key capabilities:**
- Manage SQL queries organized by modules
- Analyze queries to extract parameter metadata
- Generate C++ InParam/OutParam structs
- Mustache-like template engine for custom code generation
- Dataset CRUD callback generation
- Auto-snapshot safety system for query modifications

## Quick Start

1. **Download** `MCPHub-vX.X.X-win64.zip` from [Releases](../../releases) and unpack
2. **Run** `MCPHub.exe`
3. **Add modules** via the UI — click "Add Module", choose the type, configure connection settings
4. **Start modules** — each starts an HTTP server on its configured port
5. **Connect** from your AI assistant via `.mcp.json`

### Building from Source

Requires [RAD Studio](https://www.embarcadero.com/products/rad-studio) (C++Builder) with FireDAC.

```
powershell -File doc\cpp_build\build.ps1 -ProjectPath MCPHub.cbproj -Config Release -Platform Win64x
```

Output: `Win64x\Release\MCPHub.exe`

## Connecting from Claude Code

Add MCP servers to your project's `.mcp.json`:

```json
{
  "mcpServers": {
    "dbmcp": {
      "type": "http",
      "url": "http://localhost:8080/mcp"
    },
    "project-memory": {
      "type": "http",
      "url": "http://localhost:8766/mcp"
    },
    "helper": {
      "type": "http",
      "url": "http://localhost:8090/mcp"
    }
  }
}
```

Adjust ports to match your MCPHub configuration. Each module's port is shown in the Settings tab.

## Configuration

Runtime config is stored in `mcphub.config.json` next to the EXE. See [`mcphub.config.example.json`](mcphub.config.example.json) for a full example. Summary:

```json
{
  "modules": [
    {
      "typeId": "db_mcp",
      "displayName": "My Database",
      "autoStart": true,
      "config": {
        "mcp_port": 8080,
        "provider": "MSSQL",
        "server": "localhost",
        "database": "MyDB",
        "username": "sa",
        "password": "***"
      }
    },
    {
      "typeId": "project_memory",
      "displayName": "Project Memory",
      "autoStart": true,
      "config": {
        "mcp_port": 8766,
        "db_path": "C:\\MyProject\\project_memory.db"
      }
    },
    {
      "typeId": "merp_helper",
      "displayName": "SQL Helper",
      "autoStart": true,
      "config": {
        "mcp_port": 8090,
        "server": "localhost",
        "database": "MyDB",
        "username": "sa",
        "password": "***",
        "local_db_path": "C:\\MyProject\\helper.db",
        "output_dir": "C:\\MyProject\\generated"
      }
    }
  ]
}
```

## Documentation

Each MCP module includes agent and command documentation in its source directory:

- `src/servers/dbMcp/agents/` — DB exploration agent guide
- `src/servers/dbMcp/commands/` — DB query, explain, snapshot commands
- `src/servers/projectMemory/agents/` — Context gathering and knowledge sync agents
- `src/servers/projectMemory/commands/` — Lookup, research, remember, verify commands
- `src/servers/merpHelper/agents/` — Code generation agent guide
- `src/servers/merpHelper/commands/` — Helper and template variables reference

These docs can be copied into a project's `.claude/` directory to provide agents and slash commands.

## Adding New MCP Modules

See `src/servers/NEW_MCP_GUIDE.md` for a step-by-step guide. In short: subclass `McpModuleBase`, implement 3 methods (`OnInitialize`, `OnRegisterTools`, `OnShutdown`) + `GetConfigFields`, register in `RegisterModules.cpp`.

## Creating a Release

To create a distributable package:

1. Build in Release configuration (`-Config Release`)
2. Copy `MCPHub.exe` and `mcphub.config.example.json` into a ZIP
3. Create a GitHub Release with the tag (e.g., `v1.0.0`) and attach the ZIP

The Release build is fully standalone — no BPL/DLL files needed.

## License

This project is proprietary software.
