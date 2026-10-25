# CAD-CORE-005: Command Engine

**Статус:** Спецификация и начало реализации  
**Дата:** 2026-10-25  
**Приоритет:** P0 (блокирует Phase B и C)  
**Зависимости:** CAD-CORE-003 (CadDocument), CAD-CORE-004 (Hierarchy)  

---

## 1. Executive Summary

CAD-CORE-005 реализует **Command Engine** — систему для **безопасного выполнения команд с валидацией, откатом и историей**.

**Ключевые достижения (Phase B начало):**
- Command pattern с валидацией
- Transactional execution с atomicity гарантиями
- Command результаты с детальной информацией об ошибках
- Versioning для конфликтной детекции
- Event-driven уведомления о изменениях

---

## 2. Архитектура

### 2.1 Command Model

```cpp
// include/bendcore/commands/command.hpp

struct Command {
    std::string id;              // Уникальный идентификатор команды
    std::string type;            // Тип: "create_part", "add_instance", etc.
    json args;                   // Аргументы команды
    std::string document_version; // Версия документа при выполнении
    std::chrono::system_clock::time_point timestamp;
};

struct CommandResult {
    bool success;
    std::string error_message;   // Если success = false
    json result_data;            // Результат команды
    std::string new_document_version;  // Новая версия после выполнения
    std::vector<EntityId> affected_ids; // Какие сущности изменились
};
```

### 2.2 Command Types (MVP)

**Phase B MVP:** 6 команд для базовых операций

1. **CreatePart** — создать новую Part
   - Args: designation, TubeSignatureData
   - Result: part_id

2. **CreateInstance** — создать Instance для Part
   - Args: part_id, placement (optional)
   - Result: instance_id

3. **SetParent** — изменить иерархию
   - Args: child_id, new_parent_id
   - Result: success

4. **DeleteEntity** — удалить сущность
   - Args: entity_id
   - Result: deleted_ids (всё поддерево)

5. **EditPart** — изменить обозначение Part
   - Args: part_id, new_designation
   - Result: old_designation

6. **BatchMove** — переместить несколько entities
   - Args: moves: [(child_id, new_parent_id), ...]
   - Result: success_count, failure_count

### 2.3 CommandValidator

```cpp
class CommandValidator {
public:
    struct ValidationError {
        std::string code;  // "INVALID_ENTITY", "CYCLE_DETECTED", etc.
        std::string message;
        std::vector<std::string> details;
    };

    [[nodiscard]] std::optional<ValidationError> validate(
        const Command& cmd,
        const CadDocument& doc
    ) const;

private:
    bool validate_create_part(const json& args) const;
    bool validate_entity_exists(EntityId id, const CadDocument& doc) const;
    bool validate_no_cycles(EntityId parent, EntityId child, const CadDocument& doc) const;
};
```

### 2.4 CommandExecutor

```cpp
class CommandExecutor {
public:
    explicit CommandExecutor(CadDocument& doc);

    CommandResult execute(const Command& cmd);

private:
    CadDocument& document_;
    CommandValidator validator_;

    // Обработчики для каждого типа команды
    CommandResult execute_create_part(const Command& cmd);
    CommandResult execute_create_instance(const Command& cmd);
    CommandResult execute_set_parent(const Command& cmd);
    CommandResult execute_delete_entity(const Command& cmd);
    CommandResult execute_edit_part(const Command& cmd);
    CommandResult execute_batch_move(const Command& cmd);
};
```

---

## 3. Примеры использования

### 3.1 Простое выполнение команды

```cpp
CadDocument doc;
CommandExecutor executor(doc);

Command create_part;
create_part.id = "cmd-001";
create_part.type = "CreatePart";
create_part.args = json{
    {"designation", "PART-001"},
    {"data", json{{"od", 25.4}, {"wall", 2.0}}}
};
create_part.document_version = "0";

CommandResult result = executor.execute(create_part);
if (result.success) {
    auto part_id = result.result_data["part_id"];
    // Part успешно создана
} else {
    std::cerr << "Error: " << result.error_message << "\n";
}
```

### 3.2 Валидация перед выполнением

```cpp
CommandValidator validator;

auto validation_error = validator.validate(cmd, doc);
if (validation_error.has_value()) {
    std::cerr << "Validation failed: " << validation_error->message << "\n";
    for (const auto& detail : validation_error->details) {
        std::cerr << "  - " << detail << "\n";
    }
    return;
}

// Валидация прошла, можно выполнить
auto result = executor.execute(cmd);
```

---

## 4. Спецификация команд

### 4.1 CreatePart

**Входные аргументы:**
```json
{
    "designation": "PART-001",
    "data": {
        "od": 25.4,
        "wall": 2.0,
        "length": 500.0
    }
}
```

**Результат:**
```json
{
    "part_id": "EntityId(...)"
}
```

**Валидация:**
- designation не пусто
- od > 0, wall > 0, length > 0

**Операция:**
1. Создать Part сущность
2. Добавить в CadDocument
3. Вернуть part_id

### 4.2 CreateInstance

**Входные аргументы:**
```json
{
    "part_id": "EntityId(...)",
    "placement": {
        "x": 0, "y": 0, "z": 0
    }
}
```

**Результат:**
```json
{
    "instance_id": "EntityId(...)"
}
```

**Валидация:**
- part_id существует в документе
- part_id имеет тип Part

### 4.3 SetParent

**Входные аргументы:**
```json
{
    "child_id": "EntityId(...)",
    "parent_id": "EntityId(...)"
}
```

**Результат:**
```json
{
    "success": true
}
```

**Валидация:**
- child_id и parent_id существуют
- child_id != parent_id
- parent_id не является потомком child_id (проверка циклов)

