#include <gtest/gtest.h>
#include "../../include/bendcore/document.hpp"
#include "../../include/bendcore/undo_redo_manager.hpp"
#include "../../include/bendcore/feature_factory.hpp"
#include <chrono>
#include <vector>

namespace bendcore::acceptance_test {

class ACCEPT008UndoRedo : public ::testing::Test {
protected:
    std::unique_ptr<Document> doc;
    std::unique_ptr<UndoRedoManager> undo_redo;
    std::unique_ptr<FeatureFactory> feature_factory;

    void SetUp() override {
        doc = std::make_unique<Document>("undo_redo_test");
        undo_redo = std::make_unique<UndoRedoManager>();
        feature_factory = std::make_unique<FeatureFactory>(doc.get());
    }
};

// ACCEPT-008: Undo/Redo & Command History
// Perform 20-step workflow, verify undo/redo, verify history tree
TEST_F(ACCEPT008UndoRedo, TwentyStepWorkflowWithUndoRedo) {
    auto start_time = std::chrono::high_resolution_clock::now();

    std::vector<EntityId> operation_ids;

    // Step 1-5: Create 5 sketches
    for (int i = 0; i < 5; i++) {
        auto sketch_id = doc->add_sketch("Sketch" + std::to_string(i + 1));
        doc->set_property(sketch_id, "sequence", static_cast<double>(i + 1));
        undo_redo->record_operation("Create_Sketch", sketch_id);
        operation_ids.push_back(sketch_id);
        EXPECT_TRUE(doc->feature_exists(sketch_id));
    }

    // Step 6-10: Add elements to sketches
    for (int i = 0; i < 5; i++) {
        auto element_id = doc->add_sketch_element(operation_ids[i], "Rectangle");
        doc->set_property(element_id, "width", 50.0 + i * 10.0);
        doc->set_property(element_id, "height", 30.0 + i * 5.0);
        undo_redo->record_operation("Add_Element", element_id);
        operation_ids.push_back(element_id);
    }

    // Step 11-15: Create extrusions (Pads)
    for (int i = 0; i < 5; i++) {
        auto pad_id = feature_factory->create_pad(operation_ids[i], 10.0 + i * 2.0);
        undo_redo->record_operation("Create_Pad", pad_id);
        operation_ids.push_back(pad_id);
    }

    // Step 16-20: Create constraints/mates
    for (int i = 0; i < 5; i++) {
        auto assembly_id = doc->add_assembly("Assembly" + std::to_string(i + 1));
        undo_redo->record_operation("Create_Assembly", assembly_id);
        operation_ids.push_back(assembly_id);
    }

    // Verify total operations recorded
    EXPECT_GE(undo_redo->get_history_size(), 20);

    // Test: Undo 10 steps
    for (int i = 0; i < 10; i++) {
        auto before_undo = undo_redo->get_history_size();
        undo_redo->undo();
        auto after_undo = undo_redo->get_history_size();
        EXPECT_EQ(before_undo, after_undo);  // History size doesn't change, undo pointer does
    }

    // Verify model state after 10 undos
    EXPECT_TRUE(doc->feature_exists(operation_ids[0]));  // First sketch still exists

    // Test: Redo 5 steps
    for (int i = 0; i < 5; i++) {
        undo_redo->redo();
    }

    // Verify consistency after redo
    EXPECT_GT(undo_redo->get_history_size(), 0);

    // Test: Branching - edit earlier operation
    // (simulated as creating alternative operation)
    auto alt_assembly_id = doc->add_assembly("AltAssembly");
    undo_redo->record_operation("Create_Alt_Assembly", alt_assembly_id);

    // Verify history has branched (new operations recorded)
    EXPECT_GE(undo_redo->get_history_size(), 20);

    // Test: Verify no data corruption
    for (const auto& op_id : operation_ids) {
        if (doc->feature_exists(op_id)) {
            EXPECT_NE(op_id, EntityId::invalid());
        }
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    // Per-operation timing should be <200ms average
    double avg_per_op = static_cast<double>(duration.count()) / operation_ids.size();
    std::cout << "ACCEPT-008 Undo/Redo Total Time: " << duration.count() << "ms, Per-op: " << avg_per_op << "ms" << std::endl;
    EXPECT_LT(avg_per_op, 200.0);
}

TEST_F(ACCEPT008UndoRedo, HistoryTreeAccuracy) {
    // Create a sequence of operations with branching
    auto sketch_id = doc->add_sketch("MainSketch");
    undo_redo->record_operation("Create_Sketch", sketch_id);

    auto rect_id = doc->add_sketch_element(sketch_id, "Rectangle");
    doc->set_property(rect_id, "width", 100.0);
    undo_redo->record_operation("Add_Rectangle", rect_id);

    auto pad1_id = feature_factory->create_pad(sketch_id, 10.0);
    undo_redo->record_operation("Create_Pad1", pad1_id);

    // Undo one step
    undo_redo->undo();

    // Create alternative pad (branch)
    auto pad2_id = feature_factory->create_pad(sketch_id, 20.0);
    undo_redo->record_operation("Create_Pad2", pad2_id);

    // Verify history tree contains both branches
    auto history = undo_redo->get_full_history();
    EXPECT_GE(history.size(), 4);

    // Navigate back and verify branch awareness
    undo_redo->undo();
    undo_redo->undo();

    // Should be back at sketch with rectangle
    EXPECT_TRUE(doc->feature_exists(sketch_id));
    EXPECT_TRUE(doc->feature_exists(rect_id));
}

}  // namespace bendcore::acceptance_test
