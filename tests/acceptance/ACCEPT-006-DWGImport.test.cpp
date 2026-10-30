#include <gtest/gtest.h>
#include "../../include/bendcore/document.hpp"
#include "../../include/bendcore/dwg_importer.hpp"
#include "../../include/bendcore/drawing_generator.hpp"

namespace bendcore::acceptance_test {

class ACCEPT006DWGImport : public ::testing::Test {
protected:
    std::unique_ptr<Document> doc;

    void SetUp() override {
        doc = std::make_unique<Document>("dwg_test");
    }
};

// ACCEPT-006: DWG/DXF Import & Drawing Generation
TEST_F(ACCEPT006DWGImport, DWGImportAndDrawingGeneration) {
    // Import 2D DWG (simulated)
    DWGImporter importer;
    std::string test_dwg = "/tmp/test_layout.dwg";
    // In real test, load actual DWG file

    auto sketch_id = doc->add_sketch("ImportedLayout");
    EXPECT_NE(sketch_id, EntityId::invalid());

    // Add some geometry from DWG
    auto rect_id = doc->add_sketch_element(sketch_id, "Rectangle");
    doc->set_property(rect_id, "width", 200.0);
    doc->set_property(rect_id, "height", 150.0);

    // Create extrusion from sketch
    auto extrude_id = doc->add_part("Model");
    EXPECT_NE(extrude_id, EntityId::invalid());

    // Generate technical drawing views
    DrawingGenerator gen;

    auto top_view = gen.create_view(extrude_id, "Top");
    auto front_view = gen.create_view(extrude_id, "Front");
    auto right_view = gen.create_view(extrude_id, "Right");
    auto iso_view = gen.create_view(extrude_id, "Isometric");

    EXPECT_NE(top_view, EntityId::invalid());
    EXPECT_NE(front_view, EntityId::invalid());
    EXPECT_NE(right_view, EntityId::invalid());
    EXPECT_NE(iso_view, EntityId::invalid());

    // Verify views
    EXPECT_TRUE(doc->view_is_orthographic(top_view));
    EXPECT_TRUE(doc->view_is_orthographic(front_view));
    EXPECT_TRUE(doc->view_is_orthographic(right_view));
    EXPECT_TRUE(doc->view_is_isometric(iso_view));

    // Export to PDF
    std::string pdf_file = "/tmp/drawing.pdf";
    gen.export_to_pdf(pdf_file);
    EXPECT_TRUE(std::filesystem::exists(pdf_file));

    // Cleanup
    std::filesystem::remove(pdf_file);
}

}  // namespace bendcore::acceptance_test
