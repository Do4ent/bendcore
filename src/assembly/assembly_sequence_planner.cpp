#include "bendcore/assembly/assembly_sequence_planner.hpp"
#include <algorithm>

namespace BendCore::CadCore {

bool AssemblySequencePlanner::addComponent(const EntityId& component_id,
                                           const std::vector<EntityId>& dependencies) {
    if (component_id.isNull()) {
        return false;
    }

    // Check if component already exists
    if (dependencies_.find(component_id) != dependencies_.end()) {
        return false;
    }

    AssemblyDependency dep;
    dep.component_id = component_id;
    dep.dependencies = dependencies;

    dependencies_[component_id] = dep;
    return true;
}

bool AssemblySequencePlanner::removeComponent(const EntityId& component_id) noexcept {
    if (dependencies_.erase(component_id) == 0) {
        return false;
    }

    // Remove from other components' dependencies
    for (auto& [_, dep] : dependencies_) {
        auto it = std::find(dep.dependencies.begin(), dep.dependencies.end(), component_id);
        if (it != dep.dependencies.end()) {
            dep.dependencies.erase(it);
        }

        auto dit = std::find(dep.dependents.begin(), dep.dependents.end(), component_id);
        if (dit != dep.dependents.end()) {
            dep.dependents.erase(dit);
        }
    }

    time_estimates_.erase(component_id);
    return true;
}

bool AssemblySequencePlanner::addDependency(const EntityId& dependent_id,
                                            const EntityId& dependency_id) {
    auto dep_it = dependencies_.find(dependent_id);
    if (dep_it == dependencies_.end()) {
        return false;
    }

    auto dependency_dep_it = dependencies_.find(dependency_id);
    if (dependency_dep_it == dependencies_.end()) {
        return false;
    }

    // Check for circular dependency
    if (hasCyclicDependency(dependency_id, dependent_id, std::set<EntityId>())) {
        return false;
    }

    // Add dependency
    auto& deps = dep_it->second.dependencies;
    if (std::find(deps.begin(), deps.end(), dependency_id) == deps.end()) {
        deps.push_back(dependency_id);
    }

    // Add dependent
    auto& dependents = dependency_dep_it->second.dependents;
    if (std::find(dependents.begin(), dependents.end(), dependent_id) == dependents.end()) {
        dependents.push_back(dependent_id);
    }

    return true;
}

bool AssemblySequencePlanner::hasCircularDependencies() const noexcept {
    for (const auto& [component_id, _] : dependencies_) {
        std::set<EntityId> visited;
        if (hasCyclicDependency(component_id, component_id, visited)) {
            return true;
        }
    }
    return false;
}

bool AssemblySequencePlanner::hasCyclicDependency(const EntityId& current,
                                                   const EntityId& target,
                                                   std::set<EntityId>& visited) const noexcept {
    if (current == target && !visited.empty()) {
        return true;
    }

    if (visited.count(current) > 0) {
        return false;
    }

    visited.insert(current);

    auto it = dependencies_.find(current);
    if (it != dependencies_.end()) {
        for (const auto& dep : it->second.dependencies) {
            if (hasCyclicDependency(dep, target, visited)) {
                return true;
            }
        }
    }

    visited.erase(current);
    return false;
}

std::vector<AssemblyStep> AssemblySequencePlanner::generateOptimalSequence() {
    if (hasCircularDependencies()) {
        return {};
    }

    std::vector<AssemblyStep> sequence;
    auto sorted = topologicalSort();

    uint32_t step_number = 1;
    for (const auto& component_id : sorted) {
        AssemblyStep step;
        step.step_number = step_number++;
        step.component_ids.push_back(component_id);

        auto it = time_estimates_.find(component_id);
        if (it != time_estimates_.end()) {
            step.estimated_time_seconds = it->second;
        }

        auto dep_it = dependencies_.find(component_id);
        if (dep_it != dependencies_.end()) {
            step.blocking_steps.clear();
            for (size_t i = 0; i < sorted.size(); ++i) {
                if (sorted[i] == component_id) {
                    break;
                }
                step.blocking_steps.insert(static_cast<uint32_t>(i + 1));
            }
        }

        sequence.push_back(step);
    }

    return sequence;
}

std::vector<ParallelPath> AssemblySequencePlanner::generateParallelPaths(uint32_t num_workers) {
    std::vector<ParallelPath> paths(num_workers);
    for (uint32_t i = 0; i < num_workers; ++i) {
        paths[i].path_id = i + 1;
        paths[i].required_workers = 1;
    }

    auto sequence = generateOptimalSequence();

    uint32_t current_worker = 0;
    for (const auto& step : sequence) {
        paths[current_worker].steps.push_back(step);
        paths[current_worker].total_time_seconds += step.estimated_time_seconds;

        current_worker = (current_worker + 1) % num_workers;
    }

    return paths;
}

std::optional<AssemblyDependency> AssemblySequencePlanner::getDependency(
    const EntityId& component_id) const noexcept {
    auto it = dependencies_.find(component_id);
    if (it != dependencies_.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::vector<EntityId> AssemblySequencePlanner::calculateCriticalPath() {
    std::vector<EntityId> critical_path;
    double max_path_time = 0.0;

    auto sequence = generateOptimalSequence();
    double current_time = 0.0;

    for (const auto& step : sequence) {
        current_time += step.estimated_time_seconds;
    }

    if (current_time > max_path_time) {
        max_path_time = current_time;
        for (const auto& step : sequence) {
            if (!step.component_ids.empty()) {
                critical_path.push_back(step.component_ids[0]);
            }
        }
    }

    return critical_path;
}

bool AssemblySequencePlanner::setTimeEstimate(const EntityId& component_id,
                                              double time_seconds) noexcept {
    if (dependencies_.find(component_id) == dependencies_.end()) {
        return false;
    }

    time_estimates_[component_id] = time_seconds;

    // Recalculate total time
    total_time_estimate_ = 0.0;
    for (const auto& [_, time] : time_estimates_) {
        total_time_estimate_ += time;
    }

    return true;
}

double AssemblySequencePlanner::getTimeEstimate(const EntityId& component_id) const noexcept {
    auto it = time_estimates_.find(component_id);
    if (it != time_estimates_.end()) {
        return it->second;
    }
    return -1.0;
}

double AssemblySequencePlanner::getMinimumParallelTime(uint32_t num_workers) const noexcept {
    if (num_workers == 0) {
        return 0.0;
    }

    double total_time = 0.0;
    for (const auto& [_, time] : time_estimates_) {
        total_time += time;
    }

    return total_time / num_workers;
}

void AssemblySequencePlanner::clear() noexcept {
    dependencies_.clear();
    time_estimates_.clear();
    total_time_estimate_ = 0.0;
}

std::vector<EntityId> AssemblySequencePlanner::topologicalSort() {
    std::vector<EntityId> result;
    std::map<EntityId, uint32_t> in_degree;

    // Calculate in-degrees
    for (const auto& [component_id, _] : dependencies_) {
        if (in_degree.find(component_id) == in_degree.end()) {
            in_degree[component_id] = 0;
        }
    }

    for (const auto& [component_id, dep] : dependencies_) {
        for (const auto& d : dep.dependencies) {
            in_degree[component_id]++;
        }
    }

    // Find all nodes with in-degree 0
    std::vector<EntityId> queue;
    for (const auto& [component_id, degree] : in_degree) {
        if (degree == 0) {
            queue.push_back(component_id);
        }
    }

    // Process queue
    while (!queue.empty()) {
        EntityId current = queue.back();
        queue.pop_back();
        result.push_back(current);

        auto it = dependencies_.find(current);
        if (it != dependencies_.end()) {
            for (const auto& dependent : it->second.dependents) {
                in_degree[dependent]--;
                if (in_degree[dependent] == 0) {
                    queue.push_back(dependent);
                }
            }
        }
    }

    return result;
}

} // namespace BendCore::CadCore
