#include <cassert>
#include <iostream>
#include <string>
#include "bendcore/editing/direct_editing_manager.hpp"
#include "bendcore/document/entity_id.hpp"
#include "bendcore/geometry/vector.hpp"

using namespace BendCore::CadCore;

// Simple test macro
int test_count = 0;
int test_passed = 0;

void assert_true(bool condition, const std::string& test_name) {
    test_count++;
    if (condition) {
        test_passed++;
        std::cout << "[PASS] " << test_name << std::endl;
    } else {
        std::cout << "[FAIL] " << test_name << std::endl;
    }
}

void assert_false(bool condition, const std::string& test_name) {
    assert_true(!condition, test_name);
}

void assert_equal(double a, double b, const std::string& test_name, double epsilon = 1e-6) {
    assert_true(std::abs(a - b) < epsilon, test_name);
}

void assert_not_empty(const std::string& str, const std::string& test_name) {
    assert_true(!str.empty(), test_name);
}

// ===== Tests =====

void test_push_pull_face_positive_distance() {
    DirectEditingManager manager;
    EntityId face_id = EntityId::generate();

    auto result = manager.pushPullFace(face_id, 10.0, false);

    assert_true(result.success, "PushPullFace positive distance - success");
    assert_equal(result.applied_distance, 10.0, "PushPullFace positive distance - distance");
    assert_true(result.error_message.empty(), "PushPullFace positive distance - no error");
}

void test_push_pull_face_negative_distance() {
    DirectEditingManager manager;
    EntityId face_id = EntityId::generate();

    auto result = manager.pushPullFace(face_id, -5.0, false);

    assert_true(result.success, "PushPullFace negative distance - success");
    assert_equal(result.applied_distance, -5.0, "PushPullFace negative distance - distance");
}

void test_push_pull_face_zero_distance() {
    DirectEditingManager manager;
    EntityId face_id = EntityId::generate();

    auto result = manager.pushPullFace(face_id, 0.0, false);

    assert_false(result.success, "PushPullFace zero distance - should fail");
    assert_not_empty(result.error_message, "PushPullFace zero distance - error message");
}

void test_move_face() {
    DirectEditingManager manager;
    EntityId face_id = EntityId::generate();
    Vector3D displacement(5.0, 5.0, 5.0);

    auto result = manager.moveFace(face_id, displacement, false);

    assert_true(result.success, "MoveFace - success");
    assert_equal(result.applied_distance, displacement.magnitude(), "MoveFace - distance");
}

void test_rotate_face() {
    DirectEditingManager manager;
    EntityId face_id = EntityId::generate();
    Vector3D axis(0.0, 0.0, 1.0);
    Vector3D center(0.0, 0.0, 0.0);

    auto result = manager.rotateFace(face_id, axis, 90.0, center);

    assert_true(result.success, "RotateFace - success");
    assert_equal(result.applied_distance, 90.0, "RotateFace - angle");
}

void test_rotate_face_zero_angle() {
    DirectEditingManager manager;
    EntityId face_id = EntityId::generate();
    Vector3D axis(0.0, 0.0, 1.0);
    Vector3D center(0.0, 0.0, 0.0);

    auto result = manager.rotateFace(face_id, axis, 0.0, center);

    assert_false(result.success, "RotateFace zero angle - should fail");
}

void test_move_edge() {
    DirectEditingManager manager;
    EntityId edge_id = EntityId::generate();
    Vector3D displacement(5.0, 0.0, 0.0);

    auto result = manager.moveEdge(edge_id, displacement, true);

    assert_true(result.success, "MoveEdge - success");
    assert_equal(result.applied_distance, 5.0, "MoveEdge - distance");
}

void test_edit_edge_radius() {
    DirectEditingManager manager;
    EntityId edge_id = EntityId::generate();

    auto result = manager.editEdgeRadius(edge_id, 5.0);

    assert_true(result.success, "EditEdgeRadius - success");
    assert_equal(result.applied_distance, 5.0, "EditEdgeRadius - radius");
}

void test_edit_edge_radius_negative() {
    DirectEditingManager manager;
    EntityId edge_id = EntityId::generate();

    auto result = manager.editEdgeRadius(edge_id, -1.0);

    assert_false(result.success, "EditEdgeRadius negative - should fail");
}

