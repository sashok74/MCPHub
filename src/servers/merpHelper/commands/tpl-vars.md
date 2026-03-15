# Template Variables — Краткий справочник

Покажи пользователю этот справочник переменных, доступных в шаблонах helper (`render_template`, `save_template`). Выведи таблицы как есть, без вызова инструментов.

## Глобальные

| Переменная | Пример | Описание |
|---|---|---|
| `{{QUERY_NAME}}` | `doc_list_s` | Имя запроса (snake_case) |
| `{{QUERY_NAME_PASCAL}}` | `DocListS` | PascalCase (для типов, DB::) |
| `{{MODULE_NAME}}` | `DocApprove` | Имя модуля |
| `{{TABLE_NAME}}` | `dbo.DocList` | Целевая таблица |
| `{{DESCRIPTION}}` | `Список документов` | Описание запроса |
| `{{QUERY_TYPE}}` | `SELECT` | Тип: SELECT/INSERT/UPDATE/DELETE |
| `{{IN_COUNT}}` / `{{OUT_COUNT}}` | `3` / `30` | Кол-во входных/выходных параметров |

## Параметры (внутри `{{#IN_PARAMS}}` / `{{#OUT_PARAMS}}`)

| Переменная | Описание |
|---|---|
| `{{CPP_TYPE}}` | C++ тип: `int`, `std::string`, `float` |
| `{{MEMBER_NAME}}` | Имя поля: `m_iProductID` |
| `{{DB_NAME}}` | Имя колонки: `ProductID` |
| `{{DEFAULT_VALUE}}` | Значение по умолчанию: `0`, `L""` |
| `{{HAS_NULL_VALUE}}` | Флаг: nullable |
| `{{NULL_VALUE}}` | Sentinel: `GLOBAL_INVALID_ID` |
| `{{DISPLAY_EXPR}}` | *(OUT)* Выражение для отображения |
| `{{FROM_ROW_LINE}}` | *(OUT)* Код извлечения из строки БД |
| `{{IS_STRING}}` | Флаг: строковый тип (nvarchar, varchar, text) |
| `{{IS_NULLABLE}}` | Флаг: допускает NULL |
| `{{MAX_LENGTH}}` | Макс. длина строки (0 если N/A) |
| `{{IS_KEY_FIELD}}` | Флаг: первичный ключ |
| `{{FIELD_TYPE}}` | SQL тип: nvarchar, int, datetime и т.д. |
| `{{PRECISION}}` | Числовая точность (0 если N/A) |

## Ключевые поля

| Переменная | Описание |
|---|---|
| `{{IN_KEY_FIELD_NAME}}` / `{{OUT_KEY_FIELD_NAME}}` | Имя PK-поля |
| `{{IN_HAS_EXTRA_KEY}}` / `{{OUT_HAS_EXTRA_KEY}}` | Флаг: PK отличается от m_iID |
| `{{HAS_PID}}` / `{{PID_FIELD_NAME}}` | Parent ID |
| `{{HAS_TID}}` / `{{TID_FIELD_NAME}}` | Tree ID |

## Dataset (DS)

| Переменная | Пример | Описание |
|---|---|---|
| `{{#HAS_DS}}` | section | Секция: DS-поля заполнены |
| `{{DS_NAME}}` | `m_dsDocList` | Имя переменной датасета |
| `{{DS_QUERY_S}}` | `doc_list_s` | SELECT запрос |
| `{{DS_QUERY_I}}` | `doc_list_i` | INSERT запрос |
| `{{DS_QUERY_U}}` | `doc_list_u` | UPDATE запрос |
| `{{DS_QUERY_D}}` | `doc_list_d` | DELETE запрос |
| `{{DS_QUERY_ONE_S}}` | `doc_list_one_s` | Reload запрос |
| `{{DS_QUERY_*_PASCAL}}` | `DocListI` | PascalCase варианты (S/I/U/D/ONE_S) |

## Цикл (авто)

`{{_INDEX}}`, `{{_FIRST}}`, `{{_LAST}}` — доступны внутри `{{#IN_PARAMS}}` / `{{#OUT_PARAMS}}`

## Синтаксис

| Синтаксис | Назначение |
|---|---|
| `{{VARIABLE}}` | Подстановка значения |
| `{{#SECTION}}...{{/SECTION}}` | Цикл по массиву / условная секция (true) |
| `{{^SECTION}}...{{/SECTION}}` | Инвертированная секция (false/пусто) |
| `{{!comment}}` | Комментарий (игнорируется) |
