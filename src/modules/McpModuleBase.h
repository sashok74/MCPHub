//---------------------------------------------------------------------------
// McpModuleBase.h — Base implementation for all MCP modules
//---------------------------------------------------------------------------

#ifndef McpModuleBaseH
#define McpModuleBaseH

#include "IMcpModule.h"
#include "LocalMcpDb.h"
#include "McpServer.h"
#include "McpHandler.h"
#include "HttpTransport.h"
#include "ToolDefs.h"
#include <IdHTTPServer.hpp>
#include <memory>
#include <atomic>

//---------------------------------------------------------------------------
// Helper: bridges Indy OnCommandGet event to HttpTransport
//---------------------------------------------------------------------------
class THttpEventBridge : public TObject
{
public:
	Mcp::Transport::HttpTransport* FTransport;

	THttpEventBridge() : FTransport(nullptr) {}

	void __fastcall OnCommandGet(TIdContext *AContext,
		TIdHTTPRequestInfo *ARequestInfo,
		TIdHTTPResponseInfo *AResponseInfo)
	{
		if (FTransport)
			FTransport->HandleCommandGet(AContext, ARequestInfo, AResponseInfo);
	}
};

//---------------------------------------------------------------------------

class McpModuleBase : public IMcpModule
{
public:
	McpModuleBase(const std::string& instanceId, const std::string& displayName);
	virtual ~McpModuleBase();

	// --- IMcpModule: Identity ---
	std::string GetDisplayName() const override { return FDisplayName; }
	std::string GetInstanceId() const override { return FInstanceId; }
	void SetDisplayName(const std::string& name) override { FDisplayName = name; }

	// --- IMcpModule: Configuration ---
	nlohmann::json GetConfig() const override { return FConfig; }
	void SetConfig(const nlohmann::json& config) override { FConfig = config; }

	// --- IMcpModule: Lifecycle ---
	void Start() override;
	void Stop() override;
	ModuleState GetState() const override { return FState; }
	std::string GetLastError() const override { return FLastError; }

	// --- IMcpModule: Observability ---
	int GetPort() const override;
	int GetToolCount() const override { return (int)FToolNames.size(); }
	int GetRequestCount() const override { return FRequestCount.load(); }
	std::vector<std::string> GetToolNames() const override { return FToolNames; }
	time_t GetLastActivityTime() const override { return FLastActivityTime.load(); }

	// --- IMcpModule: Callbacks ---
	void SetLogCallback(LogCallback cb) override { FLogCallback = std::move(cb); }
	void SetStateChangeCallback(StateChangeCallback cb) override { FStateChangeCallback = std::move(cb); }
	void SetActivityCallback(ActivityCallback cb) override { FActivityCallback = std::move(cb); }

protected:
	// --- Subclass hooks ---
	virtual void OnInitialize(LocalMcpDb* db) = 0;
	virtual Mcp::Tools::ToolList OnRegisterTools() = 0;
	virtual void OnShutdown() = 0;
	virtual std::string GetMcpServerName() const { return GetDisplayName(); }

	// MCP stack
	std::unique_ptr<Mcp::TMcpServer> FMcpServer;
	std::unique_ptr<Mcp::TMcpHandler> FMcpHandler;
	std::unique_ptr<Mcp::Transport::HttpTransport> FHttpTransport;
	TIdHTTPServer* FHttpServer;
	std::unique_ptr<LocalMcpDb> FLocalDb;

	// Identity & config
	std::string FInstanceId;
	std::string FDisplayName;
	nlohmann::json FConfig;

	// State
	ModuleState FState;
	std::string FLastError;
	std::atomic<int> FRequestCount;
	std::atomic<time_t> FLastActivityTime;
	std::vector<std::string> FToolNames;

	// Callbacks
	LogCallback FLogCallback;
	StateChangeCallback FStateChangeCallback;
	ActivityCallback FActivityCallback;

private:
	void SetState(ModuleState state);
	THttpEventBridge* FEventBridge;
};

//---------------------------------------------------------------------------
#endif
