//---------------------------------------------------------------------------
// McpServer.h — Universal MCP (Model Context Protocol) Server Framework
//
// A modern, reusable MCP server implementation with data-driven tool
// registration. Pure C++ with nlohmann::json - NO VCL dependencies.
//
// Design Patterns Used:
//   - Strategy: Each tool is a separate execution strategy
//   - Registry: Centralized tool registry with auto-discovery
//   - Template Method: Base class for common tool logic
//   - Fluent schema construction via TMcpToolSchema
//
// Usage:
//   auto server = std::make_unique<TMcpServer>("MyServer", "1.0.0");
//   server->RegisterLambda("ping", "Check if server is alive",
//       TMcpToolSchema(),
//       [](const json&, TMcpToolContext&) {
//           return TMcpToolResult::Success(json{{"status", "ok"}});
//       });
//   std::string response = server->HandleRequest(jsonRpcRequest);
//
// Author: dbMCP Project
// License: MIT
//---------------------------------------------------------------------------

#ifndef McpServerH
#define McpServerH

//---------------------------------------------------------------------------
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>
#include <mutex>
#include <stdexcept>
#include <algorithm>

// nlohmann::json - header-only JSON library
// Path relative to project root (add project root to include path)
#include "nlohmann/json.hpp"

namespace Mcp {

// Alias for convenience
using json = nlohmann::json;

//---------------------------------------------------------------------------
// Forward declarations
//---------------------------------------------------------------------------
class IMcpTool;
class TMcpToolRegistry;
class TMcpServer;

//---------------------------------------------------------------------------
// Error codes (JSON-RPC 2.0 standard)
//---------------------------------------------------------------------------
namespace ErrorCode {
    constexpr int ParseError = -32700;
    constexpr int InvalidRequest = -32600;
    constexpr int MethodNotFound = -32601;
    constexpr int InvalidParams = -32602;
    constexpr int InternalError = -32603;
    // MCP-specific errors
    constexpr int ToolNotFound = -32001;
    constexpr int ToolExecutionError = -32002;
    constexpr int ProviderNotReady = -32003;
}

//---------------------------------------------------------------------------
// TMcpToolAnnotations — Tool behavior hints for clients
//---------------------------------------------------------------------------
struct TMcpToolAnnotations
{
    std::string Title;          // Human-readable title
    bool ReadOnlyHint = true;   // Tool doesn't modify data
    bool DestructiveHint = false; // Tool may delete/modify data
    bool IdempotentHint = true;  // Multiple calls produce same result
    bool OpenWorldHint = false;  // Tool accesses external resources

    json ToJson() const
    {
        json j;
        if (!Title.empty())
            j["title"] = Title;
        j["readOnlyHint"] = ReadOnlyHint;
        j["destructiveHint"] = DestructiveHint;
        j["idempotentHint"] = IdempotentHint;
        j["openWorldHint"] = OpenWorldHint;
        return j;
    }
};

//---------------------------------------------------------------------------
// TMcpSchemaProperty — Single property in JSON Schema
//---------------------------------------------------------------------------
struct TMcpSchemaProperty
{
    std::string Name;
    std::string Type = "string";   // "string", "integer", "boolean", "array", "object"
    std::string Description;
    bool Required = false;
    std::vector<std::string> EnumValues;  // For enum types

    TMcpSchemaProperty() = default;

    TMcpSchemaProperty(const std::string &name, const std::string &type,
        const std::string &desc, bool required = false)
        : Name(name), Type(type), Description(desc), Required(required) {}
};

//---------------------------------------------------------------------------
// TMcpToolSchema — JSON Schema for tool input
//---------------------------------------------------------------------------
class TMcpToolSchema
{
private:
    std::vector<TMcpSchemaProperty> FProperties;
    std::vector<std::string> FRequired;

public:
    // Fluent API for building schema
    TMcpToolSchema& AddString(const std::string &name, const std::string &desc,
        bool required = false)
    {
        FProperties.emplace_back(name, "string", desc, required);
        if (required) FRequired.push_back(name);
        return *this;
    }

