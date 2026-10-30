#include <gtest/gtest.h>
#include "../../include/bendcore/document.hpp"
#include "../../include/bendcore/assembly_manager.hpp"
#include "../../include/bendcore/assembly_validator.hpp"
#include <chrono>

namespace bendcore::acceptance_test {

class ACCEPT002AssemblyConstraints : public ::testing::Test {
protected:
    std::unique_ptr<Document> doc;
    std::unique_ptr<AssemblyManager> assembly_mgr;
    std::unique_ptr<AssemblyValidator> assembly_validator;

    void SetUp() override {
        doc = std::make_unique<Document>("assembly_test");
        assembly_mgr = std::make_unique<AssemblyManager>(doc.get());
        assembly_validator = std::make_unique<AssemblyValidator>();
    }
};

// ACCEPT-002: Assembly Constraint Workflow
TEST_F(ACCEPT002AssemblyConstraints, TwoPartAssemblyWithConstraints) {
    auto start_time = std::chrono::high_resolution_clock::now();

    // Create 2-part assembly: body + mounting plate
    auto body_id = doc->add_part("MainBody");
    EXPECT_NE(body_id, EntityId::invalid());
    doc->set_property(body_id, "material", "Steel");
    doc->set_property(body_id, "weight", 5.5);

    auto plate_id = doc->add_part("MountingPlate");
    EXPECT_NE(plate_id, EntityId::invalid());
    doc->set_property(plate_id, "material", "Aluminum");
    doc->set_property(plate_id, "weight", 2.3);

    // Create assembly
    auto assembly_id = doc->add_assembly("MainAssembly");
    EXPECT_NE(assembly_id, EntityId::invalid());

    // Add parts to assembly
    doc->add_component_to_assembly(assembly_id, body_id);
    doc->add_component_to_assembly(assembly_id, plate_id);

    auto components = doc->get_assembly_components(assembly_id);
    EXPECT_EQ(components.size(), 2);

    // Apply Coincident mate: surfaces flush
    auto mate1_id = assembly_mgr->create_coincident_mate(body_id, 0, plate_id, 0);
    EXPECT_NE(mate1_id, EntityId::invalid());
    doc->set_property(mate1_id, "type", "Coincident");

    // Apply Distance mate: 10mm offset
    auto mate2_id = assembly_mgr->create_distance_mate(body_id, plate_id, 10.0);
    EXPECT_NE(mate2_id, EntityId::invalid());
    doc->set_property(mate2_id, "type", "Distance");
    doc->set_property(mate2_id, "offset", 10.0);

    auto mates = doc->get_assembly_mates(assembly_id);
    EXPECT_EQ(mates.size(), 2);

    // Verify no collisions
    auto collision_result = assembly_validator->check_collisions(assembly_id);
    EXPECT_EQ(collision_result.collision_count, 0);
    EXPECT_TRUE(collision_result.is_valid);

    // Verify constraints satisfied
    EXPECT_TRUE(doc->are_constraints_satisfied(assembly_id));

    // Verify visualization updated
    auto visual_assembly = doc->get_visual_representation(assembly_id);
    EXPECT_NE(visual_assembly.id, EntityId::invalid());

    // Save and reopen assembly
    std::string filename = "/tmp/accept_002_assembly.bcad";
    doc->save(filename);
    EXPECT_TRUE(std::filesystem::exists(filename));

    auto doc_reopened = std::make_unique<Document>(filename);
    doc_reopened->load(filename);

    auto assembly_components_after = doc_reopened->get_assembly_components(assembly_id);
    EXPECT_EQ(assembly_components_after.size(), 2);

    auto assembly_mates_after = doc_reopened->get_assembly_mates(assembly_id);
    EXPECT_EQ(assembly_mates_after.size(), 2);

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    std::cout << "ACCEPT-002 Assembly Constraints Time: " << duration.count() << "ms" << std::endl;
    EXPECT_LT(duration.count(), 15000);  // Target: <15 seconds

    std::filesystem::remove(filename);
}

TEST_F(ACCEPT002AssemblyConstraints, ConstraintValidation) {
    auto body_id = doc->add_part("Body");
    auto plate_id = doc->add_part("Plate");
    auto assembly_id = doc->add_assembly("Assembly");

    doc->add_component_to_assembly(assembly_id, body_id);
    doc->add_component_to_assembly(assembly_id, plate_id);

    // Create distance mate
    auto mate_id = assembly_mgr->create_distance_mate(body_id, plate_id, 10.0);
    doc->set_property(mate_id, "offset", 10.0);

    // Verify constraint validation
    EXPECT_TRUE(doc->validate_constraint(mate_id));

    // Verify constraint type
    auto constraint_type = doc->get_property(mate_id, "type");
    EXPECT_EQ(constraint_type, "Distance");

    // Verify constraint value
    auto offset_value = doc->get_property(mate_id, "offset");
    EXPECT_EQ(offset_value, 10.0);
}

}  // namespace bendcore::acceptance_test
