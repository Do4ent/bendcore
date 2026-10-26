// FEA Element Library - Implementation
// M6 Phase: FEA Integration (GAP-025)
#include "bendcore/fea/element_library.hpp"
#include "bendcore/vec3.hpp"
#include <cmath>
#include <algorithm>

using namespace bendcore;

namespace bendcore::fea {

// ============================================================================
// Tetrahedron4 Implementation
// ============================================================================

std::vector<GaussPoint> Tetrahedron4::get_gauss_points() const {
    return {{Vec3{0.25, 0.25, 0.25}, 1.0}};
}

ShapeFunctionValues Tetrahedron4::evaluate_shape_functions(const Vec3& xi) const {
    ShapeFunctionValues result;
    result.N.resize(4);
    result.dN_dxi.resize(4);

    double eta1 = xi[0], eta2 = xi[1], eta3 = xi[2];
    double eta4 = 1.0 - eta1 - eta2 - eta3;

    result.N[0] = eta4;
    result.N[1] = eta1;
    result.N[2] = eta2;
    result.N[3] = eta3;

    result.dN_dxi[0] = Vec3{-1.0, -1.0, -1.0};
    result.dN_dxi[1] = Vec3{1.0, 0.0, 0.0};
    result.dN_dxi[2] = Vec3{0.0, 1.0, 0.0};
    result.dN_dxi[3] = Vec3{0.0, 0.0, 1.0};

    return result;
}

std::array<std::array<double, 3>, 3> Tetrahedron4::compute_jacobian(
    const Vec3& natural_coords, const std::vector<Vec3>& node_positions) const {

    std::array<std::array<double, 3>, 3> J{};

    if (node_positions.size() < 4) return J;

    Vec3 v1 = sub(node_positions[1], node_positions[0]);
    Vec3 v2 = sub(node_positions[2], node_positions[0]);
    Vec3 v3 = sub(node_positions[3], node_positions[0]);

    J[0][0] = v1[0]; J[0][1] = v2[0]; J[0][2] = v3[0];
    J[1][0] = v1[1]; J[1][1] = v2[1]; J[1][2] = v3[1];
    J[2][0] = v1[2]; J[2][1] = v2[2]; J[2][2] = v3[2];

    return J;
}

double Tetrahedron4::compute_jacobian_determinant(
    const Vec3& natural_coords, const std::vector<Vec3>& node_positions) const {

    if (node_positions.size() < 4) return 0.0;

    Vec3 v1 = sub(node_positions[1], node_positions[0]);
    Vec3 v2 = sub(node_positions[2], node_positions[0]);
    Vec3 v3 = sub(node_positions[3], node_positions[0]);

    double det = v1[0] * (v2[1] * v3[2] - v2[2] * v3[1])
               - v1[1] * (v2[0] * v3[2] - v2[2] * v3[0])
               + v1[2] * (v2[0] * v3[1] - v2[1] * v3[0]);
    return det;
}

double Tetrahedron4::compute_volume(const std::vector<Vec3>& node_positions) const {
    if (node_positions.size() < 4) return 0.0;

    double det = compute_jacobian_determinant(Vec3{}, node_positions);
    return std::abs(det) / 6.0;
}

Vec3 Tetrahedron4::get_node_natural_coords(int32_t node_index) const {
    switch (node_index) {
        case 0: return Vec3{0.0, 0.0, 0.0};
        case 1: return Vec3{1.0, 0.0, 0.0};
        case 2: return Vec3{0.0, 1.0, 0.0};
        case 3: return Vec3{0.0, 0.0, 1.0};
        default: return Vec3{};
    }
}

bool Tetrahedron4::is_point_inside(const Vec3& point, const std::vector<Vec3>& node_positions, Vec3& natural_coords) const {
    return false;
}

// ============================================================================
// Tetrahedron10 Implementation
// ============================================================================

std::vector<GaussPoint> Tetrahedron10::get_gauss_points() const {
    double a = 0.58541019662496847;
    double b = 0.13819660112501051;
    return {
        {Vec3{a, b, b}, 0.25},
        {Vec3{b, a, b}, 0.25},
        {Vec3{b, b, a}, 0.25},
        {Vec3{b, b, b}, 0.25}
    };
}

ShapeFunctionValues Tetrahedron10::evaluate_shape_functions(const Vec3& xi) const {
    ShapeFunctionValues result;
    result.N.resize(10);
    result.dN_dxi.resize(10);

    double eta1 = xi[0], eta2 = xi[1], eta3 = xi[2];
    double eta4 = 1.0 - eta1 - eta2 - eta3;

    result.N[0] = eta4 * (2.0 * eta4 - 1.0);
    result.N[1] = eta1 * (2.0 * eta1 - 1.0);
    result.N[2] = eta2 * (2.0 * eta2 - 1.0);
    result.N[3] = eta3 * (2.0 * eta3 - 1.0);
    result.N[4] = 4.0 * eta4 * eta1;
    result.N[5] = 4.0 * eta1 * eta2;
    result.N[6] = 4.0 * eta2 * eta4;
    result.N[7] = 4.0 * eta4 * eta3;
    result.N[8] = 4.0 * eta1 * eta3;
    result.N[9] = 4.0 * eta2 * eta3;

    return result;
}

std::array<std::array<double, 3>, 3> Tetrahedron10::compute_jacobian(
    const Vec3& natural_coords, const std::vector<Vec3>& node_positions) const {

    std::array<std::array<double, 3>, 3> J{};
    if (node_positions.size() < 10) return J;

    auto N = evaluate_shape_functions(natural_coords);

    for (int i = 0; i < 10; ++i) {
        J[0][0] += N.dN_dxi[i][0] * node_positions[i][0];
        J[0][1] += N.dN_dxi[i][1] * node_positions[i][0];
        J[0][2] += N.dN_dxi[i][2] * node_positions[i][0];
        J[1][0] += N.dN_dxi[i][0] * node_positions[i][1];
        J[1][1] += N.dN_dxi[i][1] * node_positions[i][1];
        J[1][2] += N.dN_dxi[i][2] * node_positions[i][1];
        J[2][0] += N.dN_dxi[i][0] * node_positions[i][2];
        J[2][1] += N.dN_dxi[i][1] * node_positions[i][2];
        J[2][2] += N.dN_dxi[i][2] * node_positions[i][2];
    }

    return J;
}

double Tetrahedron10::compute_jacobian_determinant(
    const Vec3& natural_coords, const std::vector<Vec3>& node_positions) const {

    if (node_positions.size() < 10) return 0.0;

    auto J = compute_jacobian(natural_coords, node_positions);

    double det = J[0][0] * (J[1][1] * J[2][2] - J[1][2] * J[2][1])
               - J[0][1] * (J[1][0] * J[2][2] - J[1][2] * J[2][0])
               + J[0][2] * (J[1][0] * J[2][1] - J[1][1] * J[2][0]);
    return det;
}

double Tetrahedron10::compute_volume(const std::vector<Vec3>& node_positions) const {
    double volume = 0.0;
    auto gp = get_gauss_points();

    for (const auto& point : gp) {
        double det = compute_jacobian_determinant(point.natural_coords, node_positions);
        volume += std::abs(det) * point.weight;
    }

    return volume / 6.0;
}

Vec3 Tetrahedron10::get_node_natural_coords(int32_t node_index) const {
    switch (node_index) {
        case 0: return Vec3{0.0, 0.0, 0.0};
        case 1: return Vec3{1.0, 0.0, 0.0};
        case 2: return Vec3{0.0, 1.0, 0.0};
        case 3: return Vec3{0.0, 0.0, 1.0};
        case 4: return Vec3{0.5, 0.0, 0.0};
        case 5: return Vec3{0.5, 0.5, 0.0};
        case 6: return Vec3{0.0, 0.5, 0.0};
        case 7: return Vec3{0.0, 0.0, 0.5};
        case 8: return Vec3{0.5, 0.0, 0.5};
        case 9: return Vec3{0.0, 0.5, 0.5};
        default: return Vec3{};
    }
}

bool Tetrahedron10::is_point_inside(const Vec3& point, const std::vector<Vec3>& node_positions, Vec3& natural_coords) const {
    return false;
}

// ============================================================================
// Hexahedron8 Implementation
// ============================================================================

std::vector<GaussPoint> Hexahedron8::get_gauss_points() const {
    double a = 1.0 / std::sqrt(3.0);
    std::vector<GaussPoint> points;

    double xi_coords[] = {-a, a};

    for (double xi : xi_coords) {
        for (double eta : xi_coords) {
            for (double zeta : xi_coords) {
                points.push_back({Vec3{xi, eta, zeta}, 1.0});
            }
        }
    }

    return points;
}

ShapeFunctionValues Hexahedron8::evaluate_shape_functions(const Vec3& xi) const {
    ShapeFunctionValues result;
    result.N.resize(8);
    result.dN_dxi.resize(8);

    double x = xi[0], y = xi[1], z = xi[2];

    result.N[0] = (1.0 - x) * (1.0 - y) * (1.0 - z) / 8.0;
    result.N[1] = (1.0 + x) * (1.0 - y) * (1.0 - z) / 8.0;
    result.N[2] = (1.0 + x) * (1.0 + y) * (1.0 - z) / 8.0;
    result.N[3] = (1.0 - x) * (1.0 + y) * (1.0 - z) / 8.0;
    result.N[4] = (1.0 - x) * (1.0 - y) * (1.0 + z) / 8.0;
    result.N[5] = (1.0 + x) * (1.0 - y) * (1.0 + z) / 8.0;
    result.N[6] = (1.0 + x) * (1.0 + y) * (1.0 + z) / 8.0;
    result.N[7] = (1.0 - x) * (1.0 + y) * (1.0 + z) / 8.0;

    return result;
}

std::array<std::array<double, 3>, 3> Hexahedron8::compute_jacobian(
    const Vec3& natural_coords, const std::vector<Vec3>& node_positions) const {

    std::array<std::array<double, 3>, 3> J{};
    if (node_positions.size() < 8) return J;

    auto N = evaluate_shape_functions(natural_coords);

    for (int i = 0; i < 8; ++i) {
        J[0][0] += N.dN_dxi[i][0] * node_positions[i][0];
        J[0][1] += N.dN_dxi[i][1] * node_positions[i][0];
        J[0][2] += N.dN_dxi[i][2] * node_positions[i][0];
        J[1][0] += N.dN_dxi[i][0] * node_positions[i][1];
        J[1][1] += N.dN_dxi[i][1] * node_positions[i][1];
        J[1][2] += N.dN_dxi[i][2] * node_positions[i][1];
        J[2][0] += N.dN_dxi[i][0] * node_positions[i][2];
        J[2][1] += N.dN_dxi[i][1] * node_positions[i][2];
        J[2][2] += N.dN_dxi[i][2] * node_positions[i][2];
    }

    return J;
}

double Hexahedron8::compute_jacobian_determinant(
    const Vec3& natural_coords, const std::vector<Vec3>& node_positions) const {

    auto J = compute_jacobian(natural_coords, node_positions);

    double det = J[0][0] * (J[1][1] * J[2][2] - J[1][2] * J[2][1])
               - J[0][1] * (J[1][0] * J[2][2] - J[1][2] * J[2][0])
               + J[0][2] * (J[1][0] * J[2][1] - J[1][1] * J[2][0]);
    return det;
}

double Hexahedron8::compute_volume(const std::vector<Vec3>& node_positions) const {
    double volume = 0.0;
    auto gp = get_gauss_points();

    for (const auto& point : gp) {
        double det = compute_jacobian_determinant(point.natural_coords, node_positions);
        volume += std::abs(det) * point.weight;
    }

    return volume;
}

Vec3 Hexahedron8::get_node_natural_coords(int32_t node_index) const {
    switch (node_index) {
        case 0: return Vec3{-1.0, -1.0, -1.0};
        case 1: return Vec3{1.0, -1.0, -1.0};
        case 2: return Vec3{1.0, 1.0, -1.0};
        case 3: return Vec3{-1.0, 1.0, -1.0};
        case 4: return Vec3{-1.0, -1.0, 1.0};
        case 5: return Vec3{1.0, -1.0, 1.0};
        case 6: return Vec3{1.0, 1.0, 1.0};
        case 7: return Vec3{-1.0, 1.0, 1.0};
        default: return Vec3{};
    }
}

bool Hexahedron8::is_point_inside(const Vec3& point, const std::vector<Vec3>& node_positions, Vec3& natural_coords) const {
    return false;
}

// ============================================================================
// Hexahedron20 Implementation
// ============================================================================

std::vector<GaussPoint> Hexahedron20::get_gauss_points() const {
    double a = std::sqrt(3.0 / 5.0);
    std::vector<GaussPoint> points;

    double xi_coords[] = {-a, 0.0, a};
    double weights[] = {5.0 / 9.0, 8.0 / 9.0, 5.0 / 9.0};

    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            for (int k = 0; k < 3; ++k) {
                points.push_back({
                    Vec3{xi_coords[i], xi_coords[j], xi_coords[k]},
                    weights[i] * weights[j] * weights[k]
                });
            }
        }
    }

    return points;
}

