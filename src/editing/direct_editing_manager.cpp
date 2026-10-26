#include "bendcore/editing/direct_editing_manager.hpp"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <chrono>

namespace BendCore::CadCore {

DirectEditingManager::DirectEditingManager() noexcept
    : next_preview_id_(1) {}

DirectEditingManager::~DirectEditingManager() noexcept {
    clearHistory();
    active_previews_.clear();
}

// ===== Face Editing Operations =====

EditResult DirectEditingManager::pushPullFace(
    const EntityId& face_id,
    double distance,
    bool preserve_parameters) noexcept {

    EditResult result;
    result.success = false;

    if (distance == 0.0) {
        result.error_message = "Push/pull distance cannot be zero";
        return result;
    }

    // Check for conflicts
    Vector3D displacement(0.0, 0.0, distance);
    auto conflicts = detectConflicts(face_id, displacement);

    if (!conflicts.empty()) {
        result.error_message = "Conflicts detected during push/pull operation";
        return result;
    }

    result.success = true;
    result.applied_distance = distance;

    // Infer parameters if needed
    std::vector<ParameterInference> inferences;
    if (preserve_parameters) {
        inferences = inferParameters(face_id, displacement);
        if (!inferences.empty()) {
            if (applyParameterInferences(inferences)) {
                result.affected_features = getParametersDrivingEntity(face_id);
            }
        }
    }

    // Record in history
    recordEditInHistory("PUSH_PULL_FACE", face_id, Vector3D(0, 0, 0), displacement, inferences);

    return result;
}

EditResult DirectEditingManager::moveFace(
    const EntityId& face_id,
    const Vector3D& displacement,
    bool absolute_position) noexcept {

    EditResult result;
    result.success = false;

    // Check for collision
    if (hasCollision(face_id, displacement)) {
        result.error_message = "Collision detected during face movement";
        return result;
    }

    result.success = true;
    result.applied_distance = displacement.magnitude();

    // Infer parameters
    auto inferences = inferParameters(face_id, displacement);
    if (!inferences.empty()) {
        if (!applyParameterInferences(inferences)) {
            // Log or handle parameter application failure
        }
    }

    recordEditInHistory("MOVE_FACE", face_id, Vector3D(0, 0, 0), displacement, inferences);

    return result;
}

EditResult DirectEditingManager::rotateFace(
    const EntityId& face_id,
    const Vector3D& axis,
    double angle_degrees,
    const Vector3D& center) noexcept {

    EditResult result;
    result.success = false;

    if (std::abs(angle_degrees) < 1e-6) {
        result.error_message = "Rotation angle too small";
        return result;
    }

    result.success = true;
    result.applied_distance = angle_degrees;

    auto inferences = inferParameters(face_id, axis);
    recordEditInHistory("ROTATE_FACE", face_id, Vector3D(0, 0, 0), axis, inferences);

    return result;
}

// ===== Edge Editing Operations =====

EditResult DirectEditingManager::moveEdge(
    const EntityId& edge_id,
    const Vector3D& displacement,
    bool preserve_tangency) noexcept {

    EditResult result;
    result.success = false;

    if (displacement.magnitude() < 1e-6) {
        result.error_message = "Edge displacement too small";
        return result;
    }

    // Check for collision
    if (hasCollision(edge_id, displacement)) {
        result.error_message = "Collision detected during edge movement";
        return result;
    }

    auto violations = checkDependencyViolations(edge_id, displacement);
    if (!violations.empty() && preserve_tangency) {
        result.error_message = "Cannot preserve tangency: " + violations[0];
        return result;
    }

    result.success = true;
    result.applied_distance = displacement.magnitude();

    auto inferences = inferParameters(edge_id, displacement);
    recordEditInHistory("MOVE_EDGE", edge_id, Vector3D(0, 0, 0), displacement, inferences);

    return result;
}

EditResult DirectEditingManager::editEdgeRadius(
    const EntityId& edge_id,
    double radius) noexcept {

    EditResult result;
    result.success = false;

    if (radius < 0.0) {
        result.error_message = "Edge radius cannot be negative";
        return result;
    }

    result.success = true;
    result.applied_distance = radius;

    recordEditInHistory("EDIT_EDGE_RADIUS", edge_id,
                       Vector3D(0, 0, 0), Vector3D(radius, 0, 0), {});

    return result;
}

EditResult DirectEditingManager::editEdgeChamfer(
    const EntityId& edge_id,
    double size) noexcept {

    EditResult result;
    result.success = false;

    if (size < 0.0) {
        result.error_message = "Chamfer size cannot be negative";
        return result;
    }

    result.success = true;
    result.applied_distance = size;

    recordEditInHistory("EDIT_EDGE_CHAMFER", edge_id,
                       Vector3D(0, 0, 0), Vector3D(size, 0, 0), {});

    return result;
}

// ===== Smart Editing Operations =====

EditResult DirectEditingManager::modifyHoleSize(
    const EntityId& hole_id,
    double new_diameter) noexcept {

    EditResult result;
    result.success = false;

    if (new_diameter <= 0.0) {
        result.error_message = "Hole diameter must be positive";
        return result;
    }

    result.success = true;
    result.applied_distance = new_diameter;

    recordEditInHistory("MODIFY_HOLE_SIZE", hole_id,
                       Vector3D(0, 0, 0), Vector3D(new_diameter, 0, 0), {});

    return result;
}

EditResult DirectEditingManager::modifyShellThickness(
    const EntityId& shell_id,
    double new_thickness) noexcept {

    EditResult result;
    result.success = false;

    if (new_thickness <= 0.0) {
        result.error_message = "Shell thickness must be positive";
        return result;
    }

    result.success = true;
    result.applied_distance = new_thickness;

    recordEditInHistory("MODIFY_SHELL_THICKNESS", shell_id,
                       Vector3D(0, 0, 0), Vector3D(new_thickness, 0, 0), {});

    return result;
}

EditResult DirectEditingManager::adjustPatternElement(
    const EntityId& pattern_id,
    size_t element_index,
    double adjustment_value) noexcept {

    EditResult result;
    result.success = false;

    if (element_index > 1000) {
        result.error_message = "Invalid element index";
        return result;
    }

    result.success = true;
    result.applied_distance = adjustment_value;

    recordEditInHistory("ADJUST_PATTERN", pattern_id,
                       Vector3D(0, 0, 0), Vector3D(adjustment_value, 0, 0), {});

    return result;
}

EditResult DirectEditingManager::intelligentSizing(
    const EntityId& face_id,
    const std::string& intent) noexcept {

    EditResult result;
    result.success = false;

    if (intent.empty()) {
        result.error_message = "Sizing intent cannot be empty";
        return result;
    }

    result.success = true;

    recordEditInHistory("INTELLIGENT_SIZING", face_id,
                       Vector3D(0, 0, 0), Vector3D(1, 0, 0), {});

    return result;
}

// ===== Parametric Association =====

std::vector<ParameterInference> DirectEditingManager::inferParameters(
    const EntityId& face_or_edge_id,
    const Vector3D& displacement) noexcept {

    std::vector<ParameterInference> inferences;

    auto it = entity_to_parameters_.find(face_or_edge_id);
    if (it != entity_to_parameters_.end()) {
        for (const auto& param_id : it->second) {
            ParameterInference inference;
            inference.parameter_id = param_id;
            inference.parameter_name = "Parameter_" + param_id.toString();
            inference.new_value = displacement.magnitude();
            inference.confidence = 0.85;
            inference.should_apply = true;

            inferences.push_back(inference);
        }
    }

    return inferences;
}

bool DirectEditingManager::applyParameterInferences(
    const std::vector<ParameterInference>& inferences) noexcept {

    if (!validateParameterConstraints(inferences)) {
        return false;
    }

    for (const auto& inference : inferences) {
        if (inference.should_apply) {
            // Update parameter value
            // In a real implementation, this would update the parameter store
        }
    }

    return true;
}

void DirectEditingManager::trackParameterAssociation(
    const EntityId& parameter_id,
    const std::vector<EntityId>& geometry_entity_ids) noexcept {

    parameter_to_entities_[parameter_id] = geometry_entity_ids;

    for (const auto& entity_id : geometry_entity_ids) {
        entity_to_parameters_[entity_id].push_back(parameter_id);
    }
}

std::vector<EntityId> DirectEditingManager::getParametersDrivingEntity(
    const EntityId& entity_id) const noexcept {

    auto it = entity_to_parameters_.find(entity_id);
    if (it != entity_to_parameters_.end()) {
        return it->second;
    }

    return {};
}

// ===== Conflict Detection and Resolution =====

std::vector<ConflictInfo> DirectEditingManager::detectConflicts(
    const EntityId& face_or_edge_id,
    const Vector3D& displacement) noexcept {

    std::vector<ConflictInfo> conflicts;

    // Check for collision
    if (hasCollision(face_or_edge_id, displacement)) {
        ConflictInfo conflict;
        conflict.type = ConflictInfo::ConflictType::INVALID_GEOMETRY;
        conflict.description = "Geometry collision detected";
        conflict.involved_features.push_back(face_or_edge_id);
        conflicts.push_back(conflict);
    }

    // Check for constraint conflicts
    auto constraint_conflicts = analyzeConstraintConflicts(face_or_edge_id, displacement);
    conflicts.insert(conflicts.end(), constraint_conflicts.begin(), constraint_conflicts.end());

    // Check for dependency violations
    auto violations = checkDependencyViolations(face_or_edge_id, displacement);
    if (!violations.empty()) {
        ConflictInfo conflict;
        conflict.type = ConflictInfo::ConflictType::FEATURE_CONFLICT;
        conflict.description = "Dependency violation: " + violations[0];
        conflict.involved_features.push_back(face_or_edge_id);
        conflicts.push_back(conflict);
    }

    return conflicts;
}

bool DirectEditingManager::hasCollision(
    const EntityId& face_or_edge_id,
    const Vector3D& displacement) const noexcept {

    // Check if the displacement would cause geometry to intersect with itself
    // In a real implementation, this would use geometric computation

    double magnitude = displacement.magnitude();
    if (magnitude > 1000.0) {
        return true;  // Unreasonable displacement
    }

    return false;
}

std::vector<std::string> DirectEditingManager::checkDependencyViolations(
    const EntityId& face_or_edge_id,
    const Vector3D& displacement) const noexcept {

    std::vector<std::string> violations;

    // Check if editing this entity would violate dependencies
    auto it = entity_to_parameters_.find(face_or_edge_id);
    if (it != entity_to_parameters_.end() && !it->second.empty()) {
        // Entity is driven by parameters - check if new position is compatible
        for (const auto& param_id : it->second) {
            // In a real implementation, validate parameter constraints
        }
    }

    return violations;
}

std::vector<std::string> DirectEditingManager::getRemediationOptions(
    const ConflictInfo& conflict) const noexcept {

    std::vector<std::string> options;

    switch (conflict.type) {
        case ConflictInfo::ConflictType::UNDER_CONSTRAINED:
            options.push_back("Add additional constraints");
            options.push_back("Use absolute positioning");
            break;

        case ConflictInfo::ConflictType::OVER_CONSTRAINED:
            options.push_back("Remove conflicting constraints");
            options.push_back("Modify constraint values");
            options.push_back("Suppress conflicting features");
            break;

        case ConflictInfo::ConflictType::CIRCULAR_DEPENDENCY:
            options.push_back("Break circular reference");
            options.push_back("Restructure feature tree");
            break;

        case ConflictInfo::ConflictType::INVALID_GEOMETRY:
            options.push_back("Reduce displacement magnitude");
            options.push_back("Change edit direction");
            options.push_back("Modify dependent features");
            break;

        case ConflictInfo::ConflictType::FEATURE_CONFLICT:
            options.push_back("Edit driving parameters instead");
            options.push_back("Suppress conflicting features");
            break;
    }

    return options;
}

// ===== Real-time Feedback =====

uint64_t DirectEditingManager::startPreview(const EntityId& face_or_edge_id) noexcept {
    PreviewSession session;
    session.id = next_preview_id_++;
    session.target_id = face_or_edge_id;
    session.original_position = Vector3D(0, 0, 0);
    session.current_displacement = Vector3D(0, 0, 0);
    session.is_valid = true;

    active_previews_[session.id] = session;
    return session.id;
}

bool DirectEditingManager::updatePreview(
    uint64_t preview_id,
    const Vector3D& displacement) noexcept {

    auto it = active_previews_.find(preview_id);
    if (it == active_previews_.end()) {
        return false;
    }

    it->second.current_displacement = displacement;

    // Check if geometry is still valid
    if (hasCollision(it->second.target_id, displacement)) {
        it->second.is_valid = false;
    } else {
        it->second.is_valid = true;
    }

    // Update dimension feedback
    double magnitude = displacement.magnitude();
    std::stringstream ss;
    ss << "Distance: " << magnitude << " mm";
    it->second.dimension_feedback = ss.str();

    return true;
}

EditResult DirectEditingManager::finalizePreview(uint64_t preview_id) noexcept {
    EditResult result;
    result.success = false;

    auto it = active_previews_.find(preview_id);
    if (it == active_previews_.end()) {
        result.error_message = "Preview session not found";
        return result;
    }

    PreviewSession session = it->second;

    if (!session.is_valid) {
        result.error_message = "Preview geometry is invalid";
        active_previews_.erase(it);
        return result;
    }

    result.success = true;
    result.applied_distance = session.current_displacement.magnitude();
    result.affected_features.push_back(session.target_id);

    recordEditInHistory("FINALIZE_PREVIEW", session.target_id,
                       session.original_position, session.current_displacement, {});

    active_previews_.erase(it);
    return result;
}

void DirectEditingManager::cancelPreview(uint64_t preview_id) noexcept {
    active_previews_.erase(preview_id);
}

std::string DirectEditingManager::getPreviewDimensionFeedback(
    uint64_t preview_id) const noexcept {

    auto it = active_previews_.find(preview_id);
    if (it != active_previews_.end()) {
        return it->second.dimension_feedback;
    }

    return "";
}

bool DirectEditingManager::isPreviewGeometryValid(uint64_t preview_id) const noexcept {
    auto it = active_previews_.find(preview_id);
    if (it != active_previews_.end()) {
        return it->second.is_valid;
    }

    return false;
}

// ===== Undo/Redo & History =====

bool DirectEditingManager::undo() noexcept {
    if (history_.empty()) {
        return false;
    }

    redo_stack_.push_back(history_.back());
    history_.pop_back();
    return true;
}

bool DirectEditingManager::redo() noexcept {
    if (redo_stack_.empty()) {
        return false;
    }

    history_.push_back(redo_stack_.back());
    redo_stack_.pop_back();
    return true;
}

bool DirectEditingManager::canUndo() const noexcept {
    return !history_.empty();
}

bool DirectEditingManager::canRedo() const noexcept {
    return !redo_stack_.empty();
}

void DirectEditingManager::clearHistory() noexcept {
    history_.clear();
    redo_stack_.clear();
}

void DirectEditingManager::startBatchEdit() noexcept {
    in_batch_edit_ = true;
    batch_results_.clear();
}

EditResult DirectEditingManager::endBatchEdit() noexcept {
    EditResult result;
    result.success = true;

    for (const auto& edit_result : batch_results_) {
        if (!edit_result.success) {
            result.success = false;
            result.error_message = "One or more batch edits failed";
            break;
        }
    }

    in_batch_edit_ = false;
    batch_results_.clear();

    return result;
}

void DirectEditingManager::abortBatchEdit() noexcept {
    in_batch_edit_ = false;
    batch_results_.clear();

    // Undo all batch operations
    while (!history_.empty()) {
        if (!undo()) {
            break;
        }
    }
}

// ===== Utility Functions =====

void DirectEditingManager::reset() noexcept {
    clearHistory();
    active_previews_.clear();
    parameter_to_entities_.clear();
    entity_to_parameters_.clear();
    in_batch_edit_ = false;
    batch_results_.clear();
    next_preview_id_ = 1;
}

// ===== Private Methods =====

bool DirectEditingManager::validateGeometry(const EntityId& shape_id) noexcept {
    // In a real implementation, this would validate the shape
    return true;
}

bool DirectEditingManager::validateParameterConstraints(
    const std::vector<ParameterInference>& inferences) noexcept {

    for (const auto& inference : inferences) {
        // Check if parameter values are within valid ranges
        if (inference.new_value < -1e6 || inference.new_value > 1e6) {
            return false;
        }
    }

    return true;
}

std::vector<ConflictInfo> DirectEditingManager::analyzeConstraintConflicts(
    const EntityId& entity_id,
    const Vector3D& displacement) noexcept {

    std::vector<ConflictInfo> conflicts;

    // Check for constraint violations
    auto parameters = getParametersDrivingEntity(entity_id);
    if (!parameters.empty()) {
        // In a real implementation, check constraint satisfaction
        // For now, return empty as validation passes
    }

    return conflicts;
}

bool DirectEditingManager::rebuildDependentFeatures(
    const std::vector<EntityId>& affected_features) noexcept {

    for (const auto& feature_id : affected_features) {
        // In a real implementation, rebuild the feature
        if (!validateGeometry(feature_id)) {
            return false;
        }
    }

    return true;
}

void DirectEditingManager::recordEditInHistory(
    const std::string& operation_type,
    const EntityId& target_id,
    const Vector3D& original_pos,
    const Vector3D& new_pos,
    const std::vector<ParameterInference>& param_changes) noexcept {

    EditHistoryEntry entry;
    entry.operation_type = operation_type;
    entry.target_id = target_id;
    entry.original_position = original_pos;
    entry.new_position = new_pos;
    entry.parameter_changes = param_changes;

    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    entry.timestamp = std::ctime(&time);

    history_.push_back(entry);
    redo_stack_.clear();  // Clear redo stack when new edit is made
}

} // namespace BendCore::CadCore
