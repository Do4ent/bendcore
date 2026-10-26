#pragma once

#include "../document/entity_id.hpp"
#include "../geometry/vector.hpp"
#include <vector>
#include <optional>
#include <map>
#include <memory>
#include <string>
#include <deque>

namespace BendCore::CadCore {

/**
 * @brief Edit operation result
 */
struct EditResult {
    bool success = false;
    std::string error_message;
    double applied_distance = 0.0;
    std::vector<EntityId> affected_features;
};

/**
 * @brief Parameter inference information
 */
struct ParameterInference {
    EntityId parameter_id;
    std::string parameter_name;
    double old_value = 0.0;
    double new_value = 0.0;
    double confidence = 0.0;  // 0.0 to 1.0
    bool should_apply = false;
};

/**
 * @brief Conflict information for edit operations
 */
struct ConflictInfo {
    enum class ConflictType {
        UNDER_CONSTRAINED,
        OVER_CONSTRAINED,
        CIRCULAR_DEPENDENCY,
        INVALID_GEOMETRY,
        FEATURE_CONFLICT
    };

    ConflictType type;
    std::string description;
    std::vector<EntityId> involved_features;
    std::vector<std::string> remediation_options;
};

/**
 * @brief Edit history entry
 */
struct EditHistoryEntry {
    std::string operation_type;
    EntityId target_id;
    Vector3D original_position;
    Vector3D new_position;
    double original_value = 0.0;
    double new_value = 0.0;
    std::vector<ParameterInference> parameter_changes;
    std::string timestamp;
};

/**
 * @brief Direct Editing Manager - Main control class for direct editing operations
 */
class DirectEditingManager {
public:
    /**
     * @brief Constructor
     */
    DirectEditingManager() noexcept;

    /**
     * @brief Destructor
     */
    ~DirectEditingManager() noexcept;

    // ===== Face Editing Operations =====

    /**
     * @brief Push/pull face (extrude or cut)
     * @param face_id ID of the face to edit
     * @param distance Distance to push/pull (positive for extrude, negative for cut)
     * @param preserve_parameters If true, try to update associated parameters
     * @return Edit result with success status
     */
    [[nodiscard]] EditResult pushPullFace(
        const EntityId& face_id,
        double distance,
        bool preserve_parameters = true) noexcept;

    /**
     * @brief Move face to new position
     * @param face_id ID of the face
     * @param displacement Vector to move the face by
     * @param absolute_position If true, move to absolute position instead of relative
     * @return Edit result
     */
    [[nodiscard]] EditResult moveFace(
        const EntityId& face_id,
        const Vector3D& displacement,
        bool absolute_position = false) noexcept;

    /**
     * @brief Rotate face around axis
     * @param face_id ID of the face
     * @param axis Rotation axis (unit vector)
     * @param angle_degrees Rotation angle in degrees
     * @param center Point to rotate around
     * @return Edit result
     */
    [[nodiscard]] EditResult rotateFace(
        const EntityId& face_id,
        const Vector3D& axis,
        double angle_degrees,
        const Vector3D& center) noexcept;

    // ===== Edge Editing Operations =====

    /**
     * @brief Move edge to new position
     * @param edge_id ID of the edge
     * @param displacement Vector to move by
     * @param preserve_tangency If true, preserve tangency relationships
     * @return Edit result
     */
    [[nodiscard]] EditResult moveEdge(
        const EntityId& edge_id,
        const Vector3D& displacement,
        bool preserve_tangency = true) noexcept;

    /**
     * @brief Edit edge radius/fillet
     * @param edge_id ID of the edge
     * @param radius New radius value
     * @return Edit result
     */
    [[nodiscard]] EditResult editEdgeRadius(
        const EntityId& edge_id,
        double radius) noexcept;

    /**
     * @brief Edit edge chamfer
     * @param edge_id ID of the edge
     * @param size Chamfer size
     * @return Edit result
     */
    [[nodiscard]] EditResult editEdgeChamfer(
        const EntityId& edge_id,
        double size) noexcept;

    // ===== Smart Editing Operations =====