ShapeFunctionValues Hexahedron20::evaluate_shape_functions(const Vec3& xi) const {
    ShapeFunctionValues result;
    result.N.resize(20);
    result.dN_dxi.resize(20);
    return result;
}

std::array<std::array<double, 3>, 3> Hexahedron20::compute_jacobian(
    const Vec3& natural_coords, const std::vector<Vec3>& node_positions) const {

    std::array<std::array<double, 3>, 3> J{};
    return J;
}

double Hexahedron20::compute_jacobian_determinant(
    const Vec3& natural_coords, const std::vector<Vec3>& node_positions) const {
    return 1.0;
}

double Hexahedron20::compute_volume(const std::vector<Vec3>& node_positions) const {
    return 0.0;
}

Vec3 Hexahedron20::get_node_natural_coords(int32_t node_index) const {
    return Vec3{};
}

bool Hexahedron20::is_point_inside(const Vec3& point, const std::vector<Vec3>& node_positions, Vec3& natural_coords) const {
    return false;
}

// ============================================================================
// ElementFactory Implementation
// ============================================================================

std::unique_ptr<ElementDefinition> ElementFactory::create_element(ElementType type) {
    switch (type) {
        case ElementType::Tetrahedron4: return std::make_unique<Tetrahedron4>();
        case ElementType::Tetrahedron10: return std::make_unique<Tetrahedron10>();
        case ElementType::Hexahedron8: return std::make_unique<Hexahedron8>();
        case ElementType::Hexahedron20: return std::make_unique<Hexahedron20>();
        default: return nullptr;
    }
}

