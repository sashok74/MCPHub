//---------------------------------------------------------------------------
// ProjectMemoryModule.h — ProjectMemory MCP module
//---------------------------------------------------------------------------

#ifndef ProjectMemoryModuleH
#define ProjectMemoryModuleH

#include "McpModuleBase.h"

//---------------------------------------------------------------------------

class ProjectMemoryModule : public McpModuleBase
{
public:
	ProjectMemoryModule(const std::string& instanceId)
		: McpModuleBase(instanceId, "Project Memory")
	{}

	std::string GetModuleTypeId() const override { return "project_memory"; }

	std::vector<ConfigFieldDef> GetConfigFields() const override;

protected:
	void OnInitialize(LocalMcpDb* db) override;
	Mcp::Tools::ToolList OnRegisterTools() override;
	void OnShutdown() override {}
	std::string GetMcpServerName() const override { return "project-memory"; }
};

//---------------------------------------------------------------------------
#endif