    /**
     * @brief Modify hole size (auto-detect hole and adjust)
     * @param hole_id ID of the hole feature
     * @param new_diameter New hole diameter
     * @return Edit result
     */
    [[nodiscard]] EditResult modifyHoleSize(
        const EntityId& hole_id,
        double new_diameter) noexcept;

    /**
     * @brief Modify shell thickness
     * @param shell_id ID of the shell feature
     * @param new_thickness New thickness value
     * @return Edit result
     */
    [[nodiscard]] EditResult modifyShellThickness(
        const EntityId& shell_id,
        double new_thickness) noexcept;

    /**
     * @brief Adjust pattern element (spacing, count, etc.)
     * @param pattern_id ID of the pattern feature
     * @param element_index Index of element to adjust
     * @param adjustment_value Value to adjust by
     * @return Edit result
     */
    [[nodiscard]] EditResult adjustPatternElement(
        const EntityId& pattern_id,
        size_t element_index,
        double adjustment_value) noexcept;

    /**
     * @brief Apply intelligent sizing based on intent
     * @param face_id ID of the face
     * @param intent Description of intended change
     * @return Edit result
     */
    [[nodiscard]] EditResult intelligentSizing(
        const EntityId& face_id,
        const std::string& intent) noexcept;

    // ===== Parametric Association =====

    /**
     * @brief Get parameter inferences for an edit
     * @param face_or_edge_id ID of element being edited
     * @param displacement Proposed edit displacement
     * @return Vector of parameter inferences
     */
    [[nodiscard]] std::vector<ParameterInference> inferParameters(
        const EntityId& face_or_edge_id,
        const Vector3D& displacement) noexcept;

    /**
     * @brief Apply parameter changes from edit
     * @param inferences Vector of parameter inferences to apply
     * @return True if all parameters updated successfully
     */
    [[nodiscard]] bool applyParameterInferences(
        const std::vector<ParameterInference>& inferences) noexcept;

    /**
     * @brief Track parameter-to-geometry mapping
     * @param parameter_id ID of parameter
     * @param geometry_entity_ids IDs of geometry entities driven by this parameter
     */
    void trackParameterAssociation(
        const EntityId& parameter_id,
        const std::vector<EntityId>& geometry_entity_ids) noexcept;

    /**
     * @brief Get parameters driving a geometric entity
     * @param entity_id ID of geometric entity
     * @return Vector of parameter IDs
     */
    [[nodiscard]] std::vector<EntityId> getParametersDrivingEntity(
        const EntityId& entity_id) const noexcept;

    // ===== Conflict Detection and Resolution =====

    /**
     * @brief Detect conflicts in proposed edit
     * @param face_or_edge_id ID of element to edit
     * @param displacement Proposed displacement
     * @return Vector of conflicts, empty if no conflicts
     */
    [[nodiscard]] std::vector<ConflictInfo> detectConflicts(
        const EntityId& face_or_edge_id,
        const Vector3D& displacement) noexcept;

    /**
     * @brief Check for collision during edit
     * @param face_or_edge_id ID of element being moved
     * @param displacement Proposed displacement
     * @return True if collision detected
     */
    [[nodiscard]] bool hasCollision(
        const EntityId& face_or_edge_id,
        const Vector3D& displacement) const noexcept;

    /**
     * @brief Get dependency violations from edit
     * @param face_or_edge_id ID of element
     * @param displacement Proposed displacement
     * @return Vector of violation descriptions
     */
    [[nodiscard]] std::vector<std::string> checkDependencyViolations(
        const EntityId& face_or_edge_id,
        const Vector3D& displacement) const noexcept;

    /**
     * @brief Get remediation options for a conflict
     * @param conflict The conflict information
     * @return Vector of suggested remediation steps
     */
    [[nodiscard]] std::vector<std::string> getRemediationOptions(
        const ConflictInfo& conflict) const noexcept;

    // ===== Real-time Feedback =====

    /**
     * @brief Start interactive preview of edit
     * @param face_or_edge_id ID of element
     * @return Preview session ID
     */
    [[nodiscard]] uint64_t startPreview(const EntityId& face_or_edge_id) noexcept;

    /**
     * @brief Update preview with new displacement
     * @param preview_id Preview session ID
     * @param displacement Current displacement
     * @return Updated geometry bounds (for visual feedback)
     */
    [[nodiscard]] bool updatePreview(
        uint64_t preview_id,
        const Vector3D& displacement) noexcept;

