//---------------------------------------------------------------------------

#pragma hdrstop

#include "AlignedBlock.h"
//---------------------------------------------------------------------------
#pragma package(smart_init)

UnicodeString AlignedBlock(const TCodeBlock& lines) {
	if (lines.empty()) return UnicodeString();

	// Определяем максимальное количество колонок
	size_t maxCols = 0;
	for (const auto& line : lines) {
		maxCols = std::max(maxCols, line.size());
	}

	// Находим максимальную длину для каждой колонки
	std::vector<int> maxWidths(maxCols, 0);
	for (const auto& line : lines) {
		for (size_t i = 0; i < line.size(); ++i) {
			maxWidths[i] = std::max(maxWidths[i], line[i].Length());
		}
	}

	// Используем TStringBuilder для эффективной сборки строки
	//TStringBuilder* builder = new TStringBuilder();
	String builder;
	for (const auto& line : lines) {
		String l;
		for (size_t i = 0; i < line.size(); ++i) {
			l += line[i];
            // Добавляем пробелы для выравнивания, если это не последняя колонка
			if (i < line.size() - 1) {
				int padding = maxWidths[i] - line[i].Length();
				l +=  UnicodeString::StringOfChar(L' ', padding + 1);
			}
		}
		builder += l + '\n';
	}

	// Сохраняем результат перед удалением builder
	//UnicodeString result = builder->ToString();
   //	delete builder; // Освобождаем память
	return builder;  // Возвращаем полный результат
}

std::vector<String> SplitSQLByLines(const UnicodeString& sql) {
    std::vector<String> result;
    TStringList* lines = new TStringList();
    try {
        lines->Text = sql; // Автоматически разбивает по переносам строки
        for(int i = 0; i < lines->Count; i++) {
            result.push_back(lines->Strings[i]);
        }
    }
    __finally {
        delete lines;
    }
    return result;
}
