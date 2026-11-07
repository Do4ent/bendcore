// M4 Phase - Drawing Export Tests
#include <gtest/gtest.h>
#include "../include/bendcore/drawing/drawing_export.hpp"
#include "../include/bendcore/drawing/export_options.hpp"
#include "../include/bendcore/drawing/file_handler.hpp"
#include <cstdlib>
#include <cstdio>

using namespace BendCore::Drawing;

class DrawingExportTest : public ::testing::Test {
 protected:
  std::shared_ptr<DrawingSheet> createTestSheet() {
    auto sheet = std::make_shared<DrawingSheet>(1);

    // Create a simple view
    auto view = std::make_shared<DrawingView>("view1", ViewType::Front);
    view->setLabel("Front View");
    view->setOrigin({50, 100});

    // Add some simple geometry
    Edge2D edge;
    edge.p0 = {0, 0};
    edge.p1 = {100, 0};
    view->addEdge(edge);

    Circle2D circle;
    circle.center = {50, 50};
    circle.radius = 25;
    view->addCircle(circle);

    // Add dimension
    Dimension2D dim;
    dim.p0 = {0, 0};
    dim.p1 = {100, 0};
    dim.text = "100.0";
    view->addDimension(dim);

    sheet->addView(view);

    // Setup title block
    TitleBlock title;
    title.fields["title"] = "Test Drawing";
    title.fields["part_number"] = "DRW-001";
    sheet->setTitleBlock(title);

    return sheet;
  }
};

// ============================================================================
// Export Without Format Writers
// ============================================================================

// No format writer exists until roadmap phase 4: every format must report an
// explicit error instead of claiming success.
TEST_F(DrawingExportTest, ExportReportsUnsupportedFormat) {
  auto sheet = createTestSheet();
  const ExportFormat formats[] = {ExportFormat::PDF, ExportFormat::DWG, ExportFormat::DXF,
                                  ExportFormat::SVG, ExportFormat::STEP, ExportFormat::PNG,
                                  ExportFormat::JPEG};
  for (auto format : formats) {
    ExportOptions opts;
    opts.format = format;
    opts.output_path = "/tmp/bendcore_unsupported_export";
    auto result = exportDrawing(*sheet, opts);
    EXPECT_FALSE(result.success) << exportFormatToString(format);
    EXPECT_NE(result.error_message.find("not supported"), std::string::npos)
        << exportFormatToString(format);
  }
}

TEST_F(DrawingExportTest, ExportViewReportsUnsupportedFormat) {
  auto sheet = createTestSheet();
  ExportOptions opts;
  opts.format = ExportFormat::PDF;
  opts.output_path = "/tmp/bendcore_unsupported_export";
  auto result = exportView(*sheet->getView(0), opts);
  EXPECT_FALSE(result.success);
  EXPECT_FALSE(result.error_message.empty());
}

TEST_F(DrawingExportTest, InvalidOptionsReportedBeforeUnsupported) {
  auto sheet = createTestSheet();
  ExportOptions opts;
  opts.format = ExportFormat::PDF;
  opts.output_path = "";
  auto result = exportDrawing(*sheet, opts);
  EXPECT_FALSE(result.success);
  EXPECT_EQ(result.error_message, "Output path is empty");
}

// ============================================================================
// Export Options Tests
// ============================================================================

TEST_F(DrawingExportTest, ValidateExportOptions) {
  ExportOptions opts;
  opts.format = ExportFormat::PDF;
  opts.output_path = "/tmp/test.pdf";

  auto [valid, error] = validateExportOptions(opts);
  EXPECT_TRUE(valid);
}

TEST_F(DrawingExportTest, ValidateExportOptionsInvalidDPI) {
  ExportOptions opts;
  opts.format = ExportFormat::PDF;
  opts.output_path = "/tmp/test.pdf";
  opts.pdf.dpi = 10;  // Too low

  auto [valid, error] = validateExportOptions(opts);
  EXPECT_FALSE(valid);
  EXPECT_FALSE(error.empty());
}

