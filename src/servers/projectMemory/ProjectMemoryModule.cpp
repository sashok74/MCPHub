//---------------------------------------------------------------------------
#pragma hdrstop
#include "ProjectMemoryModule.h"
#include "LocalMcpDbInit.h"
#include "ProjectMemoryTools.h"
//---------------------------------------------------------------------------
#pragma package(smart_init)

std::vector<ConfigFieldDef> ProjectMemoryModule::GetConfigFields() const
{
	return {
		{"mcp_port", "MCP Port", ConfigFieldType::Integer, {}, "8766", true},
		{"db_path", "Database Path", ConfigFieldType::FilePath, {}, "", true},
		{"dbmcp_port", "dbMCP Port (federation, 0=off)", ConfigFieldType::Integer, {}, "0", false},
	};
}

void ProjectMemoryModule::OnInitialize(LocalMcpDb* db)
{
	if (db)
		InitializeProjectMemorySchema(db);

	int dbmcpPort = 0;
	if (FConfig.contains("dbmcp_port")) {
		auto &v = FConfig["dbmcp_port"];
		if (v.is_number())
			dbmcpPort = v.get<int>();
		else if (v.is_string())
			dbmcpPort = std::stoi(v.get<std::string>());
	}

	if (dbmcpPort > 0)
		FDbMcpClient = std::make_unique<Federation::DbMcpClient>(dbmcpPort);
}

Mcp::Tools::ToolList ProjectMemoryModule::OnRegisterTools()
{
	return Mcp::Tools::GetProjectMemoryTools(FLocalDb.get(), FDbMcpClient.get());
}
//---------------------------------------------------------------------------
