# How to Create a New MCP Server in MCPHub

## Overview

MCPHub uses a modular architecture where each MCP server is a **module**. A module is a class that inherits from `McpModuleBase` and provides:
- Configuration fields (port, credentials, paths)
- Initialization logic (open DB, connect to service)
- A list of tools (functions callable via JSON-RPC)
- Cleanup logic

The base class handles everything else: HTTP server, MCP protocol, request routing, logging, activity tracking, thread-safe UI callbacks.

## Architecture

```
IMcpModule           (interface — identity, config, lifecycle, observability, callbacks)
    │
McpModuleBase        (base class — HTTP server, MCP protocol stack, tool registration)
    │
YourModule           (your code — config fields, init, tools, cleanup)
```

### What McpModuleBase does for you (DO NOT reimplement):
- Creates `TIdHTTPServer` on configured port
- Opens `LocalMcpDb` (SQLite) if `db_path` or `local_db_path` is in config
- Creates `TMcpServer` + `TMcpHandler` + `HttpTransport`
- Wires `OnCommandGet` event to transport
- Registers all tools from `OnRegisterTools()`
- Tracks request count, last activity time
- Fires `LogCallback`, `ActivityCallback`, `StateChangeCallback` via `TThread::Queue`
- Handles Start/Stop lifecycle with error recovery

### What you implement (3 methods + config):
- `GetConfigFields()` — what the UI shows for configuration
- `OnInitialize(LocalMcpDb* db)` — your setup logic (called after DB opened, before tools registered)
- `OnRegisterTools()` — return your tool list
- `OnShutdown()` — your cleanup logic

## File Structure

```
src/servers/
  your_server/
    YourModule.h          — module class declaration
    YourModule.cpp         — config fields, init, shutdown
    YourTools.h            — tool definitions (inline ToolList function)
```

## Step-by-Step

### Step 1: Create the directory

```
src/servers/your_server/
```

### Step 2: Create YourTools.h

This is where all tool logic lives. Follow the **data-driven pattern**: tools are structs in a vector, not classes.

```cpp
//---------------------------------------------------------------------------
#ifndef YourToolsH
#define YourToolsH

#include "ToolDefs.h"       // ToolDef, ToolList, RegisterTools
#include "LocalMcpDb.h"     // if you need SQLite
#include <string>

namespace Mcp { namespace Tools {

// ── Helpers (optional, keep local to this file) ──────────────────────

using json = nlohmann::json;

inline std::string GetStr(const json &args, const std::string &key,
    const std::string &def = "")
{
    if (!args.contains(key) || args[key].is_null()) return def;
    if (args[key].is_string()) return args[key].get<std::string>();
    return args[key].dump();
}

inline int GetInt(const json &args, const std::string &key, int def = 0)
{
    if (!args.contains(key) || args[key].is_null()) return def;
    if (args[key].is_number_integer()) return args[key].get<int>();
    if (args[key].is_string()) {
        try { return std::stoi(args[key].get<std::string>()); }
        catch (...) { return def; }
    }
    return def;
}

// ── Tool list ────────────────────────────────────────────────────────

// Pass whatever dependencies your tools need (DB, connection, adapter).
// Tools capture these by pointer in lambdas.
inline ToolList GetYourTools(LocalMcpDb *db /*, OtherDependency *dep */)
{
    return {

        // ── Tool 1 ──────────────────────────────────────────────
        {
            "tool_name",                          // unique name (snake_case)
            "Short description for tools/list",   // shown to LLM
            TMcpToolSchema()
                .AddString("param1", "Description of param1", true)   // required
                .AddInteger("limit", "Max results to return")         // optional
                .AddEnum("mode", "Operating mode", {"fast", "full"})  // optional enum
                .AddBoolean("verbose", "Include extra details"),      // optional bool
            [db](const json &args, TMcpToolContext &) -> TMcpToolResult
            {
                // 1. Extract parameters
                std::string param1 = GetStr(args, "param1");
                int limit = GetInt(args, "limit", 100);

                if (param1.empty())
                    return TMcpToolResult::Error("param1 is required");

                // 2. Do work
                try
                {
                    // Use db->Query(), db->Execute(), or your own dependency
                    auto rows = db->Query(
                        "SELECT * FROM my_table WHERE name = :name LIMIT :lim",
                        {  {":name", param1},
                           {":lim",  std::to_string(limit)}  });

                    // 3. Build response
                    json resp;
                    resp["results"] = rows;
                    resp["count"] = rows.size();

                    return TMcpToolResult::Success(resp);
                }
                catch (const std::exception &e)
                {
                    return TMcpToolResult::Error(e.what());
                }
            }
        },

        // ── Tool 2 ──────────────────────────────────────────────
        {
            "another_tool",
            "Description",
            TMcpToolSchema()
                .AddString("query", "Search query", true),
            [db](const json &args, TMcpToolContext &) -> TMcpToolResult
            {
                std::string query = GetStr(args, "query");
                // ... implementation ...
                return TMcpToolResult::Success(json{{"status", "ok"}});
            }
        }

    }; // end ToolList
}

}} // namespace Mcp::Tools
#endif
```

