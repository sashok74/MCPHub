//---------------------------------------------------------------------------
// ModuleRegistry.h — Factory registry for module types
//---------------------------------------------------------------------------

#ifndef ModuleRegistryH
#define ModuleRegistryH

#include "IMcpModule.h"
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <memory>

//---------------------------------------------------------------------------

class ModuleRegistry
{
public:
	using Creator = std::function<std::unique_ptr<IMcpModule>(const std::string& instanceId)>;

	void RegisterType(const std::string& typeId, const std::string& displayTypeName, Creator fn);
	std::unique_ptr<IMcpModule> Create(const std::string& typeId, const std::string& instanceId);
	std::vector<std::pair<std::string, std::string>> GetRegisteredTypes() const;

private:
	struct TypeEntry {
		std::string displayName;
		Creator creator;
	};
	std::map<std::string, TypeEntry> FTypes;
};

//---------------------------------------------------------------------------
#endif
