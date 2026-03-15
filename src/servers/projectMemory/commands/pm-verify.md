# /pm-verify - Проверка актуальности знаний в project-memory

Сравнивает знания о сущности в project-memory с текущим состоянием живой базы данных. Находит расхождения: добавленные/удалённые/изменённые колонки, новые FK, устаревшие факты.

## Использование

```
/pm-verify [entity]     # entity: имя таблицы (например Orders, Products)
```

## Алгоритм выполнения

### Шаг 1: Получить знания из PM (project-memory)

Выполни параллельно:
- `get_facts(entity="$ARGUMENTS", fact_type="schema")` — сохранённые схемы
- `get_relationships(entity="$ARGUMENTS")` — сохранённые связи
- `get_entity_status(entity="$ARGUMENTS")` — текущее покрытие

Если покрытие = `unknown` и фактов нет -> сообщи:
```
"Сущность $ARGUMENTS не найдена в project-memory. Используйте /pm-research $ARGUMENTS для первичного исследования."
```
И завершай.

### Шаг 2: Получить актуальное состояние из БД (dbmcp)

Выполни параллельно:
- `dbmcp: get_table_schema(table="$ARGUMENTS")` — текущая схема
- `dbmcp: get_table_relations(table_name="$ARGUMENTS")` — текущие FK

### Шаг 3: Сравнить схемы

Из фактов типа `schema` извлеки список известных колонок (парси текст описания).
Из `get_table_schema` получи актуальный список колонок.

Определи:
- **Новые колонки** — есть в БД, нет в PM
- **Удалённые колонки** — есть в PM, нет в БД
- **Изменённые типы** — колонка есть в обоих, но тип данных отличается

### Шаг 4: Сравнить связи

Из `get_relationships` получи список сохранённых FK.
Из `get_table_relations` получи актуальный список FK.

Определи:
- **Новые FK** — есть в БД, нет в PM
- **Удалённые FK** — есть в PM, нет в БД

### Шаг 5: Обновить PM (если есть расхождения)

**Если найдены расхождения:**
- `set_entity_status(entity="$ARGUMENTS", coverage="needs_review", notes="Verification found differences: N new columns, M removed columns, K new FK")`
- Для новых FK: `bulk_save(relationships=[...])` — автоматически добавить

**Если расхождений нет:**
- Подтверди актуальность, обнови timestamp:
- `set_entity_status(entity="$ARGUMENTS", coverage=<текущее>, notes="Verified: schema matches DB as of YYYY-MM-DD")`

### Шаг 6: Отчёт

```
## Верификация: $ARGUMENTS

### Статус
Актуально / Есть расхождения / Сильно устарело

### Покрытие
| До проверки | После проверки |
|-------------|----------------|
| full        | needs_review   |

### Расхождения в схеме
| Тип | Колонка | В PM | В БД |
|-----|---------|------|------|
| Добавлена | NewColumnName | — | nvarchar(100) |
| Удалена | OldColumnName | int | — |
| Изменена | ChangedCol | varchar(50) | nvarchar(100) |

(Если расхождений нет: "Схема полностью соответствует БД")

### Расхождения в связях
| Тип | Связь | Описание |
|-----|-------|----------|
| Новый FK | $ARGUMENTS -> NewTable | FK: NewColumnID |
| Удалён FK | $ARGUMENTS -> OldTable | Был в PM, нет в БД |

(Если расхождений нет: "Все FK-связи актуальны")

### Рекомендации
- [конкретные действия если нужны: обновить факт #ID, удалить устаревший факт, запустить /pm-research]
```

## Классификация расхождений

- **Актуально**: 0 расхождений в схеме, 0-2 новых FK (незначительные)
- **Есть расхождения**: 1-5 изменений в схеме ИЛИ 3+ новых FK
- **Сильно устарело**: 5+ изменений в схеме ИЛИ удалённые колонки ИЛИ удалённые FK

## Ограничения

- **Одна сущность за вызов**
- **Сравнение текстовое** — парсит описания фактов, может пропустить нюансы
- **Не обновляет факты автоматически** — только помечает `needs_review` и добавляет новые FK
- **Для полного обновления** после верификации используй `/pm-research $ARGUMENTS`

## Примеры

### Пример 1: Актуальная сущность
```
/pm-verify Orders
-> PM: 3 schema facts, 14 relationships, coverage=full
-> DB: 28 columns, 14 FK
-> Сравнение: 0 расхождений
-> "Актуально. Схема Orders полностью соответствует БД."
-> set_entity_status("Orders", "full", "Verified: matches DB as of 2026-03-15")
```

### Пример 2: Устаревшая сущность
```
/pm-verify Products
-> PM: 1 schema fact (из 2025), 6 relationships, coverage=partial
-> DB: 45 columns, 51 FK
-> Сравнение: 3 новые колонки, 45 FK не в PM
-> "Есть расхождения: 3 новые колонки, 45 FK отсутствуют в PM"
-> set_entity_status("Products", "needs_review")
-> bulk_save(relationships=[45 новых FK])
-> "Рекомендация: запустите /pm-research Products для полного обновления"
```

### Пример 3: Неизвестная сущность
```
/pm-verify SomeNewTable
-> PM: coverage=unknown, 0 фактов
-> "Сущность SomeNewTable не найдена в project-memory."
-> "Используйте /pm-research SomeNewTable для первичного исследования."
```

## Связанные команды

- `/pm-import-relations [entity]` — только импорт FK (без верификации схемы)
- `/pm-research [тема]` — полный цикл исследования
- `/pm-lookup [запрос]` — поиск по project-memory