### Step 3: Create YourModule.h

```cpp
//---------------------------------------------------------------------------
#ifndef YourModuleH
#define YourModuleH

#include "McpModuleBase.h"

class YourModule : public McpModuleBase
{
public:
    YourModule(const std::string &instanceId)
        : McpModuleBase(instanceId, "Your Server Name")
    {}

    std::string GetModuleTypeId() const override { return "your_type_id"; }
    std::vector<ConfigFieldDef> GetConfigFields() const override;

protected:
    void OnInitialize(LocalMcpDb *db) override;
    Mcp::Tools::ToolList OnRegisterTools() override;
    void OnShutdown() override;
    std::string GetMcpServerName() const override { return "your-server"; }

private:
    // Your own members: connections, adapters, state
};

#endif
```

### Step 4: Create YourModule.cpp

```cpp
//---------------------------------------------------------------------------
#pragma hdrstop
#include "YourModule.h"
#include "YourTools.h"
//---------------------------------------------------------------------------
#pragma package(smart_init)

std::vector<ConfigFieldDef> YourModule::GetConfigFields() const
{
    return {
        // id            label          type                     enumValues  default  required
        {"mcp_port",    "MCP Port",    ConfigFieldType::Integer, {},         "8770",  true},
        {"db_path",     "DB Path",     ConfigFieldType::FilePath,{},         "",      true},
        // Add your fields:
        // {"api_key",  "API Key",     ConfigFieldType::Password,{},         "",      true},
        // {"mode",     "Mode",        ConfigFieldType::Enum, {"a","b"},     "a",     true},
    };
}

void YourModule::OnInitialize(LocalMcpDb *db)
{
    // Called AFTER LocalMcpDb opened (if db_path configured).
    // db may be nullptr if no db_path in config.

    // Create your schema:
    if (db)
    {
        db->Exec(
            "CREATE TABLE IF NOT EXISTS your_table ("
            "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  name TEXT NOT NULL"
            ");"
        );
    }

    // Initialize your own resources (connections, adapters):
    // std::string apiKey = FConfig.value("api_key", "");
    // FMyConnection = std::make_unique<MyConnection>(apiKey);
}

Mcp::Tools::ToolList YourModule::OnRegisterTools()
{
    // Pass dependencies to tool factory function.
    // FLocalDb is the base class member (LocalMcpDb*).
    return Mcp::Tools::GetYourTools(FLocalDb.get());
}

void YourModule::OnShutdown()
{
    // Cleanup your resources. Called during Stop().
    // FMyConnection.reset();
}
```

### Step 5: Register the module

Edit `src/modules/RegisterModules.cpp`:

```cpp
#include "RegisterModules.h"
#include "ProjectMemoryModule.h"
#include "DbMcpModule.h"
#include "../servers/your_server/YourModule.h"    // ← add

void RegisterAllModuleTypes(ModuleRegistry &registry)
{
    // existing...
    registry.RegisterType("project_memory", "Project Memory", ...);
    registry.RegisterType("db_mcp", "Database MCP", ...);

    // ← add your module
    registry.RegisterType("your_type_id", "Your Display Name",
        [](const std::string &instanceId) {
            return std::make_unique<YourModule>(instanceId);
        });
}
```

### Step 6: Add to MCPHub.cbproj

Add two entries to the `<ItemGroup>` section:

```xml
<!-- Your module .cpp -->
<CppCompile Include="src\servers\your_server\YourModule.cpp">
    <DependentOn>src\servers\your_server\YourModule.h</DependentOn>
    <BuildOrder>30</BuildOrder>
</CppCompile>
```

Add include path for your directory. In `<IncludePath>`, prepend:

```
src\servers\your_server\;
```

(The tools .h file is header-only — no CppCompile entry needed.)

### Step 7: Build and test

```bash
./doc/cpp_build/build_and_parse.sh ./MCPHub.cbproj
```

After starting in the UI:
```bash
curl -X POST http://localhost:PORT/mcp \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","method":"initialize","id":1}'

curl -X POST http://localhost:PORT/mcp \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","method":"tools/list","id":2}'

curl -X POST http://localhost:PORT/mcp \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","method":"tools/call","id":3,"params":{"name":"tool_name","arguments":{"param1":"value"}}}'
```

---

## Design Patterns Reference

### Pattern 1: Data-Driven Tools (ToolDef)

Tools are **data**, not classes. Each tool is a struct with name, description, schema, and a lambda. No inheritance, no virtual methods.

```cpp
struct ToolDef {
    std::string Name;
    std::string Description;
    TMcpToolSchema Schema;
    std::function<TMcpToolResult(const json&, TMcpToolContext&)> Execute;
};
using ToolList = std::vector<ToolDef>;
```