### 4.4 DeleteEntity

**Входные аргументы:**
```json
{
    "entity_id": "EntityId(...)"
}
```

**Результат:**
```json
{
    "deleted_ids": ["EntityId(...)", ...],
    "count": 5
}
```

**Валидация:**
- entity_id существует

**Операция:**
1. Пометить сущность как deleted
2. Рекурсивно пометить всех потомков как deleted
3. Вернуть список всех удалённых ID

### 4.5 EditPart

**Входные аргументы:**
```json
{
    "part_id": "EntityId(...)",
    "designation": "NEW-PART-001"
}
```

**Результат:**
```json
{
    "old_designation": "OLD-PART-001",
    "new_designation": "NEW-PART-001"
}
```

### 4.6 BatchMove

**Входные аргументы:**
```json
{
    "moves": [
        {
            "child_id": "EntityId(...)",
            "parent_id": "EntityId(...)"
        },
        ...
    ]
}
```

**Результат:**
```json
{
    "success_count": 3,
    "failure_count": 0,
    "failures": []
}
```

---

## 5. Валидация

### 5.1 Уровни валидации

1. **Синтаксис** — args имеет нужную структуру (JSON schema)
2. **Семантика** — referenced entities существуют (CadDocument lookup)
3. **Бизнес-логика** — операция не нарушает инварианты (циклы, типы)

### 5.2 Коды ошибок

| Код | Значение | Пример |
|-----|----------|--------|
| INVALID_ARGS | Неверный формат аргументов | missing field "designation" |
| ENTITY_NOT_FOUND | Referenced entity не найдена | part_id "abc" not in document |
| TYPE_MISMATCH | Неверный тип сущности | expected Part, got Instance |
| CYCLE_DETECTED | Операция создаст цикл | SetParent would create cycle |
| CONSTRAINT_VIOLATION | Нарушена какая-то constraint | OD <= 0 not allowed |

---

## 6. Тестирование

### 6.1 Unit Tests (test_command_executor.cpp)

1. **test_create_part_success** — успешное создание Part
2. **test_create_part_invalid_args** — обработка invalid args
3. **test_create_instance_nonexistent_part** — ошибка при Part не найдена
4. **test_set_parent_cycle_prevention** — блокировка циклов
5. **test_delete_entity_recursive** — удаление поддерева
6. **test_edit_part_updates_designation** — обновление обозначения
7. **test_batch_move_partial_failure** — откат при ошибке в batch

### 6.2 Интеграционные тесты (test_command_integration.cpp)

8. **test_command_sequence** — выполнение последовательности команд
9. **test_document_versioning** — проверка версионирования
10. **test_affected_entities_tracking** — правильное отслеживание изменённых entities

---

## 7. Deliverables (CAD-CORE-005)

**Phase B начало:**
- ✓ `include/bendcore/commands/command.hpp` — структуры Command, CommandResult
- ✓ `include/bendcore/commands/command_validator.hpp` — валидатор
- ✓ `include/bendcore/commands/command_executor.hpp` — исполнитель команд
- ✓ `src/commands/command_executor.cpp` — реализация
- ✓ `src/commands/command_validator.cpp` — реализация валидатора
- ✓ `tests/commands/test_command_executor.cpp` — unit тесты
- ✓ `tests/commands/test_command_integration.cpp` — интеграционные тесты
- ✓ CMakeLists.txt — обновить sources и тесты

---

## 8. Next Steps (CAD-CORE-006, CAD-CORE-007)

После CAD-CORE-005:

1. **CAD-CORE-006: TransactionManager** — атомарное выполнение batch операций
2. **CAD-CORE-007: HistoryStack** — Undo/Redo функциональность
3. **CAD-CORE-008: DocumentEventBus** — уведомления об изменениях

---

## 9. Performance & Scalability

| Операция | Сложность | Лимит |
|---------|----------|-------|
| Validate command | O(n) | < 1ms для 10K entities |
| Execute simple | O(1) | < 100µs |
| Execute batch | O(m·d) | m = count, d = depth |
| Record result | O(1) | < 10µs |

---

## 10. Integration Points

### С CadDocument
```cpp
// CommandExecutor использует CadDocument
class CommandExecutor {
    CommandResult execute(const Command& cmd) {
        // Валидация
        auto error = validator_.validate(cmd, document_);
        if (error) return CommandResult{false, error.value().message};
        
        // Выполнение
        document_.add_entity(...);
        document_.set_parent(...);
        document_.increment_revision();
        
        return CommandResult{true, ...};
    }
};
```

### С UI (React)
```
React Component
    ↓
/api/execute_command (HTTP POST)
    ↓
CommandExecutor::execute(cmd)
    ↓
CadDocument (modified)
    ↓
DocumentEventBus (emit changes)
    ↓
React re-renders
```

---

## 11. Migration Path

От старого API (core_api.cpp JSON RPC) к новому:

```cpp
// Старый код (core_api.cpp)
json Core::call(const std::string& command, const json& args) {
    if (command == "create_part") {
        // прямое выполнение, нет валидации
        return api::create_part(args);
    }
}

// Новый код (CAD-CORE-005)
json Core::call(const std::string& command, const json& args) {
    Command cmd{
        .id = generate_id(),
        .type = command,
        .args = args,
        .document_version = doc_.revision()
    };
    
    auto result = executor_.execute(cmd);
    return json{
        {"success", result.success},
        {"error", result.error_message},
        {"data", result.result_data},
        {"new_version", result.new_document_version}
    };
}
```

Адаптер может работать через CommandExecutor, обеспечивая совместимость.
