// M4 Phase - Print Manager Tests
#include <gtest/gtest.h>
#include "../include/bendcore/drawing/print_manager.hpp"
#include <cstdio>

using namespace BendCore::Drawing;

class PrintManagerTest : public ::testing::Test {
 protected:
  std::shared_ptr<DrawingSheet> createTestSheet() {
    auto sheet = std::make_shared<DrawingSheet>(1);
    auto view = std::make_shared<DrawingView>("view1", ViewType::Front);

    Edge2D edge;
    edge.p0 = {0, 0};
    edge.p1 = {100, 100};
    view->addEdge(edge);

    sheet->addView(view);

    return sheet;
  }
};

// ============================================================================
// Print Preview Tests
// ============================================================================

TEST_F(PrintManagerTest, GeneratePrintPreview) {
  auto sheet = createTestSheet();

  PrintSettings settings;
  settings.paper_size = PageSize::A4;
  settings.orientation = PageOrientation::Portrait;
  settings.print_quality_dpi = 300;

  auto preview = generatePrintPreview(*sheet, settings);

  EXPECT_GT(preview.width_pixels, 0);
  EXPECT_GT(preview.height_pixels, 0);
  EXPECT_EQ(preview.page_number, 1);
  EXPECT_GT(preview.image_data.size(), 0);
}

TEST_F(PrintManagerTest, GenerateMultiPagePreview) {
  auto sheet = createTestSheet();

  PrintSettings settings;
  settings.paper_size = PageSize::A4;
  settings.enable_tiling = true;
  settings.tile_overlap_mm = 10;

  auto previews = generateMultiPagePreview(*sheet, settings);

  EXPECT_GT(previews.size(), 0);
  for (size_t i = 0; i < previews.size(); ++i) {
    EXPECT_EQ(previews[i].page_number, static_cast<int>(i + 1));
    EXPECT_GT(previews[i].width_pixels, 0);
    EXPECT_GT(previews[i].height_pixels, 0);
  }
}

// ============================================================================
// Print Job Tests
// ============================================================================

TEST_F(PrintManagerTest, PrintDrawing) {
  auto sheet = createTestSheet();

  PrintJob job;
  job.job_id = "print_001";
  job.sheet = sheet;
  job.settings.printer_name = "Default Printer";
  job.settings.paper_size = PageSize::A4;
  job.settings.copies = 1;

  auto result = printDrawing(job);

  EXPECT_TRUE(result.success);
  EXPECT_EQ(result.printer_name, "Default Printer");
  EXPECT_GT(result.pages_printed, 0);
  EXPECT_TRUE(result.completed_successfully);
}

TEST_F(PrintManagerTest, PrintToFile) {
  auto sheet = createTestSheet();
  std::string output_file = "/tmp/test_print.pdf";

  PrintSettings settings;
  settings.printer_name = "Print to File";
  settings.paper_size = PageSize::A4;
  settings.copies = 1;

  auto result = printToFile(*sheet, output_file, settings);

  EXPECT_TRUE(result.success);
  EXPECT_EQ(result.printer_name, "Print to File");
  EXPECT_TRUE(result.completed_successfully);
}

// ============================================================================
// Printer Management Tests
// ============================================================================

TEST_F(PrintManagerTest, GetAvailablePrinters) {
  auto printers = getAvailablePrinters();

  EXPECT_GT(printers.size(), 0);
  EXPECT_FALSE(printers[0].printer_name.empty());
}

TEST_F(PrintManagerTest, GetDefaultPrinter) {
  auto printer = getDefaultPrinter();

  EXPECT_TRUE(printer.has_value());
  EXPECT_FALSE(printer.value().printer_name.empty());
}

TEST_F(PrintManagerTest, GetPrinterInfo) {
  auto printer = getPrinterInfo("Default Printer");

  EXPECT_TRUE(printer.has_value());
  EXPECT_EQ(printer.value().printer_name, "Default Printer");
}

TEST_F(PrintManagerTest, GetPrinterInfoNotFound) {
  auto printer = getPrinterInfo("Nonexistent Printer");

  EXPECT_FALSE(printer.has_value());
}

TEST_F(PrintManagerTest, GetSupportedPageSizes) {
  auto sizes = getSupportedPageSizes("Default Printer");

  EXPECT_GT(sizes.size(), 0);
  EXPECT_TRUE(std::find(sizes.begin(), sizes.end(), PageSize::A4) != sizes.end());
}

TEST_F(PrintManagerTest, GetSupportedDPIs) {
  auto dpis = getSupportedDPIs("Default Printer");

  EXPECT_GT(dpis.size(), 0);
  EXPECT_TRUE(std::find(dpis.begin(), dpis.end(), 300) != dpis.end());
}

