#include "bendcore/parameters/parameter_store.hpp"

#include <algorithm>
#include <cmath>
#include <set>
#include <sstream>

namespace bendcore::parameters {

// ==================== ParameterValue ====================

ParameterValue::ParameterValue(double val) : type(ParameterType::Double), value(val) {}

ParameterValue::ParameterValue(int val) : type(ParameterType::Integer), value(val) {}

ParameterValue::ParameterValue(const std::string& val) : type(ParameterType::String), value(val) {}

ParameterValue::ParameterValue(bool val) : type(ParameterType::Boolean), value(val) {}

ParameterValue::ParameterValue(EntityId ref) : type(ParameterType::Reference), value(ref) {}

double ParameterValue::as_double() const {
    if (type == ParameterType::Double) {
        return std::any_cast<double>(value);
    }
    if (type == ParameterType::Integer) {
        return static_cast<double>(std::any_cast<int>(value));
    }
    throw std::invalid_argument("Cannot convert " + type_name() + " to double");
}

int ParameterValue::as_integer() const {
    if (type == ParameterType::Integer) {
        return std::any_cast<int>(value);
    }
    if (type == ParameterType::Double) {
        return static_cast<int>(std::any_cast<double>(value));
    }
    throw std::invalid_argument("Cannot convert " + type_name() + " to int");
}

std::string ParameterValue::as_string() const {
    if (type == ParameterType::String) {
        return std::any_cast<std::string>(value);
    }
    throw std::invalid_argument("Cannot convert " + type_name() + " to string");
}

bool ParameterValue::as_boolean() const {
    if (type == ParameterType::Boolean) {
        return std::any_cast<bool>(value);
    }
    throw std::invalid_argument("Cannot convert " + type_name() + " to boolean");
}

EntityId ParameterValue::as_reference() const {
    if (type == ParameterType::Reference) {
        return std::any_cast<EntityId>(value);
    }
    throw std::invalid_argument("Cannot convert " + type_name() + " to reference");
}

std::vector<double> ParameterValue::as_vector() const {
    if (type == ParameterType::Vector) {
        return std::any_cast<std::vector<double>>(value);
    }
    throw std::invalid_argument("Cannot convert " + type_name() + " to vector");
}

std::string ParameterValue::type_name() const {
    switch (type) {
        case ParameterType::Double: return "Double";
        case ParameterType::Integer: return "Integer";
        case ParameterType::String: return "String";
        case ParameterType::Boolean: return "Boolean";
        case ParameterType::Reference: return "Reference";
        case ParameterType::Vector: return "Vector";
        default: return "Invalid";
    }
}

// ==================== ParameterStore ====================

ParameterStore::ParameterStore(CadDocument& doc, DocumentEventBus& bus)
    : document_(doc), event_bus_(bus) {}

std::optional<ParameterValue> ParameterStore::get(EntityId entity_id, const std::string& param_name) const {
    auto entity_it = parameters_.find(entity_id);
    if (entity_it == parameters_.end()) {
        return std::nullopt;
    }

    auto param_it = entity_it->second.find(param_name);
    if (param_it == entity_it->second.end()) {
        return std::nullopt;
    }

    return param_it->second;
}

bool ParameterStore::set(
    EntityId entity_id,
    const std::string& param_name,
    const ParameterValue& value,
    const std::string& reason
) {
    if (!document_.contains(entity_id)) {
        return false;
    }

    if (!validate(entity_id, param_name, value)) {
        return false;
    }

    // Получить старое значение (если было)
    auto old_value = get(entity_id, param_name).value_or(ParameterValue());

    // Установить новое значение
    parameters_[entity_id][param_name] = value;

    // Инвалидировать кэш
    invalidate_cache(entity_id, param_name);

    // Записать изменение в историю
    ParameterChange change;
    change.entity_id = entity_id;
    change.param_name = param_name;
    change.old_value = old_value;
    change.new_value = value;
    change.timestamp = std::chrono::system_clock::now();
    change.reason = reason;

    record_change(change);

    // Обновить метаданные
    if (metadata_[entity_id].count(param_name)) {
        metadata_[entity_id][param_name].modified_at = std::chrono::system_clock::now();
        metadata_[entity_id][param_name].modification_count++;
    }

    // Уведомить об изменении
    notify_parameter_changed(change);

    return true;
}

bool ParameterStore::set_with_formula(
    EntityId entity_id,
    const std::string& param_name,
    const std::string& formula,
    const std::vector<EntityId>& dependencies
) {
    ParameterValue value;
    value.type = ParameterType::Double;
    value.value = 0.0;  // Будет переуже formula
    value.formula = formula;
    value.dependencies = dependencies;

    return set(entity_id, param_name, value, "SetByFormula");
}

std::vector<std::string> ParameterStore::get_all_names(EntityId entity_id) const {
    std::vector<std::string> names;
    auto entity_it = parameters_.find(entity_id);
    if (entity_it != parameters_.end()) {
        for (const auto& [name, _] : entity_it->second) {
            names.push_back(name);
        }
    }
    return names;
}

std::map<std::string, ParameterValue> ParameterStore::get_all(EntityId entity_id) const {
    std::map<std::string, ParameterValue> result;
    auto entity_it = parameters_.find(entity_id);
    if (entity_it != parameters_.end()) {
        result = entity_it->second;
    }
    return result;
}

bool ParameterStore::validate(
    EntityId entity_id,
    const std::string& param_name,
    const ParameterValue& value
) const {
    if (!value.is_valid()) {
        return false;
    }

    // Проверить метаданные если они заданы
    auto meta_it = metadata_.find(entity_id);
    if (meta_it != metadata_.end()) {
        auto param_meta_it = meta_it->second.find(param_name);
        if (param_meta_it != meta_it->second.end()) {
            const auto& meta = param_meta_it->second;

            // Проверить тип
            if (meta.type != value.type) {
                return false;
            }

            // Проверить ограничения для Double
            if (value.type == ParameterType::Double) {
                double val = value.as_double();
                if (meta.min_value && val < *meta.min_value) {
                    return false;
                }
                if (meta.max_value && val > *meta.max_value) {
                    return false;
                }
            }

            // Проверить allowed_values для String
            if (value.type == ParameterType::String && !meta.allowed_values.empty()) {
                std::string val = value.as_string();
                if (std::find(meta.allowed_values.begin(), meta.allowed_values.end(), val) ==
                    meta.allowed_values.end()) {
                    return false;
                }
            }
        }
    }

    return true;
}

void ParameterStore::register_parameter(EntityId entity_id, const ParameterMetadata& metadata) {
    metadata_[entity_id][metadata.name] = metadata;
    metadata_[entity_id][metadata.name].created_at = std::chrono::system_clock::now();
    metadata_[entity_id][metadata.name].modified_at = std::chrono::system_clock::now();
}

std::optional<ParameterMetadata> ParameterStore::get_metadata(
    EntityId entity_id,
    const std::string& param_name
) const {
    auto entity_it = metadata_.find(entity_id);
    if (entity_it == metadata_.end()) {
        return std::nullopt;
    }

    auto param_it = entity_it->second.find(param_name);
    if (param_it == entity_it->second.end()) {
        return std::nullopt;
    }

    return param_it->second;
}

std::vector<ParameterChange> ParameterStore::get_history(
    EntityId entity_id,
    const std::string& param_name
) const {
    std::vector<ParameterChange> result;
    auto entity_it = history_.find(entity_id);
    if (entity_it != history_.end()) {
        auto param_it = entity_it->second.find(param_name);
        if (param_it != entity_it->second.end()) {
            result = param_it->second;
        }
    }
    return result;
}

bool ParameterStore::undo_last_change(EntityId entity_id, const std::string& param_name) {
    auto entity_it = history_.find(entity_id);
    if (entity_it == history_.end()) {
        return false;
    }

    auto param_it = entity_it->second.find(param_name);
    if (param_it == entity_it->second.end() || param_it->second.empty()) {
        return false;
    }

    // Получить последнее изменение
    const auto& last_change = param_it->second.back();
    const auto& old_value = last_change.old_value;

    // Восстановить старое значение
    if (!set(entity_id, param_name, old_value, "UndoChange")) {
        return false;
    }

    // Удалить запись из истории
    param_it->second.pop_back();

    return true;
}

void ParameterStore::clear_history(EntityId entity_id, const std::string& param_name) {
    auto entity_it = history_.find(entity_id);
    if (entity_it != history_.end()) {
        entity_it->second.erase(param_name);
    }
}

std::vector<std::string> ParameterStore::get_dependencies(
    EntityId entity_id,
    const std::string& param_name
) const {
    std::vector<std::string> result;
    auto entity_it = parameters_.find(entity_id);
    if (entity_it != parameters_.end()) {
        auto param_it = entity_it->second.find(param_name);
        if (param_it != entity_it->second.end()) {
            // Собрать имена параметров из EntityId
            for (const auto& dep_id : param_it->second.dependencies) {
                // Здесь можем найти имя параметра если есть информация
                // Пока просто добавим строковое представление
                result.push_back(dep_id.to_string());
            }
        }
    }
    return result;
}

std::vector<std::pair<EntityId, std::string>> ParameterStore::get_dependents(
    EntityId entity_id,
    const std::string& param_name
) const {
    std::vector<std::pair<EntityId, std::string>> result;

    // Найти все параметры, которые зависят от данного
    for (const auto& [ent_id, params] : parameters_) {
        for (const auto& [param_n, param_val] : params) {
            // Проверить, зависит ли этот параметр от заданного
            for (const auto& dep : param_val.dependencies) {
                if (dep == entity_id) {
                    // TODO: Сравнить имена параметров
                    result.push_back({ent_id, param_n});
                    break;
                }
            }
        }
    }

    return result;
}

bool ParameterStore::has_circular_dependency(EntityId entity_id, const std::string& param_name) const {
    // Простая проверка: пройти по цепи зависимостей
    std::set<std::pair<EntityId, std::string>> visited;
    std::pair<EntityId, std::string> current = {entity_id, param_name};

    while (true) {
        if (visited.count(current)) {
            return true;  // Найден цикл
        }

        visited.insert(current);

        // Получить зависимости текущего параметра
        auto entity_it = parameters_.find(current.first);
        if (entity_it == parameters_.end()) {
            return false;  // Конец цепи
        }

        auto param_it = entity_it->second.find(current.second);
        if (param_it == entity_it->second.end() || param_it->second.dependencies.empty()) {
            return false;  // Конец цепи
        }

        // Перейти к первой зависимости (упрощенная проверка)
        current = {param_it->second.dependencies[0], ""};
    }
}

void ParameterStore::invalidate_cache(EntityId entity_id, const std::string& param_name) {
    if (cache_.count(entity_id)) {
        cache_[entity_id].erase(param_name);
    }
}

void ParameterStore::invalidate_entity_cache(EntityId entity_id) {
    cache_.erase(entity_id);
}

void ParameterStore::clear_cache() {
    cache_.clear();
}

void ParameterStore::begin_batch_update() {
    batch_mode_ = true;
    batch_queue_.clear();
}

void ParameterStore::end_batch_update() {
    batch_mode_ = false;

    // Отправить все отложенные уведомления
    for (const auto& change : batch_queue_) {
        notify_parameter_changed(change);
    }
    batch_queue_.clear();
}

size_t ParameterStore::count_parameters(EntityId entity_id) const {
    auto it = parameters_.find(entity_id);
    if (it == parameters_.end()) {
        return 0;
    }
    return it->second.size();
}

bool ParameterStore::has_parameter(EntityId entity_id, const std::string& param_name) const {
    auto entity_it = parameters_.find(entity_id);
    if (entity_it == parameters_.end()) {
        return false;
    }
    return entity_it->second.count(param_name) > 0;
}

bool ParameterStore::remove_parameter(EntityId entity_id, const std::string& param_name) {
    auto entity_it = parameters_.find(entity_id);
    if (entity_it == parameters_.end()) {
        return false;
    }

    if (entity_it->second.erase(param_name) == 0) {
        return false;
    }

    invalidate_cache(entity_id, param_name);
    return true;
}

void ParameterStore::notify_parameter_changed(const ParameterChange& change) {
    if (batch_mode_) {
        batch_queue_.push_back(change);
    } else {
        DocumentEvent event(DocumentEventType::EntityModified, change.entity_id,
                          "Parameter " + change.param_name + " changed");
        event_bus_.publish(event);
    }
}

void ParameterStore::update_dependents_cache(EntityId entity_id, const std::string& param_name) {
    // Инвалидировать кэш всех зависимых параметров
    auto dependents = get_dependents(entity_id, param_name);
    for (const auto& [dep_entity, dep_param] : dependents) {
        invalidate_cache(dep_entity, dep_param);
    }
}

bool ParameterStore::validate_parameter_value(const ParameterValue& value) const {
    return value.is_valid();
}

void ParameterStore::record_change(const ParameterChange& change) {
    history_[change.entity_id][change.param_name].push_back(change);

    // Ограничить размер истории
    const size_t max_history = 100;
    auto& param_history = history_[change.entity_id][change.param_name];
    if (param_history.size() > max_history) {
        param_history.erase(param_history.begin());
    }
}

}  // namespace bendcore::parameters
