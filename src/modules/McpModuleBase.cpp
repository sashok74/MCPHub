//---------------------------------------------------------------------------
#pragma hdrstop
#include "McpModuleBase.h"
#include <System.Classes.hpp>
//---------------------------------------------------------------------------
#pragma package(smart_init)

McpModuleBase::McpModuleBase(const std::string& instanceId, const std::string& displayName)
	: FInstanceId(instanceId),
	  FDisplayName(displayName),
	  FState(ModuleState::Stopped),
	  FRequestCount(0),
	  FLastActivityTime(0)
{
}

McpModuleBase::~McpModuleBase()
{
	if (FState == ModuleState::Running)
		Stop();
}

int McpModuleBase::GetPort() const
{
	if (FConfig.contains("mcp_port"))
		return FConfig["mcp_port"].get<int>();
	return 0;
}

void McpModuleBase::SetState(ModuleState state)
{
	FState = state;
	if (FStateChangeCallback)
	{
		auto cb = FStateChangeCallback;
		TThread::Queue(nullptr, [cb, state]() {
			cb(state);
		});
	}
}

void McpModuleBase::Start()
{
	if (FState == ModuleState::Running)
		return;

	try
	{
		FLastError.clear();

		// 1. Create LocalMcpDb if db_path configured
		std::string dbPath;
		if (FConfig.contains("db_path") && FConfig["db_path"].is_string())
			dbPath = FConfig["db_path"].get<std::string>();
		else if (FConfig.contains("local_db_path") && FConfig["local_db_path"].is_string())
			dbPath = FConfig["local_db_path"].get<std::string>();

		if (!dbPath.empty())
		{
			FLocalDb = std::make_unique<LocalMcpDb>();
			FLocalDb->Open(dbPath);
		}

		// 2. Call subclass initialization
		OnInitialize(FLocalDb.get());

		// 3. Create TMcpServer
		FMcpServer = std::make_unique<Mcp::TMcpServer>(GetMcpServerName(), "1.0.0");

		// 4. Register tools
		auto tools = OnRegisterTools();
		FToolNames.clear();
		for (const auto& t : tools)
			FToolNames.push_back(t.Name);
		Mcp::Tools::RegisterTools(*FMcpServer, tools);

		// 5. Wire tool-executed callback for activity tracking
		FMcpServer->SetOnToolExecuted(
			[this](const std::string& toolName, bool success, const std::string& /*errorMsg*/) {
				FRequestCount.fetch_add(1);
				FLastActivityTime.store(std::time(nullptr));
				if (FActivityCallback)
				{
					auto cb = FActivityCallback;
					TThread::Queue(nullptr, [cb, toolName, success]() {
						cb(toolName, success);
					});
				}
			});

		// 6. Set state
		SetState(ModuleState::Running);
	}
	catch (const std::exception& e)
	{
		FLastError = e.what();
		Stop();
		SetState(ModuleState::Error);
	}
	catch (const Exception& e)
	{
		FLastError = AnsiString(e.Message).c_str();
		Stop();
		SetState(ModuleState::Error);
	}
}

void McpModuleBase::Stop()
{
	try
	{
		FMcpServer.reset();

		OnShutdown();

		if (FLocalDb)
		{
			FLocalDb->Close();
			FLocalDb.reset();
		}
		FToolNames.clear();
	}
	catch (...)
	{
		// Swallow errors during shutdown
	}

	if (FState == ModuleState::Running)
		SetState(ModuleState::Stopped);
}

std::string McpModuleBase::HandleJsonRpc(const std::string& requestJson)
{
	if (!FMcpServer)
		return R"({"jsonrpc":"2.0","error":{"code":-32603,"message":"Server not running"},"id":null})";

	std::string response = FMcpServer->HandleRequest(requestJson);

	// Fire log callback
	if (FLogCallback)
	{
		auto cb = FLogCallback;
		auto req = requestJson;
		auto resp = response;
		TThread::Queue(nullptr, [cb, req, resp]() {
			cb(req, resp);
		});
	}

	return response;
}
//---------------------------------------------------------------------------
