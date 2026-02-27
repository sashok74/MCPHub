# /ui-navigate — Smart UI navigation with route caching

Navigate to any application form, fill fields, verify behavior.
Learns from every interaction — saves routes to project-memory.

## Аргумент

Свободный текст — куда навигироваться и что делать.

```
/ui-navigate order form, find field Number, read value
/ui-navigate materials transfer, fill all fields, check logic
/ui-navigate snapshot
```

## Инструкция

Используй инструмент **Task** для запуска агента `ui-navigate`:

```
Task(
  subagent_type="ui-navigate",
  prompt="Выполни навигацию: $ARGUMENTS",
  description="UI navigate: <краткое описание>"
)
```

Передай полный текст аргумента в prompt агента. Агент самостоятельно:
1. Проверит project-memory на наличие кэшированного маршрута
2. Откроет нужную форму (или использует кэш)
3. Установит параметры, прочитает значения, проверит поведение
4. Сохранит успешный маршрут для повторного использования
5. Вернёт сводку состояния формы

После получения результата от агента — выведи пользователю краткую сводку.