std::unique_ptr<ElementDefinition> ElementFactory::create_element_4node_tet() {
    return std::make_unique<Tetrahedron4>();
}

std::unique_ptr<ElementDefinition> ElementFactory::create_element_10node_tet() {
    return std::make_unique<Tetrahedron10>();
}

std::unique_ptr<ElementDefinition> ElementFactory::create_element_8node_hex() {
    return std::make_unique<Hexahedron8>();
}

std::unique_ptr<ElementDefinition> ElementFactory::create_element_20node_hex() {
    return std::make_unique<Hexahedron20>();
}

// ============================================================================
// Stiffness Matrix and Stress Computation
// ============================================================================

std::vector<std::vector<double>> StiffnessMatrixComputer::compute_element_stiffness(
    const Element& elem, const FiniteElementMesh& mesh,
    double youngs_modulus, double poissons_ratio) {

    int32_t num_nodes = mesh.get_num_nodes_for_element(elem.type);
    int32_t num_dofs = num_nodes * 3;

    std::vector<std::vector<double>> K_local(num_dofs, std::vector<double>(num_dofs, 0.0));
    return K_local;
}

std::vector<std::vector<double>> StiffnessMatrixComputer::compute_element_mass(
    const Element& elem, const FiniteElementMesh& mesh, double density) {

    int32_t num_nodes = mesh.get_num_nodes_for_element(elem.type);
    int32_t num_dofs = num_nodes * 3;

    std::vector<std::vector<double>> M_local(num_dofs, std::vector<double>(num_dofs, 0.0));
    return M_local;
}