    TMcpToolSchema& AddInteger(const std::string &name, const std::string &desc,
        bool required = false)
    {
        FProperties.emplace_back(name, "integer", desc, required);
        if (required) FRequired.push_back(name);
        return *this;
    }

    TMcpToolSchema& AddBoolean(const std::string &name, const std::string &desc,
        bool required = false)
    {
        FProperties.emplace_back(name, "boolean", desc, required);
        if (required) FRequired.push_back(name);
        return *this;
    }

    TMcpToolSchema& AddEnum(const std::string &name, const std::string &desc,
        const std::vector<std::string> &values, bool required = false)
    {
        TMcpSchemaProperty prop(name, "string", desc, required);
        prop.EnumValues = values;
        FProperties.push_back(prop);
        if (required) FRequired.push_back(name);
        return *this;
    }

    json ToJson() const
    {
        json schema;
        schema["type"] = "object";

        json props = json::object();
        for (const auto &p : FProperties)
        {
            json propDef;
            propDef["type"] = p.Type;
            if (!p.Description.empty())
                propDef["description"] = p.Description;
            if (!p.EnumValues.empty())
                propDef["enum"] = p.EnumValues;
            props[p.Name] = propDef;
        }
        schema["properties"] = props;

        if (!FRequired.empty())
            schema["required"] = FRequired;

        return schema;
    }

    
};

//---------------------------------------------------------------------------
// TMcpToolContext — Execution context passed to tools
//---------------------------------------------------------------------------
class TMcpToolContext
{
public:
    TMcpToolContext() = default;
};

//---------------------------------------------------------------------------
// TMcpToolResult — Result from tool execution
//---------------------------------------------------------------------------
struct TMcpToolResult
{
    json Content;           // JSON result
    bool IsError = false;   // true if execution failed
    std::string ErrorMessage;

    static TMcpToolResult Success(const json &content)
    {
        TMcpToolResult r;
        r.Content = content;
        r.IsError = false;
        return r;
    }

    static TMcpToolResult Success(const std::string &jsonString)
    {
        TMcpToolResult r;
        try {
            r.Content = json::parse(jsonString);
        } catch (...) {
            // If not valid JSON, wrap as string
            r.Content = jsonString;
        }
        r.IsError = false;
        return r;
    }

    static TMcpToolResult Error(const std::string &message)
    {
        TMcpToolResult r;
        r.Content = json{{"error", message}};
        r.IsError = true;
        r.ErrorMessage = message;
        return r;
    }
};

//---------------------------------------------------------------------------
// IMcpTool — Abstract interface for MCP tools
//
// Implement this interface to create custom tools. Tools are registered
// with TMcpServer and automatically included in tools/list responses.
//---------------------------------------------------------------------------
class IMcpTool
{
public:
    virtual ~IMcpTool() = default;

    /// Unique tool name (e.g., "execute_query", "list_tables")
    virtual std::string GetName() const = 0;

    /// Human-readable description for LLM
    virtual std::string GetDescription() const = 0;

    /// JSON Schema for input parameters
    virtual TMcpToolSchema GetInputSchema() const = 0;

    /// Optional: behavior hints for clients
    virtual TMcpToolAnnotations GetAnnotations() const
    {
        TMcpToolAnnotations ann;
        ann.Title = GetName();
        return ann;
    }

    /// Execute the tool with given arguments
    /// @param args JSON object with input parameters
    /// @param context Execution context
    /// @return TMcpToolResult with content or error
    virtual TMcpToolResult Execute(const json &args, TMcpToolContext &context) = 0;

    /// Generate full JSON for tools/list response
    json ToToolJson() const
    {
        json tool;
        tool["name"] = GetName();
        tool["description"] = GetDescription();
        tool["inputSchema"] = GetInputSchema().ToJson();
        tool["annotations"] = GetAnnotations().ToJson();
        return tool;
    }
};

//---------------------------------------------------------------------------
// TMcpToolBase — Base class for tools with common functionality
//
// Provides helper methods for argument extraction and validation.
// Inherit from this instead of IMcpTool for convenience.
//---------------------------------------------------------------------------
class TMcpToolBase : public IMcpTool
{
public:
    // Static helper methods for argument extraction — public for use in lambdas

