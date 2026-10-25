#pragma once

#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <queue>
#include <string>
#include <vector>

#include "bendcore/commands/document_event_bus.hpp"
#include "bendcore/dependencies/dependency_graph.hpp"
#include "bendcore/document/cad_document.hpp"
#include "bendcore/document/entity_id.hpp"
#include "bendcore/parameters/parameter_store.hpp"

namespace bendcore::rebuild {

using namespace bendcore::document;
using namespace bendcore::commands;
using namespace bendcore::dependencies;
using namespace bendcore::parameters;

/**
 * Статус перестроения параметра
 */
enum class RebuildStatus {
    Pending = 0,
    Computing = 1,
    Success = 2,
    Failed = 3,
    Skipped = 4
};

/**
 * Результат перестроения одного параметра
 */
struct RebuildResult {
    EntityId entity_id;
    std::string param_name;
    RebuildStatus status;
    ParameterValue new_value;
    std::string error_message;
    std::chrono::milliseconds compute_time;

    RebuildResult() : status(RebuildStatus::Pending), compute_time(0) {}
};

/**
 * Глобальный результат перестроения
 */
struct RebuildReport {
    size_t total_parameters;
    size_t successful;
    size_t failed;
    size_t skipped;
    std::chrono::milliseconds total_time;
    std::vector<RebuildResult> results;

    [[nodiscard]] bool is_success() const { return failed == 0; }
    [[nodiscard]] double success_rate() const {
        return total_parameters > 0 ? static_cast<double>(successful) / total_parameters * 100.0 : 0.0;
    }
};

/**
 * RebuildEngine — движок для инкрементального пересчета параметров
 * CAD-CORE-011: Cascading updates with incremental rebuild
 *
 * Функциональность:
 * - Инкрементальное вычисление параметров (только затронутые пересчитываются)
 * - Оптимизация порядка вычисления через DependencyGraph
 * - Параллельная поддержка обновления (thread-safe update queues)
 * - Профилирование производительности (время пересчета)
 * - Интеграция с конвейером FEA/рендеринга
 * - Откат при ошибках вычисления
 */
class RebuildEngine {
public:
    /**
     * Функция для вычисления параметра
     * Возвращает новое значение параметра на основе его зависимостей
     */
    using ComputeFunction = std::function<
        std::optional<ParameterValue>(
            EntityId entity_id,
            const std::string& param_name,
            const ParameterStore& store
        )
    >;

    /**
     * Обработчик события перестроения
     */
    using RebuildEventHandler = std::function<void(const RebuildResult&)>;

    explicit RebuildEngine(
        CadDocument& doc,
        ParameterStore& store,
        DependencyGraph& graph,
        DocumentEventBus& bus
    );
    ~RebuildEngine() = default;

    // ==================== Регистрация функций вычисления ====================

    /**
     * Зарегистрировать функцию вычисления параметра
     */
    void register_compute_function(
        EntityId entity_id,
        const std::string& param_name,
        ComputeFunction func
    );

    /**
     * Зарегистрировать функцию для типа параметра
     */
    void register_type_compute_function(
        const std::string& entity_type,
        const std::string& param_name,
        ComputeFunction func
    );

    /**
     * Отменить регистрацию функции
     */
    void unregister_compute_function(EntityId entity_id, const std::string& param_name);

    // ==================== Основное перестроение ====================

    /**
     * Пересчитать один параметр
     */
    [[nodiscard]] RebuildResult rebuild_parameter(
        EntityId entity_id,
        const std::string& param_name
    );

    /**
     * Пересчитать все параметры сущности
     */
    [[nodiscard]] RebuildReport rebuild_entity(EntityId entity_id);

    /**
     * Инкрементальное перестроение (только измененные и зависимые параметры)
     */
    [[nodiscard]] RebuildReport rebuild_incremental();

    /**
     * Полное перестроение документа
     */
    [[nodiscard]] RebuildReport rebuild_all();

    /**
     * Отложенное перестроение (начать в фоне)
     */
    void schedule_rebuild(EntityId entity_id, const std::string& param_name);

    /**
     * Обработать все отложенные перестроения
     */
    [[nodiscard]] RebuildReport flush_scheduled_rebuilds();

    // ==================== Откат ====================

    /**
     * Откатить последнее успешное перестроение
     */
    [[nodiscard]] bool undo_rebuild();

    /**
     * Откатить все перестроения и вернуть исходные значения
     */
    void reset_all();

    // ==================== Прерывание ====================

