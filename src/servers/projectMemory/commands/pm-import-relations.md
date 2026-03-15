# /pm-import-relations - Импорт FK-связей из БД в project-memory

Автоматический импорт всех foreign key связей для указанной сущности (таблицы) из живой базы данных в project-memory.

## Использование

```
/pm-import-relations [entity]     # entity: имя таблицы (например Orders, Products)
```

## Алгоритм выполнения

### Шаг 1: Проверить текущее состояние (project-memory)

Выполни параллельно:
- `get_entity_status(entity="$ARGUMENTS")` — текущее покрытие
- `get_relationships(entity="$ARGUMENTS")` — уже сохранённые связи

Запомни количество существующих связей (before).

### Шаг 2: Получить FK из живой БД (dbmcp)

```
dbmcp: get_table_relations(table_name="$ARGUMENTS")
```

Из результата собери два списка:
- **Outgoing FK** (таблица ссылается НА другие): entity_from=$ARGUMENTS, entity_to=referenced_table, rel_type="references"
- **Incoming FK** (другие ссылаются НА таблицу): entity_from=referencing_table, entity_to=$ARGUMENTS, rel_type="references"

### Шаг 3: Сохранить связи (project-memory)

Для каждого FK вызови `save_relationship`:
```
save_relationship(
  entity_from=<source_table>,
  entity_to=<target_table>,
  rel_type="references",
  description="FK: <column_name> -> <referenced_table>.<referenced_column>",
  evidence="dbmcp:get_table_relations, auto-imported"
)
```

**Оптимизация**: используй `bulk_save(relationships=[...])` если связей больше 3.

### Шаг 4: Обновить статус покрытия

```
set_entity_status(entity="$ARGUMENTS", coverage="schema_only", notes="FK relations auto-imported from dbmcp")
```

**Не понижай покрытие**: если текущее покрытие выше schema_only (partial или full), НЕ вызывай set_entity_status.

### Шаг 5: Отчёт

Выведи результат:

```
## Импорт связей: $ARGUMENTS

| Метрика | Значение |
|---------|----------|
| Outgoing FK | N |
| Incoming FK | M |
| Всего импортировано | N+M |
| Было связей в PM | K |
| Стало связей в PM | K+new |
| Покрытие | schema_only (было: unknown) |

### Импортированные связи
- $ARGUMENTS ->[references] TableA (FK: ColumnX)
- $ARGUMENTS ->[references] TableB (FK: ColumnY)
- TableC ->[references] $ARGUMENTS (FK: ColumnZ)
...
```

## Ограничения

- **Одна сущность за вызов** — для нескольких вызывай команду несколько раз
- **Только FK-связи** — бизнес-связи (creates, contains, produces) требуют ручного анализа
- **UPSERT** — повторный вызов безопасен, дубликаты не создаются (save_relationship — UPSERT по from+to+type)

## Примеры

### Пример 1: Новая таблица
```
/pm-import-relations Products
-> get_table_relations -> 51 FK (12 outgoing, 39 incoming)
-> bulk_save(relationships=[...]) -> 51 связь сохранена
-> set_entity_status("Products", "schema_only")
-> "Импортировано 51 FK-связей для Products"
```

### Пример 2: Уже частично изученная таблица
```
/pm-import-relations Orders
-> get_relationships -> 6 связей уже есть
-> get_table_relations -> 14 FK
-> bulk_save -> 8 новых связей (6 были, 8 добавлено)
-> Покрытие НЕ изменено (было partial, оставляем partial)
-> "Импортировано 8 новых FK-связей для Orders (было 6, стало 14)"
```

## Связанные команды

- `/pm-research [тема]` — полный цикл исследования (включает импорт связей)
- `/pm-verify [entity]` — проверка актуальности знаний в PM
- `/pm-lookup [запрос]` — поиск по project-memory
