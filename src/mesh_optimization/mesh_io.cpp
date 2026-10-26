// Mesh Import/Export Implementation
// M7 Phase: Mesh Optimization (GAP-028)
#include "bendcore/mesh_optimization/mesh_io.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstring>

using namespace bendcore;
using namespace bendcore::mesh_optimization;
using namespace bendcore::fea;

// ============================================================================
// GmshFormatHandler Implementation
// ============================================================================

std::unique_ptr<FiniteElementMesh> GmshFormatHandler::import(
    const std::string& filename,
    const IOConfig& config) {

    auto mesh = std::make_unique<FiniteElementMesh>();

    std::ifstream file(filename);
    if (!file.is_open()) {
        return nullptr;
    }

    std::string line;
    int32_t num_nodes = 0;
    int32_t num_elements = 0;

    while (std::getline(file, line)) {
        if (line == "$Nodes") {
            std::getline(file, line);
            num_nodes = std::stoi(line);

            for (int i = 0; i < num_nodes; ++i) {
                std::getline(file, line);
                std::istringstream iss(line);

                int32_t node_id;
                double x, y, z;
                iss >> node_id >> x >> y >> z;

                mesh->add_node(node_id, x, y, z, false);
            }
        }
        else if (line == "$Elements") {
            std::getline(file, line);
            num_elements = std::stoi(line);

            for (int i = 0; i < num_elements; ++i) {
                std::getline(file, line);
                std::istringstream iss(line);

                int32_t elem_id;
                int32_t elem_type;
                int32_t num_tags;
                iss >> elem_id >> elem_type >> num_tags;

                // Skip tags
                for (int j = 0; j < num_tags; ++j) {
                    int32_t tag;
                    iss >> tag;
                }

                ElementType type = gmsh_to_element_type(elem_type);
                int32_t num_nodes_in_elem = 0;

                switch (elem_type) {
                    case 4: num_nodes_in_elem = 4; break;  // Tetrahedron
                    case 10: num_nodes_in_elem = 10; break;  // 10-node tet
                    case 5: num_nodes_in_elem = 8; break;  // Hexahedron
                    case 12: num_nodes_in_elem = 20; break;  // 20-node hex
                    default: num_nodes_in_elem = 0;
                }

                std::vector<int32_t> node_ids;
                for (int j = 0; j < num_nodes_in_elem; ++j) {
                    int32_t node_id;
                    iss >> node_id;
                    node_ids.push_back(node_id);
                }

                if (num_nodes_in_elem > 0) {
                    mesh->add_element(elem_id, type, node_ids, 0);
                }
            }
        }
    }

    file.close();

    if (config.validate_on_import && !mesh->validate()) {
        return nullptr;
    }

    return mesh;
}

bool GmshFormatHandler::export_mesh(
    const FiniteElementMesh& mesh,
    const std::string& filename,
    const IOConfig& config) {

    std::ofstream file(filename);
    if (!file.is_open()) {
        return false;
    }

    file << "$MeshFormat\n";
    file << "2.2 0 8\n";
    file << "$EndMeshFormat\n";

    file << "$Nodes\n";
    file << mesh.num_nodes() << "\n";

    for (const auto& node : mesh.get_nodes()) {
        file << node.id << " " << node.position[0] << " "
             << node.position[1] << " " << node.position[2] << "\n";
    }

    file << "$EndNodes\n";

    file << "$Elements\n";
    file << mesh.num_elements() << "\n";

    int32_t elem_count = 0;
    for (const auto& elem : mesh.get_elements()) {
        int32_t gmsh_type = element_type_to_gmsh(elem.type);
        file << ++elem_count << " " << gmsh_type << " 2 0 0";

        for (int32_t node_id : elem.node_ids) {
            file << " " << node_id;
        }
        file << "\n";
    }

    file << "$EndElements\n";

    file.close();
    return true;
}

ValidationResult GmshFormatHandler::validate(
    const FiniteElementMesh& mesh) {

    ValidationResult result;
    result.is_valid = true;
    result.total_checks = 0;
    result.failed_checks = 0;

    if (mesh.num_nodes() == 0) {
        result.is_valid = false;
        result.failed_checks++;
    }
    result.total_checks++;

    if (mesh.num_elements() == 0) {
        result.is_valid = false;
        result.failed_checks++;
    }
    result.total_checks++;

    if (!mesh.validate()) {
        result.is_valid = false;
        result.warnings.push_back("Mesh topology validation failed");
        result.failed_checks++;
    }
    result.total_checks++;

    return result;
}

