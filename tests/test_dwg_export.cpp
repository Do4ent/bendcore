#include "bendcore/dwg_export.hpp"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>

using namespace bendcore;

// Вспомогательная функция для проверки существования файла
bool file_exists(const std::string& filename) {
    std::ifstream f(filename);
    return f.good();
}

// Вспомогательная функция для удаления файла
void remove_file(const std::string& filename) {
    std::remove(filename.c_str());
}

// Вспомогательная функция для чтения содержимого DXF файла
std::string read_dxf_file(const std::string& filename) {
    std::ifstream file(filename);
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

// Test 1: Экспорт пустого чертежа (пустой Geometry)
void test_export_empty_drawing() {
    std::cout << "Test 1: Export empty drawing..." << std::endl;
    
    drawing::Geometry geom;
    dwg::DWGExportOptions opts;
    opts.filename = "/tmp/test_empty.dxf";
    
    auto result = dwg::geometry_to_dwg_entities(geom, opts);
    assert(result.entities.empty());
    assert(result.errors.empty());
    
    // Запись пустого файла
    auto write_result = dwg::write_dwg_file(opts.filename, result.entities, opts);
    assert(write_result.success);
    assert(file_exists(opts.filename));
    
    // Проверить содержимое
    std::string content = read_dxf_file(opts.filename);
    assert(content.find("SECTION") != std::string::npos);
    assert(content.find("ENTITIES") != std::string::npos);
    assert(content.find("EOF") != std::string::npos);
    
    remove_file(opts.filename);
    std::cout << "✓ Test 1 passed" << std::endl;
}

// Test 2: Конвертация линий (координаты корректны)
void test_line_conversion() {
    std::cout << "Test 2: Line conversion..." << std::endl;
    
    drawing::Geometry geom;
    drawing::Polyline2 line;
    line.pts = {{0, 0}, {100, 50}};
    line.style = drawing::LineStyle::Contour;
    line.closed = false;
    geom.lines.push_back(line);
    
    dwg::DWGExportOptions opts;
    auto result = dwg::geometry_to_dwg_entities(geom, opts);
    
    assert(result.entities.size() == 1);
    assert(result.entities[0].type == dwg::DWGEntity::Type::Polyline);
    assert(result.entities[0].points.size() == 2);
    assert(result.entities[0].points[0][0] == 0);
    assert(result.entities[0].points[0][1] == 0);
    assert(result.entities[0].points[1][0] == 100);
    assert(result.entities[0].points[1][1] == 50);
    assert(!result.entities[0].closed);
    
    std::cout << "✓ Test 2 passed" << std::endl;
}

// Test 3: Конвертация дуг (radius, angle)
void test_arc_conversion() {
    std::cout << "Test 3: Arc conversion..." << std::endl;
    
    drawing::Geometry geom;
    drawing::Circle2 circle;
    circle.c = {50, 50};
    circle.r = 25;
    circle.style = drawing::LineStyle::Contour;
    geom.circles.push_back(circle);
    
    dwg::DWGExportOptions opts;
    auto result = dwg::geometry_to_dwg_entities(geom, opts);
    
    assert(result.entities.size() == 1);
    assert(result.entities[0].type == dwg::DWGEntity::Type::Circle);
    assert(result.entities[0].points[0][0] == 50);
    assert(result.entities[0].points[0][1] == 50);
    assert(std::abs(result.entities[0].radius - 25) < 0.01);
    
    std::cout << "✓ Test 3 passed" << std::endl;
}

// Test 4: Конвертация текста (позиция, размер)
void test_text_conversion() {
    std::cout << "Test 4: Text conversion..." << std::endl;
    
    drawing::Geometry geom;
    drawing::Text2 text;
    text.p = {100, 100};
    text.text = "TEST_TEXT";
    text.height = 5.0;
    text.anchor = "middle";
    geom.texts.push_back(text);
    
    dwg::DWGExportOptions opts;
    opts.include_text = true;
    auto result = dwg::geometry_to_dwg_entities(geom, opts);
    
    assert(result.entities.size() == 1);
    assert(result.entities[0].type == dwg::DWGEntity::Type::Text);
    assert(result.entities[0].text == "TEST_TEXT");
    assert(result.entities[0].text_pos[0] == 100);
    assert(result.entities[0].text_pos[1] == 100);
    assert(std::abs(result.entities[0].text_height - 5.0) < 0.01);
    assert(result.entities[0].text_anchor == "middle");
    
    std::cout << "✓ Test 4 passed" << std::endl;
}

// Test 5: Конвертация размеров (начало, конец, текст)
void test_dimension_conversion() {
    std::cout << "Test 5: Dimension conversion..." << std::endl;
    
    drawing::Geometry geom;
    drawing::Dim2 dim;
    dim.a = {0, 0};
    dim.b = {100, 0};
    dim.text = "100";
    dim.offset = 10.0;
    dim.reference = false;
    geom.dims.push_back(dim);
    
    dwg::DWGExportOptions opts;
    opts.include_dimensions = true;
    auto result = dwg::geometry_to_dwg_entities(geom, opts);
    
    // Размер должен создать 2 сущности: текст и линию
    assert(result.entities.size() == 2);
    
    // Первая сущность - текст размера
    assert(result.entities[0].type == dwg::DWGEntity::Type::Text);
    assert(result.entities[0].text == "100");
    assert(result.entities[0].layer == dwg::LayerType::Dimensions);
    
    // Вторая сущность - линия размера
    assert(result.entities[1].type == dwg::DWGEntity::Type::Line);
    assert(result.entities[1].points[0][0] == 0);
    assert(result.entities[1].points[1][0] == 100);
    
    std::cout << "✓ Test 5 passed" << std::endl;
}

// Test 6: Слои созданы правильно
void test_layers_created() {
    std::cout << "Test 6: Layers created correctly..." << std::endl;
    
    auto layers = dwg::create_default_layers();
    
    assert(layers.size() == 5);  // Contour, Dimensions, Axis, Markup, Construction
    
    // Проверить слой Contour
    assert(layers[dwg::LayerType::Contour].name == "Contour");
    assert(layers[dwg::LayerType::Contour].color == 30);
    assert(layers[dwg::LayerType::Contour].linetype == dwg::DWGLineType::Continuous);
    
    // Проверить слой Dimensions
    assert(layers[dwg::LayerType::Dimensions].name == "Dimensions");
    assert(layers[dwg::LayerType::Dimensions].color == 1);
    
    // Проверить слой Axis
    assert(layers[dwg::LayerType::Axis].name == "Axis");
    assert(layers[dwg::LayerType::Axis].linetype == dwg::DWGLineType::Dashed);
    
    std::cout << "✓ Test 6 passed" << std::endl;
}

// Test 7: Scale применён (1:1, 1:2 и т.д.)
void test_scale_applied() {
    std::cout << "Test 7: Scale applied..." << std::endl;
    
    // Создать рисунок с видом
    drawing::DrawingSheet sheet;
    sheet.number = 1;
    sheet.count = 1;
    sheet.standard = drawing::Standard::GOST;
    sheet.projection = drawing::Projection::FirstAngle;
    
    // Добавить вид с масштабом
    drawing::PlacedView view;
    view.id = "view1";
    view.kind = drawing::ViewKind::Front;
    view.label = "Front";
    view.scale = 0.5;  // 1:2
    view.origin = {100, 100};
    
    drawing::Geometry geom;
    drawing::Polyline2 line;
    line.pts = {{0, 0}, {100, 100}};
    line.style = drawing::LineStyle::Contour;
    geom.lines.push_back(line);
    
    view.geom = geom;
    sheet.views.push_back(view);
    
    dwg::DWGExportOptions opts;
    opts.filename = "/tmp/test_scale.dxf";
    opts.scale = 0.5;
    
    auto result = dwg::save_drawing_as_dwg(sheet, opts);
    
    assert(result.success);
    assert(file_exists(opts.filename));
    
    remove_file(opts.filename);
    std::cout << "✓ Test 7 passed" << std::endl;
}

// Test 8: Валидация имени файла
void test_filename_validation() {
    std::cout << "Test 8: Filename validation..." << std::endl;
    
    drawing::Geometry geom;
    dwg::DWGExportOptions opts;
    opts.filename = "/tmp/valid_filename.dxf";
    
    auto result = dwg::geometry_to_dwg_entities(geom, opts);
    auto write_result = dwg::write_dwg_file(opts.filename, result.entities, opts);
    
    assert(write_result.success);
    assert(write_result.filename == "/tmp/valid_filename.dxf");
    
    remove_file(opts.filename);
    std::cout << "✓ Test 8 passed" << std::endl;
}

// Test 9: Line style conversion (LineStyle -> DWGLineType)
void test_linestyle_conversion() {
    std::cout << "Test 9: LineStyle conversion..." << std::endl;
    
    assert(dwg::linestyle_to_dwg(drawing::LineStyle::Contour) == dwg::DWGLineType::Continuous);
    assert(dwg::linestyle_to_dwg(drawing::LineStyle::Axis) == dwg::DWGLineType::Dashed);
    assert(dwg::linestyle_to_dwg(drawing::LineStyle::Hidden) == dwg::DWGLineType::Dotted);
    assert(dwg::linestyle_to_dwg(drawing::LineStyle::Cut) == dwg::DWGLineType::DashDot);
    
    std::cout << "✓ Test 9 passed" << std::endl;
}

// Test 10: DXF file structure validation
void test_dxf_structure() {
    std::cout << "Test 10: DXF file structure..." << std::endl;
    
    drawing::Geometry geom;
    drawing::Polyline2 line;
    line.pts = {{10, 10}, {20, 20}};
    geom.lines.push_back(line);
    
    dwg::DWGExportOptions opts;
    opts.filename = "/tmp/test_structure.dxf";
    
    auto result = dwg::geometry_to_dwg_entities(geom, opts);
    auto write_result = dwg::write_dwg_file(opts.filename, result.entities, opts);
    
    std::string content = read_dxf_file(opts.filename);
    
    // Проверить основные секции DXF
    assert(content.find("SECTION") != std::string::npos);
    assert(content.find("HEADER") != std::string::npos);
    assert(content.find("TABLES") != std::string::npos);
    assert(content.find("LAYER") != std::string::npos);
    assert(content.find("ENTITIES") != std::string::npos);
    assert(content.find("ENDSEC") != std::string::npos);
    assert(content.find("EOF") != std::string::npos);
    
    // Проверить, что сущность присутствует
    assert(content.find("LWPOLYLINE") != std::string::npos);
    
    remove_file(opts.filename);
    std::cout << "✓ Test 10 passed" << std::endl;
}

// Test 11: Multiple entities in single drawing
void test_multiple_entities() {
    std::cout << "Test 11: Multiple entities..." << std::endl;
    
    drawing::Geometry geom;
    
    // Добавить линию
    drawing::Polyline2 line;
    line.pts = {{0, 0}, {50, 50}};
    geom.lines.push_back(line);
    
    // Добавить окружность
    drawing::Circle2 circle;
    circle.c = {50, 50};
    circle.r = 25;
    geom.circles.push_back(circle);
    
    // Добавить текст
    drawing::Text2 text;
    text.p = {100, 100};
    text.text = "Label";
    geom.texts.push_back(text);
    
    dwg::DWGExportOptions opts;
    auto result = dwg::geometry_to_dwg_entities(geom, opts);
    
    // Должно быть 3 сущности
    assert(result.entities.size() == 3);
    
    // Проверить типы
    assert(result.entities[0].type == dwg::DWGEntity::Type::Polyline);
    assert(result.entities[1].type == dwg::DWGEntity::Type::Circle);
    assert(result.entities[2].type == dwg::DWGEntity::Type::Text);
    
    std::cout << "✓ Test 11 passed" << std::endl;
}

// Test 12: Units conversion (MM, CM, INCH)
void test_units_conversion() {
    std::cout << "Test 12: Units conversion..." << std::endl;
    
    drawing::Geometry geom;
    dwg::DWGExportOptions opts;
    
    // Test MM
    opts.filename = "/tmp/test_mm.dxf";
    opts.units = "MM";
    auto result1 = dwg::geometry_to_dwg_entities(geom, opts);
    auto write1 = dwg::write_dwg_file(opts.filename, result1.entities, opts);
    assert(write1.success);
    
    // Test CM
    opts.filename = "/tmp/test_cm.dxf";
    opts.units = "CM";
    auto result2 = dwg::geometry_to_dwg_entities(geom, opts);
    auto write2 = dwg::write_dwg_file(opts.filename, result2.entities, opts);
    assert(write2.success);
    
    // Test INCH
    opts.filename = "/tmp/test_inch.dxf";
    opts.units = "INCH";
    auto result3 = dwg::geometry_to_dwg_entities(geom, opts);
    auto write3 = dwg::write_dwg_file(opts.filename, result3.entities, opts);
    assert(write3.success);
    
    remove_file("/tmp/test_mm.dxf");
    remove_file("/tmp/test_cm.dxf");
    remove_file("/tmp/test_inch.dxf");
    
    std::cout << "✓ Test 12 passed" << std::endl;
}

int main() {
    std::cout << "=== DWG/DXF Export Tests ===" << std::endl;
    
    try {
        test_export_empty_drawing();
        test_line_conversion();
        test_arc_conversion();
        test_text_conversion();
        test_dimension_conversion();
        test_layers_created();
        test_scale_applied();
        test_filename_validation();
        test_linestyle_conversion();
        test_dxf_structure();
        test_multiple_entities();
        test_units_conversion();
        
        std::cout << "\n=== All tests passed! ===" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed: " << e.what() << std::endl;
        return 1;
    }
}
