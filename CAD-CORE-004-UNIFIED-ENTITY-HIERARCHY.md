# CAD-CORE-004: Unified Entity Hierarchy

**Статус:** Спецификация и реализация  
**Дата:** 2026-10-25  
**Приоритет:** P0 (блокирует всё остальное)  
**Зависимости:** CAD-CORE-002 (EntityId), CAD-CORE-003 (CadDocument)  

---

## 1. Executive Summary

CAD-CORE-003 реализовала базовый CadDocument с иерархией Part → Instance. CAD-CORE-004 **расширяет иерархию на все 12 типов сущностей** и вводит **HierarchyAdapter** для совместимости со старой `Registry`.

**Ключевые достижения:**
- Обобщённая иерархия для всех entity types
- Путь через дерево (path traversal) с кэшированием
- Batch operations для перемещения поддеревьев
- Миграция из Registry → CadDocument
- Полное тестирование (15+ сценариев)

---

## 2. Архитектура

### 2.1 Расширенная иерархия (CadDocument)

Уже реализовано в CAD-CORE-003:
- `set_parent(child_id, parent_id)` — установка родителя с проверкой циклов
- `parent_of(id)` — получение родителя за O(1)
- `children_of(id)` — получение детей за O(1)
- `tree_order()` — BFS обход для UI визуализации
- `has_cycles()` — проверка на циклы через BFS

### 2.2 Расширение: Path Traversal (новое)

**Path** — это строка вида `/Part-001/Instance-A/Body/Sketch` для быстрого доступа.

```cpp
class CadDocument {
public:
    // Path-based access
    [[nodiscard]] std::optional<EntityId> find_by_path(const std::string& path) const;
    [[nodiscard]] std::string path_of(EntityId id) const;
    
    // Batch operations
    [[nodiscard]] bool move_subtree(EntityId child_id, EntityId new_parent_id);
    [[nodiscard]] std::vector<EntityId> subtree(EntityId root) const;
    
    // Sibling operations
    [[nodiscard]] std::vector<EntityId> siblings_of(EntityId id) const;
    [[nodiscard]] bool is_ancestor_of(EntityId potential_ancestor, EntityId potential_descendant) const;
    
    // Performance
    void rebuild_paths();  // Кэшировать пути для быстрого доступа
    
private:
    std::map<EntityId, std::string> path_cache_;  // Кэш путей
    std::map<std::string, EntityId> path_index_;  // Индекс для быстрого поиска
};
```

### 2.3 HierarchyAdapter (совместимость)

Мост между старой `bendcore::assembly::Registry` и новой `CadDocument`.

```cpp
class HierarchyAdapter {
public:
    explicit HierarchyAdapter(CadDocument& doc);
    
    // Миграция из Registry
    bool import_from_registry(const bendcore::assembly::Registry& old_registry);
    
    // Экспорт в Registry (для обратной совместимости)
    std::unique_ptr<bendcore::assembly::Registry> export_to_registry() const;
    
    // Вспомогательные методы
    [[nodiscard]] Part* get_part_by_designation(const std::string& designation);
    [[nodiscard]] std::vector<EntityId> find_instances_of_part(EntityId part_id) const;
    
private:
    CadDocument& document_;
};
```

---

## 3. Примеры использования

### 3.1 Базовая работа с иерархией

```cpp
CadDocument doc;

// Создаём Part
Part part;
part.id = EntityId(EntityId::Type::Part);
part.name = "Main Part";
EntityId part_id = doc.add_entity(part);

// Создаём Instance этой Part
Instance inst;
inst.id = EntityId(EntityId::Type::Instance);
inst.name = "Instance-1";
inst.part_id = part_id;
EntityId inst_id = doc.add_entity(inst);

// Устанавливаем иерархию
doc.set_parent(inst_id, part_id);  // Instance становится дитя Part

// Создаём Body в Instance
Body body;
body.id = EntityId(EntityId::Type::Body);
body.name = "Main Body";
EntityId body_id = doc.add_entity(body);
doc.set_parent(body_id, inst_id);  // Body становится дитя Instance
```

### 3.2 Работа с путями