fea::ElementType GmshFormatHandler::gmsh_to_element_type(int32_t gmsh_type) {
    switch (gmsh_type) {
        case 4: return ElementType::Tetrahedron4;
        case 10: return ElementType::Tetrahedron10;
        case 5: return ElementType::Hexahedron8;
        case 12: return ElementType::Hexahedron20;
        default: return ElementType::Tetrahedron4;
    }
}

int32_t GmshFormatHandler::element_type_to_gmsh(fea::ElementType type) {
    switch (type) {
        case ElementType::Tetrahedron4: return 4;
        case ElementType::Tetrahedron10: return 10;
        case ElementType::Hexahedron8: return 5;
        case ElementType::Hexahedron20: return 12;
        default: return 0;
    }
}

bool GmshFormatHandler::can_handle_file(const std::string& filename) const {
    return filename.size() >= 4 && filename.substr(filename.size() - 4) == ".msh";
}

// ============================================================================
// STLFormatHandler Implementation
// ============================================================================

std::unique_ptr<FiniteElementMesh> STLFormatHandler::import(
    const std::string& filename,
    const IOConfig& config) {

    // Detect binary or ASCII
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) return nullptr;

    // Check if binary
    char header[80];
    file.read(header, 80);

    uint32_t num_triangles;
    file.read(reinterpret_cast<char*>(&num_triangles), sizeof(uint32_t));

    file.close();

    if (num_triangles > 0 && num_triangles < 1000000) {
        return read_binary_stl(filename);
    } else {
        return read_ascii_stl(filename);
    }
}

bool STLFormatHandler::export_mesh(
    const FiniteElementMesh& mesh,
    const std::string& filename,
    const IOConfig& config) {

    if (config.binary_format) {
        return write_binary_stl(mesh, filename);
    } else {
        return write_ascii_stl(mesh, filename);
    }
}

ValidationResult STLFormatHandler::validate(
    const FiniteElementMesh& mesh) {

    ValidationResult result;
    result.is_valid = true;
    result.total_checks = 2;
    result.failed_checks = 0;

    if (mesh.num_nodes() < 3) {
        result.is_valid = false;
        result.failed_checks++;
    }

    if (mesh.num_elements() == 0) {
        result.is_valid = false;
        result.failed_checks++;
    }

    return result;
}

std::unique_ptr<FiniteElementMesh> STLFormatHandler::read_binary_stl(
    const std::string& filename) {

    auto mesh = std::make_unique<FiniteElementMesh>();

    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) return nullptr;

    char header[80];
    file.read(header, 80);

    uint32_t num_triangles;
    file.read(reinterpret_cast<char*>(&num_triangles), sizeof(uint32_t));

    int32_t node_id = 1;
    int32_t elem_id = 1;

    for (uint32_t i = 0; i < num_triangles; ++i) {
        float normal[3];
        file.read(reinterpret_cast<char*>(normal), sizeof(float) * 3);

        float vertices[9];
        file.read(reinterpret_cast<char*>(vertices), sizeof(float) * 9);

        uint16_t attribute;
        file.read(reinterpret_cast<char*>(&attribute), sizeof(uint16_t));

        // Add vertices
        int32_t n1 = node_id++;
        int32_t n2 = node_id++;
        int32_t n3 = node_id++;

        mesh->add_node(n1, vertices[0], vertices[1], vertices[2]);
        mesh->add_node(n2, vertices[3], vertices[4], vertices[5]);
        mesh->add_node(n3, vertices[6], vertices[7], vertices[8]);

        // Add triangle element
        mesh->add_element(elem_id++, ElementType::Tetrahedron4, {n1, n2, n3, n1});
    }

    file.close();
    return mesh;
}