TEST_F(DrawingExportTest, ValidateExportOptionsEmptyPath) {
  ExportOptions opts;
  opts.format = ExportFormat::PDF;
  opts.output_path = "";

  auto [valid, error] = validateExportOptions(opts);
  EXPECT_FALSE(valid);
}

// ============================================================================
// Format Conversion Tests
// ============================================================================

TEST_F(DrawingExportTest, ExportFormatToString) {
  EXPECT_EQ(exportFormatToString(ExportFormat::PDF), "PDF");
  EXPECT_EQ(exportFormatToString(ExportFormat::DWG), "DWG");
  EXPECT_EQ(exportFormatToString(ExportFormat::DXF), "DXF");
  EXPECT_EQ(exportFormatToString(ExportFormat::SVG), "SVG");
  EXPECT_EQ(exportFormatToString(ExportFormat::STEP), "STEP");
  EXPECT_EQ(exportFormatToString(ExportFormat::PNG), "PNG");
  EXPECT_EQ(exportFormatToString(ExportFormat::JPEG), "JPEG");
}

TEST_F(DrawingExportTest, StringToExportFormat) {
  EXPECT_EQ(stringToExportFormat("pdf"), ExportFormat::PDF);
  EXPECT_EQ(stringToExportFormat("PDF"), ExportFormat::PDF);
  EXPECT_EQ(stringToExportFormat("dwg"), ExportFormat::DWG);
  EXPECT_EQ(stringToExportFormat("DXF"), ExportFormat::DXF);
}

// ============================================================================
// Filename Generation Tests
// ============================================================================

TEST_F(DrawingExportTest, GenerateExportFilename) {
  auto sheet = createTestSheet();

  auto filename = generateExportFilename(*sheet, ExportFormat::PDF, false);

  EXPECT_TRUE(filename.find(".pdf") != std::string::npos);
  EXPECT_TRUE(filename.find("Test") != std::string::npos);  // Contains title
}

TEST_F(DrawingExportTest, GenerateExportFilenameWithTimestamp) {
  auto sheet = createTestSheet();

  auto filename = generateExportFilename(*sheet, ExportFormat::PDF, true);

  EXPECT_TRUE(filename.find(".pdf") != std::string::npos);
  // Should contain timestamp (underscore followed by digits)
  EXPECT_TRUE(filename.find("_") != std::string::npos);
}

TEST_F(DrawingExportTest, GetFormatExtension) {
  EXPECT_EQ(getFormatExtension(ExportFormat::PDF), ".pdf");
  EXPECT_EQ(getFormatExtension(ExportFormat::DWG), ".dwg");
  EXPECT_EQ(getFormatExtension(ExportFormat::DXF), ".dxf");
  EXPECT_EQ(getFormatExtension(ExportFormat::SVG), ".svg");
  EXPECT_EQ(getFormatExtension(ExportFormat::STEP), ".step");
  EXPECT_EQ(getFormatExtension(ExportFormat::PNG), ".png");
  EXPECT_EQ(getFormatExtension(ExportFormat::JPEG), ".jpg");
}

// ============================================================================
// Format Detection Tests
// ============================================================================

TEST_F(DrawingExportTest, IsVectorFormat) {
  EXPECT_TRUE(isVectorFormat(ExportFormat::PDF));
  EXPECT_TRUE(isVectorFormat(ExportFormat::DWG));
  EXPECT_TRUE(isVectorFormat(ExportFormat::DXF));
  EXPECT_TRUE(isVectorFormat(ExportFormat::SVG));
  EXPECT_TRUE(isVectorFormat(ExportFormat::STEP));

  EXPECT_FALSE(isVectorFormat(ExportFormat::PNG));
  EXPECT_FALSE(isVectorFormat(ExportFormat::JPEG));
}

