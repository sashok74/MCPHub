//---------------------------------------------------------------------------
// VclDbProviderAdapter.cpp — Adapter from IVclDbProvider to IDbProvider (UTF-8)
//---------------------------------------------------------------------------

#include "VclDbProviderAdapter.h"
#include "UcodeUtf8.h"
#include <cstdio>

TVclDbProviderAdapter::TVclDbProviderAdapter(IVclDbProvider *provider)
	: FProvider(provider)
{
}

void TVclDbProviderAdapter::SetProvider(IVclDbProvider *provider)
{
	FProvider = provider;
}

IVclDbProvider* TVclDbProviderAdapter::GetProvider() const
{
	return FProvider;
}

bool TVclDbProviderAdapter::IsAvailable() const
{
	return FProvider != nullptr;
}

static std::string EscapeJsonString(const std::string &s)
{
	std::string out;
	out.reserve(s.size());
	for (unsigned char c : s)
	{
		switch (c)
		{
		case '\\': out += "\\\\"; break;
		case '"': out += "\\\""; break;
		case '\r': out += "\\r"; break;
		case '\n': out += "\\n"; break;
		case '\t': out += "\\t"; break;
		default:
			if (c < 0x20)
			{
				char buf[7];
				snprintf(buf, sizeof(buf), "\\u%04X", c);
				out += buf;
			}
			else
			{
				out += static_cast<char>(c);
			}
		}
	}
	return out;
}

std::string TVclDbProviderAdapter::MakeErrorJson(const std::string &message) const
{
	return std::string("{\"error\":\"") + EscapeJsonString(message) + "\"}";
}

std::string TVclDbProviderAdapter::GetProviderName() const
{
	if (!FProvider)
		return "";
	return utf8(FProvider->GetProviderName());
}

std::string TVclDbProviderAdapter::ListTables(const std::string &schemaFilter, bool includeViews)
{
	if (!FProvider)
		return MakeErrorJson("Database provider not initialized");
	return utf8(FProvider->ListTables(u(schemaFilter), includeViews));
}

std::string TVclDbProviderAdapter::GetTableSchema(const std::string &table)
{
	if (!FProvider)
		return MakeErrorJson("Database provider not initialized");
	return utf8(FProvider->GetTableSchema(u(table)));
}

std::string TVclDbProviderAdapter::GetTableRelations(const std::string &table)
{
	if (!FProvider)
		return MakeErrorJson("Database provider not initialized");
	return utf8(FProvider->GetTableRelations(u(table)));
}

std::string TVclDbProviderAdapter::ListDatabases()
{
	if (!FProvider)
		return MakeErrorJson("Database provider not initialized");
	return utf8(FProvider->ListDatabases());
}

std::string TVclDbProviderAdapter::ListObjects(const std::string &objType,
	const std::string &schemaFilter, const std::string &namePattern)
{
	if (!FProvider)
		return MakeErrorJson("Database provider not initialized");
	return utf8(FProvider->ListObjects(u(objType), u(schemaFilter), u(namePattern)));
}

std::string TVclDbProviderAdapter::GetObjectDefinition(const std::string &objName,
	const std::string &objType)
{
	if (!FProvider)
		return MakeErrorJson("Database provider not initialized");
	return utf8(FProvider->GetObjectDefinition(u(objName), u(objType)));
}

std::string TVclDbProviderAdapter::GetDependencies(const std::string &objName,
	const std::string &direction)
{
	if (!FProvider)
		return MakeErrorJson("Database provider not initialized");
	return utf8(FProvider->GetDependencies(u(objName), u(direction)));
}

std::string TVclDbProviderAdapter::SearchColumns(const std::string &pattern)
{
	if (!FProvider)
		return MakeErrorJson("Database provider not initialized");
	return utf8(FProvider->SearchColumns(u(pattern)));
}

std::string TVclDbProviderAdapter::SearchObjectSource(const std::string &pattern)
{
	if (!FProvider)
		return MakeErrorJson("Database provider not initialized");
	return utf8(FProvider->SearchObjectSource(u(pattern)));
}

std::string TVclDbProviderAdapter::ProfileColumn(const std::string &table,
	const std::string &column)
{
	if (!FProvider)
		return MakeErrorJson("Database provider not initialized");
	return utf8(FProvider->ProfileColumn(u(table), u(column)));
}

std::string TVclDbProviderAdapter::ExecuteQuery(const std::string &sql)
{
	if (!FProvider)
		return MakeErrorJson("Database provider not initialized");
	return utf8(FProvider->ExecuteQuery(u(sql)));
}

std::string TVclDbProviderAdapter::ExecuteQuery(const std::string &sql, int maxRows)
{
	if (!FProvider)
		return MakeErrorJson("Database provider not initialized");
	return utf8(FProvider->ExecuteQuery(u(sql), maxRows));
}

std::string TVclDbProviderAdapter::GetTableSample(const std::string &table, int limit)
{
	if (!FProvider)
		return MakeErrorJson("Database provider not initialized");
	return utf8(FProvider->GetTableSample(u(table), limit));
}

std::string TVclDbProviderAdapter::ExplainQuery(const std::string &sql)
{
	if (!FProvider)
		return MakeErrorJson("Database provider not initialized");
	return utf8(FProvider->ExplainQuery(u(sql)));
}

std::string TVclDbProviderAdapter::CompareTables(const std::string &table1,
	const std::string &table2)
{
	if (!FProvider)
		return MakeErrorJson("Database provider not initialized");
	return utf8(FProvider->CompareTables(u(table1), u(table2)));
}

std::string TVclDbProviderAdapter::TraceFkPath(const std::string &fromTable,
	const std::string &toTable, int maxDepth)
{
	if (!FProvider)
		return MakeErrorJson("Database provider not initialized");
	return utf8(FProvider->TraceFkPath(u(fromTable), u(toTable), maxDepth));
}

std::string TVclDbProviderAdapter::GetModuleOverview(const std::string &schemaName)
{
	if (!FProvider)
		return MakeErrorJson("Database provider not initialized");
	return utf8(FProvider->GetModuleOverview(u(schemaName)));
}
