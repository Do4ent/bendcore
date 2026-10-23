// Import Workflow Manager (Q3 2027, Task 1, Subtask 7)
// Orchestrates the complete file import workflow
#pragma once

#include <bendcore/step_reader.hpp>
#include <bendcore/dxf_reader.hpp>
#include <bendcore/geometry_analyzer.hpp>
#include <bendcore/geometry_cache.hpp>
#include <bendcore/parametric_builder.hpp>
#include <bendcore/formula.hpp>

#include <string>
#include <vector>
#include <map>
#include <chrono>

namespace bendcore::import {

using DimensionFormula = bendcore::formula::DimensionFormula;

// Import configuration
struct ImportConfig {
    std::string file_path;
    std::string author;
    bool use_cache = true;
    double precision_mm = 0.01;
    bool auto_create_formulas = true;
};

// Import result with formulas and metadata
struct ImportResult {
    bool success = false;
    std::vector<DimensionFormula> formulas;
    std::vector<std::string> warnings;
    std::string log;
    double execution_time_ms = 0.0;
    int shape_count = 0;
};

// Import workflow orchestrator
class ImportWorkflow {
public:
    ImportWorkflow();
    ~ImportWorkflow();

    // Execute complete import workflow
    ImportResult import(const ImportConfig& config);

    // Detect file format from extension
    static std::string detect_format(const std::string& file_path);

    // Check if format is supported
    static bool is_supported_format(const std::string& format);

private:
    bendcore::step::STEPReader step_reader_;
    bendcore::dxf::DXFReader dxf_reader_;
    bendcore::geometry::GeometryAnalyzer analyzer_;
    bendcore::cache::GeometryCache cache_;
    bendcore::parametric::ParametricDimensionBuilder builder_;

    std::string log_buffer_;

    // Log workflow step
    void log_step(const std::string& step_name, bool success = true);

    // Import STEP file
    ImportResult import_step_file(const ImportConfig& config);

    // Import DXF file
    ImportResult import_dxf_file(const ImportConfig& config);

    // Format detection from file extension
    static std::string get_file_extension(const std::string& file_path);
};

}  // namespace bendcore::import
