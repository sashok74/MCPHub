//---------------------------------------------------------------------------
#pragma hdrstop
#include "ModuleRegistry.h"
#include <stdexcept>
//---------------------------------------------------------------------------
#pragma package(smart_init)

void ModuleRegistry::RegisterType(const std::string& typeId,
	const std::string& displayTypeName, Creator fn)
{
	FTypes[typeId] = {displayTypeName, std::move(fn)};
}

std::unique_ptr<IMcpModule> ModuleRegistry::Create(const std::string& typeId,
	const std::string& instanceId)
{
	auto it = FTypes.find(typeId);
	if (it == FTypes.end())
		throw std::runtime_error("Unknown module type: " + typeId);
	return it->second.creator(instanceId);
}

std::vector<std::pair<std::string, std::string>> ModuleRegistry::GetRegisteredTypes() const
{
	std::vector<std::pair<std::string, std::string>> result;
	for (const auto& kv : FTypes)
		result.push_back({kv.first, kv.second.displayName});
	return result;
}
//---------------------------------------------------------------------------
