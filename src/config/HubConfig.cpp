//---------------------------------------------------------------------------
#pragma hdrstop
#include "HubConfig.h"
#include <fstream>
#include <algorithm>
//---------------------------------------------------------------------------
#pragma package(smart_init)

void HubConfig::Load(const std::string& path)
{
	FModules.clear();
	std::ifstream f(path);
	if (!f.is_open())
		return;

	try
	{
		nlohmann::json j;
		f >> j;

		if (j.contains("modules") && j["modules"].is_array())
		{
			for (const auto& m : j["modules"])
			{
				ModuleConfig mc;
				mc.instanceId = m.value("instanceId", "");
				mc.typeId = m.value("typeId", "");
				mc.displayName = m.value("displayName", "");
				mc.autoStart = m.value("autoStart", false);
				mc.config = m.value("config", nlohmann::json::object());
				if (!mc.instanceId.empty() && !mc.typeId.empty())
					FModules.push_back(std::move(mc));
			}
		}

		if (j.contains("window") && j["window"].is_object())
		{
			auto& w = j["window"];
			FWindow.left = w.value("left", -1);
			FWindow.top = w.value("top", -1);
			FWindow.width = w.value("width", -1);
			FWindow.height = w.value("height", -1);
			FWindow.maximized = w.value("maximized", false);
		}
	}
	catch (...)
	{
		// Ignore malformed config
	}
}

void HubConfig::Save(const std::string& path)
{
	nlohmann::json j;
	nlohmann::json arr = nlohmann::json::array();

	for (const auto& m : FModules)
	{
		nlohmann::json mj;
		mj["instanceId"] = m.instanceId;
		mj["typeId"] = m.typeId;
		mj["displayName"] = m.displayName;
		mj["autoStart"] = m.autoStart;
		mj["config"] = m.config;
		arr.push_back(std::move(mj));
	}

	j["modules"] = std::move(arr);

	nlohmann::json wj;
	wj["left"] = FWindow.left;
	wj["top"] = FWindow.top;
	wj["width"] = FWindow.width;
	wj["height"] = FWindow.height;
	wj["maximized"] = FWindow.maximized;
	j["window"] = std::move(wj);

	std::ofstream f(path);
	if (f.is_open())
		f << j.dump(2);
}

void HubConfig::AddModule(const ModuleConfig& m)
{
	FModules.push_back(m);
}

void HubConfig::RemoveModule(const std::string& instanceId)
{
	FModules.erase(
		std::remove_if(FModules.begin(), FModules.end(),
			[&](const ModuleConfig& m) { return m.instanceId == instanceId; }),
		FModules.end());
}
//---------------------------------------------------------------------------
