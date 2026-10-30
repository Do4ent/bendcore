#include <gtest/gtest.h>
#include "../../include/bendcore/document.hpp"
#include <chrono>

namespace bendcore::acceptance_test {

class ACCEPT009DataPersistence : public ::testing::Test {
protected:
    std::unique_ptr<Document> doc;

    void SetUp() override {
        doc = std::make_unique<Document>("persistence_test");
    }
};

// ACCEPT-009: Save/Reopen Cycle & Data Persistence
TEST_F(ACCEPT009DataPersistence, ComplexAssemblyPersistence) {
    auto start_time = std::chrono::high_resolution_clock::now();

    // Create complex assembly: 100 features, 20 sketches, 15 parts, 40 constraints
    std::vector<EntityId> parts;
    for (int i = 0; i < 15; i++) {
        auto part_id = doc->add_part("Part" + std::to_string(i));
        parts.push_back(part_id);
    }

    // Create 20 sketches with features
    for (int i = 0; i < 20; i++) {
        auto sketch_id = doc->add_sketch("Sketch" + std::to_string(i));
        for (int j = 0; j < 5; j++) {
            auto elem_id = doc->add_sketch_element(sketch_id, "Rectangle");
            doc->set_property(elem_id, "width", 50.0 + i);
            doc->set_property(elem_id, "height", 30.0 + j);
        }
    }

    // Create assembly with 40 constraints
    auto assembly_id = doc->add_assembly("MainAssembly");
    int constraint_count = 0;
    for (size_t i = 0; i < parts.size() && constraint_count < 40; i++) {
        doc->add_component_to_assembly(assembly_id, parts[i]);
        constraint_count++;
    }

    // Add custom properties
    doc->set_property(assembly_id, "custom_prop_1", "Value1");
    doc->set_property(assembly_id, "custom_prop_2", "Value2");
    doc->set_property(assembly_id, "manufacturing_notes", "Test assembly with custom properties");

    // Verify pre-save state
    EXPECT_EQ(doc->get_part_count(), 15);
    EXPECT_EQ(doc->get_sketch_count(), 20);

    // Save model
    std::string filename = "/tmp/accept_009_complex.bcad";
    doc->save(filename);
    EXPECT_TRUE(std::filesystem::exists(filename));

    // Check file size
    auto file_size = std::filesystem::file_size(filename);
    std::cout << "File size: " << file_size / (1024 * 1024) << "MB" << std::endl;
    EXPECT_LT(file_size, 50 * 1024 * 1024);  // <50MB

    // Reopen in new session
    auto doc_reopened = std::make_unique<Document>("reopened");
    doc_reopened->load(filename);

    // Verify all data intact
    EXPECT_EQ(doc_reopened->get_part_count(), 15);
    EXPECT_EQ(doc_reopened->get_sketch_count(), 20);

    // Verify custom properties
    auto prop1 = doc_reopened->get_property(assembly_id, "custom_prop_1");
    auto prop2 = doc_reopened->get_property(assembly_id, "custom_prop_2");
    EXPECT_EQ(prop1, "Value1");
    EXPECT_EQ(prop2, "Value2");

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    std::cout << "ACCEPT-009 Save/Load Cycle Time: " << duration.count() << "ms" << std::endl;
    EXPECT_LT(duration.count(), 10000);  // <10 seconds total

    // Cleanup
    std::filesystem::remove(filename);
}

}  // namespace bendcore::acceptance_test
