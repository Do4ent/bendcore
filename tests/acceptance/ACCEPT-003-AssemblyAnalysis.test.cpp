#include <gtest/gtest.h>
#include "../../include/bendcore/document.hpp"
#include "../../include/bendcore/assembly_validator.hpp"
#include <chrono>

namespace bendcore::acceptance_test {

class ACCEPT003AssemblyAnalysis : public ::testing::Test {
protected:
    std::unique_ptr<Document> doc;
    std::unique_ptr<AssemblyValidator> validator;

    void SetUp() override {
        doc = std::make_unique<Document>("analysis_test");
        validator = std::make_unique<AssemblyValidator>();
    }
};

// ACCEPT-003: Assembly Structural Analysis
TEST_F(ACCEPT003AssemblyAnalysis, FivePartCollisionDetection) {
    auto start_time = std::chrono::high_resolution_clock::now();

    // Create 5-part assembly
    std::vector<EntityId> parts;
    for (int i = 0; i < 5; i++) {
        auto part_id = doc->add_part("Part" + std::to_string(i + 1));
        doc->set_property(part_id, "material", "Steel");
        parts.push_back(part_id);
    }

    // Create assembly
    auto assembly_id = doc->add_assembly("Assembly");
    for (const auto& part_id : parts) {
        doc->add_component_to_assembly(assembly_id, part_id);
    }

    // Run collision detection
    auto collision_result = validator->check_collisions(assembly_id);

    // Verify results
    EXPECT_EQ(collision_result.collision_count, 0);
    EXPECT_TRUE(collision_result.is_valid);
    EXPECT_EQ(collision_result.interference_zones.size(), 0);

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    std::cout << "ACCEPT-003 Collision Detection Time: " << duration.count() << "ms" << std::endl;
    EXPECT_LT(duration.count(), 2000);  // Target: <2 seconds for 5-part assembly
}

}  // namespace bendcore::acceptance_test
