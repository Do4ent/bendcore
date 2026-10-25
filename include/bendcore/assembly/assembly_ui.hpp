#pragma once

#include "../document/entity_id.hpp"
#include "mate_solver.hpp"
#include <vector>
#include <string>
#include <optional>
#include <memory>

namespace BendCore::CadCore {

/**
 * @brief Constraint visualization style
 */
struct ConstraintVisual {
    /**
     * @brief Visual representation type
     */
    enum class VisualType {
        POINT_TO_POINT,      // Point coincident visualization
        PLANE_TO_PLANE,      // Plane parallel/perpendicular visualization
        DISTANCE_ARROW,      // Distance with dimension arrow
        ANGLE_ARC,          // Angle arc visualization
        TANGENT_SURFACE,    // Tangency surface indicator
        AXIS_ALIGNED,       // Axis alignment indicator
    };

    VisualType type;
    Vector3D primary_point;      // Main visual location
    Vector3D secondary_point;    // Optional second point
    bool is_satisfied = false;   // Visual feedback for satisfaction
    bool is_redundant = false;   // Visual feedback for redundancy
    double value = 0.0;          // For distance/angle constraints
};

/**
 * @brief Assembly Component UI representation
 */
class AssemblyComponentUI {
public:
    /**
     * @brief Constructor
     */
    AssemblyComponentUI(const EntityId& component_id)
        : component_id_(component_id) {}

    /**
     * @brief Get component ID
     */
    [[nodiscard]] const EntityId& getComponentId() const noexcept {
        return component_id_;
    }

    /**
     * @brief Set display name
     */
    void setDisplayName(const std::string& name) noexcept {
        display_name_ = name;
    }

    /**
     * @brief Get display name
     */
    [[nodiscard]] const std::string& getDisplayName() const noexcept {
        return display_name_;
    }

    /**
     * @brief Set component visibility
     */
    void setVisible(bool visible) noexcept { is_visible_ = visible; }

    /**
     * @brief Get visibility
     */
    [[nodiscard]] bool isVisible() const noexcept { return is_visible_; }

    /**
     * @brief Set component as selected
     */
    void setSelected(bool selected) noexcept { is_selected_ = selected; }

    /**
     * @brief Get selection state
     */
    [[nodiscard]] bool isSelected() const noexcept { return is_selected_; }

    /**
     * @brief Set component as highlighted
     */
    void setHighlighted(bool highlighted) noexcept { is_highlighted_ = highlighted; }

    /**
     * @brief Get highlight state
     */
    [[nodiscard]] bool isHighlighted() const noexcept { return is_highlighted_; }

    /**
     * @brief Add constraint to component
     */
    void addConstraint(const EntityId& constraint_id) noexcept {
        constraints_.push_back(constraint_id);
    }

    /**
     * @brief Get all constraints for this component
     */
    [[nodiscard]] const std::vector<EntityId>& getConstraints() const noexcept {
        return constraints_;
    }

private:
    EntityId component_id_;
    std::string display_name_;
    bool is_visible_ = true;
    bool is_selected_ = false;
    bool is_highlighted_ = false;
    std::vector<EntityId> constraints_;
};

/**
 * @brief Constraint UI Tree Item
 */
class ConstraintTreeItem {
public:
    /**
     * @brief Constructor
     */
    ConstraintTreeItem(const EntityId& constraint_id, MateConstraint::MateType type)
        : constraint_id_(constraint_id), mate_type_(type) {}

    /**
     * @brief Get constraint ID
     */
    [[nodiscard]] const EntityId& getId() const noexcept { return constraint_id_; }

    /**
     * @brief Get mate type
     */
    [[nodiscard]] MateConstraint::MateType getMateType() const noexcept {
        return mate_type_;
    }

    /**
     * @brief Get display name for constraint
     */
    [[nodiscard]] std::string getDisplayName() const noexcept;

    /**
     * @brief Set constraint as satisfied
     */
    void setSatisfied(bool satisfied) noexcept { is_satisfied_ = satisfied; }

    /**
     * @brief Get satisfaction state
     */
    [[nodiscard]] bool isSatisfied() const noexcept { return is_satisfied_; }

    /**
     * @brief Set constraint as redundant
     */
    void setRedundant(bool redundant) noexcept { is_redundant_ = redundant; }

    /**
     * @brief Get redundancy state
     */
    [[nodiscard]] bool isRedundant() const noexcept { return is_redundant_; }

    /**
     * @brief Get visual representation
     */
    [[nodiscard]] ConstraintVisual getVisual() const noexcept;

private:
    EntityId constraint_id_;
    MateConstraint::MateType mate_type_;
    bool is_satisfied_ = false;
    bool is_redundant_ = false;
};

/**
 * @brief Assembly Explorer - Hierarchical assembly tree
 */
class AssemblyExplorer {
public:
    /**
     * @brief Constructor
     */
    AssemblyExplorer(const EntityId& assembly_id) : assembly_id_(assembly_id) {}

