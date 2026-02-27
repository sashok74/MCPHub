//---------------------------------------------------------------------------
#pragma hdrstop
#include "McpModuleBase.h"
#include "TransportTypes.h"
#include <System.Classes.hpp>
//---------------------------------------------------------------------------
#pragma package(smart_init)

McpModuleBase::McpModuleBase(const std::string& instanceId, const std::string& displayName)
	: FHttpServer(nullptr),
	  FInstanceId(instanceId),
	  FDisplayName(displayName),
	  FState(ModuleState::Stopped),
	  FRequestCount(0),
	  FLastActivityTime(0),
	  FEventBridge(nullptr)
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

		// 1. Create TIdHTTPServer
		FHttpServer = new TIdHTTPServer(nullptr);
		int port = GetPort();
		if (port <= 0)
			throw std::runtime_error("Port not configured");
		FHttpServer->DefaultPort = port;

		// 2. Create LocalMcpDb if db_path configured
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

		// 3. Call subclass initialization
		OnInitialize(FLocalDb.get());

		// 4. Create TMcpServer
		FMcpServer = std::make_unique<Mcp::TMcpServer>(GetMcpServerName(), "1.0.0");

		// 5. Register tools
		auto tools = OnRegisterTools();
		FToolNames.clear();
		for (const auto& t : tools)
			FToolNames.push_back(t.Name);
		Mcp::Tools::RegisterTools(*FMcpServer, tools);

		// 6. Create TMcpHandler and wire callbacks
		FMcpHandler = std::make_unique<Mcp::TMcpHandler>(FMcpServer.get());

		FMcpHandler->SetToolUsageCallback(
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

		FMcpHandler->SetLogCallback(
			[this](const std::string& request, const std::string& response) {
				if (FLogCallback)
				{
					auto cb = FLogCallback;
					auto req = request;
					auto resp = response;
					TThread::Queue(nullptr, [cb, req, resp]() {
						cb(req, resp);
					});
				}
			});

		// 7. Create HttpTransport with CORS
		Mcp::Transport::TCorsConfig corsConfig;
		corsConfig.AllowLocalhost = true;
		FHttpTransport = std::make_unique<Mcp::Transport::HttpTransport>(
			FHttpServer, corsConfig);

		// 8. Wire transport to handler
		FHttpTransport->SetRequestHandler(
			[this](Mcp::Transport::ITransportRequest& req,
				   Mcp::Transport::ITransportResponse& resp) {
				FMcpHandler->HandleRequest(req, resp);
			});

		// 9. Wire Indy OnCommandGet → HttpTransport via __closure bridge
		FEventBridge = new THttpEventBridge();
		FEventBridge->FTransport = FHttpTransport.get();
		FHttpServer->OnCommandGet = FEventBridge->OnCommandGet;

		// 10. Start
		FHttpTransport->Start();

		// 10. Set state
		SetState(ModuleState::Running);
	}
	catch (const std::exception& e)
	{
		FLastError = e.what();
		// Cleanup partial init
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
		if (FHttpTransport)
		{
			FHttpTransport->Stop();
			FHttpTransport.reset();
		}
		FMcpHandler.reset();
		FMcpServer.reset();

		OnShutdown();

		if (FLocalDb)
		{
			FLocalDb->Close();
			FLocalDb.reset();
		}
		if (FEventBridge)
		{
			delete FEventBridge;
			FEventBridge = nullptr;
		}
		if (FHttpServer)
		{
			delete FHttpServer;
			FHttpServer = nullptr;
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
//---------------------------------------------------------------------------
