//---------------------------------------------------------------------------
// IMcpModule.h — Module interface for MCPHub
//---------------------------------------------------------------------------

#ifndef IMcpModuleH
#define IMcpModuleH

#include <string>
#include <vector>
#include <functional>
#include <ctime>
#include "nlohmann/json.hpp"

//---------------------------------------------------------------------------

enum class ModuleState { Stopped, Running, Error };
enum class ConfigFieldType { String, Integer, Enum, FilePath, Password, Boolean };

struct ConfigFieldDef {
	std::string id;
	std::string label;
	ConfigFieldType type;
	std::vector<std::string> enumValues;
	std::string defaultValue;
	bool required;
};

//---------------------------------------------------------------------------

class IMcpModule
{
public:
	// --- Identity ---
	virtual std::string GetModuleTypeId() const = 0;
	virtual std::string GetDisplayName() const = 0;
	virtual std::string GetInstanceId() const = 0;
	virtual void SetDisplayName(const std::string& name) = 0;

	// --- Configuration ---
	virtual std::vector<ConfigFieldDef> GetConfigFields() const = 0;
	virtual nlohmann::json GetConfig() const = 0;
	virtual void SetConfig(const nlohmann::json& config) = 0;

	// --- Lifecycle ---
	virtual void Start() = 0;
	virtual void Stop() = 0;
	virtual ModuleState GetState() const = 0;
	virtual std::string GetLastError() const = 0;

	// --- Request handling ---
	virtual std::string HandleJsonRpc(const std::string& requestJson) = 0;

	// --- Observability ---
	virtual int GetPort() const = 0;
	virtual int GetToolCount() const = 0;
	virtual int GetRequestCount() const = 0;
	virtual std::vector<std::string> GetToolNames() const = 0;
	virtual time_t GetLastActivityTime() const = 0;

	// --- Callbacks ---
	using LogCallback = std::function<void(const std::string& request, const std::string& response)>;
	using StateChangeCallback = std::function<void(ModuleState newState)>;
	using ActivityCallback = std::function<void(const std::string& toolName, bool success)>;

	virtual void SetLogCallback(LogCallback cb) = 0;
	virtual void SetStateChangeCallback(StateChangeCallback cb) = 0;
	virtual void SetActivityCallback(ActivityCallback cb) = 0;

	virtual ~IMcpModule() = default;
};

//---------------------------------------------------------------------------
#endif
