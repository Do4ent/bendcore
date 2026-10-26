#pragma once

#include "../document/entity_id.hpp"
#include <string>
#include <vector>
#include <map>
#include <set>
#include <memory>
#include <optional>
#include <cstdint>

namespace BendCore::CadCore {

/**
 * @brief Assembly dependency information
 */
struct AssemblyDependency {
    EntityId component_id;
    std::vector<EntityId> dependencies;      // Components that must be assembled first
    std::vector<EntityId> dependents;        // Components that depend on this one
    bool is_critical_path = false;           // Part of critical path
};

/**
 * @brief Assembly step in the sequence
 */
struct AssemblyStep {
    uint32_t step_number = 0;
    std::vector<EntityId> component_ids;     // Components to assemble in this step
    std::string description;
    std::vector<EntityId> required_tools;    // Tool IDs needed
    std::vector<EntityId> required_fasteners;// Fastener IDs needed
    double estimated_time_seconds = 0.0;     // Time estimate
    bool can_parallelize = false;             // Can be done in parallel with other steps
    std::set<uint32_t> blocking_steps;       // Steps that must complete first
};

/**
 * @brief Parallel assembly path information
 */
struct ParallelPath {
    uint32_t path_id = 0;
    std::vector<AssemblyStep> steps;
    double total_time_seconds = 0.0;
    uint32_t required_workers = 1;
};

/**
 * @brief Assembly Sequence Planner - Optimizes assembly order
 *
 * Provides:
 * - Dependency analysis
 * - Optimal assembly sequence generation
 * - Parallel assembly paths
 * - Resource allocation
 * - Time estimation
 */
class AssemblySequencePlanner {
public:
    /**
     * @brief Constructor
     * @param assembly_id ID of assembly to plan
     */
    explicit AssemblySequencePlanner(const EntityId& assembly_id) noexcept
        : assembly_id_(assembly_id) {}

    /**
     * @brief Add a component with dependencies
     * @param component_id Component ID
     * @param dependencies Component IDs that must be assembled first
     * @return true if added, false if already exists
     */
    bool addComponent(const EntityId& component_id,
                      const std::vector<EntityId>& dependencies = {});

    /**
     * @brief Remove a component from planning
     * @param component_id Component ID to remove
     * @return true if removed, false if not found
     */
    bool removeComponent(const EntityId& component_id) noexcept;

    /**
     * @brief Add dependency between components
     * @param dependent_id Component that depends on another
     * @param dependency_id Component that must be assembled first
     * @return true if added, false if would create cycle
     */
    bool addDependency(const EntityId& dependent_id, const EntityId& dependency_id);

    /**
     * @brief Check for circular dependencies
     * @return true if circular dependencies exist
     */
    [[nodiscard]] bool hasCircularDependencies() const noexcept;

    /**
     * @brief Generate optimal assembly sequence
     * @return Vector of assembly steps in optimal order
     */
    [[nodiscard]] std::vector<AssemblyStep> generateOptimalSequence();

    /**
     * @brief Generate parallel assembly paths
     * @param num_workers Number of available workers
     * @return Vector of parallel paths
     */
    [[nodiscard]] std::vector<ParallelPath> generateParallelPaths(uint32_t num_workers = 2);

    /**
     * @brief Get dependency information
     * @param component_id Component ID
     * @return Dependency info, or empty optional if not found
     */
    [[nodiscard]] std::optional<AssemblyDependency> getDependency(
        const EntityId& component_id) const noexcept;

    /**
     * @brief Get all components in the assembly
     */
    [[nodiscard]] const std::map<EntityId, AssemblyDependency>& getAllComponents() const noexcept {
        return dependencies_;
    }

    /**
     * @brief Calculate critical path
     * @return Vector of component IDs on the critical path
     */
    [[nodiscard]] std::vector<EntityId> calculateCriticalPath();

    /**
     * @brief Set time estimate for a component
     * @param component_id Component ID
     * @param time_seconds Time estimate in seconds
     * @return true if set, false if component not found
     */
    bool setTimeEstimate(const EntityId& component_id, double time_seconds) noexcept;

    /**
     * @brief Get time estimate for a component
     * @param component_id Component ID
     * @return Time estimate in seconds, or -1 if not found
     */
    [[nodiscard]] double getTimeEstimate(const EntityId& component_id) const noexcept;

    /**
     * @brief Get total assembly time estimate
     */
    [[nodiscard]] double getTotalTimeEstimate() const noexcept { return total_time_estimate_; }

    /**
     * @brief Get minimum assembly time with parallel work
     * @param num_workers Number of workers
     * @return Minimum time in seconds
     */
    [[nodiscard]] double getMinimumParallelTime(uint32_t num_workers) const noexcept;

    /**
     * @brief Get component count
     */
    [[nodiscard]] size_t getComponentCount() const noexcept { return dependencies_.size(); }

    /**
     * @brief Clear all planning data
     */
    void clear() noexcept;

private:
    EntityId assembly_id_;
    std::map<EntityId, AssemblyDependency> dependencies_;
    std::map<EntityId, double> time_estimates_;
    double total_time_estimate_ = 0.0;

    // Helper functions
    bool hasCyclicDependency(const EntityId& current, const EntityId& target,
                            std::set<EntityId>& visited) const noexcept;
    std::vector<EntityId> topologicalSort();
};

} // namespace BendCore::CadCore
