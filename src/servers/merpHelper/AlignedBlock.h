//---------------------------------------------------------------------------

#ifndef AlignedBlockH
#define AlignedBlockH
//---------------------------------------------------------------------------

#include <vector>
#include <string>
#include <sstream>
#include <algorithm>
#include <vector>
#include <System.Classes.hpp> // ƒл€ TStringBuilder

// ‘ункци€ принимает вектор строк, где кажда€ строка Ц это вектор полей (std::string).

using TCodeBlock = std::vector<std::vector<UnicodeString>>;



UnicodeString AlignedBlock(const TCodeBlock& lines);

std::vector<String> SplitSQLByLines(const UnicodeString& sql);

#endif