// ============================================================================
// Print Settings Validation Tests
// ============================================================================

TEST_F(PrintManagerTest, ValidatePrintSettingsValid) {
  PrintSettings settings;
  settings.paper_size = PageSize::A4;
  settings.print_color = true;
  settings.print_quality_dpi = 300;
  settings.copies = 1;

  auto printer = getDefaultPrinter().value();

  auto [valid, error] = validatePrintSettings(settings, printer);

  EXPECT_TRUE(valid);
}

TEST_F(PrintManagerTest, ValidatePrintSettingsInvalidDPI) {
  PrintSettings settings;
  settings.paper_size = PageSize::A4;
  settings.print_quality_dpi = 1200;  // Higher than printer max
  settings.copies = 1;

  auto printer = getDefaultPrinter().value();

  auto [valid, error] = validatePrintSettings(settings, printer);

  EXPECT_FALSE(valid);
  EXPECT_FALSE(error.empty());
}

TEST_F(PrintManagerTest, ValidatePrintSettingsInvalidCopies) {
  PrintSettings settings;
  settings.paper_size = PageSize::A4;
  settings.print_quality_dpi = 300;
  settings.copies = 0;  // Invalid

  auto printer = getDefaultPrinter().value();

  auto [valid, error] = validatePrintSettings(settings, printer);

  EXPECT_FALSE(valid);
}

// ============================================================================
// Page Count Calculation Tests
// ============================================================================

TEST_F(PrintManagerTest, CalculatePageCountSinglePage) {
  auto sheet = createTestSheet();

  PrintSettings settings;
  settings.enable_tiling = false;
  settings.copies = 1;

  int page_count = calculatePageCount(*sheet, settings);

  EXPECT_EQ(page_count, 1);
}

TEST_F(PrintManagerTest, CalculatePageCountMultipleCopies) {
  auto sheet = createTestSheet();

  PrintSettings settings;
  settings.enable_tiling = false;
  settings.copies = 5;

  int page_count = calculatePageCount(*sheet, settings);

  EXPECT_EQ(page_count, 5);
}

TEST_F(PrintManagerTest, CalculatePageCountTiled) {
  auto sheet = createTestSheet();

  PrintSettings settings;
  settings.enable_tiling = true;
  settings.tile_overlap_mm = 10;
  settings.copies = 1;

  int page_count = calculatePageCount(*sheet, settings);

  EXPECT_GT(page_count, 0);
}

// ============================================================================
// Tile Calculation Tests
// ============================================================================

TEST_F(PrintManagerTest, CalculateTileCountSimple) {
  auto sheet = createTestSheet();

  // Sheet fits on one page
  auto [tiles_x, tiles_y] = calculateTileCount(*sheet, 210, 297, 10);

  EXPECT_EQ(tiles_x, 1);
  EXPECT_EQ(tiles_y, 1);
}

TEST_F(PrintManagerTest, CalculateTileCountMultiple) {
  auto sheet = createTestSheet();

  // Simulate large drawing (simulate with small available space)
  auto [tiles_x, tiles_y] = calculateTileCount(*sheet, 50, 50, 10);

  EXPECT_GE(tiles_x, 1);
  EXPECT_GE(tiles_y, 1);
}

// ============================================================================
// Print Scale Tests
// ============================================================================

TEST_F(PrintManagerTest, GetOptimalPrintScale) {
  auto sheet = createTestSheet();

  double scale = getOptimalPrintScale(*sheet, 210, 297, 20);

  EXPECT_GT(scale, 0);
  EXPECT_LE(scale, 1.0);  // Should fit on page
}

TEST_F(PrintManagerTest, GetOptimalPrintScaleLargeDrawing) {
  auto sheet = createTestSheet();

  // Available area is very small
  double scale = getOptimalPrintScale(*sheet, 50, 50, 5);

  EXPECT_GT(scale, 0);
}

// ============================================================================
// Print Settings Tests
// ============================================================================

TEST_F(PrintManagerTest, PrintSettingsPageSize) {
  PrintSettings settings;

  EXPECT_EQ(settings.paper_size, PageSize::A4);
  settings.paper_size = PageSize::A3;
  EXPECT_EQ(settings.paper_size, PageSize::A3);
}

TEST_F(PrintManagerTest, PrintSettingsOrientation) {
  PrintSettings settings;

  EXPECT_EQ(settings.orientation, PageOrientation::Portrait);
  settings.orientation = PageOrientation::Landscape;
  EXPECT_EQ(settings.orientation, PageOrientation::Landscape);
}

