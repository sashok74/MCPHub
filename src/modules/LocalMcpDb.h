//---------------------------------------------------------------------------
// LocalMcpDb.h — Adapted copy for MCPHub (fixed include paths)
//
// Original: ProjectMemory/src/mcp/localdb/LocalMcpDb.h
// Changed: nlohmann/json.hpp and sqlite3.h paths for MCPHub include layout
//---------------------------------------------------------------------------

#ifndef LocalMcpDbH
#define LocalMcpDbH
//---------------------------------------------------------------------------
#include <string>
#include <map>
#include <mutex>
#include "nlohmann/json.hpp"
#include "sqlite3.h"
//---------------------------------------------------------------------------

class LocalMcpDb
{
public:
	using Params = std::map<std::string, std::string>;
	using json = nlohmann::json;

	LocalMcpDb();
	~LocalMcpDb();

	void Open(const std::string &dbPath);
	void Close();

	bool IsOpen() const { return FDb != nullptr; }

	void Exec(const std::string &sql);
	json Query(const std::string &sql, const Params &params = Params());
	int Execute(const std::string &sql, const Params &params = Params());
	long long LastInsertRowId() const;

	// Transaction support
	void BeginTransaction();
	void Commit();
	void Rollback();

private:
	sqlite3 *FDb;
	mutable std::mutex FMutex;

	void BindParams(sqlite3_stmt *stmt, const Params &params);
	json RowToJson(sqlite3_stmt *stmt);
};

//---------------------------------------------------------------------------
#endif
