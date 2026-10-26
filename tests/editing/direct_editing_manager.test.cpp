#include <gtest/gtest.h>
#include "bendcore/editing/direct_editing_manager.hpp"
#include "bendcore/document/entity_id.hpp"
#include "bendcore/geometry/vector.hpp"

using namespace BendCore::CadCore;

class DirectEditingManagerTest : public ::testing::Test {
protected:
    DirectEditingManager manager_;

    // Create test entities
    EntityId face_id_ = EntityId::generate();
    EntityId edge_id_ = EntityId::generate();
    EntityId hole_id_ = EntityId::generate();
    EntityId shell_id_ = EntityId::generate();
    EntityId pattern_id_ = EntityId::generate();
    EntityId param_id_ = EntityId::generate();

    void SetUp() override {
        // Set up parameter associations for testing
        manager_.trackParameterAssociation(param_id_, {face_id_, edge_id_});
    }

    void TearDown() override {
        manager_.reset();
    }
};

// ===== Face Push/Pull Tests =====

TEST_F(DirectEditingManagerTest, PushPullFacePositiveDistance) {
    auto result = manager_.pushPullFace(face_id_, 10.0, false);

    EXPECT_TRUE(result.success);
    EXPECT_DOUBLE_EQ(result.applied_distance, 10.0);
    EXPECT_TRUE(result.error_message.empty());
}

TEST_F(DirectEditingManagerTest, PushPullFaceNegativeDistance) {
    auto result = manager_.pushPullFace(face_id_, -5.0, false);

    EXPECT_TRUE(result.success);
    EXPECT_DOUBLE_EQ(result.applied_distance, -5.0);
    EXPECT_TRUE(result.error_message.empty());
}

TEST_F(DirectEditingManagerTest, PushPullFaceZeroDistance) {
    auto result = manager_.pushPullFace(face_id_, 0.0, false);

    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.error_message.empty());
}

TEST_F(DirectEditingManagerTest, PushPullFaceWithParameterPreservation) {
    auto result = manager_.pushPullFace(face_id_, 15.0, true);

    EXPECT_TRUE(result.success);
    EXPECT_DOUBLE_EQ(result.applied_distance, 15.0);
}

TEST_F(DirectEditingManagerTest, PushPullFaceLargeDistance) {
    auto result = manager_.pushPullFace(face_id_, 500.0, false);

    EXPECT_TRUE(result.success);
    EXPECT_DOUBLE_EQ(result.applied_distance, 500.0);
}

// ===== Face Movement Tests =====

TEST_F(DirectEditingManagerTest, MoveFacePositiveDisplacement) {
    Vector3D displacement(5.0, 5.0, 5.0);
    auto result = manager_.moveFace(face_id_, displacement, false);

    EXPECT_TRUE(result.success);
    EXPECT_DOUBLE_EQ(result.applied_distance, displacement.magnitude());
}

TEST_F(DirectEditingManagerTest, MoveFaceXDirection) {
    Vector3D displacement(10.0, 0.0, 0.0);
    auto result = manager_.moveFace(face_id_, displacement, false);

    EXPECT_TRUE(result.success);
    EXPECT_DOUBLE_EQ(result.applied_distance, 10.0);
}

TEST_F(DirectEditingManagerTest, MoveFaceYDirection) {
    Vector3D displacement(0.0, 20.0, 0.0);
    auto result = manager_.moveFace(face_id_, displacement, false);

    EXPECT_TRUE(result.success);
    EXPECT_DOUBLE_EQ(result.applied_distance, 20.0);
}

TEST_F(DirectEditingManagerTest, MoveFaceZDirection) {
    Vector3D displacement(0.0, 0.0, 30.0);
    auto result = manager_.moveFace(face_id_, displacement, false);

    EXPECT_TRUE(result.success);
    EXPECT_DOUBLE_EQ(result.applied_distance, 30.0);
}

TEST_F(DirectEditingManagerTest, MoveFaceAbsolutePosition) {
    Vector3D displacement(100.0, 100.0, 100.0);
    auto result = manager_.moveFace(face_id_, displacement, true);

    EXPECT_TRUE(result.success);
}

// ===== Face Rotation Tests =====

TEST_F(DirectEditingManagerTest, RotateFace90Degrees) {
    Vector3D axis(0.0, 0.0, 1.0);
    Vector3D center(0.0, 0.0, 0.0);
    auto result = manager_.rotateFace(face_id_, axis, 90.0, center);

    EXPECT_TRUE(result.success);
    EXPECT_DOUBLE_EQ(result.applied_distance, 90.0);
}

