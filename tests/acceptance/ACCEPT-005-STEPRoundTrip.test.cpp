#include <gtest/gtest.h>
#include "../../include/bendcore/document.hpp"
#include "../../include/bendcore/step_exporter.hpp"
#include "../../include/bendcore/step_importer.hpp"

namespace bendcore::acceptance_test {

class ACCEPT005STEPRoundTrip : public ::testing::Test {
protected:
    std::unique_ptr<Document> doc;

    void SetUp() override {
        doc = std::make_unique<Document>("step_test");
    }
};

// ACCEPT-005: STEP Import/Export Round-Trip
TEST_F(ACCEPT005STEPRoundTrip, AssemblyRoundTripPreservation) {
    // Create multi-part assembly (simulated from STEP import)
    std::vector<EntityId> parts;
    for (int i = 0; i < 5; i++) {
        auto part_id = doc->add_part("Part" + std::to_string(i));
        parts.push_back(part_id);
    }

    auto assembly_id = doc->add_assembly("MainAssembly");
    for (const auto& part_id : parts) {
        doc->add_component_to_assembly(assembly_id, part_id);
    }

    // Get original metrics
    auto original_volume = doc->compute_volume(assembly_id);
    auto original_edge_count = doc->compute_edge_count(assembly_id);
    auto original_face_count = doc->compute_face_count(assembly_id);

    // Export to STEP
    STEPExporter exporter;
    std::string step_file = "/tmp/test_export.step";
    exporter.export_assembly(assembly_id, step_file);
    EXPECT_TRUE(std::filesystem::exists(step_file));

    // Re-import STEP
    STEPImporter importer;
    auto imported_assembly_id = importer.import_assembly(step_file);
    EXPECT_NE(imported_assembly_id, EntityId::invalid());

    // Verify geometry preservation
    auto imported_volume = doc->compute_volume(imported_assembly_id);
    auto imported_edge_count = doc->compute_edge_count(imported_assembly_id);
    auto imported_face_count = doc->compute_face_count(imported_assembly_id);

    EXPECT_NEAR(imported_volume, original_volume, original_volume * 0.001);  // 0.1% tolerance
    EXPECT_EQ(imported_edge_count, original_edge_count);
    EXPECT_EQ(imported_face_count, original_face_count);

    // Cleanup
    std::filesystem::remove(step_file);
}

}  // namespace bendcore::acceptance_test
