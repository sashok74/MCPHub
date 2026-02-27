//---------------------------------------------------------------------------
// HubConfig.h — Configuration persistence for MCPHub
//---------------------------------------------------------------------------

#ifndef HubConfigH
#define HubConfigH

#include <string>
#include <vector>
#include "nlohmann/json.hpp"

//---------------------------------------------------------------------------

struct ModuleConfig {
	std::string instanceId;
	std::string typeId;
	std::string displayName;
	bool autoStart;
	nlohmann::json config;
};

//---------------------------------------------------------------------------

class HubConfig
{
public:
	void Load(const std::string& path);
	void Save(const std::string& path);

	std::vector<ModuleConfig>& GetModules() { return FModules; }
	const std::vector<ModuleConfig>& GetModules() const { return FModules; }

	void AddModule(const ModuleConfig& m);
	void RemoveModule(const std::string& instanceId);

private:
	std::vector<ModuleConfig> FModules;
};

//---------------------------------------------------------------------------
#endif
