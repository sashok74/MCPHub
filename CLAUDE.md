# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What is MCPHub

MCPHub is a C++Builder VCL desktop application that hosts multiple MCP (Model Context Protocol) servers in a single process. Each MCP module runs its own HTTP server on a dedicated port. Currently ships two module types: **Database MCP** (SQL database introspection via FireDAC) and **Project Memory** (SQLite-backed knowledge base). New module types can be added without touching existing code.

## Build

```bash
/build
```

Or manually:
```bash
powershell -ExecutionPolicy Bypass -File "C:\RADProjects\MCPHub\doc\cpp_build\build.ps1" \
  -ProjectPath "C:\RADProjects\MCPHub\MCPHub.cbproj" -Config Debug -Platform Win64x
python "C:\RADProjects\MCPHub\doc\cpp_build\parse_msbuild_errors.py"
```

Build output: `Win64x\Debug\MCPHub.exe`. Errors parsed into `doc/cpp_build/errors/index.json` and per-file `.err.txt` files.

Required defines: `_DEBUG;SQLITE_ENABLE_FTS5` (Debug), `NDEBUG;SQLITE_ENABLE_FTS5` (Release).

## Architecture

### Module system

```
IMcpModule (interface)
    └── McpModuleBase (HTTP server, MCP protocol, tool registration, lifecycle)
            ├── ProjectMemoryModule
            └── DbMcpModule
```

**To add a new MCP server**, see `src/servers/NEW_MCP_GUIDE.md`. In short: subclass `McpModuleBase`, implement 3 methods (`OnInitialize`, `OnRegisterTools`, `OnShutdown`) + `GetConfigFields`, register in `RegisterModules.cpp`, add to `.cbproj`.

### Key classes

- `IMcpModule` (`src/modules/IMcpModule.h`) — interface defining identity, config, lifecycle, observability, callbacks
- `McpModuleBase` (`src/modules/McpModuleBase.h/.cpp`) — base class that wires up `TIdHTTPServer` → `HttpTransport` → `TMcpHandler` → `TMcpServer`. Subclasses only provide config + tools + init/shutdown
- `ModuleRegistry` (`src/modules/ModuleRegistry.h`) — factory registry, maps typeId → creator function
- `RegisterModules` (`src/modules/RegisterModules.cpp`) — registers all module types at startup
- `HubConfig` (`src/config/HubConfig.h`) — loads/saves `mcphub.config.json` (module list with configs, auto-start flags)
- `ConfigPanelBuilder` (`src/ui/ConfigPanelBuilder.h`) — dynamically builds VCL config panels from `ConfigFieldDef` arrays
- `LocalMcpDb` (`src/modules/LocalMcpDb.h`) — thread-safe SQLite wrapper (adapted copy with fixed include paths)
- `TfrmMain` (`uMain.h/.cpp`) — main form: ListView of modules + PageControl (Settings/Tools/Log tabs)

### Source reuse via include paths

MCPHub does **not** copy source files from DB_MCP or ProjectMemory. Instead, it uses include paths pointing to sibling projects. The only adapted file is `src/modules/LocalMcpDb.h` (fixed paths for json.hpp and sqlite3.h).

Directory junctions provide shared dependencies:
- `external/` → `MCP4APP/external/` (nlohmann/json.hpp, codeUtf8)
- `sqlite3/` → `DB_MCP/sqlite3/` (sqlite3.c/h)

### Tools pattern

Tools are data-driven structs (`ToolDef`) in vectors, not classes. Each domain has a factory function (`GetDbTools()`, `GetProjectMemoryTools()`, etc.) that returns a `ToolList`. Module's `OnRegisterTools()` calls these and concatenates results. Tool lambdas capture dependencies via closure.

```cpp
{"tool_name", "description", TMcpToolSchema().AddString("param", "desc", true),
 [db](const json& args, TMcpToolContext&) -> TMcpToolResult { ... }}
```

## VCL-specific patterns

- Event handlers must be `__closure` methods (member functions of TObject-derived classes), **not** lambdas. Use `THttpEventBridge` pattern when bridging Indy events to non-VCL code.
- Use `TThread::Queue(nullptr, [...]() { ... })` for thread-safe UI updates from MCP request handler threads.
- `u()` from `UcodeUtf8.h` converts `std::string` → `UnicodeString`. For reverse: `AnsiString(unicodeStr).c_str()`.
- `TBrowseHelper` class wraps browse button `OnClick` events for `ConfigFieldType::FilePath` fields.

## Configuration

Runtime config is stored in `mcphub.config.json` next to the EXE. Each module entry has: `instanceId`, `typeId`, `displayName`, `autoStart`, and a `config` object with module-specific fields (port, DB path, credentials, etc.).

Config field types: `String`, `Integer`, `Enum`, `FilePath`, `Password`. Access in code: `FConfig.value("key", "default")`.

## Precompiled header

`MCPHubPCH.h` — includes VCL, FireDAC (MSSQL/FB/PG), Indy HTTP, System.JSON, Data.DB. All `.cpp` files use this PCH.