TEST_F(DrawingExportTest, IsRasterFormat) {
  EXPECT_TRUE(isRasterFormat(ExportFormat::PNG));
  EXPECT_TRUE(isRasterFormat(ExportFormat::JPEG));

  EXPECT_FALSE(isRasterFormat(ExportFormat::PDF));
  EXPECT_FALSE(isRasterFormat(ExportFormat::DWG));
  EXPECT_FALSE(isRasterFormat(ExportFormat::DXF));
}

// ============================================================================
// Page Size Tests
// ============================================================================

TEST_F(DrawingExportTest, PageSizeToString) {
  EXPECT_EQ(pageSizeToString(PageSize::A4), "A4");
  EXPECT_EQ(pageSizeToString(PageSize::A3), "A3");
  EXPECT_EQ(pageSizeToString(PageSize::A2), "A2");
  EXPECT_EQ(pageSizeToString(PageSize::Letter), "Letter");
}

TEST_F(DrawingExportTest, GetPageDimensions) {
  // A4 Portrait: 210 x 297 mm
  auto [w, h] = getPageDimensions(PageSize::A4, PageOrientation::Portrait);
  EXPECT_DOUBLE_EQ(w, 210.0);
  EXPECT_DOUBLE_EQ(h, 297.0);

  // A4 Landscape: 297 x 210 mm
  std::tie(w, h) = getPageDimensions(PageSize::A4, PageOrientation::Landscape);
  EXPECT_DOUBLE_EQ(w, 297.0);
  EXPECT_DOUBLE_EQ(h, 210.0);

  // A3 Portrait: 297 x 420 mm
  std::tie(w, h) = getPageDimensions(PageSize::A3, PageOrientation::Portrait);
  EXPECT_DOUBLE_EQ(w, 297.0);
  EXPECT_DOUBLE_EQ(h, 420.0);
}

// ============================================================================
// Unit Conversion Tests
// ============================================================================

TEST_F(DrawingExportTest, UnitTypeToString) {
  EXPECT_EQ(unitTypeToString(UnitType::Millimeter), "mm");
  EXPECT_EQ(unitTypeToString(UnitType::Centimeter), "cm");
  EXPECT_EQ(unitTypeToString(UnitType::Meter), "m");
  EXPECT_EQ(unitTypeToString(UnitType::Inch), "in");
  EXPECT_EQ(unitTypeToString(UnitType::Foot), "ft");
}

TEST_F(DrawingExportTest, StringToUnitType) {
  EXPECT_EQ(stringToUnitType("mm"), UnitType::Millimeter);
  EXPECT_EQ(stringToUnitType("cm"), UnitType::Centimeter);
  EXPECT_EQ(stringToUnitType("m"), UnitType::Meter);
  EXPECT_EQ(stringToUnitType("in"), UnitType::Inch);
  EXPECT_EQ(stringToUnitType("ft"), UnitType::Foot);
}

TEST_F(DrawingExportTest, ConvertUnits) {
  // mm to cm
  EXPECT_DOUBLE_EQ(convertUnits(10, UnitType::Millimeter, UnitType::Centimeter), 1.0);

  // cm to mm
  EXPECT_DOUBLE_EQ(convertUnits(1, UnitType::Centimeter, UnitType::Millimeter), 10.0);

  // mm to inch (25.4 mm = 1 inch)
  EXPECT_DOUBLE_EQ(convertUnits(25.4, UnitType::Millimeter, UnitType::Inch), 1.0);

  // Same unit
  EXPECT_DOUBLE_EQ(convertUnits(100, UnitType::Millimeter, UnitType::Millimeter), 100.0);
}

// ============================================================================
// Verification Tests
// ============================================================================

TEST_F(DrawingExportTest, VerifyExportedFile) {
  // Create a simple test file
  std::string test_file = "/tmp/test_verify_file.pdf";
  FILE* f = fopen(test_file.c_str(), "w");
  if (f) {
    fprintf(f, "test content");
    fclose(f);
  }

  auto [valid, error] = verifyExportedFile(test_file, ExportFormat::PDF);
  EXPECT_TRUE(valid);

  // Clean up
  std::remove(test_file.c_str());
}
