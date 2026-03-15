# /pm-lookup - Поиск знаний в project-memory

Поиск накопленных знаний о бизнес-процессах, таблицах, формах и терминах проекта в базе project-memory.

## Использование

```
/pm-lookup [запрос]     # Поиск по ключевым словам
```

## Алгоритм выполнения

### Фаза 1: Широкий поиск (1 вызов)

Выполни `ask_business(question="$ARGUMENTS")` — агрегирует все 7 хранилищ PM в одном запросе.

> **Для coding-задач** (добавить, исправить, рефакторить) предпочти `prepare_context("$ARGUMENTS")`:
> даёт те же результаты + gap identification + federation для unknown entities + feedback boosting.

Результат — карточки сущностей с количеством совпадений по каждому источнику.

### Фаза 2: Проверка покрытия (для найденных сущностей)

Для каждой найденной сущности (до 3 основных) выполни `get_entity_status(entity)`.

Уровни покрытия:
- `full` — сущность полностью изучена, данных достаточно
- `partial` — изучена частично, могут быть пробелы
- `schema_only` — известна только структура таблицы
- `unknown` / не найден статус — сущность не исследована

### Фаза 3: Углублённый поиск (по необходимости)

Если нужны детали по конкретной сущности:
- `get_facts(entity)` — все факты
- `get_relationships(entity)` — структурированные связи (входящие + исходящие)
- `find_form(table=entity)` или `find_form(class_name=entity)` — формы

### Фаза 4: Рекомендации по пробелам

Для сущностей с недостаточным покрытием — предложи конкретные шаги:

**coverage = `unknown`** (сущность не исследована):
```
[EntityName] не исследована. Рекомендуется:
  -> dbmcp: get_table_schema('EntityName')
  -> dbmcp: get_table_relations('EntityName')
  -> dbmcp: get_table_sample('EntityName', limit=3)
  -> Grep: 'EntityName' в *.h для поиска C++ классов
  -> После исследования: /pm-remember, но только если появились evidence-backed reusable findings
```

**coverage = `schema_only`** (известна только схема):
```
[EntityName] изучена частично (только схема). Рекомендуется:
  -> dbmcp: get_table_sample('EntityName', limit=5) — примеры данных
  -> Grep: 'EntityName' в *Dlg.h, *ViewForm*.h — формы и диалоги
  -> Grep: 'EntityName' в *_SQL.h — SQL запросы
  -> Изучить бизнес-правила и статусы
  -> После исследования: /pm-remember, но только для curated save set
```

**coverage = `partial`** (частично изучена):
```
[EntityName] изучена частично. Проверь что не хватает:
  -> get_facts('EntityName') — какие типы фактов уже есть?
  -> Если нет business_rule -> изучить бизнес-логику
  -> Если нет form_mapping -> найти UI-формы
  -> Если нет relationships -> get_table_relations через dbmcp
```

**coverage = `full`**:
```
[EntityName] полностью изучена. Данных достаточно.
```

## Формат вывода

```
Результаты поиска: "$ARGUMENTS"

[Карточки сущностей из ask_business]

Покрытие:
  EntityA — full (12 facts, 3 relationships)
  EntityB — schema_only
  EntityC — unknown

Рекомендации:
  [конкретные шаги для сущностей с пробелами]
```

## Примеры

### Пример 1: Всё найдено
```
/pm-lookup обработка заказов
-> ask_business("обработка заказов") -> 5 сущностей
-> get_entity_status("Orders") -> full
-> Данных достаточно для ответа
```

### Пример 2: Есть пробелы
```
/pm-lookup управление складом
-> ask_business("управление складом") -> 2 сущности (Warehouse, WarehouseTransfer)
-> get_entity_status("Warehouse") -> schema_only
-> Рекомендуется: dbmcp:get_table_sample, Grep для форм
```

### Пример 3: Ничего не найдено
```
/pm-lookup маршрутная карта
-> ask_business("маршрутная карта") -> 0 сущностей
-> В project-memory нет данных. Начни исследование:
  -> dbmcp: list_objects(name_pattern='%Route%')
  -> dbmcp: search_columns(pattern='%route%')
  -> Grep: 'Route' в *.h
  -> После исследования: /pm-remember, но не сохраняй результаты исследования автоматически
```

## Расширенный поиск (при необходимости)

Для обзора покрытия:
- `list_entities()` — все известные сущности с количеством фактов
- `list_modules()` — все модули и их сущности
- `get_entity_status()` — статус всех сущностей

Для фильтрации:
- `search_facts(query, entity_filter, fact_type_filter, limit)` — точечный поиск

## Важно

- Этот skill — **первый шаг** при любом вопросе о бизнес-логике проекта
- `ask_business` уже агрегирует все источники — **не нужно** запускать 7 отдельных запросов
- Если project-memory содержит ответ с покрытием `full` — НЕ нужно идти в dbmcp
- Если покрытие недостаточное — исследуй через dbmcp и сохраняй через `/pm-remember` только evidence-backed reusable findings