TEST_F(DirectEditingManagerTest, RotateFace45Degrees) {
    Vector3D axis(1.0, 0.0, 0.0);
    Vector3D center(5.0, 5.0, 5.0);
    auto result = manager_.rotateFace(face_id_, axis, 45.0, center);

    EXPECT_TRUE(result.success);
    EXPECT_DOUBLE_EQ(result.applied_distance, 45.0);
}

TEST_F(DirectEditingManagerTest, RotateFace180Degrees) {
    Vector3D axis(0.0, 1.0, 0.0);
    Vector3D center(0.0, 0.0, 0.0);
    auto result = manager_.rotateFace(face_id_, axis, 180.0, center);

    EXPECT_TRUE(result.success);
}

TEST_F(DirectEditingManagerTest, RotateFaceZeroAngle) {
    Vector3D axis(0.0, 0.0, 1.0);
    Vector3D center(0.0, 0.0, 0.0);
    auto result = manager_.rotateFace(face_id_, axis, 0.0, center);

    EXPECT_FALSE(result.success);
}

// ===== Edge Movement Tests =====

TEST_F(DirectEditingManagerTest, MoveEdgePositiveDisplacement) {
    Vector3D displacement(5.0, 0.0, 0.0);
    auto result = manager_.moveEdge(edge_id_, displacement, true);

    EXPECT_TRUE(result.success);
    EXPECT_DOUBLE_EQ(result.applied_distance, 5.0);
}

TEST_F(DirectEditingManagerTest, MoveEdgePreserveTangency) {
    Vector3D displacement(2.0, 2.0, 0.0);
    auto result = manager_.moveEdge(edge_id_, displacement, true);

    EXPECT_TRUE(result.success);
}

TEST_F(DirectEditingManagerTest, MoveEdgeIgnoreTangency) {
    Vector3D displacement(3.0, 3.0, 3.0);
    auto result = manager_.moveEdge(edge_id_, displacement, false);

    EXPECT_TRUE(result.success);
}

TEST_F(DirectEditingManagerTest, MoveEdgeZeroDisplacement) {
    Vector3D displacement(0.0, 0.0, 0.0);
    auto result = manager_.moveEdge(edge_id_, displacement, true);

    EXPECT_FALSE(result.success);
}

// ===== Edge Radius Tests =====

TEST_F(DirectEditingManagerTest, EditEdgeRadiusPositive) {
    auto result = manager_.editEdgeRadius(edge_id_, 5.0);

    EXPECT_TRUE(result.success);
    EXPECT_DOUBLE_EQ(result.applied_distance, 5.0);
}

TEST_F(DirectEditingManagerTest, EditEdgeRadiusSmall) {
    auto result = manager_.editEdgeRadius(edge_id_, 0.5);

    EXPECT_TRUE(result.success);
    EXPECT_DOUBLE_EQ(result.applied_distance, 0.5);
}

TEST_F(DirectEditingManagerTest, EditEdgeRadiusNegative) {
    auto result = manager_.editEdgeRadius(edge_id_, -1.0);

    EXPECT_FALSE(result.success);
}

TEST_F(DirectEditingManagerTest, EditEdgeRadiusZero) {
    auto result = manager_.editEdgeRadius(edge_id_, 0.0);

    EXPECT_TRUE(result.success);
}

// ===== Edge Chamfer Tests =====

TEST_F(DirectEditingManagerTest, EditEdgeChamferPositive) {
    auto result = manager_.editEdgeChamfer(edge_id_, 2.0);

    EXPECT_TRUE(result.success);
    EXPECT_DOUBLE_EQ(result.applied_distance, 2.0);
}

TEST_F(DirectEditingManagerTest, EditEdgeChamferSmall) {
    auto result = manager_.editEdgeChamfer(edge_id_, 0.1);

    EXPECT_TRUE(result.success);
}

TEST_F(DirectEditingManagerTest, EditEdgeChamferNegative) {
    auto result = manager_.editEdgeChamfer(edge_id_, -0.5);

    EXPECT_FALSE(result.success);
}

// ===== Smart Editing Tests =====

