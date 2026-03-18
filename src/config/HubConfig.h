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

struct WindowState {
	int left = -1;
	int top = -1;
	int width = -1;
	int height = -1;
	bool maximized = false;
};

//---------------------------------------------------------------------------

class HubConfig
{
public:
	void Load(const std::string& path);
	void Save(const std::string& path);

	std::vector<ModuleConfig>& GetModules() { return FModules; }
	const std::vector<ModuleConfig>& GetModules() const { return FModules; }

	WindowState& GetWindow() { return FWindow; }
	const WindowState& GetWindow() const { return FWindow; }

	void AddModule(const ModuleConfig& m);
	void RemoveModule(const std::string& instanceId);

private:
	std::vector<ModuleConfig> FModules;
	WindowState FWindow;
};

//---------------------------------------------------------------------------
#endif
