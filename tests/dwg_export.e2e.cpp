// E2E Tests for DWG/DXF Export (Task 5.9)
// Tests the complete export pipeline from drawing creation to file validation

#include "bendcore/dwg_export.hpp"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>

using namespace bendcore;

// Helper functions
bool file_exists(const std::string& filename) {
    std::ifstream f(filename);
    return f.good();
}

std::size_t get_file_size(const std::string& filename) {
    std::ifstream f(filename, std::ios::binary | std::ios::ate);
    if (!f.good()) return 0;
    return static_cast<std::size_t>(f.tellg());
}

std::string read_file(const std::string& filename) {
    std::ifstream file(filename);
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

void cleanup_file(const std::string& filename) {
    std::remove(filename.c_str());
}

// E2E Test 1: Complete drawing export workflow
void e2e_test_complete_drawing_export() {
    std::cout << "\nE2E Test 1: Complete drawing export workflow..." << std::endl;
    
    // Create a complex geometry with all element types
    drawing::Geometry geom;
    
    // Add contour lines (tube cross-section)
    {
        drawing::Polyline2 line;
        line.pts = {{0, 0}, {100, 0}, {100, 50}, {0, 50}, {0, 0}};
        line.style = drawing::LineStyle::Contour;
        line.closed = true;
        geom.lines.push_back(line);
    }
    
    // Add axis lines
    {
        drawing::Polyline2 axis;
        axis.pts = {{-10, 25}, {110, 25}};
        axis.style = drawing::LineStyle::Axis;
        geom.lines.push_back(axis);
    }
    
    // Add circles (bends)
    {
        drawing::Circle2 circle1;
        circle1.c = {30, 30};
        circle1.r = 10;
        circle1.style = drawing::LineStyle::Contour;
        geom.circles.push_back(circle1);
        
        drawing::Circle2 circle2;
        circle2.c = {70, 30};
        circle2.r = 10;
        circle2.style = drawing::LineStyle::Contour;
        geom.circles.push_back(circle2);
    }
    
    // Add dimensions
    {
        drawing::Dim2 dim1;
        dim1.a = {0, 0};
        dim1.b = {100, 0};
        dim1.text = "100";
        dim1.offset = 10;
        geom.dims.push_back(dim1);
        
        drawing::Dim2 dim2;
        dim2.a = {100, 0};
        dim2.b = {100, 50};
        dim2.text = "50";
        dim2.offset = 10;
        geom.dims.push_back(dim2);
    }
    
    // Add text
    {
        drawing::Text2 text;
        text.p = {50, 5};
        text.text = "TUBE PROFILE";
        text.height = 4;
        text.anchor = "middle";
        geom.texts.push_back(text);
    }
    
    // Export configuration
    dwg::DWGExportOptions opts;
    opts.filename = "/tmp/e2e_test_complete.dxf";
    opts.scale = 1.0;
    opts.units = "MM";
    opts.include_dimensions = true;
    opts.include_text = true;
    opts.layers = dwg::create_default_layers();
    
    // Perform conversion
    auto conversion_result = dwg::geometry_to_dwg_entities(geom, opts);
    
    // Verify conversion
    assert(conversion_result.errors.empty());
    assert(conversion_result.entities.size() > 0);
    
    // Expected entities:
    // - 1 contour polyline (closed)
    // - 1 axis polyline
    // - 2 circles
    // - 2 dimensions (each creates 2 entities: text + line)
    // - 1 text
    // Total: 1 + 1 + 2 + 4 + 1 = 9 entities
    assert(conversion_result.entities.size() == 9);
    
    std::cout << "  Conversion result: " << conversion_result.entities.size() << " entities" << std::endl;
    
    // Write DXF file
    auto write_result = dwg::write_dwg_file(opts.filename, conversion_result.entities, opts);
    
    assert(write_result.success);
    assert(file_exists(opts.filename));
    assert(write_result.entity_count == conversion_result.entities.size());
    
    std::cout << "  File created: " << opts.filename << std::endl;
    
    // Validate file content
    std::string content = read_file(opts.filename);
    
    // Check DXF structure
    assert(content.find("SECTION") != std::string::npos);
    assert(content.find("HEADER") != std::string::npos);
    assert(content.find("TABLES") != std::string::npos);
    assert(content.find("LAYER") != std::string::npos);
    assert(content.find("ENTITIES") != std::string::npos);
    assert(content.find("EOF") != std::string::npos);
    
    // Check for layers
    assert(content.find("Contour") != std::string::npos);
    assert(content.find("Dimensions") != std::string::npos);
    assert(content.find("Axis") != std::string::npos);
    
    // Check for entity types
    assert(content.find("LWPOLYLINE") != std::string::npos);  // lines
    assert(content.find("CIRCLE") != std::string::npos);      // circles
    assert(content.find("TEXT") != std::string::npos);        // text
    
    std::size_t file_size = get_file_size(opts.filename);
    std::cout << "  File size: " << file_size << " bytes" << std::endl;
    assert(file_size > 1000);  // Should be substantial
    
    cleanup_file(opts.filename);
    std::cout << "✓ E2E Test 1 passed" << std::endl;
}

// E2E Test 2: DrawingSheet export
void e2e_test_drawing_sheet_export() {
    std::cout << "\nE2E Test 2: DrawingSheet export..." << std::endl;
    
    // Create a complete drawing sheet
    drawing::DrawingSheet sheet;
    sheet.number = 1;
    sheet.count = 1;
    sheet.standard = drawing::Standard::GOST;
    sheet.projection = drawing::Projection::FirstAngle;
    
    // Create a view with geometry
    drawing::PlacedView view;
    view.id = "front_view";
    view.kind = drawing::ViewKind::Front;
    view.label = "Фронтальный вид";
    view.scale = 1.0;
    view.origin = {50, 100};
    
    // Create geometry
    drawing::Geometry geom;
    
    drawing::Polyline2 line;
    line.pts = {{0, 0}, {100, 0}, {100, 50}, {0, 50}, {0, 0}};
    line.closed = true;
    line.style = drawing::LineStyle::Contour;
    geom.lines.push_back(line);
    
    view.geom = geom;
    sheet.views.push_back(view);
    
    // Export options
    dwg::DWGExportOptions opts;
    opts.filename = "/tmp/e2e_test_sheet.dxf";
    opts.scale = 1.0;
    opts.units = "MM";
    
    // Perform export
    auto save_result = dwg::save_drawing_as_dwg(sheet, opts);
    
    assert(save_result.success);
    assert(file_exists(opts.filename));
    assert(save_result.filename == opts.filename);
    assert(save_result.format == "DXF");
    
    std::cout << "  Sheet exported successfully" << std::endl;
    std::cout << "  File size: " << save_result.file_size << " bytes" << std::endl;
    
    cleanup_file(opts.filename);
    std::cout << "✓ E2E Test 2 passed" << std::endl;
}

// E2E Test 3: Scale transformation validation
void e2e_test_scale_transformation() {
    std::cout << "\nE2E Test 3: Scale transformation..." << std::endl;
    
    // Create geometry
    drawing::Geometry geom;
    drawing::Polyline2 line;
    line.pts = {{0, 0}, {100, 100}};
    line.style = drawing::LineStyle::Contour;
    geom.lines.push_back(line);
    
    drawing::Circle2 circle;
    circle.c = {50, 50};
    circle.r = 20;
    circle.style = drawing::LineStyle::Contour;
    geom.circles.push_back(circle);
    
    // Test multiple scales
    std::vector<double> scales = {0.5, 1.0, 2.0};
    
    for (double scale : scales) {
        dwg::DWGExportOptions opts;
        opts.filename = "/tmp/e2e_test_scale_" + std::to_string(static_cast<int>(scale * 10)) + ".dxf";
        opts.scale = scale;
        opts.units = "MM";
        
        auto result = dwg::geometry_to_dwg_entities(geom, opts);
        auto write_result = dwg::write_dwg_file(opts.filename, result.entities, opts);
        
        assert(write_result.success);
        
        std::cout << "  Scale " << scale << "x: OK" << std::endl;
        cleanup_file(opts.filename);
    }
    
    std::cout << "✓ E2E Test 3 passed" << std::endl;
}

// E2E Test 4: All unit systems
void e2e_test_all_units() {
    std::cout << "\nE2E Test 4: Unit system support..." << std::endl;
    
    drawing::Geometry geom;
    drawing::Polyline2 line;
    line.pts = {{0, 0}, {10, 10}};
    geom.lines.push_back(line);
    
    std::vector<std::string> units = {"MM", "CM", "INCH"};
    
    for (const auto& unit : units) {
        dwg::DWGExportOptions opts;
        opts.filename = "/tmp/e2e_test_units_" + unit + ".dxf";
        opts.units = unit;
        
        auto result = dwg::geometry_to_dwg_entities(geom, opts);
        auto write_result = dwg::write_dwg_file(opts.filename, result.entities, opts);
        
        assert(write_result.success);
        
        // Verify unit appears in DXF
        std::string content = read_file(opts.filename);
        assert(content.find("HEADER") != std::string::npos);
        
        std::cout << "  Unit " << unit << ": OK" << std::endl;
        cleanup_file(opts.filename);
    }
    
    std::cout << "✓ E2E Test 4 passed" << std::endl;
}

// E2E Test 5: Large drawing performance
void e2e_test_large_drawing() {
    std::cout << "\nE2E Test 5: Large drawing performance..." << std::endl;
    
    // Create a large geometry with many elements
    drawing::Geometry geom;
    
    // Add many lines
    for (int i = 0; i < 50; ++i) {
        drawing::Polyline2 line;
        line.pts = {{0.0 + i, 0.0}, {100.0 + i, 50.0}};
        line.style = drawing::LineStyle::Contour;
        geom.lines.push_back(line);
    }
    
    // Add many circles
    for (int i = 0; i < 30; ++i) {
        drawing::Circle2 circle;
        circle.c = {50.0 + i * 2, 25.0};
        circle.r = 5.0;
        circle.style = drawing::LineStyle::Contour;
        geom.circles.push_back(circle);
    }
    
    // Add many text elements
    for (int i = 0; i < 20; ++i) {
        drawing::Text2 text;
        text.p = {50.0, 10.0 + i * 5};
        text.text = "Label " + std::to_string(i);
        text.height = 3.5;
        geom.texts.push_back(text);
    }
    
    dwg::DWGExportOptions opts;
    opts.filename = "/tmp/e2e_test_large.dxf";
    opts.units = "MM";
    
    // Perform conversion
    auto result = dwg::geometry_to_dwg_entities(geom, opts);
    auto write_result = dwg::write_dwg_file(opts.filename, result.entities, opts);
    
    assert(write_result.success);
    
    std::size_t file_size = get_file_size(opts.filename);
    std::cout << "  Large drawing: " << result.entities.size() << " entities" << std::endl;
    std::cout << "  File size: " << file_size << " bytes" << std::endl;
    assert(file_size > 10000);  // Large file
    
    cleanup_file(opts.filename);
    std::cout << "✓ E2E Test 5 passed" << std::endl;
}

int main() {
    std::cout << "=== DWG/DXF Export E2E Tests ===" << std::endl;
    
    try {
        e2e_test_complete_drawing_export();
        e2e_test_drawing_sheet_export();
        e2e_test_scale_transformation();
        e2e_test_all_units();
        e2e_test_large_drawing();
        
        std::cout << "\n=== All E2E tests passed! ===" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "E2E Test failed: " << e.what() << std::endl;
        return 1;
    }
}
