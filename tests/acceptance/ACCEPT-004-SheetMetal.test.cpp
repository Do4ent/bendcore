#include <gtest/gtest.h>
#include "../../include/bendcore/document.hpp"
#include "../../include/bendcore/sheet_metal_engine.hpp"

namespace bendcore::acceptance_test {

class ACCEPT004SheetMetal : public ::testing::Test {
protected:
    std::unique_ptr<Document> doc;
    std::unique_ptr<SheetMetalEngine> sheet_metal;

    void SetUp() override {
        doc = std::make_unique<Document>("sheet_metal_test");
        sheet_metal = std::make_unique<SheetMetalEngine>();
    }
};

// ACCEPT-004: Sheet Metal Unfolding
TEST_F(ACCEPT004SheetMetal, BentSheetMetalUnfolding) {
    // Create bent sheet metal: 90° bend, 2mm radius, 1mm thickness
    auto sheet_id = doc->add_part("BentSheet");
    doc->set_property(sheet_id, "thickness", 1.0);
    doc->set_property(sheet_id, "material", "AluminumAlloy");

    // Add bend operation
    auto bend_id = sheet_metal->create_bend(sheet_id, 90.0, 2.0);
    EXPECT_NE(bend_id, EntityId::invalid());

    // Unfold to 2D
    auto flat_pattern_id = sheet_metal->unfold(bend_id);
    EXPECT_NE(flat_pattern_id, EntityId::invalid());

    // Verify dimensions
    auto folded_volume = doc->compute_volume(bend_id);
    auto unfolded_area = doc->compute_area(flat_pattern_id);

    EXPECT_GT(folded_volume, 0.0);
    EXPECT_GT(unfolded_area, 0.0);

    // Verify re-fold
    auto refolded_id = sheet_metal->fold(flat_pattern_id);
    auto refolded_volume = doc->compute_volume(refolded_id);

    // Volume should match (within tolerance)
    EXPECT_NEAR(refolded_volume, folded_volume, folded_volume * 0.001);

    // Verify precision error <0.1%
    double precision_error = std::abs(refolded_volume - folded_volume) / folded_volume * 100.0;
    EXPECT_LT(precision_error, 0.1);
}

}  // namespace bendcore::acceptance_test
