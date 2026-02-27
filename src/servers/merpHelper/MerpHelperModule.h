//---------------------------------------------------------------------------
// MerpHelperModule.h — MERP code generation MCP module
//---------------------------------------------------------------------------

#ifndef MerpHelperModuleH
#define MerpHelperModuleH

#include "McpModuleBase.h"
#include "IQueryStorage.h"
#include "McpQueryAnalyzer.h"
#include "CodeGenerator.h"
#include <FireDAC.Comp.Client.hpp>
#include <memory>

//---------------------------------------------------------------------------

class MerpHelperModule : public McpModuleBase
{
public:
	MerpHelperModule(const std::string& instanceId)
		: McpModuleBase(instanceId, "MERP Helper")
	{}

	std::string GetModuleTypeId() const override { return "merp_helper"; }
	std::vector<ConfigFieldDef> GetConfigFields() const override;

protected:
	void OnInitialize(LocalMcpDb* db) override;
	Mcp::Tools::ToolList OnRegisterTools() override;
	void OnShutdown() override;
	std::string GetMcpServerName() const override { return "merp-helper"; }

private:
	std::unique_ptr<TFDConnection> FConnection;
	std::unique_ptr<IQueryStorage> FStorage;
	std::unique_ptr<McpQueryAnalyzer> FAnalyzer;
	std::unique_ptr<CodeGenerator> FCodeGen;

	void InitializeTemplatesSchema(LocalMcpDb* db);
};

//---------------------------------------------------------------------------
#endif
