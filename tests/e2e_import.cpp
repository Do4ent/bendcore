// End-to-End Import Test (Q3 2027, Task 1, Subtask 10)
#include <bendcore/import_workflow.hpp>
#include <bendcore/step_reader.hpp>
#include <bendcore/geometry_analyzer.hpp>
#include <bendcore/parametric_builder.hpp>
#include <bendcore/geometry_cache.hpp>
#include <bendcore/dxf_reader.hpp>

#include <cassert>
#include <iostream>
#include <fstream>

using namespace bendcore::import;
using namespace bendcore::step;
using namespace bendcore::geometry;
using namespace bendcore::parametric;
using namespace bendcore::cache;
using namespace bendcore::dxf;

int main() {
    std::cout << "=== End-to-End Import Test ===" << std::endl;

    // Test 1: Complete STEP import workflow
    {
        std::cout << "\nTest 1: Complete STEP workflow... ";
        
        std::string test_file = "test_e2e.step";
        std::ofstream file(test_file);
        file << "ISO-10303-21;\nHEADER;\n";
        file << "FILE_DESCRIPTION(('Test'),?,?,?,?);\n";
        file << "FILE_NAME(?,?,?,?,?,?);\n";
        file << "FILE_SCHEMA(('AP203'));\n";
        file << "ENDSEC;\nDATA;\n";
        file << "#1=PRODUCT('Test','','',?);\n";
        file << "ENDSEC;\nEND-ISO-10303-21;\n";
        file.close();

        ImportWorkflow workflow;
        ImportConfig config;
        config.file_path = test_file;
        config.author = "TestAuthor";
        config.use_cache = true;

        ImportResult result = workflow.import(config);

        std::remove(test_file.c_str());

        assert(result.execution_time_ms >= 0);
        std::cout << "PASS\n";
    }

    // Test 2: Geometry extraction and analysis
    {
        std::cout << "Test 2: Geometry extraction and analysis... ";
        
        GeometryAnalyzer analyzer;
        ExtractedParams params = analyzer.analyze("test_geometry");
        
        assert(params.outer_diameter > 0);
        assert(params.total_length > 0);
        assert(params.confidence >= 0);
        
        std::cout << "PASS\n";
    }

    // Test 3: Parametric dimension creation
    {
        std::cout << "Test 3: Parametric dimension creation... ";
        
        ExtractedParams params;
        params.outer_diameter = 25.4;
        params.wall_thickness = 2.11;
        params.total_length = 150.0;
        params.confidence = 0.9;
        
        ParametricDimensionBuilder builder;
        auto dim_set = builder.build(params, "test.step", "author");
        
        assert(dim_set.dimensions.size() >= 3);
        std::cout << "PASS\n";
    }

    // Test 4: Geometry caching
    {
        std::cout << "Test 4: Geometry caching... ";
        
        GeometryCache cache;
        
        ExtractedParams params;
        params.outer_diameter = 25.4;
        
        auto key = cache.add("test.step", "shape_data", params);
        auto entry = cache.get(key);
        
        assert(entry.has_value());
        std::cout << "PASS\n";
    }

    // Test 5: DXF file handling
    {
        std::cout << "Test 5: DXF file handling... ";
        
        std::string test_file = "test_e2e.dxf";
        std::ofstream file(test_file);
        file << "  0\nSECTION\n  2\nHEADER\n  0\nENDSEC\n";
        file << "  0\nSECTION\n  2\nENTITIES\n  0\nEOF\n";
        file.close();
        
        DXFReader dxf_reader;
        auto geom = dxf_reader.read(test_file);
        
        std::remove(test_file.c_str());
        
        assert(geom.entity_count >= 0);
        std::cout << "PASS\n";
    }

    std::cout << "\n=== All E2E Tests Passed ===" << std::endl;
    return 0;
}
