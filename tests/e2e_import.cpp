// End-to-End Import Test (Q3 2027, Task 1, Subtask 10)
#include <bendcore/import_workflow.hpp>
#include <bendcore/step_reader.hpp>
#include <bendcore/geometry_analyzer.hpp>
#include <bendcore/parametric_builder.hpp>
#include <bendcore/geometry_cache.hpp>
#include <bendcore/dxf_reader.hpp>
#ifdef BENDCORE_HAS_OCCT_GEOMETRY_BACKEND
#include <bendcore/tube_body.hpp>
#endif

#include <cassert>
#include <iostream>
#include <fstream>
#include <cmath>

using namespace bendcore::import;
using namespace bendcore::step;
using namespace bendcore::geometry;
using namespace bendcore::parametric;
using namespace bendcore::cache;
using namespace bendcore::dxf;

int main() {
    std::cout << "=== End-to-End Import Test ===" << std::endl;

    // Test 1: a STEP file without geometry fails cleanly and is not cached
    {
        std::cout << "\nTest 1: STEP without geometry is rejected... ";

        std::string test_file = "test_e2e_empty.step";
        std::ofstream file(test_file);
        file << "ISO-10303-21;\nHEADER;\n";
        file << "FILE_DESCRIPTION(('Test'),'2;1');\n";
        file << "FILE_NAME('t','2000-01-01T00:00:00',(''),(''),'','','');\n";
        file << "FILE_SCHEMA(('AP203'));\n";
        file << "ENDSEC;\nDATA;\n";
        file << "#1=PRODUCT('Test','','',());\n";
        file << "ENDSEC;\nEND-ISO-10303-21;\n";
        file.close();

        ImportWorkflow workflow;
        ImportConfig config;
        config.file_path = test_file;
        config.author = "TestAuthor";
        config.use_cache = true;

        bendcore::import::ImportResult first = workflow.import(config);
        bendcore::import::ImportResult second = workflow.import(config);
        std::remove(test_file.c_str());

        assert(!first.success && !first.warnings.empty());
        assert(!second.success);  // a failure must not be served from cache
        std::cout << "PASS (" << first.warnings.back() << ")\n";
    }

#ifdef BENDCORE_HAS_OCCT_GEOMETRY_BACKEND
    // Test 2: real pipeline - tube -> STEP -> STEPReader -> B-Rep analysis
    // -> parametric dimensions -> ImportWorkflow (miss, then cache hit)
    {
        std::cout << "Test 2: STEP round trip through the import pipeline... ";
        using namespace bendcore;

        const std::string test_file = "test_e2e_tube.step";
        geom::export_step(Path{Straight{100}, Bend{50, 90}, Straight{60}}, 20.0, 2.0, test_file);

        STEPReader reader;
        const auto read = reader.read(test_file);
        assert(read.success && read.shape_count == 1);
        assert(reader.get_property("format") == "STEP");
        assert(reader.get_property("solid_count") == "1");

        const ExtractedParams params = GeometryAnalyzer().analyze(reader.get_shape_data(read.shape_ids[0]));
        assert(params.is_valid());
        assert(std::abs(params.outer_diameter - 20.0) < 1e-6);
        assert(std::abs(params.wall_thickness - 2.0) < 1e-6);
        assert(params.bend_angles.size() == 1 && std::abs(params.bend_angles[0] - 90.0) < 1e-4);
        assert(std::abs(params.bend_radii[0] - 50.0) < 1e-6);

        ImportWorkflow workflow;
        ImportConfig config;
        config.file_path = test_file;
        config.author = "TestAuthor";
        config.use_cache = true;
        const auto miss = workflow.import(config);
        const auto hit = workflow.import(config);
        std::remove(test_file.c_str());

        assert(miss.success && miss.shape_count == 1 && miss.formulas.size() >= 3);
        assert(miss.log.find("Cache miss") != std::string::npos);
        assert(hit.success && hit.log.find("Cache hit") != std::string::npos);
        assert(hit.formulas.size() == miss.formulas.size());
        std::cout << "PASS (OD " << params.outer_diameter << ", wall " << params.wall_thickness
                  << ", bend " << params.bend_angles[0] << " deg R" << params.bend_radii[0]
                  << ", " << miss.formulas.size() << " formulas)\n";
    }
#endif

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
        file << "  0\nSECTION\n  2\nHEADER\n  9\n$INSUNITS\n 70\n4\n  0\nENDSEC\n";
        file << "  0\nSECTION\n  2\nENTITIES\n";
        file << "  0\nLINE\n  8\n0\n 10\n10.5\n 20\n-4\n 11\n30\n 21\n2.25\n";
        file << "  0\nENDSEC\n  0\nEOF\n";
        file.close();

        auto geom = readDxfFile(test_file);
        std::remove(test_file.c_str());

        assert(geom.ok());
        assert(geom.value().insunits == 4);
        assert(geom.value().lines.size() == 1);
        assert(geom.value().lines[0].start.x == 10.5);
        assert(geom.value().lines[0].end.y == 2.25);
        std::cout << "PASS\n";
    }

    std::cout << "\n=== All E2E Tests Passed ===" << std::endl;
    return 0;
}