```cpp
// Получить путь до Body
std::string path = doc.path_of(body_id);  
// "/Main Part/Instance-1/Main Body"

// Найти сущность по пути
auto found = doc.find_by_path("/Main Part/Instance-1");
if (found.has_value()) {
    Instance* inst = doc.get_entity<Instance>(found.value());
}

// Получить всё поддерево Body
auto subtree_ids = doc.subtree(body_id);
// Содержит Body и всех его потомков
```

### 3.3 Batch операции

```cpp
// Переместить Body из Instance-1 в Instance-2
doc.move_subtree(body_id, inst2_id);

// Получить всех братьев Body
auto siblings = doc.siblings_of(body_id);
// Все Body-like сущности в Instance-2
```

---

## 4. Спецификация методов

### 4.1 find_by_path(path)

**Входные данные:**
- `path`: строка вида "/Part-001/Instance-A/Body" или "/Part-001"

**Выход:**
- `std::optional<EntityId>`: ID найденной сущности или nullopt

**Алгоритм:**
1. Разбить path по "/"
2. Начать с root'ов (сущности без parent_id)
3. Для каждого компонента path:
   - Получить `children_of(current)`
   - Найти дитя с `name == component`
   - Если не найдено → return nullopt
   - Если найдено → current = это дитя

**Кэширование:**
- Пути кэшируются в `path_index_` для O(1) повторного доступа

### 4.2 path_of(id)

**Входные данные:**
- `id`: EntityId сущности

**Выход:**
- `std::string`: полный путь вида "/Part-001/Instance-A"

**Алгоритм:**
1. Проверить `path_cache_[id]`
2. Если есть → вернуть из кэша
3. Если нет:
   - Начать с `name` текущей сущности
   - Добавить "/" перед ней
   - Получить `parent_of(id)` и повторить рекурсивно
   - Кэшировать результат

### 4.3 move_subtree(child_id, new_parent_id)

**Входные данные:**
- `child_id`: сущность, которую перемещаем
- `new_parent_id`: новый родитель

**Выход:**
- `bool`: успешность операции

**Проверки:**
1. `child_id.is_valid()` && `new_parent_id.is_valid()`
2. `child_id != new_parent_id` (нельзя быть сам себе родителем)
3. `!is_ancestor_of(child_id, new_parent_id)` (проверка циклов)

**Операция:**
1. Получить старого родителя `old_parent`
2. Удалить `child_id` из `hierarchy_[old_parent]`
3. Вызвать `set_parent(child_id, new_parent_id)`
4. Инвалидировать кэш путей подерева
5. `increment_revision()`

### 4.4 subtree(root)

**Входные данные:**
- `root`: ID сущности (корень поддерева)

**Выход:**
- `std::vector<EntityId>`: BFS обход от root'а (включая root)

**Алгоритм:** стандартный BFS

### 4.5 siblings_of(id)

**Входные данные:**
- `id`: ID сущности

**Выход:**
- `std::vector<EntityId>`: все дети одного родителя

**Реализация:**
```cpp
EntityId parent = parent_of(id);
if (!parent.is_valid()) return {};  // root нет братьев
return children_of(parent);
```

### 4.6 is_ancestor_of(potential_ancestor, potential_descendant)

**Входные данные:**
- `potential_ancestor`: ID потенциального предка
- `potential_descendant`: ID потенциального потомка

**Выход:**
- `bool`: true если potential_ancestor действительно предок