std::unique_ptr<FiniteElementMesh> STLFormatHandler::read_ascii_stl(
    const std::string& filename) {

    auto mesh = std::make_unique<FiniteElementMesh>();

    std::ifstream file(filename);
    if (!file.is_open()) return nullptr;

    std::string line;
    int32_t node_id = 1;
    int32_t elem_id = 1;

    while (std::getline(file, line)) {
        if (line.find("facet normal") != std::string::npos) {
            // Read vertices
            float vertices[9];
            for (int i = 0; i < 3; ++i) {
                std::getline(file, line);  // outer loop

                for (int j = 0; j < 3; ++j) {
                    std::getline(file, line);

                    if (line.find("vertex") != std::string::npos) {
                        std::istringstream iss(line);
                        std::string vertex_str;
                        iss >> vertex_str;
                        iss >> vertices[i * 3 + j];
                    }
                }
            }

            // Add nodes
            int32_t n1 = node_id++;
            int32_t n2 = node_id++;
            int32_t n3 = node_id++;

            mesh->add_node(n1, vertices[0], vertices[1], vertices[2]);
            mesh->add_node(n2, vertices[3], vertices[4], vertices[5]);
            mesh->add_node(n3, vertices[6], vertices[7], vertices[8]);

            mesh->add_element(elem_id++, ElementType::Tetrahedron4, {n1, n2, n3, n1});
        }
    }

    file.close();
    return mesh;
}

bool STLFormatHandler::write_binary_stl(
    const FiniteElementMesh& mesh,
    const std::string& filename) {

    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open()) return false;

    // Write header
    char header[80] = {};
    std::strncpy(header, "BendCore STL Export", 79);
    file.write(header, 80);

    // Write number of triangles (simplified: each element is one triangle)
    uint32_t num_triangles = mesh.num_elements();
    file.write(reinterpret_cast<const char*>(&num_triangles), sizeof(uint32_t));

    // Write triangles
    for (const auto& elem : mesh.get_elements()) {
        // Write normal (simplified: all zeros)
        float normal[3] = {0, 0, 0};
        file.write(reinterpret_cast<const char*>(normal), sizeof(float) * 3);

        // Write vertices (simplified)
        float vertices[9] = {0};
        if (elem.node_ids.size() >= 3) {
            auto n1 = mesh.get_node(elem.node_ids[0]);
            auto n2 = mesh.get_node(elem.node_ids[1]);
            auto n3 = mesh.get_node(elem.node_ids[2]);

            if (n1 && n2 && n3) {
                vertices[0] = static_cast<float>(n1->position[0]);
                vertices[1] = static_cast<float>(n1->position[1]);
                vertices[2] = static_cast<float>(n1->position[2]);
                vertices[3] = static_cast<float>(n2->position[0]);
                vertices[4] = static_cast<float>(n2->position[1]);
                vertices[5] = static_cast<float>(n2->position[2]);
                vertices[6] = static_cast<float>(n3->position[0]);
                vertices[7] = static_cast<float>(n3->position[1]);
                vertices[8] = static_cast<float>(n3->position[2]);
            }
        }

        file.write(reinterpret_cast<const char*>(vertices), sizeof(float) * 9);

        // Write attribute
        uint16_t attribute = 0;
        file.write(reinterpret_cast<const char*>(&attribute), sizeof(uint16_t));
    }

    file.close();
    return true;
}

bool STLFormatHandler::write_ascii_stl(
    const FiniteElementMesh& mesh,
    const std::string& filename) {

    std::ofstream file(filename);
    if (!file.is_open()) return false;

    file << "solid BendCore_Export\n";

    for (const auto& elem : mesh.get_elements()) {
        if (elem.node_ids.size() < 3) continue;

        auto n1 = mesh.get_node(elem.node_ids[0]);
        auto n2 = mesh.get_node(elem.node_ids[1]);
        auto n3 = mesh.get_node(elem.node_ids[2]);

        if (!n1 || !n2 || !n3) continue;

        file << "  facet normal 0 0 0\n";
        file << "    outer loop\n";
        file << "      vertex " << n1->position[0] << " " << n1->position[1]
             << " " << n1->position[2] << "\n";
        file << "      vertex " << n2->position[0] << " " << n2->position[1]
             << " " << n2->position[2] << "\n";
        file << "      vertex " << n3->position[0] << " " << n3->position[1]
             << " " << n3->position[2] << "\n";
        file << "    endloop\n";
        file << "  endfacet\n";
    }

    file << "endsolid BendCore_Export\n";

    file.close();
    return true;
}