TEST_F(DirectEditingManagerTest, ModifyHoleSizePositive) {
    auto result = manager_.modifyHoleSize(hole_id_, 8.0);

    EXPECT_TRUE(result.success);
    EXPECT_DOUBLE_EQ(result.applied_distance, 8.0);
}

TEST_F(DirectEditingManagerTest, ModifyHoleSizeSmall) {
    auto result = manager_.modifyHoleSize(hole_id_, 0.5);

    EXPECT_TRUE(result.success);
}

TEST_F(DirectEditingManagerTest, ModifyHoleSizeNegative) {
    auto result = manager_.modifyHoleSize(hole_id_, -5.0);

    EXPECT_FALSE(result.success);
}

TEST_F(DirectEditingManagerTest, ModifyShellThicknessPositive) {
    auto result = manager_.modifyShellThickness(shell_id_, 2.0);

    EXPECT_TRUE(result.success);
    EXPECT_DOUBLE_EQ(result.applied_distance, 2.0);
}

TEST_F(DirectEditingManagerTest, ModifyShellThicknessSmall) {
    auto result = manager_.modifyShellThickness(shell_id_, 0.1);

    EXPECT_TRUE(result.success);
}

TEST_F(DirectEditingManagerTest, ModifyShellThicknessNegative) {
    auto result = manager_.modifyShellThickness(shell_id_, -1.0);

    EXPECT_FALSE(result.success);
}

TEST_F(DirectEditingManagerTest, AdjustPatternElementValid) {
    auto result = manager_.adjustPatternElement(pattern_id_, 0, 5.0);

    EXPECT_TRUE(result.success);
    EXPECT_DOUBLE_EQ(result.applied_distance, 5.0);
}

TEST_F(DirectEditingManagerTest, AdjustPatternElementNegative) {
    auto result = manager_.adjustPatternElement(pattern_id_, 2, -2.0);

    EXPECT_TRUE(result.success);
}

TEST_F(DirectEditingManagerTest, AdjustPatternElementInvalidIndex) {
    auto result = manager_.adjustPatternElement(pattern_id_, 10000, 1.0);

    EXPECT_FALSE(result.success);
}

TEST_F(DirectEditingManagerTest, IntelligentSizingWithIntent) {
    auto result = manager_.intelligentSizing(face_id_, "make hole larger");

    EXPECT_TRUE(result.success);
}

TEST_F(DirectEditingManagerTest, IntelligentSizingEmptyIntent) {
    auto result = manager_.intelligentSizing(face_id_, "");

    EXPECT_FALSE(result.success);
}

// ===== Parameter Inference Tests =====

TEST_F(DirectEditingManagerTest, InferParametersForFace) {
    Vector3D displacement(10.0, 0.0, 0.0);
    auto inferences = manager_.inferParameters(face_id_, displacement);

    EXPECT_FALSE(inferences.empty());
    EXPECT_EQ(inferences[0].parameter_id, param_id_);
}

TEST_F(DirectEditingManagerTest, InferParametersForEdge) {
    Vector3D displacement(5.0, 5.0, 0.0);
    auto inferences = manager_.inferParameters(edge_id_, displacement);

    EXPECT_FALSE(inferences.empty());
}

TEST_F(DirectEditingManagerTest, InferParametersUnknownEntity) {
    EntityId unknown_id = EntityId::generate();
    Vector3D displacement(10.0, 0.0, 0.0);
    auto inferences = manager_.inferParameters(unknown_id, displacement);

    EXPECT_TRUE(inferences.empty());
}

TEST_F(DirectEditingManagerTest, ApplyParameterInferences) {
    Vector3D displacement(10.0, 0.0, 0.0);
    auto inferences = manager_.inferParameters(face_id_, displacement);

    bool result = manager_.applyParameterInferences(inferences);
    EXPECT_TRUE(result);
}

TEST_F(DirectEditingManagerTest, TrackParameterAssociation) {
    EntityId new_param = EntityId::generate();
    EntityId geom_entity = EntityId::generate();

    manager_.trackParameterAssociation(new_param, {geom_entity});
    auto params = manager_.getParametersDrivingEntity(geom_entity);

    EXPECT_FALSE(params.empty());
    EXPECT_EQ(params[0], new_param);
}

// ===== Conflict Detection Tests =====

TEST_F(DirectEditingManagerTest, DetectConflicts) {
    Vector3D large_displacement(5000.0, 0.0, 0.0);  // Unreasonable displacement
    auto conflicts = manager_.detectConflicts(face_id_, large_displacement);

    EXPECT_TRUE(!conflicts.empty());
}

