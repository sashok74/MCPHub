//---------------------------------------------------------------------------

#pragma hdrstop

#include "DataTypeMapper.h"
#include <System.SysUtils.hpp>
//---------------------------------------------------------------------------
#pragma package(smart_init)
//std::chrono::system_clock::time_point
/*
	{61,   {"datetime",       ftDateTime,     "COleDateTime",  ".SetStatus(COleDateTime::null)",   "m_dt",   "TC::FormatDate(",        ")"}  },
	{40,   {"date",           ftDate,         "COleDateTime",  ".SetStatus(COleDateTime::null)",   "m_dt",   "TC::FormatDate(",        ")"}  },
	{41,   {"time",           ftTime,         "COleDateTime",  ".SetStatus(COleDateTime::null)",   "m_dt",   "TC::FormatDate(",        ")"}  },            CHRONO_NULL_DATE, bool bForQuery = false);
*/
const std::map<int, DataTypeMapper::TypeInfo>& DataTypeMapper::getSystemTypeMapping() {
	static std::map<int, TypeInfo> typeMapping = {
	{56,   {"int",            ftInteger,      "int",           "0",                                "m_i",   "TC::FormatInt({f})",         "TC::FormatInt({f},-1 , true)"         }},
	{127,  {"bigint",         ftLargeint,     "long long",     "0",                                "m_ll",  "TC::FormatInt({f})",         "TC::FormatInt({f},-1 , true)"         }},
	{106,  {"decimal",        ftBCD,          "double",        "0.0",                              "m_d",   "TC::FormatDouble({f},3)",    "TC::FormatDouble({f},3, -99999 , true)"}},
	{108,  {"numeric",        ftFMTBcd,       "double",        "0.0",                              "m_d",   "TC::FormatDouble({f},3)",    "TC::FormatDouble({f},3, -99999 , true)"}},
	{61,   {"datetime",       ftDateTime,     "std::chrono::system_clock::time_point",  "CHRONO_NULL_DATE",  "m_dt",  "TC::FormatDate({f})",        "TC::FormatDate({f},CHRONO_NULL_DATE , true)"    }},
	{40,   {"date",           ftDate,         "std::chrono::system_clock::time_point",  "CHRONO_NULL_DATE",  "m_dt",  "TC::FormatDateOnly({f})",    "TC::FormatDateOnly({f},CHRONO_NULL_DATE , true)"}},
	{41,   {"time",           ftTime,         "std::chrono::system_clock::time_point",  "CHRONO_NULL_DATE",  "m_dt",  "TC::FormatDate({f})",        "TC::FormatDate({f},CHRONO_NULL_DATE , true)"    }},
	{231,  {"nvarchar",       ftWideString,   "std::wstring",  "L\"\"",                             "m_s",   "{f}",  "{f}"}},
	{167,  {"varchar",        ftString,       "std::wstring",  "L\"\"",                             "m_s",   "{f}",  "{f}"}},
	{175,  {"char",           ftFixedChar,    "std::wstring",  "L\"\"",                             "m_s",   "{f}",  "{f}"}},
	{239,  {"nchar",          ftFixedWideChar,"std::wstring",  "L\"\"",                             "m_s",   "{f}",  "{f}"}},
	{99,   {"ntext",          ftMemo,         "std::wstring",  "L\"\"",                             "m_s",   "{f}",  "{f}"}},
	{35,   {"text",           ftMemo,         "std::wstring",  "L\"\"",                             "m_s",   "{f}",  "{f}"}},
	{36,   {"uniqueidentifier", ftGuid,       "std::wstring",  "L\"\"",                             "m_s",   "{f}",  "{f}"}},
	{104,  {"bit",            ftBoolean,      "bool",          "false",                             "m_b",   "{f} ? std::wstring(L\"TRUE\") : std::wstring(L\"FALSE\")", "{f}"}},
	{59,   {"real",           ftFloat,        "float",         "0.0f",                              "m_f",   "TC::FormatFloat({f},3)",     "TC::FormatFloat({f},3, -99999 , true)"}},
	{60,   {"money",          ftCurrency,     "double",        "0.0",                               "m_d",   "TC::FormatDouble({f},3)",    "TC::FormatDouble({f},3, -99999 , true)"}},
	{62,   {"float",          ftFloat,        "double",        "0.0",                               "m_d",   "TC::FormatDouble({f},3)",    "TC::FormatDouble({f},3, -99999 , true)"}},
	{122,  {"smallmoney",     ftCurrency,     "double",        "0.0",                               "m_d",   "TC::FormatDouble({f},3)",    "TC::FormatDouble({f},3, -99999 , true)"}},
	{48,   {"tinyint",        ftByte,         "uint8_t",       "0",                                 "m_ui8", "std::to_wstring({f})",       "std::to_wstring({f})"                 }},
	{52,   {"smallint",       ftSmallint,     "short",         "0",                                 "m_i",   "std::to_wstring({f})",       "std::to_wstring({f})"                 }},
	{189,  {"timestamp",      ftTimeStamp,    "std::wstring",  "L\"\"",                             "m_s",   "{f}",  "{f}"}},
	{165,  {"varbinary",      ftVarBytes,     "std::wstring",  "L\"\"",                             "m_s",   "{f}",  "{f}"}},
	{173,  {"binary",         ftBytes,        "std::wstring",  "L\"\"",                             "m_s",   "{f}",  "{f}"}},
	{98,   {"sql_variant",    ftVariant,      "std::any",      "std::any()",                        "m_any", "std::to_wstring({f})",       "std::to_wstring({f})"                 }},
	{240,  {"hierarchyid",    ftUnknown,      "unknown",       "unknown",                           "m_",    "{f}",  "{f}"}}
};
	return typeMapping;
}