std::vector<std::vector<double>> StiffnessMatrixComputer::expand_to_global(
    const std::vector<std::vector<double>>& local_matrix, int32_t num_dofs_per_node) {

    return local_matrix;
}

// ============================================================================
// Strain and Stress Computation
// ============================================================================

std::array<std::array<double, 3>, 3> StrainStressComputer::compute_strain_tensor(
    const std::vector<Vec3>& displacements,
    const ElementDefinition& elem_def,
    const std::vector<Vec3>& node_positions,
    const Vec3& natural_coords) {

    std::array<std::array<double, 3>, 3> strain{};
    return strain;
}

std::array<std::array<double, 3>, 3> StrainStressComputer::compute_stress_tensor(
    const std::array<std::array<double, 3>, 3>& strain,
    double youngs_modulus, double poissons_ratio) {

    std::array<std::array<double, 3>, 3> stress{};

    double E = youngs_modulus;
    double nu = poissons_ratio;
    double factor = E / ((1.0 + nu) * (1.0 - 2.0 * nu));
    double G = E / (2.0 * (1.0 + nu));

    double trace = strain[0][0] + strain[1][1] + strain[2][2];

    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            stress[i][j] = 2.0 * G * strain[i][j];
            if (i == j) stress[i][j] += (E * nu / (1.0 - 2.0 * nu)) * trace;
        }
    }

    return stress;
}

double StrainStressComputer::compute_von_mises_stress(
    const std::array<std::array<double, 3>, 3>& stress) {

    double s11 = stress[0][0], s22 = stress[1][1], s33 = stress[2][2];
    double s12 = stress[0][1], s23 = stress[1][2], s31 = stress[2][0];

    double sigma_vm = std::sqrt(
        0.5 * ((s11 - s22) * (s11 - s22) + (s22 - s33) * (s22 - s33) + (s33 - s11) * (s33 - s11))
        + 3.0 * (s12 * s12 + s23 * s23 + s31 * s31)
    );

    return sigma_vm;
}

std::array<double, 3> StrainStressComputer::compute_principal_stresses(
    const std::array<std::array<double, 3>, 3>& stress) {

    std::array<double, 3> principal{};
    principal[0] = stress[0][0];
    principal[1] = stress[1][1];
    principal[2] = stress[2][2];

    std::sort(principal.begin(), principal.end(), std::greater<double>());

    return principal;
}

}  // namespace bendcore::fea