TEST_F(DirectEditingManagerTest, HasCollisionLargeDisplacement) {
    Vector3D displacement(10000.0, 0.0, 0.0);
    bool has_collision = manager_.hasCollision(face_id_, displacement);

    EXPECT_TRUE(has_collision);
}

TEST_F(DirectEditingManagerTest, HasCollisionSmallDisplacement) {
    Vector3D displacement(5.0, 0.0, 0.0);
    bool has_collision = manager_.hasCollision(face_id_, displacement);

    EXPECT_FALSE(has_collision);
}

TEST_F(DirectEditingManagerTest, CheckDependencyViolations) {
    Vector3D displacement(10.0, 0.0, 0.0);
    auto violations = manager_.checkDependencyViolations(face_id_, displacement);

    // Should not have violations for tracked parameters
    EXPECT_TRUE(violations.empty());
}

TEST_F(DirectEditingManagerTest, GetRemediationOptions) {
    ConflictInfo conflict;
    conflict.type = ConflictInfo::ConflictType::OVER_CONSTRAINED;
    conflict.description = "Over-constrained";

    auto options = manager_.getRemediationOptions(conflict);

    EXPECT_FALSE(options.empty());
}

// ===== Preview Tests =====

TEST_F(DirectEditingManagerTest, StartPreview) {
    uint64_t preview_id = manager_.startPreview(face_id_);

    EXPECT_GT(preview_id, 0);
    EXPECT_EQ(manager_.getActiveEditCount(), 1);
}

TEST_F(DirectEditingManagerTest, UpdatePreviewValid) {
    uint64_t preview_id = manager_.startPreview(face_id_);
    Vector3D displacement(10.0, 0.0, 0.0);

    bool result = manager_.updatePreview(preview_id, displacement);

    EXPECT_TRUE(result);
    EXPECT_TRUE(manager_.isPreviewGeometryValid(preview_id));
}

TEST_F(DirectEditingManagerTest, UpdatePreviewInvalid) {
    uint64_t preview_id = manager_.startPreview(face_id_);
    Vector3D large_displacement(10000.0, 0.0, 0.0);

    manager_.updatePreview(preview_id, large_displacement);

    EXPECT_FALSE(manager_.isPreviewGeometryValid(preview_id));
}

TEST_F(DirectEditingManagerTest, GetPreviewDimensionFeedback) {
    uint64_t preview_id = manager_.startPreview(face_id_);
    Vector3D displacement(10.0, 0.0, 0.0);
    manager_.updatePreview(preview_id, displacement);

    std::string feedback = manager_.getPreviewDimensionFeedback(preview_id);

    EXPECT_FALSE(feedback.empty());
}

TEST_F(DirectEditingManagerTest, FinalizePreviewSuccess) {
    uint64_t preview_id = manager_.startPreview(face_id_);
    Vector3D displacement(10.0, 0.0, 0.0);
    manager_.updatePreview(preview_id, displacement);

    auto result = manager_.finalizePreview(preview_id);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(manager_.getActiveEditCount(), 0);
}

TEST_F(DirectEditingManagerTest, FinalizePreviewInvalid) {
    uint64_t preview_id = manager_.startPreview(face_id_);
    Vector3D large_displacement(10000.0, 0.0, 0.0);
    manager_.updatePreview(preview_id, large_displacement);

    auto result = manager_.finalizePreview(preview_id);

    EXPECT_FALSE(result.success);
}

TEST_F(DirectEditingManagerTest, CancelPreview) {
    uint64_t preview_id = manager_.startPreview(face_id_);
    manager_.cancelPreview(preview_id);

    EXPECT_EQ(manager_.getActiveEditCount(), 0);
}

// ===== Undo/Redo Tests =====

TEST_F(DirectEditingManagerTest, UndoAfterEdit) {
    auto result = manager_.pushPullFace(face_id_, 10.0, false);
    EXPECT_TRUE(result.success);
    EXPECT_TRUE(manager_.canUndo());

    bool undo_result = manager_.undo();
    EXPECT_TRUE(undo_result);
    EXPECT_TRUE(manager_.canRedo());
}

TEST_F(DirectEditingManagerTest, RedoAfterUndo) {
    auto result = manager_.pushPullFace(face_id_, 10.0, false);
    manager_.undo();

    bool redo_result = manager_.redo();
    EXPECT_TRUE(redo_result);
    EXPECT_FALSE(manager_.canRedo());
}