**Алгоритм:**
1. Начать с potential_descendant
2. Получить `parent_of(current)`
3. Если parent == potential_ancestor → return true
4. Если parent == invalid → return false (достигли root'а)
5. Repeat шаги 2-4

**Оптимизация:** кэшировать результаты для часто проверяемых пар

---

## 5. HierarchyAdapter спецификация

### 5.1 import_from_registry(old_registry)

**Входные данные:**
- `old_registry`: bendcore::assembly::Registry с Part/Instance

**Выход:**
- `bool`: успешность импорта

**Алгоритм:**
1. Для каждого Part в Registry:
   - Создать Part сущность в CadDocument
   - Сохранить mapping old_id → new_EntityId
2. Для каждого Instance в Registry:
   - Создать Instance сущность в CadDocument
   - Установить Part-Instance иерархию через `set_parent`
3. Сохранить primary_instance отношение

**Обработка ошибок:**
- Если Part имеет невалидный ID → логировать и пропустить
- Если Instance ссылается на несуществующий Part → создать orphaned Instance

### 5.2 export_to_registry()

**Выход:**
- `std::unique_ptr<bendcore::assembly::Registry>`: новый Registry

**Алгоритм:**
1. Для каждого Part в CadDocument:
   - Создать Part в Registry с теми же данными
2. Для каждого Instance в CadDocument:
   - Создать Instance в Registry
   - Установить part_id через parent_of()
3. Восстановить primary_instance отношения

---

## 6. Тестирование

### 6.1 Базовые тесты (test_hierarchy_extended.cpp)

1. **test_find_by_path_simple** — поиск в глубокой иерархии
2. **test_find_by_path_nonexistent** — поиск несуществующего пути
3. **test_path_of_entity** — получение пути для сущности
4. **test_path_cache_consistency** — проверка кэша путей
5. **test_move_subtree_simple** — перемещение поддерева
6. **test_move_subtree_cycle_prevention** — блокировка циклов при перемещении
7. **test_subtree_enumeration** — полный список поддерева
8. **test_siblings_of** — получение братьев

### 6.2 Тесты HierarchyAdapter

9. **test_import_from_registry** — миграция Registry → CadDocument
10. **test_export_to_registry** — экспорт CadDocument → Registry
11. **test_get_part_by_designation** — поиск по обозначению
12. **test_find_instances_of_part** — поиск всех Instance для Part

### 6.3 Edge Cases

13. **test_deep_hierarchy** — 100+ уровней вложенности
14. **test_circular_reference_detection** — попытка создать цикл
15. **test_batch_operations** — перемещение 1000+ entities

---

## 7. Интеграция

### 7.1 С Registry (обратная совместимость)

```cpp
// Старый код продолжает работать через adapter
HierarchyAdapter adapter(doc);
auto old_registry = adapter.export_to_registry();
// Передать old_registry в старый код
```

### 7.2 С UI (React)

```cpp
// UI использует path_of для доступа к сущностям
auto body_path = doc.path_of(body_id);
// Отправить "/Part-1/Instance-1/Body" в React
// React отправит path обратно, UI преобразует в ID
auto result = doc.find_by_path(received_path);
```

### 7.3 С параметрами (CAD-CORE-009)

```cpp
// ParameterStore будет использовать tree_order()
auto order = doc.tree_order();
for (auto id : order) {
    auto params = param_store.get_parameters_for(id);
}
```

---

## 8. Performance Characteristics

| Операция | Сложность | Примечание |
|---------|----------|-----------|
| parent_of() | O(1) | map lookup |
| children_of() | O(k) | k = количество детей |
| set_parent() | O(1) | (+ O(k) для rebuild_tree_order) |
| move_subtree() | O(n) | n = размер поддерева |
| find_by_path() | O(d) | d = глубина в tree (кэшируется) |
| path_of() | O(d) | рекурсивно вверх до root'а (кэшируется) |
| has_cycles() | O(n) | BFS через все entities |
| subtree() | O(n) | BFS поддерева |
| is_ancestor_of() | O(d) | max глубина = O(n) в худшем случае |

---

## 9. Миграция данных

### 9.1 Registry → CadDocument

```
Старая система (bendcore::assembly::Registry)
  ├─ Part { id, designation, data, instances, primary }
  └─ Instance { id, part_id, placement, adaptive }
        ↓ (миграция)
Новая система (CadDocument)
  ├─ Part { id, name, designation, instance_ids, primary_instance }
  ├─ Instance { id, name, part_id, adaptive }
  └─ Body, Sketch, Feature... (в будущих фазах)
```

**Ключевые отличия:**
1. EntityId вместо string
2. Обобщённая иерархия (не только Part → Instance)
3. Единый source of truth (не разбросано по разным компонентам)

---

## 10. Deliverables

- ✓ `include/bendcore/document/cad_document.hpp` — добавить методы пути/перемещения
- ✓ `src/document/cad_document.cpp` — реализовать методы
- ✓ `include/bendcore/document/hierarchy_adapter.hpp` — новый файл
- ✓ `src/document/hierarchy_adapter.cpp` — реализация adapter'а
- ✓ `tests/document/test_hierarchy_extended.cpp` — 15+ тестов
- ✓ `tests/document/test_hierarchy_adapter.cpp` — тесты миграции
- ✓ CMakeLists.txt — добавить новые исходники и тесты
