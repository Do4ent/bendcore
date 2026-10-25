#pragma once

#include <any>
#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "bendcore/commands/document_event_bus.hpp"
#include "bendcore/document/cad_document.hpp"
#include "bendcore/document/entity_id.hpp"

namespace bendcore::parameters {

using namespace bendcore::document;
using namespace bendcore::commands;

/**
 * ParameterType — поддерживаемые типы параметров
 */
enum class ParameterType {
    Invalid = 0,
    Double = 1,
    Integer = 2,
    String = 3,
    Boolean = 4,
    Reference = 5,
    Vector = 6
};

/**
 * ParameterValue — типизированное значение параметра
 */
struct ParameterValue {
    ParameterType type;
    std::any value;
    std::optional<std::string> formula;  // Если есть формула для вычисления
    std::vector<EntityId> dependencies;  // Параметры, от которых это зависит

    ParameterValue() : type(ParameterType::Invalid) {}

    explicit ParameterValue(double val);
    explicit ParameterValue(int val);
    explicit ParameterValue(const std::string& val);
    explicit ParameterValue(bool val);
    explicit ParameterValue(EntityId ref);

    [[nodiscard]] double as_double() const;
    [[nodiscard]] int as_integer() const;
    [[nodiscard]] std::string as_string() const;
    [[nodiscard]] bool as_boolean() const;
    [[nodiscard]] EntityId as_reference() const;
    [[nodiscard]] std::vector<double> as_vector() const;

    [[nodiscard]] bool is_valid() const { return type != ParameterType::Invalid; }
    [[nodiscard]] std::string type_name() const;
};

/**
 * ParameterMetadata — метаданные параметра
 */
struct ParameterMetadata {
    std::string name;
    std::string display_name;
    std::string description;
    ParameterType type;

    // Ограничения
    std::optional<double> min_value;
    std::optional<double> max_value;
    std::vector<std::string> allowed_values;  // Для enum-подобных параметров

    // История изменений
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point modified_at;
    int modification_count = 0;

    ParameterMetadata() : type(ParameterType::Invalid) {}
};

/**
 * ParameterChange — запись об изменении параметра
 */
struct ParameterChange {
    EntityId entity_id;
    std::string param_name;
    ParameterValue old_value;
    ParameterValue new_value;
    std::chrono::system_clock::time_point timestamp;
    std::string reason;  // Причина изменения ("SetByUser", "ComputedByFormula", etc.)

    ParameterChange() = default;
};

/**
 * ParameterStore — централизованное хранилище параметров сущностей
 * CAD-CORE-009: Parameter management with type safety and history integration
 *
 * Функциональность:
 * - Типизированное хранилище параметров по (EntityId, parameter_name)
 * - Поддержка формул и зависимостей между параметрами
 * - История изменений параметров
 * - Ленивое кэширование вычисленных значений
 * - Интеграция с HistoryStack для undo/redo
 * - Уведомление через DocumentEventBus об изменениях
 */
class ParameterStore {
public:
    explicit ParameterStore(CadDocument& doc, DocumentEventBus& bus);
    ~ParameterStore() = default;

    // ==================== Основной API ====================

    /**
     * Получить параметр сущности
     */
    [[nodiscard]] std::optional<ParameterValue> get(
        EntityId entity_id,
        const std::string& param_name
    ) const;

    /**
     * Установить параметр сущности
     * Возвращает true если успешно, false если валидация не прошла
     */
    [[nodiscard]] bool set(
        EntityId entity_id,
        const std::string& param_name,
        const ParameterValue& value,
        const std::string& reason = "SetByUser"
    );

    /**
     * Установить параметр с формулой
     */
    [[nodiscard]] bool set_with_formula(
        EntityId entity_id,
        const std::string& param_name,
        const std::string& formula,
        const std::vector<EntityId>& dependencies
    );

    /**
     * Получить все параметры сущности
     */
    [[nodiscard]] std::vector<std::string> get_all_names(EntityId entity_id) const;

    /**
     * Получить все параметры сущности с значениями
     */
    [[nodiscard]] std::map<std::string, ParameterValue> get_all(EntityId entity_id) const;

    // ==================== Валидация и типизация ====================

