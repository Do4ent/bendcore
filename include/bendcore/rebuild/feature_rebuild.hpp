/**
 * GAP-007: Надёжное перестроение дерева (Feature Rebuild)
 * Расширение RebuildEngine для поддержки:
 * - Suppress/Unsuppress функциональности
 * - Восстановление после ошибок перестроения
 * - Правильный порядок вычисления с учётом зависимостей
 * - Пересчёт только зависимых узлов (кэширование)
 * - Откат и восстановление состояния
 */

#pragma once

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "bendcore/rebuild/rebuild_engine.hpp"
#include "bendcore/document/entity_id.hpp"

namespace bendcore::rebuild {

using namespace bendcore::document;

/**
 * Состояние feature (особенность) в дереве
 */
enum class FeatureState {
    Normal = 0,           // Обычное состояние (вычисляется и видимо)
    Suppressed = 1,       // Подавлено (не вычисляется, но сохраняется)
    Disabled = 2,         // Отключено (ошибка вычисления, требует исправления)
    Hidden = 3,           // Скрыто в UI
    Rollback = 4          // В процессе отката
};

/**
 * Информация об ошибке при перестроении
 */
struct RebuildError {
    EntityId entity_id;
    std::string param_name;
    std::string error_message;
    std::chrono::system_clock::time_point timestamp;
    int error_code = 0;
    bool recoverable = false;
};

/**
 * История состояния feature для отката
 */
struct FeatureSnapshot {
    EntityId entity_id;
    FeatureState state;
    std::map<std::string, ParameterValue> parameters;
    std::chrono::system_clock::time_point created_at;
};

/**
 * Кэш результатов вычисления для оптимизации перестроения
 */
struct ComputationCache {
    EntityId entity_id;
    std::string param_name;
    ParameterValue cached_value;
    std::set<EntityId> dependency_set;  // Зависимые элементы
    bool is_valid = true;
    std::chrono::system_clock::time_point timestamp;
};

/**
 * FeatureRebuildEngine - расширенный механизм перестроения для операций
 */
class FeatureRebuildEngine {
public:
    explicit FeatureRebuildEngine(RebuildEngine& base_engine);
    ~FeatureRebuildEngine() = default;

    // ========== Управление состоянием Feature ==========

    /**
     * Suppress (подавить) feature - выключить вычисления, но сохранить определение
     */
    [[nodiscard]] bool suppress_feature(EntityId entity_id);

    /**
     * Unsuppress (восстановить) feature - включить вычисления
     */
    [[nodiscard]] bool unsuppress_feature(EntityId entity_id);

    /**
     * Проверить, подавлен ли feature
     */
    [[nodiscard]] bool is_suppressed(EntityId entity_id) const;

    /**
     * Получить состояние feature
     */
    [[nodiscard]] FeatureState get_feature_state(EntityId entity_id) const;

    /**
     * Установить состояние feature
     */
    [[nodiscard]] bool set_feature_state(EntityId entity_id, FeatureState state);

    // ========== Перестроение с обработкой ошибок ==========

    /**
     * Перестроить с обработкой ошибок и восстановлением
     */
    [[nodiscard]] RebuildReport rebuild_with_recovery(EntityId entity_id);

    /**
     * Перестроить в правильном порядке (топологический порядок по зависимостям)
     */
    [[nodiscard]] RebuildReport rebuild_in_order(
        const std::vector<EntityId>& entities
    );

    /**
     * Перестроить только изменённые элементы и их зависимые
     */
    [[nodiscard]] RebuildReport rebuild_affected_only();

    // ========== Обработка ошибок ==========

    /**
     * Получить список всех ошибок при последнем перестроении
     */
    [[nodiscard]] std::vector<RebuildError> get_last_errors() const;

    /**
     * Очистить список ошибок
     */
    void clear_errors();

    /**
     * Есть ли восстанавливаемые ошибки
     */
    [[nodiscard]] bool has_recoverable_errors() const;

    /**
     * Попытаться восстановить после ошибок
     */
    [[nodiscard]] RebuildReport attempt_recovery();

    // ========== Откат и снимки состояния ==========

    /**
     * Создать снимок состояния feature
     */
    [[nodiscard]] bool create_snapshot(EntityId entity_id);

    /**
     * Восстановить состояние из снимка
     */
    [[nodiscard]] bool restore_from_snapshot(EntityId entity_id);

    /**
     * Откатить перестроение к последнему удачному состоянию
     */
    [[nodiscard]] bool rollback_to_last_successful();

    /**
     * Получить все снимки для entity
     */
    [[nodiscard]] std::vector<FeatureSnapshot> get_snapshots(EntityId entity_id) const;

    /**
     * Очистить снимки
     */
    void clear_snapshots();

    // ========== Кэширование вычислений ==========

    /**
     * Включить кэширование
     */
    void enable_caching(bool enabled) { caching_enabled_ = enabled; }

    /**
     * Проверить кэш
     */
    [[nodiscard]] std::optional<ParameterValue> get_cached_value(
        EntityId entity_id,
        const std::string& param_name
    ) const;

    /**
     * Инвалидировать кэш для параметра
     */
    void invalidate_cache(EntityId entity_id, const std::string& param_name);

    /**
     * Инвалидировать кэш для всех зависимых элементов
     */
    void invalidate_dependent_cache(EntityId entity_id);

    /**
     * Очистить весь кэш
     */
    void clear_cache();

    /**
     * Получить статистику кэша
     */
    struct CacheStats {
        size_t total_entries;
        size_t valid_entries;
        size_t invalid_entries;
        double hit_rate;
    };

    [[nodiscard]] CacheStats get_cache_stats() const;

    // ========== Диагностика и отладка ==========

    /**
     * Проверить целостность дерева зависимостей
     */
    [[nodiscard]] bool validate_dependency_tree() const;

    /**
     * Получить статус перестроения для всех feature
     */
    struct AllFeatureStatus {
        EntityId entity_id;
        FeatureState state;
        RebuildStatus rebuild_status;
        bool has_errors;
    };

    [[nodiscard]] std::vector<AllFeatureStatus> get_all_feature_status() const;

    /**
     * Эмулировать перестроение без изменения состояния (dry-run)
     */
    [[nodiscard]] RebuildReport dry_run_rebuild(EntityId entity_id);

private:
    RebuildEngine& base_engine_;

    // Состояние feature
    std::map<EntityId, FeatureState> feature_states_;

    // История ошибок
    std::vector<RebuildError> error_history_;

    // Снимки состояния
    std::map<EntityId, std::vector<FeatureSnapshot>> snapshots_;

    // Кэш вычислений
    std::map<std::pair<EntityId, std::string>, ComputationCache> computation_cache_;
    bool caching_enabled_ = true;

    // История успешных перестроений
    std::vector<std::pair<EntityId, std::chrono::system_clock::time_point>> rebuild_history_;

    // Вспомогательные методы
    [[nodiscard]] std::vector<EntityId> topological_sort(
        const std::vector<EntityId>& entities
    ) const;

    void record_error(const RebuildError& error);

    [[nodiscard]] bool check_circular_dependencies(EntityId entity_id) const;

    void update_cache_on_rebuild(EntityId entity_id, const ParameterValue& value);
};

}  // namespace bendcore::rebuild