TEST_F(DirectEditingManagerTest, UndoWithoutEdits) {
    bool undo_result = manager_.undo();
    EXPECT_FALSE(undo_result);
}

TEST_F(DirectEditingManagerTest, RedoWithoutUndos) {
    bool redo_result = manager_.redo();
    EXPECT_FALSE(redo_result);
}

TEST_F(DirectEditingManagerTest, MultipleUndos) {
    manager_.pushPullFace(face_id_, 10.0, false);
    manager_.pushPullFace(face_id_, 5.0, false);
    manager_.pushPullFace(face_id_, 3.0, false);

    EXPECT_TRUE(manager_.canUndo());
    manager_.undo();
    EXPECT_TRUE(manager_.canUndo());
    manager_.undo();
    EXPECT_TRUE(manager_.canUndo());
    manager_.undo();
    EXPECT_FALSE(manager_.canUndo());
}

TEST_F(DirectEditingManagerTest, ClearHistory) {
    manager_.pushPullFace(face_id_, 10.0, false);
    EXPECT_TRUE(manager_.canUndo());

    manager_.clearHistory();
    EXPECT_FALSE(manager_.canUndo());
}

// ===== Batch Edit Tests =====

TEST_F(DirectEditingManagerTest, BatchEditStartAndEnd) {
    manager_.startBatchEdit();
    EXPECT_TRUE(true);

    auto result = manager_.endBatchEdit();
    EXPECT_TRUE(result.success);
}

TEST_F(DirectEditingManagerTest, BatchEditMultipleOperations) {
    manager_.startBatchEdit();

    manager_.pushPullFace(face_id_, 10.0, false);
    manager_.moveEdge(edge_id_, Vector3D(5.0, 0.0, 0.0), true);
    manager_.editEdgeRadius(edge_id_, 2.0);

    auto result = manager_.endBatchEdit();
    EXPECT_TRUE(result.success);
}

TEST_F(DirectEditingManagerTest, AbortBatchEdit) {
    manager_.startBatchEdit();
    manager_.pushPullFace(face_id_, 10.0, false);

    manager_.abortBatchEdit();
    EXPECT_FALSE(manager_.canUndo());
}

// ===== Reset Tests =====

TEST_F(DirectEditingManagerTest, ResetManager) {
    manager_.pushPullFace(face_id_, 10.0, false);
    manager_.startPreview(face_id_);

    manager_.reset();

    EXPECT_FALSE(manager_.canUndo());
    EXPECT_EQ(manager_.getActiveEditCount(), 0);
}

// ===== Complex Geometry Tests =====

TEST_F(DirectEditingManagerTest, PushPullComplexFace) {
    // Simulate complex geometry with multiple constraints
    EntityId complex_face = EntityId::generate();
    manager_.trackParameterAssociation(param_id_, {complex_face});

    auto result = manager_.pushPullFace(complex_face, 8.0, true);
    EXPECT_TRUE(result.success);
}

TEST_F(DirectEditingManagerTest, EditMultipleFacesSequentially) {
    EntityId face1 = EntityId::generate();
    EntityId face2 = EntityId::generate();

    auto result1 = manager_.pushPullFace(face1, 5.0, false);
    auto result2 = manager_.pushPullFace(face2, 3.0, false);

    EXPECT_TRUE(result1.success);
    EXPECT_TRUE(result2.success);
    EXPECT_TRUE(manager_.canUndo());
}

// ===== Edge Cases =====

TEST_F(DirectEditingManagerTest, VerySmallDisplacements) {
    Vector3D tiny_displacement(0.00001, 0.0, 0.0);
    auto result = manager_.moveFace(face_id_, tiny_displacement, false);

    EXPECT_TRUE(result.success);
}

TEST_F(DirectEditingManagerTest, VeryLargeDisplacements) {
    Vector3D huge_displacement(999.0, 999.0, 999.0);
    auto result = manager_.moveFace(face_id_, huge_displacement, false);

    EXPECT_TRUE(result.success);
}

TEST_F(DirectEditingManagerTest, ParameterInferenceWithHighConfidence) {
    Vector3D displacement(10.0, 0.0, 0.0);
    auto inferences = manager_.inferParameters(face_id_, displacement);

    EXPECT_FALSE(inferences.empty());
    EXPECT_GT(inferences[0].confidence, 0.8);
}
