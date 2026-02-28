//---------------------------------------------------------------------------
// DbMcpModule.h — Database MCP module
//---------------------------------------------------------------------------

#ifndef DbMcpModuleH
#define DbMcpModuleH

#include "McpModuleBase.h"
#include "IVclDbProvider.h"
#include "VclDbProviderAdapter.h"
#include <FireDAC.Comp.Client.hpp>
#include <memory>

//---------------------------------------------------------------------------

class DbMcpModule : public McpModuleBase
{
public:
	DbMcpModule(const std::string& instanceId)
		: McpModuleBase(instanceId, "Database MCP"),
		  FDbProvider(nullptr)
	{}

	std::string GetModuleTypeId() const override { return "db_mcp"; }

	std::vector<ConfigFieldDef> GetConfigFields() const override;

protected:
	void OnInitialize(LocalMcpDb* db) override;
	Mcp::Tools::ToolList OnRegisterTools() override;
	void OnShutdown() override;
	std::string GetMcpServerName() const override { return "db-mcp"; }

private:
	std::unique_ptr<TFDConnection> FConnection;
	IVclDbProvider* FDbProvider;
	std::unique_ptr<TVclDbProviderAdapter> FAdapter;

	void InitializeDbSchema(LocalMcpDb* db);
};

//---------------------------------------------------------------------------
#endif