bool STLFormatHandler::can_handle_file(const std::string& filename) const {
    return filename.size() >= 4 &&
           (filename.substr(filename.size() - 4) == ".stl");
}

// ============================================================================
// VTKFormatHandler Implementation
// ============================================================================

std::unique_ptr<FiniteElementMesh> VTKFormatHandler::import(
    const std::string& filename,
    const IOConfig& config) {

    auto mesh = std::make_unique<FiniteElementMesh>();

    std::ifstream file(filename);
    if (!file.is_open()) return nullptr;

    std::string line;
    while (std::getline(file, line)) {
        if (line.find("POINTS") != std::string::npos) {
            std::istringstream iss(line);
            std::string points_str;
            int32_t num_points;
            iss >> points_str >> num_points >> points_str;

            for (int32_t i = 0; i < num_points; ++i) {
                std::getline(file, line);
                std::istringstream node_iss(line);
                double x, y, z;
                node_iss >> x >> y >> z;
                mesh->add_node(i + 1, x, y, z);
            }
        }
        else if (line.find("CELLS") != std::string::npos) {
            std::istringstream iss(line);
            std::string cells_str;
            int32_t num_cells;
            iss >> cells_str >> num_cells;

            for (int32_t i = 0; i < num_cells; ++i) {
                std::getline(file, line);
                std::istringstream cell_iss(line);
                int32_t num_nodes;
                cell_iss >> num_nodes;

                std::vector<int32_t> node_ids;
                for (int32_t j = 0; j < num_nodes; ++j) {
                    int32_t node_id;
                    cell_iss >> node_id;
                    node_ids.push_back(node_id + 1);
                }

                ElementType type = num_nodes == 4 ? ElementType::Tetrahedron4
                                                  : ElementType::Hexahedron8;
                mesh->add_element(i + 1, type, node_ids);
            }
        }
    }

    file.close();

    if (config.validate_on_import && !mesh->validate()) {
        return nullptr;
    }

    return mesh;
}

bool VTKFormatHandler::export_mesh(
    const FiniteElementMesh& mesh,
    const std::string& filename,
    const IOConfig& config) {

    std::ofstream file(filename);
    if (!file.is_open()) return false;

    file << "# vtk DataFile Version 2.0\n";
    file << "BendCore FEA Mesh\n";
    file << "ASCII\n";
    file << "DATASET UNSTRUCTURED_GRID\n";

    write_vtk_header(file, mesh.num_nodes(), mesh.num_elements());

    file << "POINTS " << mesh.num_nodes() << " float\n";
    for (const auto& node : mesh.get_nodes()) {
        file << node.position[0] << " " << node.position[1] << " "
             << node.position[2] << "\n";
    }

    file << "CELLS " << mesh.num_elements() << " "
         << (mesh.num_elements() * 5) << "\n";

    for (const auto& elem : mesh.get_elements()) {
        file << elem.node_ids.size();
        for (int32_t node_id : elem.node_ids) {
            file << " " << (node_id - 1);
        }
        file << "\n";
    }

    file << "CELL_TYPES " << mesh.num_elements() << "\n";
    for (const auto& elem : mesh.get_elements()) {
        file << element_type_to_vtk(elem.type) << "\n";
    }

    file.close();
    return true;
}

ValidationResult VTKFormatHandler::validate(
    const FiniteElementMesh& mesh) {

    ValidationResult result;
    result.is_valid = mesh.validate();
    result.total_checks = 1;
    result.failed_checks = result.is_valid ? 0 : 1;

    return result;
}

fea::ElementType VTKFormatHandler::vtk_to_element_type(int32_t vtk_type) {
    switch (vtk_type) {
        case 10: return ElementType::Tetrahedron4;
        case 12: return ElementType::Hexahedron8;
        default: return ElementType::Tetrahedron4;
    }
}

int32_t VTKFormatHandler::element_type_to_vtk(fea::ElementType type) {
    switch (type) {
        case ElementType::Tetrahedron4: return 10;
        case ElementType::Tetrahedron10: return 24;
        case ElementType::Hexahedron8: return 12;
        case ElementType::Hexahedron20: return 25;
        default: return 0;
    }
}