void test_edit_edge_chamfer() {
    DirectEditingManager manager;
    EntityId edge_id = EntityId::generate();

    auto result = manager.editEdgeChamfer(edge_id, 2.0);

    assert_true(result.success, "EditEdgeChamfer - success");
    assert_equal(result.applied_distance, 2.0, "EditEdgeChamfer - size");
}

void test_modify_hole_size() {
    DirectEditingManager manager;
    EntityId hole_id = EntityId::generate();

    auto result = manager.modifyHoleSize(hole_id, 8.0);

    assert_true(result.success, "ModifyHoleSize - success");
    assert_equal(result.applied_distance, 8.0, "ModifyHoleSize - diameter");
}

void test_modify_hole_size_negative() {
    DirectEditingManager manager;
    EntityId hole_id = EntityId::generate();

    auto result = manager.modifyHoleSize(hole_id, -5.0);

    assert_false(result.success, "ModifyHoleSize negative - should fail");
}

void test_modify_shell_thickness() {
    DirectEditingManager manager;
    EntityId shell_id = EntityId::generate();

    auto result = manager.modifyShellThickness(shell_id, 2.0);

    assert_true(result.success, "ModifyShellThickness - success");
    assert_equal(result.applied_distance, 2.0, "ModifyShellThickness - thickness");
}

void test_adjust_pattern_element() {
    DirectEditingManager manager;
    EntityId pattern_id = EntityId::generate();

    auto result = manager.adjustPatternElement(pattern_id, 0, 5.0);

    assert_true(result.success, "AdjustPatternElement - success");
    assert_equal(result.applied_distance, 5.0, "AdjustPatternElement - value");
}

void test_intelligent_sizing() {
    DirectEditingManager manager;
    EntityId face_id = EntityId::generate();

    auto result = manager.intelligentSizing(face_id, "make hole larger");

    assert_true(result.success, "IntelligentSizing - success");
}

void test_intelligent_sizing_empty_intent() {
    DirectEditingManager manager;
    EntityId face_id = EntityId::generate();

    auto result = manager.intelligentSizing(face_id, "");

    assert_false(result.success, "IntelligentSizing empty - should fail");
}

void test_infer_parameters() {
    DirectEditingManager manager;
    EntityId face_id = EntityId::generate();
    EntityId param_id = EntityId::generate();

    manager.trackParameterAssociation(param_id, {face_id});

    Vector3D displacement(10.0, 0.0, 0.0);
    auto inferences = manager.inferParameters(face_id, displacement);

    assert_true(!inferences.empty(), "InferParameters - not empty");
}

void test_infer_parameters_unknown_entity() {
    DirectEditingManager manager;
    EntityId unknown_id = EntityId::generate();
    Vector3D displacement(10.0, 0.0, 0.0);

    auto inferences = manager.inferParameters(unknown_id, displacement);

    assert_true(inferences.empty(), "InferParameters unknown - empty");
}

void test_track_parameter_association() {
    DirectEditingManager manager;
    EntityId param_id = EntityId::generate();
    EntityId geom_entity = EntityId::generate();

    manager.trackParameterAssociation(param_id, {geom_entity});
    auto params = manager.getParametersDrivingEntity(geom_entity);

    assert_true(!params.empty(), "TrackParameterAssociation - not empty");
}

void test_collision_detection() {
    DirectEditingManager manager;
    EntityId face_id = EntityId::generate();
    Vector3D large_displacement(10000.0, 0.0, 0.0);

    bool has_collision = manager.hasCollision(face_id, large_displacement);

    assert_true(has_collision, "HasCollision - detected");
}

void test_no_collision() {
    DirectEditingManager manager;
    EntityId face_id = EntityId::generate();
    Vector3D small_displacement(5.0, 0.0, 0.0);

    bool has_collision = manager.hasCollision(face_id, small_displacement);

    assert_false(has_collision, "NoCollision - not detected");
}

void test_start_preview() {
    DirectEditingManager manager;
    EntityId face_id = EntityId::generate();

    uint64_t preview_id = manager.startPreview(face_id);

    assert_true(preview_id > 0, "StartPreview - valid ID");
    assert_true(manager.getActiveEditCount() == 1, "StartPreview - active count");
}

