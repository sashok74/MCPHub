//---------------------------------------------------------------------------

#pragma hdrstop

#include "sqlHelper.h"
#include <vector>
#include <regex>
#include <algorithm>
#include <iostream>
#include <sstream>

//---------------------------------------------------------------------------
#pragma package(smart_init)

// Функция извлечения текста, заключённого в кавычки (поддерживаются префиксы _T и L).
// Функция для извлечения текста, исключая строки с комментарием `//`.
std::string extract_text(const std::string& input) {
	std::string result;
    // Регулярное выражение для извлечения текста в кавычках
	std::regex regex_pattern(R"([L_T]*\"(.*?)\")");

	std::istringstream stream(input);
	std::string line;

	// Переходим по строкам исходного текста
	while (std::getline(stream, line)) {
        // Проверяем, начинается ли строка с комментария "//"
		size_t comment_pos = line.find("//");
		if (comment_pos != std::string::npos) {
			// Если находится комментарий, убираем его: просто продолжаем
            continue;
		}

		// Ищем совпадения в строках, которые не содержат комментариев
		auto words_begin = std::sregex_iterator(line.begin(), line.end(), regex_pattern);
		auto words_end = std::sregex_iterator();

        for (std::sregex_iterator it = words_begin; it != words_end; ++it) {
			std::smatch match = *it;
			if (!result.empty()) {
                result += " ";  // Добавляем пробел между найденными выражениями
			}
			result += match[1].str();
		}
    }

    return result;
}


// Функция поиска спецификаторов формата и замены их на '?'
void find_format_specifiers(std::string& duty_string, std::vector<char>& source_param) {
    source_param.clear();
    // Сначала заменяем спецификаторы в кавычках (например, '%s') на ?
    std::regex quoted_fmt(R"('(%(?!%)(\d+\$)?[-+#0 ]*(\*|\d+)*(\.(\*|\d+))?[hlLzjtI]*(?:64|32)?[diuoxXfFeEgGaAcCsSpn])')");
    std::smatch match;
    std::string result;
    std::string::const_iterator searchStart(duty_string.cbegin());
    size_t last_pos = 0;
    while (std::regex_search(searchStart, duty_string.cend(), match, quoted_fmt)) {
        result.append(searchStart, match[0].first);
        char spec = match.str(1).back();
        source_param.push_back(toupper(spec));
        result += "?";
        searchStart = match[0].second;
    }
    result.append(searchStart, duty_string.cend());
    duty_string = result;

    // Теперь заменяем обычные спецификаторы формата (без кавычек)
    std::regex plain_fmt(R"(%(?!%)(\d+\$)?[-+#0 ]*(\*|\d+)*(\.(\*|\d+))?[hlLzjtI]*(?:64|32)?[diuoxXfFeEgGaAcCsSpn])");
    result.clear();
    searchStart = duty_string.cbegin();
    while (std::regex_search(searchStart, duty_string.cend(), match, plain_fmt)) {
        result.append(searchStart, match[0].first);
        char spec = match.str().back();
        source_param.push_back(toupper(spec));
        result += "?";
        searchStart = match[0].second;
    }
    result.append(searchStart, duty_string.cend());
    duty_string = result;
}

// Функция поиска параметров начинающихся с : и замены их на '?'
void find_parameters (std::string& duty_string, std::vector<std::string>& source_param) {
	source_param.clear();
	for (size_t pos = 0; pos < duty_string.size(); ++pos) {
		if (duty_string[pos] == ':' && pos + 1 < duty_string.size()) {
			size_t end_pos = pos + 1;
            while (end_pos < duty_string.size() && (isalnum(duty_string[end_pos]) || duty_string[end_pos] == '_')) {
                end_pos++;
            }

            // Извлекаем параметр между двоеточием и следующим не-алфавитным символом
            std::string param = duty_string.substr(pos + 1, end_pos - (pos + 1));
			if (!param.empty()) {
                source_param.push_back(param); // Сохраняем параметр
                duty_string.replace(pos, end_pos - pos, "?"); // Заменяем на '?'
                pos--; // Смещаем позицию для следующей проверки
            }
        }
    }
}


void replace_escape_sequences(std::string &sql_text) {
    size_t pos = 0;
    // Заменяем "\n" на реальный символ новой строки (перенос строки)
	while ((pos = sql_text.find("\\n", pos)) != std::string::npos) {
		sql_text.replace(pos, 2, "\n");
		pos += 1;  // Двигаемся вперёд
    }

    // Заменяем "\t" на реальный символ табуляции
	pos = 0;
	while ((pos = sql_text.find("\\t", pos)) != std::string::npos) {
        sql_text.replace(pos, 2, "\t");
        pos += 1;  // Двигаемся вперёд
	}

	// Избавляемся от возвратов каретки если требуется
	pos = 0;
	while ((pos = sql_text.find("\\r", pos)) != std::string::npos) {
		sql_text.erase(pos, 2);
	}
}