void VTKFormatHandler::write_vtk_header(
    std::ofstream& file,
    int32_t num_nodes,
    int32_t num_elements) {

    // Header already written in export_mesh
}

bool VTKFormatHandler::can_handle_file(const std::string& filename) const {
    return filename.size() >= 4 &&
           (filename.substr(filename.size() - 4) == ".vtu" ||
            filename.substr(filename.size() - 4) == ".vtk");
}

// ============================================================================
// FormatConverter Implementation
// ============================================================================

bool FormatConverter::convert_format(
    const std::string& input_file,
    const std::string& output_file,
    MeshFormat output_format,
    const IOConfig& config) {

    // Detect input format
    MeshFormat input_format = detect_format(input_file);

    // Import mesh
    auto input_handler = get_handler(input_format);
    if (!input_handler) return false;

    auto mesh = input_handler->import(input_file, config);
    if (!mesh) return false;

    // Normalize mesh
    normalize_mesh(*mesh);

    // Export mesh
    auto output_handler = get_handler(output_format);
    if (!output_handler) return false;

    return output_handler->export_mesh(*mesh, output_file, config);
}

MeshFormat FormatConverter::detect_format(const std::string& filename) {
    std::string ext = filename.substr(filename.find_last_of('.'));

    if (ext == ".msh") return MeshFormat::Gmsh;
    else if (ext == ".stl") return MeshFormat::STL;
    else if (ext == ".vtu" || ext == ".vtk") return MeshFormat::VTK;

    return MeshFormat::Gmsh;  // Default
}

std::unique_ptr<MeshFormatHandler> FormatConverter::get_handler(MeshFormat format) {
    switch (format) {
        case MeshFormat::Gmsh:
            return std::make_unique<GmshFormatHandler>();
        case MeshFormat::STL:
            return std::make_unique<STLFormatHandler>();
        case MeshFormat::VTK:
            return std::make_unique<VTKFormatHandler>();
        default:
            return std::make_unique<GmshFormatHandler>();
    }
}

void FormatConverter::normalize_mesh(FiniteElementMesh& mesh) {
    // Ensure mesh is valid and consistent
    // Simplified: would ensure proper element orientation, node ordering, etc.
}

// ============================================================================
// MeshIO Implementation
// ============================================================================

MeshIO::MeshIO() {
    register_handlers();
}

std::unique_ptr<FiniteElementMesh> MeshIO::import_mesh(
    const std::string& filename,
    const IOConfig& config) {

    MeshFormat format = FormatConverter::detect_format(filename);
    return import_mesh(filename, format, config);
}

std::unique_ptr<FiniteElementMesh> MeshIO::import_mesh(
    const std::string& filename,
    MeshFormat format,
    const IOConfig& config) {

    auto handler = FormatConverter::get_handler(format);
    if (!handler || !handler->can_handle_file(filename)) {
        return nullptr;
    }

    return handler->import(filename, config);
}

bool MeshIO::export_mesh(
    const FiniteElementMesh& mesh,
    const std::string& filename,
    const IOConfig& config) {

    MeshFormat format = FormatConverter::detect_format(filename);
    return export_mesh(mesh, filename, format, config);
}

bool MeshIO::export_mesh(
    const FiniteElementMesh& mesh,
    const std::string& filename,
    MeshFormat format,
    const IOConfig& config) {

    auto handler = FormatConverter::get_handler(format);
    if (!handler) return false;

    return handler->export_mesh(mesh, filename, config);
}

ValidationResult MeshIO::validate_import(
    const std::string& filename,
    const IOConfig& config) {

    auto mesh = import_mesh(filename, config);

    ValidationResult result;
    result.is_valid = mesh != nullptr;
    result.total_checks = 1;
    result.failed_checks = result.is_valid ? 0 : 1;

    return result;
}

std::vector<std::string> MeshIO::get_supported_formats() {
    return {"Gmsh (.msh)", "STL (.stl)", "VTK (.vtu, .vtk)"};
}

void MeshIO::register_handlers() {
    handlers_.push_back(std::make_unique<GmshFormatHandler>());
    handlers_.push_back(std::make_unique<STLFormatHandler>());
    handlers_.push_back(std::make_unique<VTKFormatHandler>());
}

}  // namespace bendcore::mesh_optimization
