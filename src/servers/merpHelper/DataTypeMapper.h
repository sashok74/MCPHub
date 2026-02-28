#ifndef DataTypeMapperH
#define DataTypeMapperH

#include <map>
#include <vector>
#include <string>
#include <Data.DB.hpp>
#include <FireDAC.Stan.Intf.hpp>

class DataTypeMapper {
public:
    struct TypeInfo {
		UnicodeString sqlType;  // Название типа в SQL Server
		TFieldType fireDacType; // Тип FireDAC
		UnicodeString cppType;  // Название типа в C++
		UnicodeString defaultValue;
		UnicodeString prefix;
		UnicodeString displayFmt;      // формат с {f} плейсхолдером, например "TC::FormatInt({f})"
		UnicodeString displayZeroFmt;  // формат для DisplayZero=true
	};

	static const std::map<int, TypeInfo>& getSystemTypeMapping();

    // Получение типа MSSQL по system_type_id
    static UnicodeString getSqlTypeById(int typeID);

    // Получение типа FireDAC по system_type_id
    static TFieldType getFireDacTypeById(int typeID);

    // Получение типа C++ по system_type_id MSSQL
	static UnicodeString getCppTypeById(int typeID);

    // ?? Новый метод: Получение MSSQL-типа по FireDAC-типу
	static UnicodeString getSqlTypeByFireDac(TFieldType fireDacType);

	// ?? Новый метод: Получение C++-типа по MSSQL-типу
	static UnicodeString getCppTypeBySqlType(const UnicodeString& sqlType);

	static std::vector<UnicodeString> getSqlTypeList();

	static UnicodeString getDefaultValueById(int typeID);

	// получение префикса по C++ типу
	static UnicodeString getCppPrefixByCppType(const UnicodeString& cppType);

	// получение дефолтного значения по C++ типу
	static UnicodeString getDefaultValueByCppType(const UnicodeString& cppType);

	// функии преобразования в текст по C++ типу
	static UnicodeString getWstringByCppType(const UnicodeString& cppType, const UnicodeString& prmName, const bool DisplayZero);

	// функии преобразования в текст по Db типу
	static UnicodeString getWstringBySQLType(const UnicodeString& sqlType, const UnicodeString& prmName, const bool DisplayZero);

	// получить префикс по типу бд
	static UnicodeString getCppPrefixBySqlType(const UnicodeString& sqlType);

    static TFieldType getFireDacTypeSqlType(const UnicodeString& sqlType);

};

#endif

