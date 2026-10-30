#include <gtest/gtest.h>
#include "../../include/bendcore/document.hpp"
#include "../../include/bendcore/sketch_engine.hpp"
#include "../../include/bendcore/feature_factory.hpp"
#include "../../include/bendcore/undo_redo_manager.hpp"
#include <chrono>
#include <iostream>

namespace bendcore::acceptance_test {

class ACCEPT001SketchExtrudeFillet : public ::testing::Test {
protected:
    std::unique_ptr<Document> doc;
    std::unique_ptr<SketchEngine> sketch_engine;
    std::unique_ptr<FeatureFactory> feature_factory;
    std::unique_ptr<UndoRedoManager> undo_redo;

    void SetUp() override {
        doc = std::make_unique<Document>("test_project");
        sketch_engine = std::make_unique<SketchEngine>();
        feature_factory = std::make_unique<FeatureFactory>(doc.get());
        undo_redo = std::make_unique<UndoRedoManager>();
    }
};

// ACCEPT-001: Sketch -> Extrude -> Hole -> Fillet Workflow
// Target time: <30 seconds total workflow
TEST_F(ACCEPT001SketchExtrudeFillet, FullWorkflowWith3DGeometry) {
    auto start_time = std::chrono::high_resolution_clock::now();

    // Step 1: Create 2D sketch with rectangle (100x50mm) and circle (10mm diameter)
    auto sketch_id = doc->add_sketch("Sketch1");
    EXPECT_NE(sketch_id, EntityId::invalid());

    // Add rectangle 100x50mm
    auto rect_id = doc->add_sketch_element(sketch_id, "Rectangle");
    doc->set_property(rect_id, "width", 100.0);  // mm
    doc->set_property(rect_id, "height", 50.0);
    doc->set_property(rect_id, "position_x", 0.0);
    doc->set_property(rect_id, "position_y", 0.0);
    EXPECT_EQ(doc->get_property(rect_id, "width"), 100.0);

    // Add circle 10mm diameter (centered inside rectangle)
    auto circle_id = doc->add_sketch_element(sketch_id, "Circle");
    doc->set_property(circle_id, "radius", 5.0);  // 10mm diameter
    doc->set_property(circle_id, "center_x", 50.0);  // centered on rectangle
    doc->set_property(circle_id, "center_y", 25.0);
    EXPECT_EQ(doc->get_property(circle_id, "radius"), 5.0);

    // Verify sketch has 2 elements
    auto sketch_elements = doc->get_sketch_elements(sketch_id);
    EXPECT_EQ(sketch_elements.size(), 2);

    // Step 2: Extrude sketch to 3D (Pad feature, 20mm height)
    auto pad_id = feature_factory->create_pad(sketch_id, 20.0);  // 20mm height
    EXPECT_NE(pad_id, EntityId::invalid());
    doc->set_property(pad_id, "height", 20.0);
    doc->set_property(pad_id, "sketch_id", sketch_id);

    auto volume_after_pad = doc->compute_volume(pad_id);
    EXPECT_GT(volume_after_pad, 0.0);

    // Expected volume: (100*50 - pi*5^2) * 20 = (5000 - 78.54) * 20 = 96,291.6 mm^3
    // With some tolerance for floating point
    double expected_volume = (100.0 * 50.0 - 3.14159 * 5.0 * 5.0) * 20.0;
    EXPECT_NEAR(volume_after_pad, expected_volume, expected_volume * 0.01);  // 1% tolerance

    // Step 3: Add hole (Circle + Pocket, 5mm diameter)
    auto hole_sketch_id = doc->add_sketch("HoleSketch");
    auto hole_circle_id = doc->add_sketch_element(hole_sketch_id, "Circle");
    doc->set_property(hole_circle_id, "radius", 2.5);  // 5mm diameter
    doc->set_property(hole_circle_id, "center_x", 50.0);
    doc->set_property(hole_circle_id, "center_y", 25.0);

    auto pocket_id = feature_factory->create_pocket(hole_sketch_id, 20.0);  // Through hole
    EXPECT_NE(pocket_id, EntityId::invalid());

    auto volume_after_hole = doc->compute_volume(pocket_id);
    double expected_hole_volume = expected_volume - (3.14159 * 2.5 * 2.5 * 20.0);
    EXPECT_NEAR(volume_after_hole, expected_hole_volume, expected_hole_volume * 0.01);

    // Step 4: Apply Fillet to edges (2mm radius)
    auto fillet_id = feature_factory->create_fillet(pocket_id, 2.0);
    EXPECT_NE(fillet_id, EntityId::invalid());
    doc->set_property(fillet_id, "radius", 2.0);

    // Step 5: Verify geometry is correct
    EXPECT_TRUE(doc->is_valid_feature(fillet_id));
    EXPECT_GT(doc->compute_edge_count(fillet_id), 0);

    // Step 6: Test Undo/Redo
    undo_redo->record_operation("Add_Fillet", fillet_id);
    EXPECT_GE(undo_redo->get_history_size(), 1);

    // Undo fillet
    undo_redo->undo();
    EXPECT_TRUE(doc->feature_exists(pocket_id));

    // Redo fillet
    undo_redo->redo();
    EXPECT_TRUE(doc->feature_exists(fillet_id));

    // Step 7: Save and reopen model
    std::string filename = "/tmp/accept_001_test_model.bcad";
    doc->save(filename);
    EXPECT_TRUE(std::filesystem::exists(filename));

    auto doc_reopened = std::make_unique<Document>(filename);
    doc_reopened->load(filename);
    EXPECT_TRUE(doc_reopened->feature_exists(fillet_id));

    auto volume_after_reopen = doc_reopened->compute_volume(fillet_id);
    EXPECT_NEAR(volume_after_reopen, doc->compute_volume(fillet_id), 0.01);

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    std::cout << "ACCEPT-001 Workflow Time: " << duration.count() << "ms" << std::endl;
    EXPECT_LT(duration.count(), 30000);  // Target: <30 seconds

    // Cleanup
    std::filesystem::remove(filename);
}

TEST_F(ACCEPT001SketchExtrudeFillet, UndoRedoConsistency) {
    // Verify undo/redo maintains model state
    auto sketch_id = doc->add_sketch("Sketch1");
    auto rect_id = doc->add_sketch_element(sketch_id, "Rectangle");
    doc->set_property(rect_id, "width", 100.0);
    doc->set_property(rect_id, "height", 50.0);

    auto pad_id = feature_factory->create_pad(sketch_id, 20.0);
    auto vol_with_pad = doc->compute_volume(pad_id);

    // Record in undo/redo
    undo_redo->record_operation("Create_Pad", pad_id);

    // Modify and record
    auto hole_sketch_id = doc->add_sketch("HoleSketch");
    auto hole_circle_id = doc->add_sketch_element(hole_sketch_id, "Circle");
    doc->set_property(hole_circle_id, "radius", 2.5);
    auto pocket_id = feature_factory->create_pocket(hole_sketch_id, 20.0);
    undo_redo->record_operation("Create_Pocket", pocket_id);

    auto vol_with_pocket = doc->compute_volume(pocket_id);
    EXPECT_LT(vol_with_pocket, vol_with_pad);

    // Undo to pad state
    undo_redo->undo();
    auto vol_after_undo = doc->compute_volume(pad_id);
    EXPECT_NEAR(vol_after_undo, vol_with_pad, 0.1);

    // Redo to pocket state
    undo_redo->redo();
    auto vol_after_redo = doc->compute_volume(pocket_id);
    EXPECT_NEAR(vol_after_redo, vol_with_pocket, 0.1);
}

}  // namespace bendcore::acceptance_test

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
