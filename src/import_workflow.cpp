// Import Workflow Manager Implementation (Q3 2027, Task 1, Subtask 7)
#include <bendcore/import_workflow.hpp>
#ifdef BENDCORE_HAS_OCCT_GEOMETRY_BACKEND
#include <bendcore/occt/geometry_backend.hpp>
#endif

#include <chrono>
#include <algorithm>
#include <cctype>
#include <sstream>
#include <iomanip>
#include <fstream>

namespace bendcore::import {

ImportWorkflow::ImportWorkflow() {
#ifdef BENDCORE_HAS_OCCT_GEOMETRY_BACKEND
    // Real B-Rep analysis (Open CASCADE); without it analyze() reports
    // confidence 0 and STEP imports fail with a clear message.
    if (!bendcore::geometry::has_geometry_backend()) {
        bendcore::occt::install_geometry_backend();
    }
#endif
}

ImportWorkflow::~ImportWorkflow() {
}

std::string ImportWorkflow::get_file_extension(const std::string& file_path) {
    size_t dot_pos = file_path.rfind('.');
    if (dot_pos == std::string::npos) {
        return "";
    }

    std::string ext = file_path.substr(dot_pos + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return ext;
}

std::string ImportWorkflow::detect_format(const std::string& file_path) {
    std::string ext = get_file_extension(file_path);

    if (ext == "step" || ext == "stp") {
        return "STEP";
    } else if (ext == "iges" || ext == "igs") {
        return "IGES";
    } else if (ext == "dxf") {
        return "DXF";
    }

    return "UNKNOWN";
}

bool ImportWorkflow::is_supported_format(const std::string& format) {
    return format == "STEP" || format == "IGES" || format == "DXF";
}

void ImportWorkflow::log_step(const std::string& step_name, bool success) {
    std::ostringstream oss;
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);

    oss << "[" << std::put_time(std::gmtime(&time), "%H:%M:%S") << "] "
        << step_name << (success ? " ✓" : " ✗") << "\n";

    log_buffer_ += oss.str();
}

ImportResult ImportWorkflow::import(const ImportConfig& config) {
    auto start_time = std::chrono::high_resolution_clock::now();

    ImportResult result;
    result.success = false;
    log_buffer_ = "";

    log_step("Import started");

    // Validate input
    if (config.file_path.empty()) {
        result.warnings.push_back("File path is empty");
        log_step("Validation", false);
        return result;
    }

    if (config.author.empty()) {
        result.warnings.push_back("Author not specified");
    }

    // Check file exists
    std::ifstream file_check(config.file_path);
    if (!file_check.good()) {
        result.warnings.push_back("File not found: " + config.file_path);
        log_step("File check", false);
        return result;
    }
    file_check.close();

    // Detect format
    std::string format = detect_format(config.file_path);
    log_step("Format detected: " + format);

    if (!is_supported_format(format)) {
        result.warnings.push_back("Unsupported file format: " + format);
        return result;
    }

    // Check cache
    if (config.use_cache) {
        auto cache_key = bendcore::cache::GeometryCache::compute_hash(config.file_path);
        auto cached_entry = cache_.get(cache_key);

        if (cached_entry.has_value()) {
            log_step("Cache hit");

            // Build dimensions from cached params
            auto dim_set = builder_.build(cached_entry->params, config.file_path, config.author);
            result.formulas = dim_set.dimensions;
            result.success = true;
            result.shape_count = 1;

            auto end_time = std::chrono::high_resolution_clock::now();
            result.execution_time_ms = std::chrono::duration<double, std::milli>(
                end_time - start_time).count();

            log_step("Cache retrieval successful");
            result.log = log_buffer_;
            return result;
        }
    }

    log_step("Cache miss");

    // Route to appropriate importer
    if (format == "STEP" || format == "IGES") {
        result = import_step_file(config);
    } else if (format == "DXF") {
        result = import_dxf_file(config);
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    result.execution_time_ms = std::chrono::duration<double, std::milli>(
        end_time - start_time).count();

    result.log = log_buffer_;
    return result;
}

ImportResult ImportWorkflow::import_step_file(const ImportConfig& config) {
    ImportResult result;
    result.success = false;

    log_step("Reading STEP file");

    // Read STEP file
    auto step_result = step_reader_.read(config.file_path);

    if (!step_result.success) {
        result.warnings.push_back("Failed to read STEP file: " + step_result.error_message);
        log_step("STEP read", false);
        return result;
    }

    log_step("STEP file read successfully");
    result.shape_count = step_result.shape_count;

    // Analyze geometry
    log_step("Analyzing geometry");

    // Analyse the B-Rep of the first top-level shape (BRepTools text from
    // STEPReader). Previously the shape *ID* ("shape_0") was analysed, which
    // can never be recognised.
    if (step_result.shape_ids.empty()) {
        result.warnings.push_back("No shapes found in STEP file");
        log_step("Geometry analysis", false);
        return result;
    }
    const std::string geometry_data = step_reader_.get_shape_data(step_result.shape_ids[0]);
    if (geometry_data.empty()) {
        result.warnings.push_back("STEP reader returned no geometry for " + step_result.shape_ids[0]);
        log_step("Geometry analysis", false);
        return result;
    }

    auto params = analyzer_.analyze(geometry_data);

    // Unrecognised or low-confidence (< 0.5, see is_valid) geometry must not
    // turn into zero dimensions, nor be cached
    if (!params.is_valid()) {
        result.warnings.push_back("Geometry not recognised as a tube: " +
                                  (params.analysis_notes.empty() ? std::string("no details")
                                                                 : params.analysis_notes));
        log_step("Geometry analysis", false);
        return result;
    }
    log_step("Geometry analysis complete");

    // Build parametric dimensions
    log_step("Building parametric dimensions");

    auto dim_set = builder_.build(params, config.file_path, config.author);

    if (dim_set.dimensions.empty()) {
        result.warnings.push_back("No dimensions created");
        log_step("Dimension creation", false);
        return result;
    }

    log_step("Parametric dimensions created");

    // Cache results
    if (config.use_cache) {
        log_step("Caching results");
        cache_.add(config.file_path, geometry_data, params);
    }

    // Return success
    result.formulas = dim_set.dimensions;
    result.success = true;

    log_step("Import complete");

    return result;
}

ImportResult ImportWorkflow::import_dxf_file(const ImportConfig& config) {
    ImportResult result;
    result.success = false;

    log_step("Reading DXF file");

    auto doc = bendcore::dxf::readDxfFile(config.file_path);
    if (!doc) {
        result.warnings.push_back(doc.error().message);
        log_step("DXF read", false);
        return result;
    }
    const auto& dxf_doc = doc.value();
    result.warnings.insert(result.warnings.end(), dxf_doc.warnings.begin(), dxf_doc.warnings.end());
    for (const auto& [type, count] : dxf_doc.skipped) {
        result.warnings.push_back("DXF: " + std::to_string(count) + " " + type + " entities are not imported");
    }
    if (dxf_doc.entity_count() == 0) {
        result.warnings.push_back("No entities found in DXF file");
        log_step("DXF read", false);
        return result;
    }

    log_step("DXF file read successfully");

    // Параметрические размеры строятся только по B-rep (STEP/IGES); из DXF берётся
    // число прочитанных примитивов.
    result.success = true;
    result.shape_count = static_cast<int>(dxf_doc.entity_count());

    log_step("Import complete");

    return result;
}

}  // namespace bendcore::import
