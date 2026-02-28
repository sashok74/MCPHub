//---------------------------------------------------------------------------
// LocalMcpDb.cpp — VCL-free SQLite wrapper for MCP local storage
//---------------------------------------------------------------------------

#include "LocalMcpDb.h"
#include <stdexcept>

//---------------------------------------------------------------------------
LocalMcpDb::LocalMcpDb()
	: FDb(nullptr)
{
}
//---------------------------------------------------------------------------
LocalMcpDb::~LocalMcpDb()
{
	Close();
}
//---------------------------------------------------------------------------
void LocalMcpDb::Open(const std::string &dbPath)
{
	std::lock_guard<std::mutex> lock(FMutex);
	if (FDb)
		return;

	int rc = sqlite3_open(dbPath.c_str(), &FDb);
	if (rc != SQLITE_OK)
	{
		std::string err = FDb ? sqlite3_errmsg(FDb) : "Failed to allocate SQLite handle";
		if (FDb)
		{
			sqlite3_close(FDb);
			FDb = nullptr;
		}
		throw std::runtime_error("SQLite open error: " + err);
	}

	// Set WAL mode for concurrent reads
	sqlite3_exec(FDb, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);
	// Enable foreign keys
	sqlite3_exec(FDb, "PRAGMA foreign_keys=ON;", nullptr, nullptr, nullptr);
}
//---------------------------------------------------------------------------
void LocalMcpDb::Close()
{
	std::lock_guard<std::mutex> lock(FMutex);
	if (FDb)
	{
		sqlite3_close(FDb);
		FDb = nullptr;
	}
}
//---------------------------------------------------------------------------
void LocalMcpDb::BindParams(sqlite3_stmt *stmt, const Params &params)
{
	for (const auto &pair : params)
	{
		const std::string &name = pair.first;
		const std::string &value = pair.second;

		std::string paramName = ":" + name;
		int idx = sqlite3_bind_parameter_index(stmt, paramName.c_str());
		if (idx > 0)
		{
			sqlite3_bind_text(stmt, idx, value.c_str(),
				static_cast<int>(value.size()), SQLITE_TRANSIENT);
		}
	}
}
//---------------------------------------------------------------------------
LocalMcpDb::json LocalMcpDb::RowToJson(sqlite3_stmt *stmt)
{
	json row = json::object();
	int colCount = sqlite3_column_count(stmt);

	for (int i = 0; i < colCount; i++)
	{
		const char *colName = sqlite3_column_name(stmt, i);
		int colType = sqlite3_column_type(stmt, i);
		std::string name = colName ? colName : "";

		switch (colType)
		{
		case SQLITE_NULL:
			row[name] = nullptr;
			break;
		case SQLITE_INTEGER:
			row[name] = static_cast<long long>(sqlite3_column_int64(stmt, i));
			break;
		case SQLITE_FLOAT:
			row[name] = sqlite3_column_double(stmt, i);
			break;
		case SQLITE_TEXT:
		{
			const unsigned char *text = sqlite3_column_text(stmt, i);
			row[name] = text ? reinterpret_cast<const char*>(text) : "";
			break;
		}
		case SQLITE_BLOB:
		{
			int blobSize = sqlite3_column_bytes(stmt, i);
			row[name] = "[BLOB " + std::to_string(blobSize) + " bytes]";
			break;
		}
		default:
		{
			const unsigned char *text = sqlite3_column_text(stmt, i);
			row[name] = text ? reinterpret_cast<const char*>(text) : "";
			break;
		}
		}
	}
	return row;
}
//---------------------------------------------------------------------------
LocalMcpDb::json LocalMcpDb::Query(const std::string &sql, const Params &params)
{
	std::lock_guard<std::mutex> lock(FMutex);
	if (!FDb)
		throw std::runtime_error("SQLite database not open");

	sqlite3_stmt *stmt = nullptr;
	int rc = sqlite3_prepare_v2(FDb, sql.c_str(), -1, &stmt, nullptr);
	if (rc != SQLITE_OK)
	{
		std::string err = sqlite3_errmsg(FDb);
		throw std::runtime_error("SQLite prepare error: " + err);
	}

	BindParams(stmt, params);

	json rows = json::array();
	while ((rc = sqlite3_step(stmt)) == SQLITE_ROW)
	{
		rows.push_back(RowToJson(stmt));
	}

	if (rc != SQLITE_DONE)
	{
		std::string err = sqlite3_errmsg(FDb);
		sqlite3_finalize(stmt);
		throw std::runtime_error("SQLite step error: " + err);
	}

	sqlite3_finalize(stmt);
	return rows;
}
//---------------------------------------------------------------------------
int LocalMcpDb::Execute(const std::string &sql, const Params &params)
{
	std::lock_guard<std::mutex> lock(FMutex);
	if (!FDb)
		throw std::runtime_error("SQLite database not open");

	sqlite3_stmt *stmt = nullptr;
	int rc = sqlite3_prepare_v2(FDb, sql.c_str(), -1, &stmt, nullptr);
	if (rc != SQLITE_OK)
	{
		std::string err = sqlite3_errmsg(FDb);
		throw std::runtime_error("SQLite prepare error: " + err);
	}

	BindParams(stmt, params);

	rc = sqlite3_step(stmt);
	if (rc != SQLITE_DONE && rc != SQLITE_ROW)
	{
		std::string err = sqlite3_errmsg(FDb);
		sqlite3_finalize(stmt);
		throw std::runtime_error("SQLite execute error: " + err);
	}

	int affected = sqlite3_changes(FDb);
	sqlite3_finalize(stmt);
	return affected;
}
//---------------------------------------------------------------------------
long long LocalMcpDb::LastInsertRowId() const
{
	std::lock_guard<std::mutex> lock(FMutex);
	if (!FDb)
		return 0;
	return static_cast<long long>(sqlite3_last_insert_rowid(FDb));
}
//---------------------------------------------------------------------------
void LocalMcpDb::Exec(const std::string &sql)
{
	std::lock_guard<std::mutex> lock(FMutex);
	if (!FDb)
		throw std::runtime_error("SQLite database not open");

	char *errMsg = nullptr;
	int rc = sqlite3_exec(FDb, sql.c_str(), nullptr, nullptr, &errMsg);
	if (rc != SQLITE_OK)
	{
		std::string err = errMsg ? errMsg : "Unknown error";
		if (errMsg)
			sqlite3_free(errMsg);
		throw std::runtime_error("SQLite exec error: " + err);
	}
}
//---------------------------------------------------------------------------
void LocalMcpDb::BeginTransaction()
{
	Exec("BEGIN TRANSACTION");
}
//---------------------------------------------------------------------------
void LocalMcpDb::Commit()
{
	Exec("COMMIT");
}
//---------------------------------------------------------------------------
void LocalMcpDb::Rollback()
{
	Exec("ROLLBACK");
}
//---------------------------------------------------------------------------
