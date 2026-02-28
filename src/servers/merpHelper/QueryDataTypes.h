//---------------------------------------------------------------------------
#pragma once

#include <string>
#include <vector>

enum class DBType { MSSQL, PGSQL };

struct ParamInfo {
	std::string CppType;
	std::string sqlType;
	std::string MemberName;   // с Hungarian prefix (m_iID, m_sName)
	std::string DbName;       // оригинальное имя колонки
	bool IsKey = false;
	bool IsTid = false;       // TreeIdent == "ID"
	bool IsPid = false;       // TreeIdent == "PID"
	bool IsNullable = false;
	bool DisplayZero = false;
	std::string NullValue;
	std::string DefaultValue;
	int MaxLength = 0;
	int Precision = 0;
	int Scale = 0;
};

struct SqlInfo {
	std::string queryName;
	std::string Description;
	std::string QueryType;     // SELECT/INSERT/UPDATE/DELETE
	std::string methodName;    // CamelCase
	std::string TypeName;      // "T" + methodName
	std::string inType;        // TypeName + "::in inPrm"
	std::string outType;       // TypeName + "::out outPrm"
	std::vector<ParamInfo> inParams;
	std::vector<ParamInfo> outParams;
	std::string MS_SQL;
	std::string PG_SQL;
	int QueryID = -1;
};

// DTO для IQueryStorage
struct ModuleRecord {
	int moduleID = 0;
	std::string moduleName;
};

struct QueryRecord {
	int queryID = 0;
	std::string queryName;
	std::string queryType;
	std::string description;
	std::string sourceSQL;     // MS SQL текст
	std::string pgSQL;         // PostgreSQL текст
	int moduleID = 0;
	std::string moduleName;
	std::string tableName;
	std::string resultClass;
	std::string testInfo;
	bool isActive = true;
	// DS_* поля для Dataset mapping
	std::string dsName;
	std::string dsQueryS;
	std::string dsQueryI;
	std::string dsQueryU;
	std::string dsQueryD;
	std::string dsQueryOneS;
};

struct ParamRecord {        // raw данные из dbo.SqlQueryParams
	int paramID = 0;
	int queryID = 0;
	std::string paramName;
	std::string paramType;  // SQL тип
	std::string direction;  // IN/OUT
	bool isNullable = false;
	std::string defaultValue;
	int paramOrder = 0;
	int maxLength = 0;
	int precision = 0;
	int scale = 0;
	std::string nullValue;
	bool isKeyField = false;
	bool userModify = false;
};

struct FieldRecord {        // raw данные из dbo.SqlQueryFields
	int fieldID = 0;
	int queryID = 0;
	std::string fieldName;
	std::string fieldType;  // SQL тип
	std::string treeIdent;  // "ID"/"PID"/""
	bool isNullable = false;
	bool displayZero = false;
	int fieldOrder = 0;
	std::string aliasName;
	int maxLength = 0;
	int precision = 0;
	int scale = 0;
	bool isKeyField = false;
	bool userModify = false;
	std::string defaultValue;
};
//---------------------------------------------------------------------------
