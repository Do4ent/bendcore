// FEA Mesh Generation - Implementation
// M6 Phase: FEA Integration (GAP-025)
#include "bendcore/fea/mesh_generator.hpp"
#include "bendcore/vec3.hpp"
#include <cmath>
#include <algorithm>

using namespace bendcore;

namespace bendcore::fea {

// ============================================================================
// FiniteElementMesh Implementation
// ============================================================================

FiniteElementMesh::FiniteElementMesh() = default;
FiniteElementMesh::~FiniteElementMesh() = default;

void FiniteElementMesh::add_node(int32_t id, const Vec3& position, bool is_boundary) {
    nodes_.push_back({id, position, is_boundary, 0});
}

void FiniteElementMesh::add_node(int32_t id, double x, double y, double z, bool is_boundary) {
    add_node(id, Vec3{x, y, z}, is_boundary);
}

Node* FiniteElementMesh::get_node(int32_t id) {
    for (auto& node : nodes_) {
        if (node.id == id) return &node;
    }
    return nullptr;
}

const Node* FiniteElementMesh::get_node(int32_t id) const {
    for (const auto& node : nodes_) {
        if (node.id == id) return &node;
    }
    return nullptr;
}

void FiniteElementMesh::add_element(int32_t id, ElementType type, const std::vector<int32_t>& node_ids, int32_t material_id) {
    elements_.push_back({id, type, node_ids, material_id});
}

Element* FiniteElementMesh::get_element(int32_t id) {
    for (auto& elem : elements_) {
        if (elem.id == id) return &elem;
    }
    return nullptr;
}

const Element* FiniteElementMesh::get_element(int32_t id) const {
    for (const auto& elem : elements_) {
        if (elem.id == id) return &elem;
    }
    return nullptr;
}

int32_t FiniteElementMesh::get_num_nodes_for_element(ElementType type) const {
    switch (type) {
        case ElementType::Tetrahedron4: return 4;
        case ElementType::Tetrahedron10: return 10;
        case ElementType::Hexahedron8: return 8;
        case ElementType::Hexahedron20: return 20;
        default: return 0;
    }
}

int32_t FiniteElementMesh::get_num_gauss_points(ElementType type) const {
    switch (type) {
        case ElementType::Tetrahedron4: return 1;
        case ElementType::Tetrahedron10: return 4;
        case ElementType::Hexahedron8: return 8;
        case ElementType::Hexahedron20: return 27;
        default: return 0;
    }
}

void FiniteElementMesh::mark_boundary_node(int32_t node_id, int32_t marker) {
    for (auto& node : nodes_) {
        if (node.id == node_id) {
            node.is_boundary = true;
            node.boundary_marker = marker;
            break;
        }
    }
    boundary_nodes_.push_back(node_id);
}

bool FiniteElementMesh::validate() const {
    for (const auto& elem : elements_) {
        for (int32_t node_id : elem.node_ids) {
            if (get_node(node_id) == nullptr) return false;
        }
    }
    return true;
}

MeshQuality FiniteElementMesh::compute_quality() const {
    MeshQuality quality{0.0, 180.0, 0.0, 1.0, 0.0};

    if (elements_.empty()) return quality;

    for (const auto& elem : elements_) {
        double aspect = MeshQualityChecker::compute_aspect_ratio(*this, elem);
        quality.aspect_ratio = std::max(quality.aspect_ratio, aspect);

        double jac = MeshQualityChecker::compute_jacobian(*this, elem);
        quality.jacobian_min = std::min(quality.jacobian_min, jac);

        double skew = MeshQualityChecker::compute_skewness(*this, elem);
        quality.skewness = std::max(quality.skewness, skew);
    }

    return quality;
}

double FiniteElementMesh::compute_element_volume(const Element& elem) const {
    if (elem.type == ElementType::Tetrahedron4 && elem.node_ids.size() == 4) {
        const Node* n0 = get_node(elem.node_ids[0]);
        const Node* n1 = get_node(elem.node_ids[1]);
        const Node* n2 = get_node(elem.node_ids[2]);
        const Node* n3 = get_node(elem.node_ids[3]);

        if (n0 && n1 && n2 && n3) {
            Vec3 v1 = sub(n1->position, n0->position);
            Vec3 v2 = sub(n2->position, n0->position);
            Vec3 v3 = sub(n3->position, n0->position);

            double det = v1[0] * (v2[1] * v3[2] - v2[2] * v3[1])
                       - v1[1] * (v2[0] * v3[2] - v2[2] * v3[0])
                       + v1[2] * (v2[0] * v3[1] - v2[1] * v3[0]);
            return std::abs(det) / 6.0;
        }
    }
    return 0.0;
}

double FiniteElementMesh::compute_element_jacobian(const Element& elem) const {
    return MeshQualityChecker::compute_jacobian(*this, elem);
}

void FiniteElementMesh::clear() {
    nodes_.clear();
    elements_.clear();
    boundary_nodes_.clear();
}

// ============================================================================
// TetrahedralMeshGenerator Implementation
// ============================================================================

TetrahedralMeshGenerator::TetrahedralMeshGenerator() : next_node_id_(1), next_element_id_(1) {}

std::unique_ptr<FiniteElementMesh> TetrahedralMeshGenerator::generate_uniform_mesh(
    const Vec3& min_bounds, const Vec3& max_bounds,
    int32_t divisions_x, int32_t divisions_y, int32_t divisions_z) {

    auto mesh = std::make_unique<FiniteElementMesh>();

    double dx = (max_bounds[0] - min_bounds[0]) / divisions_x;
    double dy = (max_bounds[1] - min_bounds[1]) / divisions_y;
    double dz = (max_bounds[2] - min_bounds[2]) / divisions_z;

    std::vector<std::vector<std::vector<int32_t>>> node_ids(divisions_x + 1,
        std::vector<std::vector<int32_t>>(divisions_y + 1, std::vector<int32_t>(divisions_z + 1)));

    for (int32_t i = 0; i <= divisions_x; ++i) {
        for (int32_t j = 0; j <= divisions_y; ++j) {
            for (int32_t k = 0; k <= divisions_z; ++k) {
                double x = min_bounds[0] + i * dx;
                double y = min_bounds[1] + j * dy;
                double z = min_bounds[2] + k * dz;

                node_ids[i][j][k] = next_node_id_;
                bool is_boundary = (i == 0 || i == divisions_x ||
                                   j == 0 || j == divisions_y ||
                                   k == 0 || k == divisions_z);
                mesh->add_node(next_node_id_++, x, y, z, is_boundary);
            }
        }
    }

    for (int32_t i = 0; i < divisions_x; ++i) {
        for (int32_t j = 0; j < divisions_y; ++j) {
            for (int32_t k = 0; k < divisions_z; ++k) {
                int32_t n000 = node_ids[i][j][k];
                int32_t n100 = node_ids[i+1][j][k];
                int32_t n010 = node_ids[i][j+1][k];
                int32_t n110 = node_ids[i+1][j+1][k];
                int32_t n001 = node_ids[i][j][k+1];
                int32_t n101 = node_ids[i+1][j][k+1];
                int32_t n011 = node_ids[i][j+1][k+1];
                int32_t n111 = node_ids[i+1][j+1][k+1];

                mesh->add_element(next_element_id_++, ElementType::Tetrahedron4, {n000, n100, n010, n001});
                mesh->add_element(next_element_id_++, ElementType::Tetrahedron4, {n100, n110, n010, n101});
                mesh->add_element(next_element_id_++, ElementType::Tetrahedron4, {n010, n110, n011, n101});
                mesh->add_element(next_element_id_++, ElementType::Tetrahedron4, {n001, n101, n011, n111});
                mesh->add_element(next_element_id_++, ElementType::Tetrahedron4, {n100, n110, n101, n111});
            }
        }
    }

    return mesh;
}

std::unique_ptr<FiniteElementMesh> TetrahedralMeshGenerator::generate_adaptive_mesh(
    const Vec3& min_bounds, const Vec3& max_bounds,
    double target_element_size, int32_t max_refinement_levels) {

    Vec3 diff = sub(max_bounds, min_bounds);
    double diag = norm(diff);
    int32_t div = static_cast<int32_t>(std::ceil(diag / target_element_size));
    div = std::max(1, std::min(div, 10));

    return generate_uniform_mesh(min_bounds, max_bounds, div, div, div);
}

std::unique_ptr<FiniteElementMesh> TetrahedralMeshGenerator::generate_from_point_cloud(
    const std::vector<Vec3>& points, double characteristic_length) {

    auto mesh = std::make_unique<FiniteElementMesh>();

    for (size_t i = 0; i < points.size(); ++i) {
        mesh->add_node(static_cast<int32_t>(i) + 1, points[i]);
    }

    return mesh;
}

void TetrahedralMeshGenerator::refine_mesh(FiniteElementMesh& mesh,
    const std::vector<int32_t>& element_ids_to_refine) {}

// ============================================================================
// HexahedralMeshGenerator Implementation
// ============================================================================

HexahedralMeshGenerator::HexahedralMeshGenerator()
    : next_node_id_(1), next_element_id_(1) {}

std::unique_ptr<FiniteElementMesh> HexahedralMeshGenerator::generate_structured_mesh(
    const Vec3& min_bounds, const Vec3& max_bounds,
    int32_t divisions_x, int32_t divisions_y, int32_t divisions_z) {

    auto mesh = std::make_unique<FiniteElementMesh>();

    double dx = (max_bounds[0] - min_bounds[0]) / divisions_x;
    double dy = (max_bounds[1] - min_bounds[1]) / divisions_y;
    double dz = (max_bounds[2] - min_bounds[2]) / divisions_z;

    std::vector<std::vector<std::vector<int32_t>>> node_ids(divisions_x + 1,
        std::vector<std::vector<int32_t>>(divisions_y + 1, std::vector<int32_t>(divisions_z + 1)));

    for (int32_t i = 0; i <= divisions_x; ++i) {
        for (int32_t j = 0; j <= divisions_y; ++j) {
            for (int32_t k = 0; k <= divisions_z; ++k) {
                double x = min_bounds[0] + i * dx;
                double y = min_bounds[1] + j * dy;
                double z = min_bounds[2] + k * dz;

                node_ids[i][j][k] = next_node_id_;
                bool is_boundary = (i == 0 || i == divisions_x ||
                                   j == 0 || j == divisions_y ||
                                   k == 0 || k == divisions_z);
                mesh->add_node(next_node_id_++, x, y, z, is_boundary);
            }
        }
    }

    for (int32_t i = 0; i < divisions_x; ++i) {
        for (int32_t j = 0; j < divisions_y; ++j) {
            for (int32_t k = 0; k < divisions_z; ++k) {
                std::vector<int32_t> hex_nodes{
                    node_ids[i][j][k],
                    node_ids[i+1][j][k],
                    node_ids[i+1][j+1][k],
                    node_ids[i][j+1][k],
                    node_ids[i][j][k+1],
                    node_ids[i+1][j][k+1],
                    node_ids[i+1][j+1][k+1],
                    node_ids[i][j+1][k+1]
                };
                mesh->add_element(next_element_id_++, ElementType::Hexahedron8, hex_nodes);
            }
        }
    }

    return mesh;
}

std::unique_ptr<FiniteElementMesh> HexahedralMeshGenerator::generate_unstructured_mesh(
    const Vec3& min_bounds, const Vec3& max_bounds,
    double target_element_size) {

    Vec3 diff = sub(max_bounds, min_bounds);
    double diag = norm(diff);
    int32_t div = static_cast<int32_t>(std::ceil(diag / target_element_size));
    div = std::max(1, std::min(div, 10));

    return generate_structured_mesh(min_bounds, max_bounds, div, div, div);
}

// ============================================================================
// Mesh Refinement and Quality Checking
// ============================================================================

void MeshRefiner::refine_uniform(FiniteElementMesh& mesh) {}
void MeshRefiner::refine_adaptive(FiniteElementMesh& mesh, const std::vector<double>& element_errors, double refinement_threshold) {}
void MeshRefiner::mark_elements_for_refinement(FiniteElementMesh& mesh, const std::vector<int32_t>& element_ids) {}
void MeshRefiner::coarsen_mesh(FiniteElementMesh& mesh, double size_threshold) {}

bool MeshQualityChecker::check_element_validity(const FiniteElementMesh& mesh, const Element& elem) {
    for (int32_t node_id : elem.node_ids) {
        if (mesh.get_node(node_id) == nullptr) return false;
    }
    double jac = compute_jacobian(mesh, elem);
    return jac > 1e-12;
}

double MeshQualityChecker::compute_aspect_ratio(const FiniteElementMesh& mesh, const Element& elem) {
    if (elem.node_ids.size() < 2) return 1.0;

    double max_edge = 0.0;
    double min_edge = 1e12;

    for (size_t i = 0; i < elem.node_ids.size(); ++i) {
        for (size_t j = i + 1; j < elem.node_ids.size(); ++j) {
            const Node* ni = mesh.get_node(elem.node_ids[i]);
            const Node* nj = mesh.get_node(elem.node_ids[j]);
            if (ni && nj) {
                double len = norm(sub(ni->position, nj->position));
                max_edge = std::max(max_edge, len);
                min_edge = std::min(min_edge, len);
            }
        }
    }

    return (min_edge > 1e-12) ? max_edge / min_edge : 1e12;
}

double MeshQualityChecker::compute_skewness(const FiniteElementMesh& mesh, const Element& elem) {
    return 0.0;
}

double MeshQualityChecker::compute_jacobian(const FiniteElementMesh& mesh, const Element& elem) {
    if (elem.type == ElementType::Tetrahedron4 && elem.node_ids.size() == 4) {
        const Node* n0 = mesh.get_node(elem.node_ids[0]);
        const Node* n1 = mesh.get_node(elem.node_ids[1]);
        const Node* n2 = mesh.get_node(elem.node_ids[2]);
        const Node* n3 = mesh.get_node(elem.node_ids[3]);

        if (n0 && n1 && n2 && n3) {
            Vec3 v1 = sub(n1->position, n0->position);
            Vec3 v2 = sub(n2->position, n0->position);
            Vec3 v3 = sub(n3->position, n0->position);

            double det = v1[0] * (v2[1] * v3[2] - v2[2] * v3[1])
                       - v1[1] * (v2[0] * v3[2] - v2[2] * v3[0])
                       + v1[2] * (v2[0] * v3[1] - v2[1] * v3[0]);
            return det;
        }
    }
    return 0.0;
}

std::vector<double> MeshQualityChecker::compute_dihedral_angles(const FiniteElementMesh& mesh, const Element& elem) {
    std::vector<double> angles;
    return angles;
}

}  // namespace bendcore::fea
