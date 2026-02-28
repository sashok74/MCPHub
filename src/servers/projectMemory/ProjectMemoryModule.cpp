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
	};
}

void ProjectMemoryModule::OnInitialize(LocalMcpDb* db)
{
	if (db)
		InitializeProjectMemorySchema(db);
}

Mcp::Tools::ToolList ProjectMemoryModule::OnRegisterTools()
{
	return Mcp::Tools::GetProjectMemoryTools(FLocalDb.get());
}
//---------------------------------------------------------------------------