    /// Get string value from args, or default if missing
    static std::string GetString(const json &args, const std::string &key,
        const std::string &defaultValue = "")
    {
        if (args.is_null() || !args.contains(key))
            return defaultValue;
        const auto &val = args[key];
        if (val.is_null())
            return defaultValue;
        if (val.is_string())
            return val.get<std::string>();
        return val.dump();  // Convert non-string to string
    }

    /// Get integer value from args
    static int GetInt(const json &args, const std::string &key, int defaultValue = 0)
    {
        if (args.is_null() || !args.contains(key))
            return defaultValue;
        const auto &val = args[key];
        if (val.is_null())
            return defaultValue;
        if (val.is_number_integer())
            return val.get<int>();
        if (val.is_string()) {
            try { return std::stoi(val.get<std::string>()); }
            catch (...) { return defaultValue; }
        }
        return defaultValue;
    }

    /// Get boolean value from args
    static bool GetBool(const json &args, const std::string &key, bool defaultValue = false)
    {
        if (args.is_null() || !args.contains(key))
            return defaultValue;
        const auto &val = args[key];
        if (val.is_null())
            return defaultValue;
        if (val.is_boolean())
            return val.get<bool>();
        if (val.is_string()) {
            std::string s = val.get<std::string>();
            std::transform(s.begin(), s.end(), s.begin(), ::tolower);
            return (s == "true" || s == "1" || s == "yes");
        }
        return defaultValue;
    }

    
};

//---------------------------------------------------------------------------
// TMcpLambdaTool — Tool created from lambda/function
//
// Allows creating tools without subclassing:
//   auto tool = std::make_unique<TMcpLambdaTool>(
//       "ping", "Check if server is alive",
//       TMcpToolSchema(),
//       [](const json&, TMcpToolContext&) {
//           return TMcpToolResult::Success(json{{"status", "ok"}});
//       }
//   );
//---------------------------------------------------------------------------
class TMcpLambdaTool : public TMcpToolBase
{
public:
    using ExecuteFunc = std::function<TMcpToolResult(const json&, TMcpToolContext&)>;

private:
    std::string FName;
    std::string FDescription;
    TMcpToolSchema FSchema;
    TMcpToolAnnotations FAnnotations;
    ExecuteFunc FExecute;

public:
    TMcpLambdaTool(const std::string &name, const std::string &description,
        const TMcpToolSchema &schema, ExecuteFunc executeFunc)
        : FName(name)
        , FDescription(description)
        , FSchema(schema)
        , FExecute(std::move(executeFunc))
    {
        FAnnotations.Title = name;
    }

    TMcpLambdaTool& WithAnnotations(const TMcpToolAnnotations &ann)
    {
        FAnnotations = ann;
        return *this;
    }

    std::string GetName() const override { return FName; }
    std::string GetDescription() const override { return FDescription; }
    TMcpToolSchema GetInputSchema() const override { return FSchema; }
    TMcpToolAnnotations GetAnnotations() const override { return FAnnotations; }

    TMcpToolResult Execute(const json &args, TMcpToolContext &context) override
    {
        if (FExecute)
            return FExecute(args, context);
        return TMcpToolResult::Error("No execute function defined");
    }
};

//---------------------------------------------------------------------------
// TMcpToolRegistry — Central registry for MCP tools
//---------------------------------------------------------------------------
class TMcpToolRegistry
{
private:
    std::map<std::string, std::unique_ptr<IMcpTool>> FTools;
    mutable std::mutex FMutex;

public:
    TMcpToolRegistry() = default;

    // Non-copyable
    TMcpToolRegistry(const TMcpToolRegistry&) = delete;
    TMcpToolRegistry& operator=(const TMcpToolRegistry&) = delete;

    /// Register a tool (takes ownership)
    void Register(std::unique_ptr<IMcpTool> tool)
    {
        if (!tool) return;
        std::lock_guard<std::mutex> lock(FMutex);
        std::string name = tool->GetName();
        FTools[name] = std::move(tool);
    }