void test_update_preview() {
    DirectEditingManager manager;
    EntityId face_id = EntityId::generate();

    uint64_t preview_id = manager.startPreview(face_id);
    Vector3D displacement(10.0, 0.0, 0.0);

    bool result = manager.updatePreview(preview_id, displacement);

    assert_true(result, "UpdatePreview - success");
    assert_true(manager.isPreviewGeometryValid(preview_id), "UpdatePreview - valid geometry");
}

void test_cancel_preview() {
    DirectEditingManager manager;
    EntityId face_id = EntityId::generate();

    uint64_t preview_id = manager.startPreview(face_id);
    manager.cancelPreview(preview_id);

    assert_true(manager.getActiveEditCount() == 0, "CancelPreview - count");
}

void test_undo_after_edit() {
    DirectEditingManager manager;
    EntityId face_id = EntityId::generate();

    auto result = manager.pushPullFace(face_id, 10.0, false);
    assert_true(result.success, "UndoAfterEdit - edit succeeded");
    assert_true(manager.canUndo(), "UndoAfterEdit - can undo");

    bool undo_result = manager.undo();
    assert_true(undo_result, "UndoAfterEdit - undo succeeded");
    assert_true(manager.canRedo(), "UndoAfterEdit - can redo");
}

void test_undo_without_edits() {
    DirectEditingManager manager;

    bool undo_result = manager.undo();

    assert_false(undo_result, "UndoWithoutEdits - should fail");
}

void test_batch_edit() {
    DirectEditingManager manager;
    EntityId face_id = EntityId::generate();
    EntityId edge_id = EntityId::generate();

    manager.startBatchEdit();
    manager.pushPullFace(face_id, 10.0, false);
    manager.editEdgeRadius(edge_id, 2.0);
    auto result = manager.endBatchEdit();

    assert_true(result.success, "BatchEdit - success");
}

void test_reset_manager() {
    DirectEditingManager manager;
    EntityId face_id = EntityId::generate();

    manager.pushPullFace(face_id, 10.0, false);
    manager.startPreview(face_id);

    manager.reset();

    assert_false(manager.canUndo(), "ResetManager - no undo");
    assert_true(manager.getActiveEditCount() == 0, "ResetManager - no active edits");
}

void test_multiple_operations() {
    DirectEditingManager manager;
    EntityId face1 = EntityId::generate();
    EntityId face2 = EntityId::generate();

    auto result1 = manager.pushPullFace(face1, 5.0, false);
    auto result2 = manager.pushPullFace(face2, 3.0, false);

    assert_true(result1.success, "MultipleOps - result1");
    assert_true(result2.success, "MultipleOps - result2");
    assert_true(manager.canUndo(), "MultipleOps - can undo");
}

int main() {
    std::cout << "Starting Direct Editing Manager Tests..." << std::endl;
    std::cout << std::string(50, '=') << std::endl;

    // Face editing tests
    test_push_pull_face_positive_distance();
    test_push_pull_face_negative_distance();
    test_push_pull_face_zero_distance();
    test_move_face();
    test_rotate_face();
    test_rotate_face_zero_angle();

    // Edge editing tests
    test_move_edge();
    test_edit_edge_radius();
    test_edit_edge_radius_negative();
    test_edit_edge_chamfer();

    // Smart editing tests
    test_modify_hole_size();
    test_modify_hole_size_negative();
    test_modify_shell_thickness();
    test_adjust_pattern_element();
    test_intelligent_sizing();
    test_intelligent_sizing_empty_intent();

    // Parameter inference tests
    test_infer_parameters();
    test_infer_parameters_unknown_entity();
    test_track_parameter_association();

    // Conflict detection tests
    test_collision_detection();
    test_no_collision();

    // Preview tests
    test_start_preview();
    test_update_preview();
    test_cancel_preview();

    // Undo/redo tests
    test_undo_after_edit();
    test_undo_without_edits();

    // Batch edit tests
    test_batch_edit();

    // Reset and multiple operations tests
    test_reset_manager();
    test_multiple_operations();

    std::cout << std::string(50, '=') << std::endl;
    std::cout << "Tests passed: " << test_passed << "/" << test_count << std::endl;

    if (test_passed == test_count) {
        std::cout << "All tests passed!" << std::endl;
        return 0;
    } else {
        std::cout << "Some tests failed!" << std::endl;
        return 1;
    }
}
