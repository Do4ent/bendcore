// Mesh Import/Export System
// M7 Phase: Mesh Optimization (GAP-028)
#pragma once

#include "bendcore/fea/mesh_generator.hpp"
#include <string>
#include <memory>
#include <vector>
#include <cstdint>

namespace bendcore::mesh_optimization {

// Supported mesh file formats
enum class MeshFormat {
    Gmsh,      // Gmsh format (.msh)
    STL,       // Stereolithography format (.stl)
    VTK,       // VTK Unstructured Grid (.vtu)
    Exodus,    // ExodusII format (.e)
    MedFile,   // MED file format (.med)
    NASTRAN,   // NASTRAN format (.nas)
    Abaqus,    // ABAQUS format (.inp)
    OpenFOAM   // OpenFOAM format
};

// Import/Export configuration
struct IOConfig {
    bool validate_on_import = true;
    bool compress_on_export = false;
    bool binary_format = true;  // vs ASCII
    int32_t precision = 15;     // Floating point precision
};

// Validation result for imported mesh
struct ValidationResult {
    bool is_valid;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
    int32_t total_checks;
    int32_t failed_checks;
};

// Base class for mesh format handlers
class MeshFormatHandler {
public:
    virtual ~MeshFormatHandler() = default;

    // Import mesh from file
    virtual std::unique_ptr<fea::FiniteElementMesh> import(
        const std::string& filename,
        const IOConfig& config) = 0;

    // Export mesh to file
    virtual bool export_mesh(
        const fea::FiniteElementMesh& mesh,
        const std::string& filename,
        const IOConfig& config) = 0;

    // Validate imported mesh
    virtual ValidationResult validate(
        const fea::FiniteElementMesh& mesh) = 0;

    // Get format name
    virtual std::string get_format_name() const = 0;

    // Check if file is this format
    virtual bool can_handle_file(const std::string& filename) const = 0;
};

// Gmsh format handler
class GmshFormatHandler : public MeshFormatHandler {
public:
    std::unique_ptr<fea::FiniteElementMesh> import(
        const std::string& filename,
        const IOConfig& config) override;

    bool export_mesh(
        const fea::FiniteElementMesh& mesh,
        const std::string& filename,
        const IOConfig& config) override;

    ValidationResult validate(
        const fea::FiniteElementMesh& mesh) override;

    std::string get_format_name() const override { return "Gmsh"; }

    bool can_handle_file(const std::string& filename) const override;

private:
    // Parse Gmsh element type
    fea::ElementType gmsh_to_element_type(int32_t gmsh_type);
    int32_t element_type_to_gmsh(fea::ElementType type);
};

// STL format handler
class STLFormatHandler : public MeshFormatHandler {
public:
    std::unique_ptr<fea::FiniteElementMesh> import(
        const std::string& filename,
        const IOConfig& config) override;

    bool export_mesh(
        const fea::FiniteElementMesh& mesh,
        const std::string& filename,
        const IOConfig& config) override;

    ValidationResult validate(
        const fea::FiniteElementMesh& mesh) override;

    std::string get_format_name() const override { return "STL"; }

    bool can_handle_file(const std::string& filename) const override;

private:
    // Read binary STL
    std::unique_ptr<fea::FiniteElementMesh> read_binary_stl(
        const std::string& filename);

    // Read ASCII STL
    std::unique_ptr<fea::FiniteElementMesh> read_ascii_stl(
        const std::string& filename);

    // Write binary STL
    bool write_binary_stl(
        const fea::FiniteElementMesh& mesh,
        const std::string& filename);

    // Write ASCII STL
    bool write_ascii_stl(
        const fea::FiniteElementMesh& mesh,
        const std::string& filename);
};

// VTK format handler
class VTKFormatHandler : public MeshFormatHandler {
public:
    std::unique_ptr<fea::FiniteElementMesh> import(
        const std::string& filename,
        const IOConfig& config) override;

    bool export_mesh(
        const fea::FiniteElementMesh& mesh,
        const std::string& filename,
        const IOConfig& config) override;

    ValidationResult validate(
        const fea::FiniteElementMesh& mesh) override;

    std::string get_format_name() const override { return "VTK"; }

    bool can_handle_file(const std::string& filename) const override;

private:
    // Parse VTK cell type
    fea::ElementType vtk_to_element_type(int32_t vtk_type);
    int32_t element_type_to_vtk(fea::ElementType type);

    // Write VTK header
    void write_vtk_header(
        std::ofstream& file,
        int32_t num_nodes,
        int32_t num_elements);
};

// Format conversion utilities
class FormatConverter {
public:
    FormatConverter() = default;
    ~FormatConverter() = default;

    // Convert between formats
    bool convert_format(
        const std::string& input_file,
        const std::string& output_file,
        MeshFormat output_format,
        const IOConfig& config);

    // Auto-detect format from filename
    static MeshFormat detect_format(const std::string& filename);

    // Get handler for format
    static std::unique_ptr<MeshFormatHandler> get_handler(MeshFormat format);

private:
    // Normalize mesh before conversion
    void normalize_mesh(fea::FiniteElementMesh& mesh);
};

// Main mesh I/O interface
class MeshIO {
public:
    MeshIO();
    ~MeshIO() = default;

    // Import mesh from file
    std::unique_ptr<fea::FiniteElementMesh> import_mesh(
        const std::string& filename,
        const IOConfig& config = IOConfig());

    // Import with format specification
    std::unique_ptr<fea::FiniteElementMesh> import_mesh(
        const std::string& filename,
        MeshFormat format,
        const IOConfig& config = IOConfig());

    // Export mesh to file
    bool export_mesh(
        const fea::FiniteElementMesh& mesh,
        const std::string& filename,
        const IOConfig& config = IOConfig());

    // Export with format specification
    bool export_mesh(
        const fea::FiniteElementMesh& mesh,
        const std::string& filename,
        MeshFormat format,
        const IOConfig& config = IOConfig());

    // Validate mesh on import
    ValidationResult validate_import(
        const std::string& filename,
        const IOConfig& config = IOConfig());

    // Get supported formats
    static std::vector<std::string> get_supported_formats();

private:
    std::vector<std::unique_ptr<MeshFormatHandler>> handlers_;

    // Register all handlers
    void register_handlers();
};

}  // namespace bendcore::mesh_optimization
