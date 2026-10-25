# CAD-CORE-008: Document Event Bus

**Статус:** Спецификация и реализация  
**Дата:** 2026-10-25  
**Приоритет:** P0 (основа для Phase C)  
**Зависимости:** CAD-CORE-005 (CommandEngine)  

---

## 1. Описание

DocumentEventBus реализует **observer pattern** для оповещения подписчиков об изменениях в документе.

**Цель:** Позволить компонентам (UI, DependencyGraph, ParameterStore) реагировать на изменения без прямой связи.

---

## 2. События

```cpp
enum class DocumentEventType {
    EntityCreated,
    EntityDeleted,
    EntityModified,
    HierarchyChanged,
    DocumentRevisionChanged
};

struct DocumentEvent {
    DocumentEventType type;
    EntityId affected_id;
    std::chrono::system_clock::time_point timestamp;
    std::string description;
};
```

---

## 3. API

```cpp
class DocumentEventBus {
public:
    // Подписка на события
    [[nodiscard]] std::string subscribe(
        DocumentEventType type,
        std::function<void(const DocumentEvent&)> handler
    );

    // Отписка
    void unsubscribe(const std::string& subscription_id);

    // Публикация события
    void publish(const DocumentEvent& event);

    // Состояние
    [[nodiscard]] size_t subscriber_count(DocumentEventType type) const;

    // Batch операции
    void begin_batch();
    void end_batch();
};
```

---

## 4. Примеры

```cpp
DocumentEventBus bus;

// Подписка на создание сущностей
auto sub_id = bus.subscribe(
    DocumentEventType::EntityCreated,
    [](const DocumentEvent& event) {
        std::cout << "Created: " << event.description << "\n";
    }
);

// Публикация
DocumentEvent evt{
    .type = DocumentEventType::EntityCreated,
    .affected_id = part_id,
    .description = "Created PART-001"
};
bus.publish(evt);

// Отписка
bus.unsubscribe(sub_id);
```

---

## 5. Интеграция с CommandEngine

```cpp
class CommandExecutor {
public:
    CommandExecutor(CadDocument& doc, DocumentEventBus& bus)
        : document_(doc), event_bus_(bus) {}

    CommandResult execute(const Command& cmd) {
        // ... валидация и выполнение ...
        
        // Публикация события
        DocumentEvent evt{
            .type = DocumentEventType::EntityModified,
            .affected_id = modified_id,
            .description = "Executed: " + cmd.type
        };
        event_bus_.publish(evt);
        
        return result;
    }
};
```

---

## 6. Тестирование

**Тесты (test_document_event_bus.cpp):**

1. **test_subscribe_publish** — подписка и публикация событий
2. **test_multiple_subscribers** — несколько подписчиков
3. **test_unsubscribe** — отписка работает
4. **test_event_types** — все типы событий
5. **test_batch_events** — batch операции

---

## 7. Deliverables

- ✓ `include/bendcore/commands/document_event_bus.hpp`
- ✓ `src/commands/document_event_bus.cpp`
- ✓ `tests/commands/test_document_event_bus.cpp`
- ✓ CMakeLists.txt обновлён

**Размер:** ~500 линий

---

## 8. Next: CAD-CORE-009

После CAD-CORE-008, можно интегрировать:
- ParameterStore с DocumentEventBus для отслеживания изменений параметров
- UI слой может подписаться на события для обновления
- DependencyGraph может реагировать на изменения сущностей