/*
std::wstring FormatInt(int value, bool bForQuery, int nullValue);
std::wstring FormatDouble(double value, int iPrecision, bool bForQuery, int nullValue);
std::wstring FormatFloat(float value, int iPrecision, bool bForQuery, int nullValue);
std::wstring FormatDate(OleDateTime date, bool bForQuery, OleDateTime nullData);
*/

// ?? Получение типа MSSQL по system_type_id
UnicodeString DataTypeMapper::getSqlTypeById(int typeID) {
	const auto& mapping = getSystemTypeMapping();
	auto it = mapping.find(typeID);
	return (it != mapping.end()) ? it->second.sqlType : "unknown";
}

// ?? Получение типа FireDAC по system_type_id
TFieldType DataTypeMapper::getFireDacTypeById(int typeID) {
	const auto& mapping = getSystemTypeMapping();
	auto it = mapping.find(typeID);
    return (it != mapping.end()) ? it->second.fireDacType : ftUnknown;
}

// ?? Получение типа C++ по system_type_id
UnicodeString DataTypeMapper::getCppTypeById(int typeID) {
	const auto& mapping = getSystemTypeMapping();
    auto it = mapping.find(typeID);
	return (it != mapping.end()) ? it->second.cppType : "unknown";
}

// ?? Получение MSSQL-типа по FireDAC-типу
UnicodeString DataTypeMapper::getSqlTypeByFireDac(TFieldType fireDacType) {
	for (const auto& [_, typeInfo] : getSystemTypeMapping()) {
		if (typeInfo.fireDacType == fireDacType) {
			return typeInfo.sqlType;
		}
	}
	return "unknown";
}

// ?? Получение C++-типа по MSSQL-типу
UnicodeString DataTypeMapper::getCppTypeBySqlType(const UnicodeString& sqlType) {
	for (const auto& [_, typeInfo] : getSystemTypeMapping()) {
		if (typeInfo.sqlType == sqlType) {
			return typeInfo.cppType;
		}
	}
	return "unknown";
}

std::vector<UnicodeString> DataTypeMapper::getSqlTypeList() {
	std::vector<UnicodeString> typeList;
	for (const auto& [_, typeInfo] : getSystemTypeMapping()) {
		typeList.push_back(typeInfo.sqlType);
	}
	return typeList;
}

UnicodeString DataTypeMapper::getDefaultValueById(int typeID) {
	const auto& mapping = getSystemTypeMapping();
	auto it = mapping.find(typeID);
	return (it != mapping.end()) ? it->second.defaultValue : UnicodeString("unknown");
}

UnicodeString DataTypeMapper::getCppPrefixByCppType(const UnicodeString& cppType) {
	const auto& mapping = getSystemTypeMapping();
	for (const auto& [_, typeInfo] : mapping) {
		if (typeInfo.cppType == cppType)
			return typeInfo.prefix;
	}
	return "m_"; // По умолчанию
}

// ?? Получение типа FireDAC по system_type_id
TFieldType DataTypeMapper::getFireDacTypeSqlType(const UnicodeString& sqlType) {
	const auto& mapping = getSystemTypeMapping();
	for (const auto& [_, typeInfo] : mapping) {
		if (typeInfo.sqlType == sqlType)
			return typeInfo.fireDacType;
	}
	return ftUnknown;
}

// ?? Получение префикса MSSQL-типу
UnicodeString DataTypeMapper::getCppPrefixBySqlType(const UnicodeString& sqlType) {
	const auto& mapping = getSystemTypeMapping();
	for (const auto& [_, typeInfo] : mapping) {
		if (typeInfo.sqlType == sqlType)
			return typeInfo.prefix;
	}
	return "m_"; // По умолчанию
}


UnicodeString DataTypeMapper::getDefaultValueByCppType(const UnicodeString& cppType) {
	const auto& mapping = getSystemTypeMapping();
	for (const auto& [_, typeInfo] : mapping) {
		if (typeInfo.cppType == cppType) {
			return typeInfo.defaultValue;
		}
	}
	return "unknown";
}

UnicodeString DataTypeMapper::getWstringByCppType(const UnicodeString& cppType, const UnicodeString& prmName, const bool DisplayZero){
	const auto& mapping = getSystemTypeMapping();
	for (const auto& [_, typeInfo] : mapping) {
		if (typeInfo.cppType == cppType) {
			UnicodeString fmt = DisplayZero ? typeInfo.displayZeroFmt : typeInfo.displayFmt;
			return StringReplace(fmt, "{f}", "m." + prmName, TReplaceFlags() << rfReplaceAll);
		}
	}
	return "unknown";
}

UnicodeString DataTypeMapper::getWstringBySQLType(const UnicodeString& sqlType, const UnicodeString& prmName, const bool DisplayZero){
	const auto& mapping = getSystemTypeMapping();
	for (const auto& [_, typeInfo] : mapping) {
		if (typeInfo.sqlType == sqlType) {
			UnicodeString fmt = DisplayZero ? typeInfo.displayZeroFmt : typeInfo.displayFmt;
			return StringReplace(fmt, "{f}", "m." + prmName, TReplaceFlags() << rfReplaceAll);
		}
	}
	return "unknown";
}
