# /pm-remember - Сохранить знания в project-memory

Анализирует недавний контекст беседы и автоматически сохраняет найденные знания в project-memory базу данных.

## Использование

```
/pm-remember                    # Автоматически найти и сохранить знания из последних сообщений
/pm-remember [что сохранить]    # Сохранить конкретную информацию
```

## Что делает команда

1. **Анализирует контекст беседы** (последние 5-10 сообщений)
2. **Извлекает ключевую информацию:**
   - Таблицы БД и их структуры → facts (schema)
   - Бизнес-правила и логику → facts (business_rule)
   - Связи между таблицами → facts (relationship) + relationships (структурированные)
   - Архитектурные паттерны → patterns
   - Формы и классы → form_mappings
   - Термины предметной области → glossary
   - SQL запросы → queries
   - Бизнес-процессы → processes
   - Направленные связи между сущностями → relationships (creates, contains, references, depends_on, produces, consumes)
   - Принадлежность к модулям → modules
3. **Сохраняет в project-memory** с правильными категориями
4. **Обновляет статус исследованности** через `set_entity_status(entity, coverage)`
5. **Выдает отчет** о том, что сохранено

## Автоматические триггеры (когда Claude должен использовать)

**ОБЯЗАТЕЛЬНО** использовать /pm-remember после:
- Выполнения `dbmcp` запросов (get_table_schema, get_table_relations, get_table_sample)
- Чтения файлов документации
- Ответа на вопрос "что такое X?" или "как работает Y?"
- Изучения нового класса или модуля
- Написания или оптимизации SQL запроса
- Обнаружения нового доменного термина
- Изучения связей между документами/таблицами
- Завершения задачи исследования или изучения кода

## Workflow для Claude

```
1. Пользователь спрашивает: "что такое Orders?"
2. Claude исследует → dbmcp → читает код → отвечает пользователю
3. Claude АВТОМАТИЧЕСКИ вызывает /pm-remember (или пользователь напоминает)
4. /pm-remember сохраняет все найденное в project-memory
5. Выдает отчет: "Сохранено: 4 facts, 1 glossary, 1 form_mapping"
```

## Что сохранять и куда

| Тип информации | Куда сохранять | Тип/Параметры |
|----------------|----------------|---------------|
| Структура таблицы БД (поля, типы, PK/FK) | facts | schema |
| Бизнес-правила, статусы, enum'ы | facts | business_rule |
| Связи между таблицами (FK, текст) | facts | relationship |
| **Направленные связи (A создаёт B)** | **relationships** | **rel_type: creates/contains/references/depends_on/produces/consumes** |
| Архитектурные паттерны | patterns | - |
| Формы и классы (dialog, viewform) | form_mappings | - |
| Доменные термины | glossary | - |
| SQL запросы (проверенные) | queries | - |
| Пошаговые workflow | processes | - |
| Важные особенности, gotchas | facts | gotcha |
| Статистика (количество записей) | facts | statistics |
| Примеры кода, usage patterns | facts | code_pattern |
| **Группировка сущностей по модулям** | **modules** | **save_module(module_name, entities[])** |
| **Уровень изученности сущности** | **entity_status** | **coverage: unknown/schema_only/partial/full/needs_review** |

## Правила сохранения

### DO (что нужно делать):
- Сохранять сразу после исследования
- Включать evidence (путь к файлу, номер строки)
- Помечать confidence: verified / unverified / uncertain
- Связывать сущности через `save_relationship(entity_from, entity_to, rel_type)`
- Сохранять синонимы для терминов
- Группировать сущности по модулям
- Обновлять статус после исследования: `set_entity_status(entity, coverage="full")`
- Использовать `update_fact(id, ...)` для исправления неточных фактов
- Использовать `delete_fact(id)` для удаления устаревших фактов
- Использовать `bulk_save(...)` для пакетного сохранения (3+ элементов)

### DON'T (чего избегать):
- Не откладывать сохранение "на потом"
- Не дублировать существующие факты (проверить через search_facts; исправлять через update_fact)
- Не сохранять очевидные вещи без контекста
- Не забывать про glossary при новых терминах
- Не сохранять непроверенный код или SQL
- Не забывать про relationships при обнаружении связей
- Не забывать обновлять entity_status после исследования

## Проверка сохранения

После /pm-remember всегда показывать отчет:
```
Сохранено в project-memory:
  - 4 facts (Orders: schema, business_rule, relationship, code_pattern)
  - 1 glossary (Purchase Order)
  - 1 form_mapping (Orders -> COrderDlg)
  - 2 relationships (Orders ->[contains] OrderLines, Orders ->[references] Suppliers)
  - 1 module (Sales += Orders)
  - 1 entity_status (Orders: full)

Можно найти:
  - search_facts(query="Orders")
  - get_glossary(term="Purchase Order")
  - find_form(table="Orders")
  - get_relationships(entity="Orders")
  - get_entity_status(entity="Orders")
```

## Интеграция с другими командами

- После исследования → автоматически `/pm-remember`
- После чтения документации → автоматически `/pm-remember`
- После серии dbmcp запросов → автоматически `/pm-remember`
- Перед завершением сессии → проверить `/pm-remember` всё ли сохранено

## Расположение БД

База данных project-memory настраивается в MCPHub через модуль Project Memory (поле "DB Path" в настройках модуля).
Все данные персистентны между сессиями Claude Code.