    /// Register a lambda tool (convenience method)
    void RegisterLambda(const std::string &name, const std::string &description,
        const TMcpToolSchema &schema, TMcpLambdaTool::ExecuteFunc func)
    {
        Register(std::make_unique<TMcpLambdaTool>(name, description, schema, std::move(func)));
    }

    /// Get tool by name (returns nullptr if not found)
    IMcpTool* Get(const std::string &name) const
    {
        std::lock_guard<std::mutex> lock(FMutex);
        auto it = FTools.find(name);
        return (it != FTools.end()) ? it->second.get() : nullptr;
    }


    /// Generate JSON for tools/list response
    json GenerateToolsListJson() const
    {
        std::lock_guard<std::mutex> lock(FMutex);
        json toolsArray = json::array();
        for (const auto &pair : FTools)
            toolsArray.push_back(pair.second->ToToolJson());
        return json{{"tools", toolsArray}};
    }

    
};

//---------------------------------------------------------------------------
// TMcpServerInfo — Server identification
//---------------------------------------------------------------------------
struct TMcpServerInfo
{
    std::string Name = "McpServer";
    std::string Version = "1.0.0";

    TMcpServerInfo() = default;
    TMcpServerInfo(const std::string &name, const std::string &version)
        : Name(name), Version(version) {}

    json ToJson() const
    {
        json j;
        j["name"] = Name;
        j["version"] = Version;
        return j;
    }
};

//---------------------------------------------------------------------------
// TMcpServerCapabilities — Server capabilities
//---------------------------------------------------------------------------
struct TMcpServerCapabilities
{
    bool ToolsListChanged = false;   // Server can notify about tool list changes

    json ToJson() const
    {
        json j;
        j["tools"] = json{{"listChanged", ToolsListChanged}};
        return j;
    }
};

//---------------------------------------------------------------------------
// Event handlers (callbacks)
//---------------------------------------------------------------------------
using TOnToolExecuted = std::function<void(const std::string &toolName, bool success,
    const std::string &errorMessage)>;
using TOnRequestReceived = std::function<void(const std::string &method,
    const std::string &requestJson)>;
using TOnResponseSent = std::function<void(const std::string &responseJson)>;

//---------------------------------------------------------------------------
// TMcpServer — Main MCP server class
//
// Handles JSON-RPC protocol, tool registry, and request routing.
// Independent of transport layer (HTTP, WebSocket, stdio, etc.)
//---------------------------------------------------------------------------
class TMcpServer
{
private:
    TMcpServerInfo FServerInfo;
    TMcpServerCapabilities FCapabilities;
    std::string FProtocolVersion = "2024-11-05";
    std::unique_ptr<TMcpToolRegistry> FToolRegistry;
    TMcpToolContext FContext;

    // Event handlers
    TOnToolExecuted FOnToolExecuted;
    TOnRequestReceived FOnRequestReceived;
    TOnResponseSent FOnResponseSent;

public:
    //-----------------------------------------------------------------------
    // Construction / Destruction
    //-----------------------------------------------------------------------

    explicit TMcpServer(const std::string &name = "McpServer",
        const std::string &version = "1.0.0")
        : FServerInfo(name, version)
        , FToolRegistry(std::make_unique<TMcpToolRegistry>())
    {}

    // Non-copyable
    TMcpServer(const TMcpServer&) = delete;
    TMcpServer& operator=(const TMcpServer&) = delete;

    // Movable
    TMcpServer(TMcpServer&&) = default;
    TMcpServer& operator=(TMcpServer&&) = default;

    //-----------------------------------------------------------------------
    // Configuration
    //-----------------------------------------------------------------------

    

    //-----------------------------------------------------------------------
    // Tool Registration
    //-----------------------------------------------------------------------

    /// Register a lambda tool
    void RegisterLambda(const std::string &name, const std::string &description,
        const TMcpToolSchema &schema, TMcpLambdaTool::ExecuteFunc func)
    {
        FToolRegistry->RegisterLambda(name, description, schema, std::move(func));
    }

    //-----------------------------------------------------------------------
    // Event Handlers
    //-----------------------------------------------------------------------

    void SetOnToolExecuted(TOnToolExecuted handler) { FOnToolExecuted = std::move(handler); }
    void SetOnRequestReceived(TOnRequestReceived handler) { FOnRequestReceived = std::move(handler); }
    void SetOnResponseSent(TOnResponseSent handler) { FOnResponseSent = std::move(handler); }