    /**
     * Валидировать значение параметра
     */
    [[nodiscard]] bool validate(
        EntityId entity_id,
        const std::string& param_name,
        const ParameterValue& value
    ) const;

    /**
     * Зарегистрировать параметр с метаданными
     */
    void register_parameter(
        EntityId entity_id,
        const ParameterMetadata& metadata
    );

    /**
     * Получить метаданные параметра
     */
    [[nodiscard]] std::optional<ParameterMetadata> get_metadata(
        EntityId entity_id,
        const std::string& param_name
    ) const;

    // ==================== История изменений ====================

    /**
     * Получить историю изменений параметра
     */
    [[nodiscard]] std::vector<ParameterChange> get_history(
        EntityId entity_id,
        const std::string& param_name
    ) const;

    /**
     * Откатить последнее изменение параметра
     */
    [[nodiscard]] bool undo_last_change(EntityId entity_id, const std::string& param_name);

    /**
     * Очистить историю параметра
     */
    void clear_history(EntityId entity_id, const std::string& param_name);

    // ==================== Зависимости между параметрами ====================

    /**
     * Получить параметры, от которых зависит данный параметр
     */
    [[nodiscard]] std::vector<std::string> get_dependencies(
        EntityId entity_id,
        const std::string& param_name
    ) const;

    /**
     * Получить параметры, которые зависят от данного параметра
     */
    [[nodiscard]] std::vector<std::pair<EntityId, std::string>> get_dependents(
        EntityId entity_id,
        const std::string& param_name
    ) const;

    /**
     * Проверить, есть ли циклические зависимости
     */
    [[nodiscard]] bool has_circular_dependency(
        EntityId entity_id,
        const std::string& param_name
    ) const;

    // ==================== Кэширование ====================

    /**
     * Инвалидировать кэш параметра
     */
    void invalidate_cache(EntityId entity_id, const std::string& param_name);

    /**
     * Инвалидировать кэш всех параметров сущности
     */
    void invalidate_entity_cache(EntityId entity_id);

    /**
     * Инвалидировать весь кэш
     */
    void clear_cache();

    // ==================== Пакетные операции ====================

    /**
     * Начать пакетное обновление (отложить уведомления)
     */
    void begin_batch_update();

    /**
     * Завершить пакетное обновление (отправить все отложенные уведомления)
     */
    void end_batch_update();

    [[nodiscard]] bool in_batch() const { return batch_mode_; }

    // ==================== Информация ====================

    /**
     * Получить количество параметров сущности
     */
    [[nodiscard]] size_t count_parameters(EntityId entity_id) const;

    /**
     * Проверить наличие параметра
     */
    [[nodiscard]] bool has_parameter(EntityId entity_id, const std::string& param_name) const;

    /**
     * Удалить параметр
     */
    [[nodiscard]] bool remove_parameter(EntityId entity_id, const std::string& param_name);

private:
    CadDocument& document_;
    DocumentEventBus& event_bus_;

    // Основное хранилище: EntityId -> (param_name -> ParameterValue)
    std::map<EntityId, std::map<std::string, ParameterValue>> parameters_;

    // Метаданные параметров
    std::map<EntityId, std::map<std::string, ParameterMetadata>> metadata_;

    // История изменений
    std::map<EntityId, std::map<std::string, std::vector<ParameterChange>>> history_;

    // Кэш: EntityId -> (param_name -> cached_value)
    mutable std::map<EntityId, std::map<std::string, std::pair<ParameterValue, bool>>> cache_;

    // Зависимости: (EntityId, param_name) -> список зависимостей (EntityId, param_name)
    std::map<std::pair<EntityId, std::string>, std::vector<std::pair<EntityId, std::string>>> dependency_map_;

    // Режим пакетного обновления
    bool batch_mode_ = false;
    std::vector<ParameterChange> batch_queue_;

    // Вспомогательные методы
    void notify_parameter_changed(const ParameterChange& change);
    void update_dependents_cache(EntityId entity_id, const std::string& param_name);
    [[nodiscard]] bool validate_parameter_value(const ParameterValue& value) const;
    void record_change(const ParameterChange& change);
};

}  // namespace bendcore::parameters
