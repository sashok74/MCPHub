//---------------------------------------------------------------------------

#ifndef sqlHelperH
#define sqlHelperH
#include <vector>
#include <string>
//---------------------------------------------------------------------------

std::string extract_text(const std::string& input);
void find_format_specifiers(std::string &duty_string, std::vector<char> &source_param);
void find_parameters(std::string& duty_string, std::vector<std::string>& source_param);
void replace_escape_sequences(std::string &sql_text);


// Структура для динамического параметра
struct SQLParameter
{
	std::wstring m_sName;        // Имя параметра (без двоеточия)
	std::wstring m_sType;        // Тип данных (INT, VARCHAR, DATETIME, etc.)
	std::wstring m_sValue;       // Значение как строка ("NULL" для null)

	SQLParameter(const std::wstring& name, const std::wstring& type, const std::wstring& value)
		: m_sName(name), m_sType(type), m_sValue(value) {}
};

std::wstring EscapeString(const std::wstring& value); 						  /// Экранирует одинарные кавычки в строке
std::wstring FormatValue(const SQLParameter& param);   						  /// Форматирует значение в зависимости от типа
void ReplaceDynamicParameter(std::wstring& sql, const SQLParameter& param);   /// Заменяет параметр в SQL строке
void AdjustDynamicNullComparisons(std::wstring& sql);                         /// Корректирует NULL сравнения для динамических параметров

// Основная функция для динамической замены параметров
std::wstring BuildSQLStringDynamic(const std::wstring& sql, const std::vector<SQLParameter>& parameters);

#endif