**Why**: Adding a tool = adding an entry to a vector. No boilerplate.

### Pattern 2: Factory Function per Domain

Each tools file exports one function: `GetXxxTools(dependencies...)` → `ToolList`.

```cpp
inline ToolList GetDbTools(IDbProvider *db);
inline ToolList GetStatsTools(LocalMcpDb *sqlite);
inline ToolList GetSnapshotTools(IDbProvider *db, LocalMcpDb *sqlite);
inline ToolList GetProjectMemoryTools(LocalMcpDb *db);
```

Module's `OnRegisterTools()` calls these and concatenates:

```cpp
ToolList all;
auto a = GetSomeTools(dep1);
auto b = GetOtherTools(dep2);
all.insert(all.end(), a.begin(), a.end());
all.insert(all.end(), b.begin(), b.end());
return all;
```

**Why**: Tools are grouped by domain. Easy to reuse across modules.

### Pattern 3: Fluent Schema Builder

```cpp
TMcpToolSchema()
    .AddString("name", "Entity name", true)          // required string
    .AddString("filter", "Optional filter")           // optional string
    .AddInteger("limit", "Max rows")                  // optional integer
    .AddEnum("type", "Object type", {"table","view"}) // optional enum
    .AddBoolean("include_system", "Include sys objs") // optional bool
```

Parameters: `(name, description, required=false)`. Enum adds values list.

### Pattern 4: Result Convention

```cpp
// Success — return JSON object
return TMcpToolResult::Success(json{
    {"results", rows},
    {"count", rows.size()}
});

// Error — return error message
return TMcpToolResult::Error("Table not found: " + tableName);
```

Always return structured JSON in Success. Always return human-readable string in Error.

### Pattern 5: Dependency Injection via Lambda Capture

Tools capture their dependencies (db, provider, adapter) in the lambda closure:

```cpp
[db, adapter](const json &args, TMcpToolContext &) -> TMcpToolResult
{
    auto rows = db->Query("...");
    // ...
}
```

Dependencies are passed to the factory function, NOT stored as globals.

### Pattern 6: Parameter Extraction

Use helper functions, not raw JSON access:

```cpp
// TMcpToolBase static helpers (from McpServer.h):
std::string val = TMcpToolBase::GetString(args, "key", "default");
int num       = TMcpToolBase::GetInt(args, "key", 0);
bool flag     = TMcpToolBase::GetBool(args, "key", false);

// Or local helpers (same logic, more flexible):
std::string val = GetStr(args, "key", "default");
int num        = GetInt(args, "key", 0);
```

### Pattern 7: LocalMcpDb Usage

```cpp
// Query — returns json array of objects
json rows = db->Query(
    "SELECT * FROM items WHERE category = :cat",
    {{":cat", categoryValue}});

// Execute — returns affected row count
int affected = db->Execute(
    "INSERT INTO items (name, value) VALUES (:name, :val)",
    {{":name", name}, {":val", value}});

// Last insert ID
long long id = db->LastInsertRowId();

// Transactions
db->BeginTransaction();
try {
    db->Execute("...", params1);
    db->Execute("...", params2);
    db->Commit();
} catch (...) {
    db->Rollback();
    throw;
}

// DDL
db->Exec("CREATE TABLE IF NOT EXISTS ...");
```

All parameters are string-typed (`:name` style). Binding is always TEXT.

### Pattern 8: Config Field Types

```cpp
{"id",    "Label",   ConfigFieldType::String,   {},                   "default", required}
{"port",  "Port",    ConfigFieldType::Integer,  {},                   "8080",    true}
{"mode",  "Mode",    ConfigFieldType::Enum,     {"opt1","opt2"},      "opt1",    true}
{"path",  "DB Path", ConfigFieldType::FilePath, {},                   "",        true}
{"pass",  "Password",ConfigFieldType::Password, {},                   "",        false}
```

Access in code: `FConfig.value("id", "default")` or `FConfig["id"].get<std::string>()`.
Integer fields are stored as JSON numbers after UI collection.

### Pattern 9: Module Identity

- `GetModuleTypeId()` → `"your_type_id"` — used in config, registry (snake_case)
- `GetMcpServerName()` → `"your-server"` — returned in MCP `initialize` response (kebab-case)
- Constructor default display name → `"Your Server Name"` — shown in UI, editable by user

---

## Checklist

- [ ] `src/servers/your_server/` directory created
- [ ] `YourTools.h` with `GetYourTools()` returning `ToolList`
- [ ] `YourModule.h` inheriting `McpModuleBase`
- [ ] `YourModule.cpp` with config fields, init, register tools, shutdown
- [ ] `RegisterModules.cpp` updated with new type
- [ ] `MCPHub.cbproj` updated: include path + CppCompile entry
- [ ] Build passes (`/build`)
- [ ] curl `initialize` returns server name
- [ ] curl `tools/list` returns your tools
- [ ] curl `tools/call` executes a tool correctly
