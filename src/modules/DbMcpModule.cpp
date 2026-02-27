//---------------------------------------------------------------------------
#pragma hdrstop
#include "DbMcpModule.h"
#include "DbProviderFactory.h"
#include "DbTools.h"
#include "StatsTools.h"
#include "SnapshotTools.h"
#include "UcodeUtf8.h"
//---------------------------------------------------------------------------
#pragma package(smart_init)

std::vector<ConfigFieldDef> DbMcpModule::GetConfigFields() const
{
	return {
		{"mcp_port", "MCP Port", ConfigFieldType::Integer, {}, "8080", true},
		{"provider", "Provider", ConfigFieldType::Enum,
			{"MSSQL", "Firebird", "PostgreSQL"}, "MSSQL", true},
		{"server", "Server", ConfigFieldType::String, {}, "", true},
		{"database", "Database", ConfigFieldType::String, {}, "", true},
		{"username", "Username", ConfigFieldType::String, {}, "", false},
		{"password", "Password", ConfigFieldType::Password, {}, "", false},
		{"port", "DB Port", ConfigFieldType::Integer, {}, "", false},
		{"charset", "Charset (Firebird)", ConfigFieldType::Enum,
			{"UTF8", "WIN1251", "NONE"}, "UTF8", false},
		{"local_db_path", "Local DB Path", ConfigFieldType::FilePath, {}, "", false},
	};
}

void DbMcpModule::InitializeDbSchema(LocalMcpDb* db)
{
	if (!db) return;

	db->Exec(
		"CREATE TABLE IF NOT EXISTS tool_usage ("
		"  id INTEGER PRIMARY KEY AUTOINCREMENT,"
		"  tool_name TEXT NOT NULL,"
		"  called_at TEXT DEFAULT (datetime('now')),"
		"  success INTEGER DEFAULT 1,"
		"  error_message TEXT"
		");"
	);

	db->Exec(
		"CREATE TABLE IF NOT EXISTS feature_requests ("
		"  id INTEGER PRIMARY KEY AUTOINCREMENT,"
		"  title TEXT NOT NULL,"
		"  description TEXT NOT NULL,"
		"  use_case TEXT,"
		"  priority TEXT DEFAULT 'normal',"
		"  status TEXT DEFAULT 'new',"
		"  created_at TEXT DEFAULT (datetime('now'))"
		");"
	);

	db->Exec(
		"CREATE TABLE IF NOT EXISTS query_snapshots ("
		"  id INTEGER PRIMARY KEY AUTOINCREMENT,"
		"  name TEXT NOT NULL UNIQUE,"
		"  sql_text TEXT,"
		"  source_db TEXT,"
		"  row_count INTEGER,"
		"  column_list TEXT,"
		"  checksum TEXT,"
		"  execution_ms INTEGER,"
		"  created_at TEXT DEFAULT (datetime('now'))"
		");"
	);

	db->Exec(
		"CREATE TABLE IF NOT EXISTS snapshot_rows ("
		"  snapshot_id INTEGER,"
		"  row_num INTEGER,"
		"  row_data TEXT,"
		"  row_hash TEXT,"
		"  PRIMARY KEY (snapshot_id, row_num),"
		"  FOREIGN KEY (snapshot_id) REFERENCES query_snapshots(id) ON DELETE CASCADE"
		");"
	);

	db->Exec(
		"CREATE INDEX IF NOT EXISTS idx_snapshot_rows_hash "
		"ON snapshot_rows(snapshot_id, row_hash);"
	);
}

void DbMcpModule::OnInitialize(LocalMcpDb* db)
{
	InitializeDbSchema(db);

	// Create FireDAC connection
	FConnection = std::make_unique<TFDConnection>(nullptr);
	FConnection->LoginPrompt = false;

	// Determine driver
	std::string provider = "MSSQL";
	if (FConfig.contains("provider") && FConfig["provider"].is_string())
		provider = FConfig["provider"].get<std::string>();

	String driverID;
	if (provider == "MSSQL") driverID = L"MSSQL";
	else if (provider == "Firebird") driverID = L"FB";
	else if (provider == "PostgreSQL") driverID = L"PG";
	else driverID = L"MSSQL";

	FConnection->DriverName = driverID;

	// Set connection params
	auto setParam = [&](const char* configKey, const String& paramName) {
		if (FConfig.contains(configKey) && FConfig[configKey].is_string())
		{
			std::string val = FConfig[configKey].get<std::string>();
			if (!val.empty())
				FConnection->Params->Values[paramName] = u(val);
		}
	};

	auto setIntParam = [&](const char* configKey, const String& paramName) {
		if (FConfig.contains(configKey))
		{
			int val = 0;
			if (FConfig[configKey].is_number())
				val = FConfig[configKey].get<int>();
			else if (FConfig[configKey].is_string())
			{
				try { val = std::stoi(FConfig[configKey].get<std::string>()); }
				catch (...) {}
			}
			if (val > 0)
				FConnection->Params->Values[paramName] = IntToStr(val);
		}
	};

	setParam("server", L"Server");
	setParam("database", L"Database");
	setParam("username", L"User_Name");
	setParam("password", L"Password");
	setIntParam("port", L"Port");

	if (provider == "Firebird")
		setParam("charset", L"CharacterSet");

	// Connect
	FConnection->Connected = true;

	// Create provider via factory
	FDbProvider = TDbProviderFactory::CreateProvider(driverID, FConnection.get());
	FAdapter = std::make_unique<TVclDbProviderAdapter>(FDbProvider);
}

Mcp::Tools::ToolList DbMcpModule::OnRegisterTools()
{
	Mcp::Tools::ToolList allTools;

	auto dbTools = Mcp::Tools::GetDbTools(FAdapter.get());
	allTools.insert(allTools.end(), dbTools.begin(), dbTools.end());

	if (FLocalDb)
	{
		auto statsTools = Mcp::Tools::GetStatsTools(FLocalDb.get());
		allTools.insert(allTools.end(), statsTools.begin(), statsTools.end());

		auto snapTools = Mcp::Tools::GetSnapshotTools(FAdapter.get(), FLocalDb.get());
		allTools.insert(allTools.end(), snapTools.begin(), snapTools.end());
	}

	return allTools;
}

void DbMcpModule::OnShutdown()
{
	FAdapter.reset();

	if (FDbProvider)
	{
		delete FDbProvider;
		FDbProvider = nullptr;
	}

	if (FConnection)
	{
		if (FConnection->Connected)
			FConnection->Connected = false;
		FConnection.reset();
	}
}
//---------------------------------------------------------------------------