    //-----------------------------------------------------------------------
    // Request Handling (Main Entry Point)
    //-----------------------------------------------------------------------

    /// Handle a JSON-RPC request and return response
    /// @param requestJson JSON-RPC request string
    /// @return JSON-RPC response string (empty for notifications)
    std::string HandleRequest(const std::string &requestJson)
    {
        try
        {
            json root = json::parse(requestJson);
            if (root.is_array())
                return EmitResponse(HandleBatchRequestInternal(root));
            return EmitResponse(HandleRequestInternal(root, requestJson));
        }
        catch (const json::parse_error &e)
        {
            if (FOnRequestReceived)
                FOnRequestReceived("parse_error", requestJson);
            return EmitResponse(MakeError("null", ErrorCode::ParseError,
                std::string("Parse error: ") + e.what()));
        }
        catch (const std::exception &e)
        {
            return EmitResponse(MakeError("null", ErrorCode::InternalError,
                std::string("Internal error: ") + e.what()));
        }
        catch (...)
        {
            return EmitResponse(MakeError("null", ErrorCode::InternalError,
                "Unknown internal error"));
        }
    }

    /// Handle batch request (array of JSON-RPC requests)
    std::string HandleBatchRequest(const std::string &requestJson)
    {
        try
        {
            json val = json::parse(requestJson);

            if (!val.is_array())
                return HandleRequest(requestJson);

            return EmitResponse(HandleBatchRequestInternal(val));
        }
        catch (const json::parse_error &e)
        {
            if (FOnRequestReceived)
                FOnRequestReceived("parse_error", requestJson);
            return EmitResponse(MakeError("null", ErrorCode::ParseError,
                std::string("Parse error: ") + e.what()));
        }
    }

private:
    //-----------------------------------------------------------------------
    // Internal helpers
    //-----------------------------------------------------------------------

    std::string EmitResponse(const std::string &responseJson)
    {
        if (!responseJson.empty() && FOnResponseSent)
            FOnResponseSent(responseJson);
        return responseJson;
    }

    std::string HandleBatchRequestInternal(const json &batch)
    {
        if (!batch.is_array())
            return MakeError("null", ErrorCode::InvalidRequest, "Invalid JSON-RPC batch");

        json responses = json::array();
        for (const auto &req : batch)
        {
            std::string resp = HandleRequestInternal(req, req.dump());
            if (!resp.empty())
                responses.push_back(json::parse(resp));
        }

        if (responses.empty())
            return "";
        return responses.dump();
    }

    std::string HandleRequestInternal(const json &reqJson, const std::string &rawJson)
    {
        if (!reqJson.is_object())
        {
            if (FOnRequestReceived)
                FOnRequestReceived("invalid_request", rawJson);
            return MakeError("null", ErrorCode::InvalidRequest, "Invalid JSON-RPC request");
        }

        std::string id;
        bool isNotification = true;
        if (reqJson.contains("id"))
        {
            const json &idVal = reqJson["id"];
            if (idVal.is_null())
            {
                id = "null";
            }
            else if (idVal.is_string() || idVal.is_number())
            {
                id = idVal.dump();
                isNotification = false;
            }
            else
            {
                if (FOnRequestReceived)
                    FOnRequestReceived("invalid_request", rawJson);
                return MakeError("null", ErrorCode::InvalidRequest,
                    "Invalid 'id' type");
            }
        }

        if (reqJson.contains("jsonrpc"))
        {
            if (!reqJson["jsonrpc"].is_string() ||
                reqJson["jsonrpc"].get<std::string>() != "2.0")
            {
                if (FOnRequestReceived)
                    FOnRequestReceived("invalid_request", rawJson);
                return MakeError(id, ErrorCode::InvalidRequest,
                    "Invalid 'jsonrpc' version");
            }
        }

        if (!reqJson.contains("method") || !reqJson["method"].is_string())
        {
            if (FOnRequestReceived)
                FOnRequestReceived("invalid_request", rawJson);
            return MakeError(id, ErrorCode::InvalidRequest, "Missing 'method'");
        }

        std::string method = reqJson["method"].get<std::string>();
        if (FOnRequestReceived)
            FOnRequestReceived(method, rawJson);

        std::string response;
        if (method == "initialize")
            response = HandleInitialize(reqJson, id);
        else if (method == "tools/list")
            response = HandleToolsList(reqJson, id);
        else if (method == "tools/call")
            response = HandleToolsCall(reqJson, id);
        else if (method == "ping")
            response = HandlePing(id);
        else
            response = MakeError(id, ErrorCode::MethodNotFound, "Unknown method: " + method);

        if (isNotification)
            return "";
        return response;
    }