// Определяет, нужно ли заключать значение в кавычки
bool RequiresQuotes(const std::wstring& type)
{
	std::wstring upperType = type;
	std::transform(upperType.begin(), upperType.end(), upperType.begin(), ::towupper);

	return upperType.find(L"VARCHAR") != std::wstring::npos ||
		   upperType.find(L"NVARCHAR") != std::wstring::npos ||
		   upperType.find(L"TEXT") != std::wstring::npos ||
		   upperType.find(L"CHAR") != std::wstring::npos ||
		   upperType.find(L"DATE") != std::wstring::npos ||
		   upperType.find(L"DATETIME") != std::wstring::npos ||
		   upperType.find(L"TEXT") != std::wstring::npos ||
		   upperType.find(L"TIME") != std::wstring::npos;
}

// Экранирует одинарные кавычки в строке
std::wstring EscapeString(const std::wstring& value)
{
	std::wstring result = value;
	size_t pos = 0;
	while ((pos = result.find(L'\'', pos)) != std::wstring::npos)
	{
		result.replace(pos, 1, L"''");
		pos += 2;
	}
	return result;
}

// Форматирует значение в зависимости от типа
std::wstring FormatValue(const SQLParameter& param)
{
	// Проверяем на NULL
	if (param.m_sValue == L"NULL")
	{
		return L"NULL";
	}

	// Если тип требует кавычек
	if (RequiresQuotes(param.m_sType))
	{
		return L"'" + EscapeString(param.m_sValue) + L"'";
	}

	// Числовые типы, BIT и прочие - без кавычек
    return param.m_sValue;
}

// Заменяет параметр в SQL строке
void ReplaceDynamicParameter(std::wstring& sql, const SQLParameter& param)
{
    std::wstring placeholder = L":" + param.m_sName;
	std::wstring formattedValue = FormatValue(param);

	// Используем word boundary для точного совпадения
    std::wregex regex(L"(" + placeholder + L")\\b");
	sql = std::regex_replace(sql, regex, formattedValue);
}

// Корректирует NULL сравнения для динамических параметров
void AdjustDynamicNullComparisons(std::wstring& sql)
{
	std::wstring lowered_sql = sql;
    std::transform(lowered_sql.begin(), lowered_sql.end(), lowered_sql.begin(), ::towlower);

	bool bSelectFind = lowered_sql.find(L"select ") != std::wstring::npos;
	bool bDeclareFind = lowered_sql.find(L"declare ") != std::wstring::npos;

	if (bSelectFind && !bDeclareFind)
	{
		// Обрабатываем различные варианты сравнения с NULL
		std::wregex regex_not_equals_null(L"\\s*(!= |<> )NULL\\b", std::regex_constants::icase);
		sql = std::regex_replace(sql, regex_not_equals_null, L" IS NOT NULL");

		std::wregex regex_equals_null(L"\\s*= NULL\\b", std::regex_constants::icase);
		sql = std::regex_replace(sql, regex_equals_null, L" IS NULL");

		// Обрабатываем IN (NULL) и NOT IN (NULL)
		std::wregex regex_in_null(L"\\s+IN\\s*\\(\\s*NULL\\s*\\)", std::regex_constants::icase);
		sql = std::regex_replace(sql, regex_in_null, L" IS NULL");

		std::wregex regex_not_in_null(L"\\s+NOT\\s+IN\\s*\\(\\s*NULL\\s*\\)", std::regex_constants::icase);
		sql = std::regex_replace(sql, regex_not_in_null, L" IS NOT NULL");
	}
}

void ReplaceUnusedPlaceholders(std::wstring &sql) {
    std::wregex regex(L"\\s:[a-zA-Z0-9_]+(\\s|[),;])");
    sql = std::regex_replace(sql, regex, L" NULL$1");
}

// Основная функция для динамической замены параметров
std::wstring BuildSQLStringDynamic(const std::wstring& sql, const std::vector<SQLParameter>& parameters)
{
	std::wstring result = sql;

	// Заменяем все параметры
	for (const auto& param : parameters)
	{
		ReplaceDynamicParameter(result, param);
	}

	// Заменяем неиспользованные плейсхолдеры на NULL
	ReplaceUnusedPlaceholders(result);

	// Корректируем NULL сравнения
	AdjustDynamicNullComparisons(result);

	return result;
}

