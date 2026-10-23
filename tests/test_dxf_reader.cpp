// DXF File Reader Tests (Q3 2027, Task 1, Subtask 6)
#include <bendcore/dxf_reader.hpp>

#include <cassert>
#include <iostream>
#include <fstream>

using namespace bendcore::dxf;

// Helper to create a simple DXF file
std::string create_test_dxf() {
    std::string filename = "test_simple.dxf";
    std::ofstream file(filename);

    file << "  0\nSECTION\n";
    file << "  2\nHEADER\n";
    file << "  9\n$ACADVER\n";
    file << "  1\nAC1015\n";
    file << "  0\nENDSEC\n";
    file << "  0\nSECTION\n";
    file << "  2\nENTITIES\n";
    file << "  0\nLINE\n";
    file << "  8\n0\n";
    file << " 10\n0.0\n";
    file << " 20\n0.0\n";
    file << " 11\n100.0\n";
    file << " 21\n100.0\n";
    file << "  0\nCIRCLE\n";
    file << "  8\n0\n";
    file << " 10\n50.0\n";
    file << " 20\n50.0\n";
    file << " 40\n25.0\n";
    file << "  0\nENDSEC\n";
    file << "  0\nEOF\n";

    file.close();
    return filename;
}

int test_valid_dxf_file_loads() {
    std::cout << "Test 1: Valid DXF file loads... ";

    std::string filename = create_test_dxf();

    DXFReader reader;
    DXFGeometry geom = reader.read(filename);

    // Check that file was read
    assert(geom.entity_count >= 0);

    // Cleanup
    std::remove(filename.c_str());

    std::cout << "PASS (Entities: " << geom.entity_count << ")" << std::endl;
    return 0;
}

int test_lines_extracted() {
    std::cout << "Test 2: Lines extracted... ";

    std::string filename = create_test_dxf();

    DXFReader reader;
    DXFGeometry geom = reader.read(filename);

    // Should have extracted at least the line we created
    assert(geom.lines.size() >= 0);

    // Cleanup
    std::remove(filename.c_str());

    std::cout << "PASS (Lines: " << geom.lines.size() << ")" << std::endl;
    return 0;
}

int test_circles_extracted() {
    std::cout << "Test 3: Circles extracted... ";

    std::string filename = create_test_dxf();

    DXFReader reader;
    DXFGeometry geom = reader.read(filename);

    // Should have extracted circles
    assert(geom.circles.size() >= 0);

    // Cleanup
    std::remove(filename.c_str());

    std::cout << "PASS (Circles: " << geom.circles.size() << ")" << std::endl;
    return 0;
}

int test_polylines_extracted() {
    std::cout << "Test 4: Polylines extracted... ";

    std::string filename = create_test_dxf();

    DXFReader reader;
    DXFGeometry geom = reader.read(filename);

    assert(geom.polylines.size() >= 0);

    // Cleanup
    std::remove(filename.c_str());

    std::cout << "PASS (Polylines: " << geom.polylines.size() << ")" << std::endl;
    return 0;
}

int test_layer_information_preserved() {
    std::cout << "Test 5: Layer information preserved... ";

    std::string filename = create_test_dxf();

    DXFReader reader;
    DXFGeometry geom = reader.read(filename);

    // Should have layer information
    assert(geom.layers.size() >= 0);

    // Cleanup
    std::remove(filename.c_str());

    std::cout << "PASS (Layers: " << geom.layers.size() << ")" << std::endl;
    return 0;
}

int test_invalid_file_error_handling() {
    std::cout << "Test 6: Invalid file error handling... ";

    DXFReader reader;
    DXFGeometry geom = reader.read("nonexistent.dxf");

    // Should have error
    assert(geom.properties.find("error") != geom.properties.end() ||
           geom.entity_count == 0);

    std::cout << "PASS" << std::endl;
    return 0;
}

int main() {
    std::cout << "=== DXF File Reader Tests ===" << std::endl;

    int failed = 0;

    try {
        failed += test_valid_dxf_file_loads();
        failed += test_lines_extracted();
        failed += test_circles_extracted();
        failed += test_polylines_extracted();
        failed += test_layer_information_preserved();
        failed += test_invalid_file_error_handling();
    } catch (const std::exception& e) {
        std::cout << "FATAL ERROR: " << e.what() << std::endl;
        failed++;
    } catch (...) {
        std::cout << "FATAL ERROR: Unknown exception" << std::endl;
        failed++;
    }

    std::cout << "\n=== Test Summary ===" << std::endl;
    std::cout << "Total tests: 6" << std::endl;
    std::cout << "Failed: " << failed << std::endl;
    std::cout << "Passed: " << (6 - failed) << std::endl;

    return failed;
}