    //-----------------------------------------------------------------------
    // Protocol Handlers
    //-----------------------------------------------------------------------

    std::string HandleInitialize(const json &reqJson, const std::string &id)
    {
        json result;
        result["protocolVersion"] = FProtocolVersion;
        result["capabilities"] = FCapabilities.ToJson();
        result["serverInfo"] = FServerInfo.ToJson();
        return MakeResponse(id, result);
    }

    std::string HandleToolsList(const json &reqJson, const std::string &id)
    {
        return MakeResponse(id, FToolRegistry->GenerateToolsListJson());
    }

    std::string HandleToolsCall(const json &reqJson, const std::string &id)
    {
        // Extract params
        if (!reqJson.contains("params") || reqJson["params"].is_null())
            return MakeError(id, ErrorCode::InvalidParams, "Missing 'params'");

        const json &params = reqJson["params"];
        if (!params.is_object())
            return MakeError(id, ErrorCode::InvalidParams, "Invalid 'params'");

        // Extract tool name
        if (!params.contains("name") || !params["name"].is_string())
            return MakeError(id, ErrorCode::InvalidParams, "Missing 'params.name'");

        std::string toolName = params["name"].get<std::string>();

        // Get arguments (may be null or missing)
        json args = json::object();
        if (params.contains("arguments") && !params["arguments"].is_null())
            args = params["arguments"];

        // Find and execute tool
        IMcpTool *tool = FToolRegistry->Get(toolName);
        if (!tool)
        {
            if (FOnToolExecuted)
                FOnToolExecuted(toolName, false, "Tool not found");
            return MakeError(id, ErrorCode::ToolNotFound, "Unknown tool: " + toolName);
        }

        TMcpToolResult result;
        try
        {
            result = tool->Execute(args, FContext);
        }
        catch (const std::exception &e)
        {
            result = TMcpToolResult::Error(std::string("Tool execution failed: ") + e.what());
        }

        // Fire event
        if (FOnToolExecuted)
            FOnToolExecuted(toolName, !result.IsError, result.ErrorMessage);

        // Build MCP tool response
        json mcpResult = BuildToolResponse(result);
        return MakeResponse(id, mcpResult);
    }

    std::string HandlePing(const std::string &id)
    {
        return MakeResponse(id, json{{"status", "ok"}});
    }

    //-----------------------------------------------------------------------
    // Response Builders
    //-----------------------------------------------------------------------

    static json BuildToolResponse(const TMcpToolResult &result)
    {
        // Convert content to string for MCP format
        std::string contentStr;
        if (result.Content.is_string())
            contentStr = result.Content.get<std::string>();
        else
            contentStr = result.Content.dump();

        json response;
        response["content"] = json::array({
            json{{"type", "text"}, {"text", contentStr}}
        });

        if (result.IsError)
            response["isError"] = true;

        return response;
    }

    static std::string MakeResponse(const std::string &id, const json &result)
    {
        json response;
        response["jsonrpc"] = "2.0";
        response["id"] = json::parse(id.empty() ? "null" : id);
        response["result"] = result;
        return response.dump();
    }

    static std::string MakeError(const std::string &id, int code, const std::string &message)
    {
        json response;
        response["jsonrpc"] = "2.0";
        response["id"] = json::parse(id.empty() ? "null" : id);
        response["error"] = json{{"code", code}, {"message", message}};
        return response.dump();
    }
};

} // namespace Mcp

//---------------------------------------------------------------------------
#endif // McpServerH
