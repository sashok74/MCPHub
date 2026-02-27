# /navigate — Универсальная навигация к форме десктоп-приложения

Быстрая навигация к нужному состоянию любой формы приложения.
Вся логика берётся из project-memory + UI snapshot. Никаких хардкодов.

## Аргумент

Свободный текст — куда навигироваться.

```
/navigate orders list
/navigate purchase order form, supplier "Acme"
/navigate materials transfer, warehouse "Main"
/navigate snapshot
```

## Алгоритм

### Шаг 1: Сбор контекста из project-memory

Извлеки из аргумента ключевые слова (форма, параметры).

```
# 1. Что мы знаем об этой форме?
ask_business(question="<форма из аргумента>", mode="compact")

# 2. Маппинг формы → тип для ui_open_document
find_form(query="<ключевое слово>")

# 3. Контролы формы — IDs, gotchas
get_facts(entity="<Entity>_Form")
```

Из результатов извлечь:
- **formType** → тип для `ui_open_document` (из find_form или ask_business)
- **control IDs** → combobox, tree, tab, checkbox (из facts)
- **gotchas** → известные ограничения

Если данных недостаточно — они будут получены из snapshot.

### Шаг 2: Если аргумент = "snapshot"

```
ui_get_snapshot(scope="mdi")
```
Вывести краткую сводку текущего состояния. Готово.

### Шаг 3: Открыть форму

```
ui_open_document(type=formType, id=0)
sleep 2
```

### Шаг 4: Snapshot + определение контролов

```
ui_get_snapshot(scope="mdi")
```

Из snapshot определить все доступные контролы:
- **ComboBox** → по label (Static рядом) или items
- **TabControl** → по tabs list
- **TreeView** → по nodes
- **ListView** → по columns
- **CheckBox** → по caption

### Шаг 5: Установить параметры

Для каждого параметра из аргумента:

1. **Найти контрол** — сопоставить параметр с контролом:
   - По label Static: "Type:" → ближайший ComboBox
   - По items содержимому: если параметр = "Main", искать в ComboBox items
   - По facts из project-memory: знания о назначении контрола

2. **Установить значение**:
   - ComboBox: `ui_set_control(name=id, value=text_or_index, scope="mdi")`
   - TreeView: `ui_set_control(name=id, value="node text", scope="mdi")`
   - TabControl: `ui_set_control(name=id, value=index, scope="mdi")`
   - CheckBox: `ui_set_control(name=id, value="true"/"false", scope="mdi")`
   - ListView: `ui_mouse_click(x=..., y=...)` (см. формулу ниже)

3. **Пауза**: `sleep 1` между операциями

4. **Порядок**: ComboBox-ы первыми (могут менять содержимое дерева/грида), потом TreeView, потом Tab, потом Grid

### Шаг 6: Финальный snapshot и сводка

```
ui_get_snapshot(scope="mdi")
```

Вывести краткую сводку:
- Форма: название
- Вкладка: текущая (если есть TabControl)
- Ключевые значения контролов (ComboBox text, CheckBox state)
- Дерево: itemCount, selectedItem
- Гриды: itemCount, selectedCount, selectedIndex

**НЕ выводить полный JSON** — только осмысленную сводку.

## Выбор строки в гриде (ListView)

`ui_set_control` может не работать для обёрнутых гридов.
Используй `ui_mouse_click`:

```
y = gridRect.top + 26 + rowIndex * 23 + 12
x = (gridRect.left + gridRect.right) / 2
```

## Правила

- **project-memory FIRST** — всегда начинай со сбора контекста
- **scope="mdi"** — для всех операций с контролами
- **Не хардкодить ID** — всё из runtime (pm + snapshot)
- **Ribbon без scope** — для чтения ribbon: `ui_get_snapshot()` без scope
- Если форма уже открыта — не переоткрывать, только менять параметры
- Grid selection — ТОЛЬКО через mouse_click
