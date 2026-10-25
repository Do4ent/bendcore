#include "bendcore/rebuild/rebuild_engine.hpp"

#include <algorithm>
#include <chrono>

namespace bendcore::rebuild {

// ==================== RebuildEngine ====================

RebuildEngine::RebuildEngine(
    CadDocument& doc,
    ParameterStore& store,
    DependencyGraph& graph,
    DocumentEventBus& bus
)
    : document_(doc), store_(store), graph_(graph), event_bus_(bus) {}

void RebuildEngine::register_compute_function(
    EntityId entity_id,
    const std::string& param_name,
    ComputeFunction func
) {
    auto key = std::make_pair(entity_id, param_name);
    compute_functions_[key] = func;
    rebuild_status_[key] = RebuildStatus::Pending;
}

void RebuildEngine::register_type_compute_function(
    const std::string& entity_type,
    const std::string& param_name,
    ComputeFunction func
) {
    auto key = std::make_pair(entity_type, param_name);
    type_compute_functions_[key] = func;
}

void RebuildEngine::unregister_compute_function(EntityId entity_id, const std::string& param_name) {
    auto key = std::make_pair(entity_id, param_name);
    compute_functions_.erase(key);
    rebuild_status_.erase(key);
}

RebuildResult RebuildEngine::rebuild_parameter(
    EntityId entity_id,
    const std::string& param_name
) {
    RebuildResult result;
    result.entity_id = entity_id;
    result.param_name = param_name;

    if (is_stop_requested()) {
        result.status = RebuildStatus::Skipped;
        return result;
    }

    auto key = std::make_pair(entity_id, param_name);
    rebuild_status_[key] = RebuildStatus::Computing;

    auto start_time = std::chrono::high_resolution_clock::now();

    // Найти функцию вычисления
    auto compute_func = find_compute_function(entity_id, param_name);
    if (!compute_func) {
        result.status = RebuildStatus::Failed;
        result.error_message = "No compute function registered for parameter";
        rebuild_status_[key] = RebuildStatus::Failed;
        return result;
    }

    try {
        // Выполнить функцию вычисления
        auto new_value = compute_func(entity_id, param_name, store_);

        if (!new_value) {
            result.status = RebuildStatus::Failed;
            result.error_message = "Compute function returned no value";
            rebuild_status_[key] = RebuildStatus::Failed;
            return result;
        }

        result.new_value = *new_value;

        // Установить новое значение в store
        if (!store_.set(entity_id, param_name, *new_value, "ComputedByEngine")) {
            result.status = RebuildStatus::Failed;
            result.error_message = "Failed to set parameter value";
            rebuild_status_[key] = RebuildStatus::Failed;
            return result;
        }

        result.status = RebuildStatus::Success;
        rebuild_status_[key] = RebuildStatus::Success;
    } catch (const std::exception& e) {
        result.status = RebuildStatus::Failed;
        result.error_message = std::string("Exception: ") + e.what();
        rebuild_status_[key] = RebuildStatus::Failed;
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    result.compute_time =
        std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    if (profiling_enabled_) {
        record_performance(entity_id, param_name, result.compute_time);
    }

    notify_rebuild_event(result);

    return result;
}

RebuildReport RebuildEngine::rebuild_entity(EntityId entity_id) {
    std::vector<RebuildResult> results;

    // Получить все параметры сущности
    auto param_names = store_.get_all_names(entity_id);
    for (const auto& param_name : param_names) {
        auto result = rebuild_parameter(entity_id, param_name);
        results.push_back(result);

        if (is_stop_requested()) {
            break;
        }
    }

    return build_report(results);
}

RebuildReport RebuildEngine::rebuild_incremental() {
    std::vector<RebuildResult> results;

    // Получить грязные параметры из графа зависимостей
    auto dirty_params = graph_.get_dirty_parameters();

    // Топологически отсортировать для правильного порядка вычисления
    auto topo_order = graph_.topological_sort();

    for (const auto& [entity_id, param_name] : topo_order) {
        // Проверить, грязный ли этот параметр
        bool is_dirty = std::find(dirty_params.begin(), dirty_params.end(),
                                  std::make_pair(entity_id, param_name)) != dirty_params.end();

        if (is_dirty) {
            auto result = rebuild_parameter(entity_id, param_name);
            results.push_back(result);

            if (is_stop_requested()) {
                break;
            }
        }
    }

    return build_report(results);
}

RebuildReport RebuildEngine::rebuild_all() {
    std::vector<RebuildResult> results;

    // Получить топологический порядок
    auto topo_order = graph_.topological_sort();

    for (const auto& [entity_id, param_name] : topo_order) {
        auto result = rebuild_parameter(entity_id, param_name);
        results.push_back(result);

        if (is_stop_requested()) {
            break;
        }
    }

    auto report = build_report(results);
    rebuild_history_.push_back(report);

    // Ограничить размер истории
    const size_t max_history = 50;
    if (rebuild_history_.size() > max_history) {
        rebuild_history_.erase(rebuild_history_.begin());
    }

    return report;
}

void RebuildEngine::schedule_rebuild(EntityId entity_id, const std::string& param_name) {
    rebuild_queue_.push({entity_id, param_name});
}

RebuildReport RebuildEngine::flush_scheduled_rebuilds() {
    std::vector<RebuildResult> results;

    while (!rebuild_queue_.empty()) {
        auto [entity_id, param_name] = rebuild_queue_.front();
        rebuild_queue_.pop();

        auto result = rebuild_parameter(entity_id, param_name);
        results.push_back(result);

        if (is_stop_requested()) {
            break;
        }
    }

    return build_report(results);
}

bool RebuildEngine::undo_rebuild() {
    if (rebuild_history_.empty()) {
        return false;
    }

    rebuild_history_.pop_back();
    return true;
}

void RebuildEngine::reset_all() {
    rebuild_queue_ = std::queue<std::pair<EntityId, std::string>>();
    rebuild_history_.clear();
    rebuild_status_.clear();
    compute_times_.clear();
    clear_stop_request();
}

void RebuildEngine::request_stop() {
    stop_requested_ = true;
}

void RebuildEngine::subscribe_rebuild_event(RebuildEventHandler handler) {
    event_handlers_.push_back(handler);
}

void RebuildEngine::clear_event_handlers() {
    event_handlers_.clear();
}

bool RebuildEngine::can_rebuild(EntityId entity_id, const std::string& param_name) const {
    return find_compute_function(entity_id, param_name) != nullptr;
}

bool RebuildEngine::has_circular_dependencies(EntityId entity_id, const std::string& param_name) const {
    // Проверить в графе зависимостей
    auto transitive_preds = graph_.get_transitive_predecessors(entity_id, param_name);

    // Если данный параметр есть в своих предшественниках - есть цикл
    for (const auto& pred : transitive_preds) {
        auto my_dependents = graph_.get_transitive_dependents(pred.first, pred.second);
        if (std::find(my_dependents.begin(), my_dependents.end(),
                     std::make_pair(entity_id, param_name)) != my_dependents.end()) {
            return true;
        }
    }

    return false;
}

std::optional<std::chrono::milliseconds> RebuildEngine::get_last_compute_time(
    EntityId entity_id,
    const std::string& param_name
) const {
    auto key = std::make_pair(entity_id, param_name);
    auto it = compute_times_.find(key);
    if (it == compute_times_.end() || it->second.empty()) {
        return std::nullopt;
    }
    return it->second.back();
}

RebuildEngine::PerformanceStats RebuildEngine::get_performance_stats(
    EntityId entity_id,
    const std::string& param_name
) const {
    PerformanceStats stats{};
    stats.computation_count = 0;

    auto key = std::make_pair(entity_id, param_name);
    auto it = compute_times_.find(key);
    if (it == compute_times_.end() || it->second.empty()) {
        return stats;
    }

    const auto& times = it->second;
    stats.computation_count = times.size();

    stats.total_time = std::chrono::milliseconds(0);
    stats.min_time = times[0];
    stats.max_time = times[0];

    for (const auto& time : times) {
        stats.total_time += time;
        if (time < stats.min_time) {
            stats.min_time = time;
        }
        if (time > stats.max_time) {
            stats.max_time = time;
        }
    }

    if (!times.empty()) {
        stats.average_time = std::chrono::milliseconds(
            stats.total_time.count() / times.size()
        );
    }

    return stats;
}

RebuildStatus RebuildEngine::get_rebuild_status(EntityId entity_id, const std::string& param_name) const {
    auto key = std::make_pair(entity_id, param_name);
    auto it = rebuild_status_.find(key);
    if (it == rebuild_status_.end()) {
        return RebuildStatus::Pending;
    }
    return it->second;
}

size_t RebuildEngine::pending_count() const {
    return rebuild_queue_.size();
}

void RebuildEngine::clear_pending() {
    rebuild_queue_ = std::queue<std::pair<EntityId, std::string>>();
}

void RebuildEngine::notify_fea_pipeline_dirty() {
    fea_pipeline_dirty_ = true;
}

void RebuildEngine::notify_render_pipeline_dirty() {
    render_pipeline_dirty_ = true;
}

void RebuildEngine::clear_dirty_flags() {
    fea_pipeline_dirty_ = false;
    render_pipeline_dirty_ = false;
}

RebuildEngine::ComputeFunction RebuildEngine::find_compute_function(
    EntityId entity_id,
    const std::string& param_name
) const {
    // Сначала проверить специфичную функцию для сущности
    auto key = std::make_pair(entity_id, param_name);
    auto it = compute_functions_.find(key);
    if (it != compute_functions_.end()) {
        return it->second;
    }

    // Потом проверить функцию для типа сущности
    // TODO: Получить тип сущности из document_ и найти функцию

    return nullptr;
}

void RebuildEngine::notify_rebuild_event(const RebuildResult& result) {
    for (auto& handler : event_handlers_) {
        if (handler) {
            handler(result);
        }
    }
}

void RebuildEngine::record_performance(
    EntityId entity_id,
    const std::string& param_name,
    std::chrono::milliseconds time
) {
    auto key = std::make_pair(entity_id, param_name);
    compute_times_[key].push_back(time);

    // Ограничить размер истории
    const size_t max_history = 100;
    auto& times = compute_times_[key];
    if (times.size() > max_history) {
        times.erase(times.begin());
    }
}

RebuildReport RebuildEngine::build_report(const std::vector<RebuildResult>& results) {
    RebuildReport report;
    report.total_parameters = results.size();
    report.successful = 0;
    report.failed = 0;
    report.skipped = 0;
    report.total_time = std::chrono::milliseconds(0);

    for (const auto& result : results) {
        report.results.push_back(result);
        report.total_time += result.compute_time;

        switch (result.status) {
            case RebuildStatus::Success:
                report.successful++;
                break;
            case RebuildStatus::Failed:
                report.failed++;
                break;
            case RebuildStatus::Skipped:
                report.skipped++;
                break;
            default:
                break;
        }
    }

    return report;
}

}  // namespace bendcore::rebuild
