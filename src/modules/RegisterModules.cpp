//---------------------------------------------------------------------------
#pragma hdrstop
#include "RegisterModules.h"
#include "ProjectMemoryModule.h"
#include "DbMcpModule.h"
#include "MerpHelperModule.h"
//---------------------------------------------------------------------------
#pragma package(smart_init)

void RegisterAllModuleTypes(ModuleRegistry& registry)
{
	registry.RegisterType("project_memory", "Project Memory",
		[](const std::string& instanceId) {
			return std::make_unique<ProjectMemoryModule>(instanceId);
		});

	registry.RegisterType("db_mcp", "Database MCP",
		[](const std::string& instanceId) {
			return std::make_unique<DbMcpModule>(instanceId);
		});

	registry.RegisterType("merp_helper", "MERP Helper",
		[](const std::string& instanceId) {
			return std::make_unique<MerpHelperModule>(instanceId);
		});
}
//---------------------------------------------------------------------------
