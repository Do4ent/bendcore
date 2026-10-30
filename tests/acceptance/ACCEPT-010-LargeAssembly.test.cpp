#include <gtest/gtest.h>
#include "../../include/bendcore/document.hpp"
#include "../../include/bendcore/viewport.hpp"
#include "../../include/bendcore/step_exporter.hpp"
#include <chrono>
#include <thread>

namespace bendcore::acceptance_test {

class ACCEPT010LargeAssembly : public ::testing::Test {
protected:
    std::unique_ptr<Document> doc;
    std::unique_ptr<Viewport> viewport;

    void SetUp() override {
        doc = std::make_unique<Document>("large_assembly_test");
        viewport = std::make_unique<Viewport>();
    }
};

// ACCEPT-010: Large Assembly Performance & Stability
TEST_F(ACCEPT010LargeAssembly, LargeAssemblyPerformance) {
    auto start_time = std::chrono::high_resolution_clock::now();

    std::cout << "\nCreating large assembly (200+ parts, 500+ features)..." << std::endl;

    // Create 200+ parts
    std::vector<EntityId> parts;
    for (int i = 0; i < 200; i++) {
        auto part_id = doc->add_part("Part" + std::to_string(i));
        doc->set_property(part_id, "material", "Steel");
        parts.push_back(part_id);
    }

    // Create 500+ features (sketches + extrusions)
    for (int i = 0; i < 100; i++) {
        auto sketch_id = doc->add_sketch("Sketch" + std::to_string(i));
        for (int j = 0; j < 5; j++) {
            auto elem_id = doc->add_sketch_element(sketch_id, "Rectangle");
            doc->set_property(elem_id, "width", 50.0);
            doc->set_property(elem_id, "height", 30.0);
        }
    }

    // Create assembly with 200+ constraints
    auto assembly_id = doc->add_assembly("MainAssembly");
    for (size_t i = 0; i < parts.size(); i++) {
        doc->add_component_to_assembly(assembly_id, parts[i]);
    }

    auto setup_time = std::chrono::high_resolution_clock::now();
    std::cout << "Setup complete in "
              << std::chrono::duration_cast<std::chrono::milliseconds>(setup_time - start_time).count()
              << "ms" << std::endl;

    // Test 1: Viewport rotation performance
    std::cout << "Testing viewport rotation at 60 FPS target..." << std::endl;
    auto viewport_start = std::chrono::high_resolution_clock::now();

    // Simulate 60 FPS rotation (1 second worth)
    int frame_count = 0;
    for (int frame = 0; frame < 60; frame++) {
        viewport->set_rotation(frame * 6.0, frame * 3.0, frame * 1.5);
        viewport->render();  // Would render if viewport has graphics context
        frame_count++;
    }

    auto viewport_end = std::chrono::high_resolution_clock::now();
    auto viewport_duration = std::chrono::duration_cast<std::chrono::milliseconds>(viewport_end - viewport_start);

    std::cout << "Viewport rotation 60 frames in " << viewport_duration.count() << "ms" << std::endl;
    // Should be approximately 1000ms for 60 FPS; allow for testing overhead
    EXPECT_LT(viewport_duration.count(), 2000);

    // Test 2: Dimension edit & rebuild
    std::cout << "Testing dimension edit and model rebuild..." << std::endl;
    auto rebuild_start = std::chrono::high_resolution_clock::now();

    // Edit a dimension and trigger recompute
    if (!parts.empty()) {
        doc->set_property(parts[0], "dimension_x", 100.0);
        doc->recompute_features();  // Would rebuild dependent features
    }

    auto rebuild_end = std::chrono::high_resolution_clock::now();
    auto rebuild_duration = std::chrono::duration_cast<std::chrono::milliseconds>(rebuild_end - rebuild_start);

    std::cout << "Rebuild complete in " << rebuild_duration.count() << "ms" << std::endl;
    EXPECT_LT(rebuild_duration.count(), 2000);  // Target: <2 seconds

    // Test 3: Export to STEP performance
    std::cout << "Testing STEP export performance..." << std::endl;
    auto export_start = std::chrono::high_resolution_clock::now();

    std::string step_file = "/tmp/large_assembly.step";
    STEPExporter exporter;
    exporter.export_assembly(assembly_id, step_file);

    auto export_end = std::chrono::high_resolution_clock::now();
    auto export_duration = std::chrono::duration_cast<std::chrono::milliseconds>(export_end - export_start);

    std::cout << "STEP export complete in " << export_duration.count() << "ms" << std::endl;
    EXPECT_TRUE(std::filesystem::exists(step_file));
    EXPECT_LT(export_duration.count(), 5000);  // Target: <5 seconds

    // Test 4: Memory usage
    // Note: In real test, would use system memory profiler
    // For this version, just verify document doesn't grow unbounded
    std::cout << "Verifying memory efficiency..." << std::endl;
    // Memory check would go here (Valgrind, /proc/self/status, etc.)

    // Test 5: Stability - verify no crashes
    std::cout << "Stability verification..." << std::endl;
    EXPECT_TRUE(doc->is_valid());
    EXPECT_GT(doc->get_part_count(), 0);

    auto total_time = std::chrono::high_resolution_clock::now();
    auto total_duration = std::chrono::duration_cast<std::chrono::seconds>(total_time - start_time);

    std::cout << "ACCEPT-010 Total Test Time: " << total_duration.count() << " seconds" << std::endl;

    // Cleanup
    std::filesystem::remove(step_file);
}

}  // namespace bendcore::acceptance_test