TEST_F(PrintManagerTest, PrintSettingsCopies) {
  PrintSettings settings;

  EXPECT_EQ(settings.copies, 1);
  settings.copies = 10;
  EXPECT_EQ(settings.copies, 10);
}

TEST_F(PrintManagerTest, PrintSettingsScaling) {
  PrintSettings settings;

  EXPECT_EQ(settings.scaling, PrintScaling::Actual);
  settings.scaling = PrintScaling::FitToPage;
  EXPECT_EQ(settings.scaling, PrintScaling::FitToPage);
}

TEST_F(PrintManagerTest, PrintSettingsColor) {
  PrintSettings settings;

  EXPECT_TRUE(settings.print_color);
  settings.print_color = false;
  EXPECT_FALSE(settings.print_color);
}

TEST_F(PrintManagerTest, PrintSettingsDuplex) {
  PrintSettings settings;

  EXPECT_EQ(settings.duplex, DuplexMode::None);
  settings.duplex = DuplexMode::Duplex_LongEdge;
  EXPECT_EQ(settings.duplex, DuplexMode::Duplex_LongEdge);
}

// ============================================================================
// Printer Type Tests
// ============================================================================

TEST_F(PrintManagerTest, PrinterTypeInkJet) {
  PrinterInfo printer;
  printer.type = PrinterType::InkJet;

  EXPECT_EQ(printer.type, PrinterType::InkJet);
}

TEST_F(PrintManagerTest, PrinterTypeLaserJet) {
  PrinterInfo printer;
  printer.type = PrinterType::LaserJet;

  EXPECT_EQ(printer.type, PrinterType::LaserJet);
}

TEST_F(PrintManagerTest, PrinterCapabilities) {
  PrinterInfo printer;
  printer.supports_color = true;
  printer.supports_duplex = false;
  printer.max_dpi = 600;

  EXPECT_TRUE(printer.supports_color);
  EXPECT_FALSE(printer.supports_duplex);
  EXPECT_EQ(printer.max_dpi, 600);
}

// ============================================================================
// Print Job Control Tests
// ============================================================================

TEST_F(PrintManagerTest, CancelPrintJob) {
  auto [success, message] = cancelPrintJob("job_001");

  EXPECT_TRUE(success);
  EXPECT_FALSE(message.empty());
}

TEST_F(PrintManagerTest, GetPrintJobStatus) {
  auto status = getPrintJobStatus("job_001");

  EXPECT_FALSE(status.empty());
  EXPECT_TRUE(status.find("Completed") != std::string::npos ||
              status.find("Pending") != std::string::npos);
}

// ============================================================================
// Tiled Printing Tests
// ============================================================================

TEST_F(PrintManagerTest, PrintTiled) {
  auto sheet = createTestSheet();

  PrintSettings settings;
  settings.enable_tiling = true;
  settings.tile_overlap_mm = 10;
  settings.copies = 1;

  auto result = printTiled(*sheet, settings);

  EXPECT_TRUE(result.success);
  EXPECT_GT(result.tiles_printed, 0);
  EXPECT_TRUE(result.completed_successfully);
}

// ============================================================================
// Print Preview Image Tests
// ============================================================================

TEST_F(PrintManagerTest, PrintPreviewImageDimensions) {
  auto sheet = createTestSheet();

  PrintSettings settings;
  settings.paper_size = PageSize::A4;
  settings.orientation = PageOrientation::Portrait;

  auto preview = generatePrintPreview(*sheet, settings);

  // A4 at 96 DPI: ~794 x 1123 pixels
  EXPECT_NEAR(preview.width_pixels, 794, 50);
  EXPECT_NEAR(preview.height_pixels, 1123, 50);
}

TEST_F(PrintManagerTest, PrintPreviewImageFormat) {
  auto sheet = createTestSheet();

  PrintSettings settings;
  settings.paper_size = PageSize::Letter;
  settings.orientation = PageOrientation::Landscape;

  auto preview = generatePrintPreview(*sheet, settings);

  // Should be RGBA format (4 bytes per pixel)
  int expected_size = preview.width_pixels * preview.height_pixels * 4;
  EXPECT_EQ(preview.image_data.size(), expected_size);
}

// ============================================================================
// Print Dialog Tests
// ============================================================================

TEST_F(PrintManagerTest, PrintWithDialogReturnsResult) {
  auto sheet = createTestSheet();

  PrintSettings defaults;
  defaults.paper_size = PageSize::A4;
  defaults.copies = 1;

  auto result = printDrawingWithDialog(*sheet, defaults);

  EXPECT_TRUE(result.has_value());
  if (result.has_value()) {
    EXPECT_TRUE(result.value().success);
  }
}