    /**
     * Запросить остановку текущего перестроения
     */
    void request_stop();

    /**
     * Проверить, был ли запрос остановки
     */
    [[nodiscard]] bool is_stop_requested() const { return stop_requested_; }

    /**
     * Очистить флаг остановки
     */
    void clear_stop_request() { stop_requested_ = false; }

    // ==================== Подписка на события ====================

    /**
     * Подписать на события перестроения
     */
    void subscribe_rebuild_event(RebuildEventHandler handler);

    /**
     * Очистить всех подписчиков
     */
    void clear_event_handlers();

    // ==================== Валидация ====================

    /**
     * Проверить возможность перестроения параметра (есть ли функция вычисления)
     */
    [[nodiscard]] bool can_rebuild(EntityId entity_id, const std::string& param_name) const;

    /**
     * Проверить, есть ли циклические зависимости для параметра
     */
    [[nodiscard]] bool has_circular_dependencies(EntityId entity_id, const std::string& param_name) const;

    // ==================== Профилирование ====================

    /**
     * Включить профилирование времени вычисления
     */
    void enable_profiling(bool enabled) { profiling_enabled_ = enabled; }

    /**
     * Получить время последнего перестроения параметра
     */
    [[nodiscard]] std::optional<std::chrono::milliseconds> get_last_compute_time(
        EntityId entity_id,
        const std::string& param_name
    ) const;

    /**
     * Получить статистику производительности
     */
    struct PerformanceStats {
        std::chrono::milliseconds total_time;
        std::chrono::milliseconds average_time;
        std::chrono::milliseconds min_time;
        std::chrono::milliseconds max_time;
        size_t computation_count;
    };

    [[nodiscard]] PerformanceStats get_performance_stats(
        EntityId entity_id,
        const std::string& param_name
    ) const;

    // ==================== Состояние ====================

    /**
     * Получить состояние перестроения параметра
     */
    [[nodiscard]] RebuildStatus get_rebuild_status(EntityId entity_id, const std::string& param_name) const;

    /**
     * Получить количество параметров, требующих перестроения
     */
    [[nodiscard]] size_t pending_count() const;

    /**
     * Очистить все отложенные перестроения
     */
    void clear_pending();

    // ==================== Интеграция с конвейером ====================

    /**
     * Уведомить FEA модуль о необходимости пересчета
     * (для интеграции с FEA/рендерингом)
     */
    void notify_fea_pipeline_dirty();

    /**
     * Уведомить рендеринг модуль о необходимости перерисовки
     */
    void notify_render_pipeline_dirty();

    /**
     * Получить флаг грязного состояния FEA
     */
    [[nodiscard]] bool is_fea_dirty() const { return fea_pipeline_dirty_; }

    /**
     * Получить флаг грязного состояния рендеринга
     */
    [[nodiscard]] bool is_render_dirty() const { return render_pipeline_dirty_; }

    /**
     * Сбросить флаги грязного состояния
     */
    void clear_dirty_flags();

private:
    CadDocument& document_;
    ParameterStore& store_;
    DependencyGraph& graph_;
    DocumentEventBus& event_bus_;

    // Регистрация функций вычисления
    std::map<std::pair<EntityId, std::string>, ComputeFunction> compute_functions_;
    std::map<std::pair<std::string, std::string>, ComputeFunction> type_compute_functions_;

    // Очередь отложенных перестроений
    std::queue<std::pair<EntityId, std::string>> rebuild_queue_;

    // История перестроений (для отката)
    std::vector<RebuildReport> rebuild_history_;

    // Статусы перестроений
    std::map<std::pair<EntityId, std::string>, RebuildStatus> rebuild_status_;

    // Профилирование
    bool profiling_enabled_ = false;
    std::map<std::pair<EntityId, std::string>, std::vector<std::chrono::milliseconds>> compute_times_;

    // Обработчики событий
    std::vector<RebuildEventHandler> event_handlers_;

    // Флаги управления
    bool stop_requested_ = false;
    bool fea_pipeline_dirty_ = false;
    bool render_pipeline_dirty_ = false;

    // Вспомогательные методы
    [[nodiscard]] ComputeFunction find_compute_function(
        EntityId entity_id,
        const std::string& param_name
    ) const;
    void notify_rebuild_event(const RebuildResult& result);
    void record_performance(EntityId entity_id, const std::string& param_name, std::chrono::milliseconds time);
    [[nodiscard]] RebuildReport build_report(const std::vector<RebuildResult>& results);
};

}  // namespace bendcore::rebuild