    /**
     * @brief Add component to assembly
     */
    void addComponent(const std::shared_ptr<AssemblyComponentUI>& component) noexcept {
        components_.push_back(component);
    }

    /**
     * @brief Get all components
     */
    [[nodiscard]] const std::vector<std::shared_ptr<AssemblyComponentUI>>&
    getComponents() const noexcept {
        return components_;
    }

    /**
     * @brief Add constraint to tree
     */
    void addConstraint(const std::shared_ptr<ConstraintTreeItem>& constraint) noexcept {
        constraints_.push_back(constraint);
    }

    /**
     * @brief Get all constraints in tree
     */
    [[nodiscard]] const std::vector<std::shared_ptr<ConstraintTreeItem>>&
    getConstraints() const noexcept {
        return constraints_;
    }

    /**
     * @brief Get component count
     */
    [[nodiscard]] size_t getComponentCount() const noexcept {
        return components_.size();
    }

    /**
     * @brief Get constraint count
     */
    [[nodiscard]] size_t getConstraintCount() const noexcept {
        return constraints_.size();
    }

    /**
     * @brief Expand/collapse tree node
     */
    void setNodeExpanded(const EntityId& node_id, bool expanded) noexcept {
        expanded_nodes_[node_id] = expanded;
    }

    /**
     * @brief Check if node is expanded
     */
    [[nodiscard]] bool isNodeExpanded(const EntityId& node_id) const noexcept {
        auto it = expanded_nodes_.find(node_id);
        return it != expanded_nodes_.end() ? it->second : false;
    }

private:
    EntityId assembly_id_;
    std::vector<std::shared_ptr<AssemblyComponentUI>> components_;
    std::vector<std::shared_ptr<ConstraintTreeItem>> constraints_;
    std::map<EntityId, bool> expanded_nodes_;
};

/**
 * @brief Conflict Resolution Manager
 */
class ConflictResolver {
public:
    /**
     * @brief Conflict severity level
     */
    enum class SeverityLevel {
        INFO,       // Just information
        WARNING,    // Warning - may need attention
        ERROR,      // Error - must be resolved
        CRITICAL,   // Critical - assembly cannot be solved
    };

    /**
     * @brief Conflict record
     */
    struct ConflictRecord {
        EntityId conflict_id = EntityId();
        SeverityLevel severity = SeverityLevel::WARNING;
        std::string description;
        std::vector<EntityId> involved_constraints;
        std::string suggested_resolution;
    };

    /**
     * @brief Add conflict to resolver
     */
    void addConflict(const ConflictRecord& conflict) noexcept {
        conflicts_.push_back(conflict);
    }

    /**
     * @brief Get all conflicts
     */
    [[nodiscard]] const std::vector<ConflictRecord>& getConflicts() const noexcept {
        return conflicts_;
    }

    /**
     * @brief Clear all conflicts
     */
    void clear() noexcept { conflicts_.clear(); }

    /**
     * @brief Get critical conflict count
     */
    [[nodiscard]] size_t getCriticalCount() const noexcept {
        size_t count = 0;
        for (const auto& c : conflicts_) {
            if (c.severity == SeverityLevel::CRITICAL) count++;
        }
        return count;
    }

    /**
     * @brief Get all conflicts of specific severity
     */
    [[nodiscard]] std::vector<ConflictRecord> getConflictsBySeverity(
        SeverityLevel level) const noexcept {
        std::vector<ConflictRecord> result;
        for (const auto& c : conflicts_) {
            if (c.severity == level) result.push_back(c);
        }
        return result;
    }

private:
    std::vector<ConflictRecord> conflicts_;
};

/**
 * @brief Assembly UI Coordinator - Main assembly UI control
 */
class AssemblyUICoordinator {
public:
    /**
     * @brief Constructor
     */
    AssemblyUICoordinator(const EntityId& assembly_id)
        : assembly_id_(assembly_id),
          explorer_(assembly_id),
          solver_() {}

    /**
     * @brief Get assembly explorer
     */
    [[nodiscard]] AssemblyExplorer& getExplorer() noexcept {
        return explorer_;
    }

    /**
     * @brief Get constraint solver
     */
    [[nodiscard]] MateSolver& getSolver() noexcept {
        return solver_;
    }

    /**
     * @brief Get conflict resolver
     */
    [[nodiscard]] ConflictResolver& getConflictResolver() noexcept {
        return conflict_resolver_;
    }

    /**
     * @brief Update assembly visualization
     */
    void updateVisualization() noexcept;

    /**
     * @brief Check for conflicts
     */
    void checkForConflicts() noexcept;

    /**
     * @brief Get assembly health status
     */
    [[nodiscard]] std::string getHealthStatus() const noexcept;

private:
    EntityId assembly_id_;
    AssemblyExplorer explorer_;
    MateSolver solver_;
    ConflictResolver conflict_resolver_;
};

} // namespace BendCore::CadCore
