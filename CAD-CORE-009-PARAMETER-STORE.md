# CAD-CORE-009: Parameter Store

**Статус:** Спецификация и начало реализации  
**Дата:** 2026-10-25  
**Приоритет:** P0 (Phase C ядро)  
**Зависимости:** CAD-CORE-003 (CadDocument), CAD-CORE-005 (CommandEngine), CAD-CORE-008 (EventBus)  

---

## 1. Описание

ParameterStore — централизованное хранилище **параметров сущностей** (OD, Wall, Length, и т.д.) с поддержкой:
- Типизированных значений (Double, String, Reference)
- Формул и зависимостей
- Подписки на изменения через EventBus

---

## 2. API

```cpp
class ParameterStore {
public:
    explicit ParameterStore(CadDocument& doc, DocumentEventBus& bus);

    // Получение параметра
    [[nodiscard]] std::optional<Parameter> get(EntityId entity_id, const std::string& param_name) const;

    // Установка параметра
    [[nodiscard]] bool set(EntityId entity_id, const std::string& param_name, const Parameter& value);

    // Все параметры сущности
    [[nodiscard]] std::vector<Parameter> get_all(EntityId entity_id) const;

    // Валидация
    [[nodiscard]] bool validate(EntityId entity_id, const Parameter& value) const;

private:
    CadDocument& document_;
    DocumentEventBus& event_bus_;
    std::map<EntityId, std::map<std::string, Parameter>> parameters_;
};
```

---

## 3. Параметры по типам сущностей

**Part:**
- designation (String)
- material (String)

**Instance:**
- adaptive (Boolean)
- placement_x, placement_y, placement_z (Double)

**Body:**
- name (String)

**Feature:**
- feature_type (String)
- radius (Double, если применимо)
- depth (Double, если применимо)

---

## 4. Тестирование

**Тесты (test_parameter_store.cpp):**

1. **test_get_set_parameter** — получение и установка
2. **test_all_parameters** — получить все параметры
3. **test_validate_parameter** — валидация значений
4. **test_parameter_types** — различные типы параметров
5. **test_event_notification** — уведомления через EventBus

---

## 5. Deliverables

- `include/bendcore/parameters/parameter_store.hpp`
- `src/parameters/parameter_store.cpp`
- `tests/parameters/test_parameter_store.cpp`
- CMakeLists.txt обновлён

**Размер:** ~600 линий
