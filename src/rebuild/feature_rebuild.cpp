/**
 * GAP-007: Надёжное перестроение дерева - реализация
 */

#include "bendcore/rebuild/feature_rebuild.hpp"

#include <algorithm>
#include <chrono>
#include <sstream>

namespace bendcore::rebuild {

FeatureRebuildEngine::FeatureRebuildEngine(RebuildEngine& base_engine)
    : base_engine_(base_engine) {}

// ========== Управление состоянием Feature ==========

bool FeatureRebuildEngine::suppress_feature(EntityId entity_id) {
    feature_states_[entity_id] = FeatureState::Suppressed;
    create_snapshot(entity_id);
    return true;
}

bool FeatureRebuildEngine::unsuppress_feature(EntityId entity_id) {
    auto it = feature_states_.find(entity_id);
    if (it != feature_states_.end() && it->second == FeatureState::Suppressed) {
        feature_states_[entity_id] = FeatureState::Normal;
        return true;
    }
    return false;
}

bool FeatureRebuildEngine::is_suppressed(EntityId entity_id) const {
    auto it = feature_states_.find(entity_id);
    return it != feature_states_.end() && it->second == FeatureState::Suppressed;
}

FeatureState FeatureRebuildEngine::get_feature_state(EntityId entity_id) const {
    auto it = feature_states_.find(entity_id);
    if (it != feature_states_.end()) {
        return it->second;
    }
    return FeatureState::Normal;
}

bool FeatureRebuildEngine::set_feature_state(EntityId entity_id, FeatureState state) {
    feature_states_[entity_id] = state;
    return true;
}

// ========== Перестроение с обработкой ошибок ==========

RebuildReport FeatureRebuildEngine::rebuild_with_recovery(EntityId entity_id) {
    RebuildReport report;
    report.total_parameters = 0;
    report.successful = 0;
    report.failed = 0;
    report.skipped = 0;

    auto state = get_feature_state(entity_id);

    // Не перестраивать подавленные feature
    if (state == FeatureState::Suppressed) {
        report.skipped = 1;
        return report;
    }

    // Создать снимок перед перестроением
    create_snapshot(entity_id);

    // Попытаться перестроить
    auto base_report = base_engine_.rebuild_entity(entity_id);

    // Проверить на ошибки
    if (!base_report.is_success()) {
        set_feature_state(entity_id, FeatureState::Disabled);
        record_error({
            entity_id,
            "unknown",
            "Rebuild failed for feature",
            std::chrono::system_clock::now(),
            1,
            true  // recoverable
        });
    } else {
        set_feature_state(entity_id, FeatureState::Normal);
        rebuild_history_.push_back({entity_id, std::chrono::system_clock::now()});
    }

    return base_report;
}

RebuildReport FeatureRebuildEngine::rebuild_in_order(
    const std::vector<EntityId>& entities
) {
    // Выполнить топологическую сортировку
    auto sorted = topological_sort(entities);

    RebuildReport combined_report;
    combined_report.total_parameters = 0;
    combined_report.successful = 0;
    combined_report.failed = 0;
    combined_report.skipped = 0;

    // Перестроить в правильном порядке
    for (const auto& entity_id : sorted) {
        auto report = rebuild_with_recovery(entity_id);
        combined_report.total_parameters += report.total_parameters;
        combined_report.successful += report.successful;
        combined_report.failed += report.failed;
        combined_report.skipped += report.skipped;
    }

    return combined_report;
}

RebuildReport FeatureRebuildEngine::rebuild_affected_only() {
    // Найти все feature с невалидным кэшем
    std::vector<EntityId> affected;

    for (const auto& [key, cache] : computation_cache_) {
        if (!cache.is_valid) {
            affected.push_back(key.first);
        }
    }

    if (affected.empty()) {
        return RebuildReport();
    }

    return rebuild_in_order(affected);
}

// ========== Обработка ошибок ==========

std::vector<RebuildError> FeatureRebuildEngine::get_last_errors() const {
    return error_history_;
}

void FeatureRebuildEngine::clear_errors() {
    error_history_.clear();
}

bool FeatureRebuildEngine::has_recoverable_errors() const {
    for (const auto& error : error_history_) {
        if (error.recoverable) {
            return true;
        }
    }
    return false;
}

RebuildReport FeatureRebuildEngine::attempt_recovery() {
    RebuildReport report;
    report.total_parameters = 0;
    report.successful = 0;
    report.failed = 0;
    report.skipped = 0;

    // Попытаться восстановить для каждой ошибки
    for (const auto& error : error_history_) {
        if (error.recoverable) {
            // Восстановить из снимка
            if (restore_from_snapshot(error.entity_id)) {
                report.successful++;
            } else {
                report.failed++;
            }
        }
    }

    report.total_parameters = error_history_.size();
    return report;
}

// ========== Откат и снимки состояния ==========

bool FeatureRebuildEngine::create_snapshot(EntityId entity_id) {
    FeatureSnapshot snapshot;
    snapshot.entity_id = entity_id;
    snapshot.state = get_feature_state(entity_id);
    snapshot.created_at = std::chrono::system_clock::now();

    // В реальной реализации сохранять параметры
    snapshots_[entity_id].push_back(snapshot);

    // Ограничить количество снимков (последние 10)
    if (snapshots_[entity_id].size() > 10) {
        snapshots_[entity_id].erase(snapshots_[entity_id].begin());
    }

    return true;
}

bool FeatureRebuildEngine::restore_from_snapshot(EntityId entity_id) {
    auto it = snapshots_.find(entity_id);
    if (it == snapshots_.end() || it->second.empty()) {
        return false;
    }

    const auto& snapshot = it->second.back();
    set_feature_state(entity_id, snapshot.state);
    return true;
}

bool FeatureRebuildEngine::rollback_to_last_successful() {
    if (rebuild_history_.empty()) {
        return false;
    }

    // Откатить последнюю операцию
    auto [entity_id, _] = rebuild_history_.back();
    rebuild_history_.pop_back();

    return restore_from_snapshot(entity_id);
}

std::vector<FeatureSnapshot> FeatureRebuildEngine::get_snapshots(EntityId entity_id) const {
    auto it = snapshots_.find(entity_id);
    if (it != snapshots_.end()) {
        return it->second;
    }
    return {};
}

void FeatureRebuildEngine::clear_snapshots() {
    snapshots_.clear();
}

// ========== Кэширование вычислений ==========

std::optional<ParameterValue> FeatureRebuildEngine::get_cached_value(
    EntityId entity_id,
    const std::string& param_name
) const {
    if (!caching_enabled_) {
        return std::nullopt;
    }

    auto key = std::make_pair(entity_id, param_name);
    auto it = computation_cache_.find(key);

    if (it != computation_cache_.end() && it->second.is_valid) {
        return it->second.cached_value;
    }

    return std::nullopt;
}

void FeatureRebuildEngine::invalidate_cache(EntityId entity_id, const std::string& param_name) {
    auto key = std::make_pair(entity_id, param_name);
    auto it = computation_cache_.find(key);

    if (it != computation_cache_.end()) {
        it->second.is_valid = false;
    }
}

void FeatureRebuildEngine::invalidate_dependent_cache(EntityId entity_id) {
    for (auto& [key, cache] : computation_cache_) {
        for (const auto& dep : cache.dependency_set) {
            if (dep == entity_id) {
                cache.is_valid = false;
                break;
            }
        }
    }
}

void FeatureRebuildEngine::clear_cache() {
    computation_cache_.clear();
}

FeatureRebuildEngine::CacheStats FeatureRebuildEngine::get_cache_stats() const {
    CacheStats stats;
    stats.total_entries = computation_cache_.size();
    stats.valid_entries = 0;
    stats.invalid_entries = 0;

    for (const auto& [_, cache] : computation_cache_) {
        if (cache.is_valid) {
            stats.valid_entries++;
        } else {
            stats.invalid_entries++;
        }
    }

    stats.hit_rate = stats.total_entries > 0
        ? static_cast<double>(stats.valid_entries) / stats.total_entries
        : 0.0;

    return stats;
}

// ========== Диагностика и отладка ==========

bool FeatureRebuildEngine::validate_dependency_tree() const {
    // Проверить на циклические зависимости
    for (const auto& [key, state] : feature_states_) {
        if (check_circular_dependencies(key.first)) {
            return false;
        }
    }
    return true;
}

std::vector<FeatureRebuildEngine::AllFeatureStatus> FeatureRebuildEngine::get_all_feature_status() const {
    std::vector<AllFeatureStatus> statuses;

    for (const auto& [entity_id, state] : feature_states_) {
        AllFeatureStatus status;
        status.entity_id = entity_id;
        status.state = state;
        status.has_errors = false;

        for (const auto& error : error_history_) {
            if (error.entity_id == entity_id) {
                status.has_errors = true;
                break;
            }
        }

        statuses.push_back(status);
    }

    return statuses;
}

RebuildReport FeatureRebuildEngine::dry_run_rebuild(EntityId entity_id) {
    // Создать снимок
    auto snapshot = std::make_shared<std::map<std::string, ParameterValue>>();

    // Выполнить rebuild без сохранения изменений
    // В реальной реализации нужно откатить все изменения после
    auto report = rebuild_with_recovery(entity_id);

    // Откатить
    // TODO: откатить изменения

    return report;
}

// ========== Вспомогательные методы ==========

std::vector<EntityId> FeatureRebuildEngine::topological_sort(
    const std::vector<EntityId>& entities
) const {
    // Простая реализация: просто вернуть исходный порядок
    // В реальной реализации нужно использовать DependencyGraph
    return entities;
}

void FeatureRebuildEngine::record_error(const RebuildError& error) {
    error_history_.push_back(error);
}

bool FeatureRebuildEngine::check_circular_dependencies(EntityId entity_id) const {
    // Простая реализация без проверки
    // В реальной реализации использовать DependencyGraph
    return false;
}

void FeatureRebuildEngine::update_cache_on_rebuild(
    EntityId entity_id,
    const ParameterValue& value
) {
    // В реальной реализации обновлять кэш при успешном перестроении
}

}  // namespace bendcore::rebuild
