//---------------------------------------------------------------------------
#pragma hdrstop
#include "MerpHelperModule.h"
#include "MerpHelperTools.h"
#include "MsSqlQueryStorage.h"
#include "UcodeUtf8.h"
//---------------------------------------------------------------------------
#pragma package(smart_init)

std::vector<ConfigFieldDef> MerpHelperModule::GetConfigFields() const
{
	return {
		{"mcp_port",      "MCP Port",      ConfigFieldType::Integer,  {},  "8090",  true},
		{"server",        "SQL Server",    ConfigFieldType::String,   {},  "",      true},
		{"database",      "Database",      ConfigFieldType::String,   {},  "",      true},
		{"username",      "Username",      ConfigFieldType::String,   {},  "",      false},
		{"password",      "Password",      ConfigFieldType::Password, {},  "",      false},
		{"output_dir",    "Output Dir",    ConfigFieldType::FilePath, {},  "",      false},
		{"local_db_path", "Local DB Path", ConfigFieldType::FilePath, {},  "",      false},
	};
}

void MerpHelperModule::InitializeTemplatesSchema(LocalMcpDb* db)
{
	if (!db) return;

	db->Exec(
		"CREATE TABLE IF NOT EXISTS code_templates ("
		"  id INTEGER PRIMARY KEY AUTOINCREMENT,"
		"  name TEXT NOT NULL UNIQUE,"
		"  description TEXT DEFAULT '',"
		"  template_text TEXT NOT NULL,"
		"  category TEXT DEFAULT 'custom',"
		"  created_at TEXT DEFAULT (datetime('now')),"
		"  updated_at TEXT DEFAULT (datetime('now'))"
		");"
	);

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
		"CREATE TABLE IF NOT EXISTS query_snapshots ("
		"  id INTEGER PRIMARY KEY AUTOINCREMENT,"
		"  query_name TEXT NOT NULL,"
		"  operation TEXT NOT NULL,"
		"  data_json TEXT NOT NULL,"
		"  created_at TEXT DEFAULT (datetime('now'))"
		");"
	);
}

void MerpHelperModule::OnInitialize(LocalMcpDb* db)
{
	InitializeTemplatesSchema(db);

	// Create FireDAC connection to MSSQL
	FConnection = std::make_unique<TFDConnection>(nullptr);
	FConnection->LoginPrompt = false;
	FConnection->DriverName = L"MSSQL";

	auto setParam = [&](const char* configKey, const String& paramName) {
		if (FConfig.contains(configKey) && FConfig[configKey].is_string()) {
			std::string val = FConfig[configKey].get<std::string>();
			if (!val.empty())
				FConnection->Params->Values[paramName] = u(val);
		}
	};

	setParam("server", L"Server");
	setParam("database", L"Database");
	setParam("username", L"User_Name");
	setParam("password", L"Password");

	FConnection->Connected = true;

	// Create dependencies
	FStorage = std::make_unique<MsSqlQueryStorage>(FConnection.get());
	FAnalyzer = std::make_unique<McpQueryAnalyzer>(FConnection.get());
	FCodeGen = std::make_unique<CodeGenerator>(FStorage.get());
}

Mcp::Tools::ToolList MerpHelperModule::OnRegisterTools()
{
	return Mcp::Tools::GetMerpHelperTools(
		FStorage.get(),
		FAnalyzer.get(),
		FCodeGen.get(),
		FLocalDb.get(),
		FConnection.get(),
		&FConfig
	);
}

void MerpHelperModule::OnShutdown()
{
	FCodeGen.reset();
	FAnalyzer.reset();
	FStorage.reset();

	if (FConnection) {
		if (FConnection->Connected)
			FConnection->Connected = false;
		FConnection.reset();
	}
}
//---------------------------------------------------------------------------