    /**
     * @brief Finalize preview (commit the edit)
     * @param preview_id Preview session ID
     * @return Final edit result
     */
    [[nodiscard]] EditResult finalizePreview(uint64_t preview_id) noexcept;

    /**
     * @brief Cancel preview (discard the edit)
     * @param preview_id Preview session ID
     */
    void cancelPreview(uint64_t preview_id) noexcept;

    /**
     * @brief Get dimension feedback for current preview
     * @param preview_id Preview session ID
     * @return Dimension string for display
     */
    [[nodiscard]] std::string getPreviewDimensionFeedback(
        uint64_t preview_id) const noexcept;

    /**
     * @brief Check if geometry is valid after preview
     * @param preview_id Preview session ID
     * @return True if geometry is valid
     */
    [[nodiscard]] bool isPreviewGeometryValid(uint64_t preview_id) const noexcept;

    // ===== Undo/Redo & History =====

    /**
     * @brief Undo last edit operation
     * @return True if undo was successful
     */
    [[nodiscard]] bool undo() noexcept;

    /**
     * @brief Redo last undone operation
     * @return True if redo was successful
     */
    [[nodiscard]] bool redo() noexcept;

    /**
     * @brief Check if undo is available
     * @return True if there are operations to undo
     */
    [[nodiscard]] bool canUndo() const noexcept;

    /**
     * @brief Check if redo is available
     * @return True if there are operations to redo
     */
    [[nodiscard]] bool canRedo() const noexcept;

    /**
     * @brief Get edit history
     * @return Vector of history entries
     */
    [[nodiscard]] const std::deque<EditHistoryEntry>& getHistory() const noexcept {
        return history_;
    }

    /**
     * @brief Clear undo/redo history
     */
    void clearHistory() noexcept;

    /**
     * @brief Batch edit operations - start batch
     */
    void startBatchEdit() noexcept;

    /**
     * @brief Batch edit operations - end batch and apply all
     * @return Result of entire batch
     */
    [[nodiscard]] EditResult endBatchEdit() noexcept;

    /**
     * @brief Abort batch edit without applying changes
     */
    void abortBatchEdit() noexcept;

    // ===== Utility Functions =====

    /**
     * @brief Reset manager state
     */
    void reset() noexcept;

    /**
     * @brief Get active edit count
     * @return Number of active edit operations
     */
    [[nodiscard]] size_t getActiveEditCount() const noexcept {
        return active_previews_.size();
    }

private:
    // Internal state
    std::deque<EditHistoryEntry> history_;
    std::deque<EditHistoryEntry> redo_stack_;

    // Parameter tracking
    std::map<EntityId, std::vector<EntityId>> parameter_to_entities_;
    std::map<EntityId, std::vector<EntityId>> entity_to_parameters_;

    // Active preview sessions
    struct PreviewSession {
        uint64_t id;
        EntityId target_id;
        Vector3D original_position;
        Vector3D current_displacement;
        bool is_valid = true;
        std::string dimension_feedback;
        EntityId parent_shape;
    };

    std::map<uint64_t, PreviewSession> active_previews_;
    uint64_t next_preview_id_ = 1;

    // Batch edit state
    bool in_batch_edit_ = false;
    std::vector<EditResult> batch_results_;

    // Internal methods
    [[nodiscard]] bool validateGeometry(const EntityId& shape_id) noexcept;
    [[nodiscard]] bool validateParameterConstraints(
        const std::vector<ParameterInference>& inferences) noexcept;
    [[nodiscard]] std::vector<ConflictInfo> analyzeConstraintConflicts(
        const EntityId& entity_id,
        const Vector3D& displacement) noexcept;
    [[nodiscard]] bool rebuildDependentFeatures(
        const std::vector<EntityId>& affected_features) noexcept;
    void recordEditInHistory(
        const std::string& operation_type,
        const EntityId& target_id,
        const Vector3D& original_pos,
        const Vector3D& new_pos,
        const std::vector<ParameterInference>& param_changes) noexcept;
};

} // namespace BendCore::CadCore
