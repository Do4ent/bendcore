#include <gtest/gtest.h>
#include "../../include/bendcore/document.hpp"
#include "../../include/bendcore/dwfx_generator.hpp"

namespace bendcore::acceptance_test {

class ACCEPT007DWFxMarkup : public ::testing::Test {
protected:
    std::unique_ptr<Document> doc;

    void SetUp() override {
        doc = std::make_unique<Document>("dwfx_test");
    }
};

// ACCEPT-007: DWFx Markup & Collaboration
TEST_F(ACCEPT007DWFxMarkup, DWFxMarkupPersistence) {
    // Create a 3D model
    auto sketch_id = doc->add_sketch("Sketch");
    auto rect_id = doc->add_sketch_element(sketch_id, "Rectangle");
    doc->set_property(rect_id, "width", 100.0);
    doc->set_property(rect_id, "height", 50.0);

    // Generate DWFx
    DWFxGenerator gen;
    std::string dwfx_file = "/tmp/model.dwfx";
    gen.generate_from_document(doc.get(), dwfx_file);
    EXPECT_TRUE(std::filesystem::exists(dwfx_file));

    // Add markup
    auto markup_id = gen.add_comment("Test comment at origin");
    EXPECT_NE(markup_id, EntityId::invalid());

    auto dimension_id = gen.add_dimension(100.0, "Width dimension");
    EXPECT_NE(dimension_id, EntityId::invalid());

    auto cloud_id = gen.add_revision_cloud("Revision area");
    EXPECT_NE(cloud_id, EntityId::invalid());

    // Save with markup
    gen.save_with_markup(dwfx_file);

    // Reload and verify markup persists
    auto reloaded_gen = std::make_unique<DWFxGenerator>();
    reloaded_gen->load(dwfx_file);

    auto markup_items = reloaded_gen->get_all_markup();
    EXPECT_GE(markup_items.size(), 3);

    // Verify file size is reasonable
    auto file_size = std::filesystem::file_size(dwfx_file);
    EXPECT_LT(file_size, 50 * 1024 * 1024);  // <50MB

    // Cleanup
    std::filesystem::remove(dwfx_file);
}

}  // namespace bendcore::acceptance_test
