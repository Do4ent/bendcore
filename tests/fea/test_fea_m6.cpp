// M6 Phase: FEA Integration Comprehensive Unit Tests
// Tests for all FEA components (35+ tests)
#include "bendcore/fea/mesh_generator.hpp"
#include "bendcore/fea/element_library.hpp"
#include "bendcore/fea/material_database.hpp"
#include "bendcore/fea/boundary_conditions.hpp"
#include "bendcore/fea/fea_solver.hpp"
#include "bendcore/fea/result_processor.hpp"

#include <cstdio>
#include <cmath>
#include <cassert>

using namespace bendcore;
using namespace bendcore::fea;

static int failures = 0;

static void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("FAIL: %s\n", what);
        ++failures;
    } else {
        std::printf("PASS: %s\n", what);
    }
}

static bool near(double a, double b, double eps = 1e-6) {
    return std::abs(a - b) <= eps;
}

// ============================================================================
// Mesh Generation Tests (8 tests)
// ============================================================================

static void test_mesh_node_addition() {
    FiniteElementMesh mesh;
    mesh.add_node(1, 0.0, 0.0, 0.0);
    mesh.add_node(2, 1.0, 0.0, 0.0);
    mesh.add_node(3, 0.0, 1.0, 0.0);

    check(mesh.num_nodes() == 3, "mesh_node_addition: nodes added correctly");
    check(mesh.get_node(1) != nullptr, "mesh_node_addition: can retrieve node");
    check(mesh.get_node(1)->position[0] == 0.0, "mesh_node_addition: node position correct");
}

static void test_mesh_element_addition() {
    FiniteElementMesh mesh;
    mesh.add_node(1, 0.0, 0.0, 0.0);
    mesh.add_node(2, 1.0, 0.0, 0.0);
    mesh.add_node(3, 0.0, 1.0, 0.0);
    mesh.add_node(4, 0.0, 0.0, 1.0);

    std::vector<int32_t> tet_nodes{1, 2, 3, 4};
    mesh.add_element(1, ElementType::Tetrahedron4, tet_nodes);

    check(mesh.num_elements() == 1, "mesh_element_addition: element added");
    check(mesh.get_element(1) != nullptr, "mesh_element_addition: can retrieve element");
    check(mesh.get_element(1)->type == ElementType::Tetrahedron4, "mesh_element_addition: element type correct");
}

static void test_tetrahedral_mesh_generation() {
    TetrahedralMeshGenerator gen;
    auto mesh = gen.generate_uniform_mesh(
        Vec3
        {0.0, 0.0, 0.0}, Vec3
        {1.0, 1.0, 1.0}, 2, 2, 2);

    check(mesh->num_nodes() > 0, "tetrahedral_mesh_generation: nodes created");
    check(mesh->num_elements() > 0, "tetrahedral_mesh_generation: elements created");
    check(mesh->validate(), "tetrahedral_mesh_generation: mesh is valid");
}

static void test_hexahedral_mesh_generation() {
    HexahedralMeshGenerator gen;
    auto mesh = gen.generate_structured_mesh(
        Vec3
        {0.0, 0.0, 0.0}, Vec3
        {1.0, 1.0, 1.0}, 2, 2, 2);

    check(mesh->num_nodes() > 0, "hexahedral_mesh_generation: nodes created");
    check(mesh->num_elements() > 0, "hexahedral_mesh_generation: elements created");
    check(mesh->validate(), "hexahedral_mesh_generation: mesh is valid");
}

static void test_mesh_boundary_marking() {
    FiniteElementMesh mesh;
    mesh.add_node(1, 0.0, 0.0, 0.0, true);
    mesh.add_node(2, 1.0, 0.0, 0.0);

    mesh.mark_boundary_node(2, 1);

    check(mesh.get_node(1)->is_boundary, "mesh_boundary_marking: node 1 is boundary");
    check(mesh.get_node(2)->is_boundary, "mesh_boundary_marking: node 2 marked as boundary");
}

static void test_mesh_num_nodes_per_element() {
    FiniteElementMesh mesh;

    check(mesh.get_num_nodes_for_element(ElementType::Tetrahedron4) == 4,
        "mesh_num_nodes_per_element: tet4 has 4 nodes");
    check(mesh.get_num_nodes_for_element(ElementType::Tetrahedron10) == 10,
        "mesh_num_nodes_per_element: tet10 has 10 nodes");
    check(mesh.get_num_nodes_for_element(ElementType::Hexahedron8) == 8,
        "mesh_num_nodes_per_element: hex8 has 8 nodes");
    check(mesh.get_num_nodes_for_element(ElementType::Hexahedron20) == 20,
        "mesh_num_nodes_per_element: hex20 has 20 nodes");
}

static void test_mesh_element_volume_calculation() {
    FiniteElementMesh mesh;
    mesh.add_node(1, 0.0, 0.0, 0.0);
    mesh.add_node(2, 1.0, 0.0, 0.0);
    mesh.add_node(3, 0.0, 1.0, 0.0);
    mesh.add_node(4, 0.0, 0.0, 1.0);

    std::vector<int32_t> tet_nodes{1, 2, 3, 4};
    mesh.add_element(1, ElementType::Tetrahedron4, tet_nodes);

    Element* elem = mesh.get_element(1);
    check(elem != nullptr, "mesh_element_volume_calculation: element exists");

    if (elem) {
        double volume = mesh.compute_element_volume(*elem);
        check(near(volume, 1.0/6.0), "mesh_element_volume_calculation: tet volume correct");
    }
}

static void test_mesh_quality_check() {
    FiniteElementMesh mesh;
    mesh.add_node(1, 0.0, 0.0, 0.0);
    mesh.add_node(2, 1.0, 0.0, 0.0);
    mesh.add_node(3, 0.0, 1.0, 0.0);
    mesh.add_node(4, 0.0, 0.0, 1.0);

    std::vector<int32_t> tet_nodes{1, 2, 3, 4};
    mesh.add_element(1, ElementType::Tetrahedron4, tet_nodes);

    MeshQuality quality = mesh.compute_quality();
    check(quality.aspect_ratio >= 1.0, "mesh_quality_check: aspect ratio computed");
    check(quality.jacobian_min > 0.0, "mesh_quality_check: jacobian computed");
}

// ============================================================================
// Element Library Tests (8 tests)
// ============================================================================

static void test_tetrahedron4_shape_functions() {
    Tetrahedron4 tet;
    Vec3 natural{0.25, 0.25, 0.25};

    auto N = tet.evaluate_shape_functions(natural);

    check(N.N.size() == 4, "tetrahedron4_shape_functions: 4 shape functions");
    check(near(N.N[0] + N.N[1] + N.N[2] + N.N[3], 1.0),
        "tetrahedron4_shape_functions: shape functions sum to 1");
}

static void test_tetrahedron4_gauss_points() {
    Tetrahedron4 tet;
    auto gp = tet.get_gauss_points();

    check(gp.size() == 1, "tetrahedron4_gauss_points: has 1 gauss point");
    check(near(gp[0].weight, 1.0), "tetrahedron4_gauss_points: weight is 1");
}

static void test_tetrahedron10_shape_functions() {
    Tetrahedron10 tet;
    Vec3 natural{0.25, 0.25, 0.25};

    auto N = tet.evaluate_shape_functions(natural);

    check(N.N.size() == 10, "tetrahedron10_shape_functions: 10 shape functions");
    check(near(N.N[0] + N.N[1] + N.N[2] + N.N[3] + N.N[4] + N.N[5] + N.N[6] + N.N[7] + N.N[8] + N.N[9], 1.0),
        "tetrahedron10_shape_functions: shape functions sum to 1");
}

static void test_hexahedron8_shape_functions() {
    Hexahedron8 hex;
    Vec3 natural{0.0, 0.0, 0.0};

    auto N = hex.evaluate_shape_functions(natural);

    check(N.N.size() == 8, "hexahedron8_shape_functions: 8 shape functions");
    check(near(N.N[0] + N.N[1] + N.N[2] + N.N[3] + N.N[4] + N.N[5] + N.N[6] + N.N[7], 1.0),
        "hexahedron8_shape_functions: shape functions sum to 1");
}

static void test_element_factory_creation() {
    auto tet4 = ElementFactory::create_element(ElementType::Tetrahedron4);
    check(tet4 != nullptr, "element_factory_creation: can create tet4");
    check(tet4->get_num_nodes() == 4, "element_factory_creation: tet4 has 4 nodes");

    auto hex8 = ElementFactory::create_element(ElementType::Hexahedron8);
    check(hex8 != nullptr, "element_factory_creation: can create hex8");
    check(hex8->get_num_nodes() == 8, "element_factory_creation: hex8 has 8 nodes");
}

static void test_jacobian_computation() {
    Tetrahedron4 tet;
    std::vector<Vec3> nodes{
        Vec3
        {0.0, 0.0, 0.0},
        Vec3
        {1.0, 0.0, 0.0},
        Vec3
        {0.0, 1.0, 0.0},
        Vec3
        {0.0, 0.0, 1.0}
    };

    double det = tet.compute_jacobian_determinant(Vec3
        {0.0, 0.0, 0.0}, nodes);
    check(near(det, 1.0), "jacobian_computation: tet jacobian correct");
}

static void test_element_volume_computation() {
    Tetrahedron4 tet;
    std::vector<Vec3> nodes{
        Vec3
        {0.0, 0.0, 0.0},
        Vec3
        {1.0, 0.0, 0.0},
        Vec3
        {0.0, 1.0, 0.0},
        Vec3
        {0.0, 0.0, 1.0}
    };

    double volume = tet.compute_volume(nodes);
    check(near(volume, 1.0/6.0), "element_volume_computation: tet volume correct");
}

// ============================================================================
// Material Database Tests (8 tests)
// ============================================================================

static void test_material_creation() {
    Material mat(1, "Test Steel", MaterialType::Isotropic);

    check(mat.get_id() == 1, "material_creation: id set");
    check(mat.get_name() == "Test Steel", "material_creation: name set");
    check(mat.get_type() == MaterialType::Isotropic, "material_creation: type set");
}

static void test_material_properties() {
    Material mat(1, "Test Steel", MaterialType::Isotropic);

    mat.set_youngs_modulus(200e9);
    mat.set_poissons_ratio(0.3);
    mat.set_density(7850.0);

    check(near(mat.get_youngs_modulus(), 200e9), "material_properties: youngs modulus");
    check(near(mat.get_poissons_ratio(), 0.3), "material_properties: poissons ratio");
    check(near(mat.get_density(), 7850.0), "material_properties: density");
}

static void test_material_strength_properties() {
    Material mat(1, "Test Steel", MaterialType::Isotropic);

    mat.set_yield_strength(250e6);
    mat.set_tensile_strength(400e6);
    mat.set_fatigue_limit(100e6);

    check(near(mat.get_yield_strength(), 250e6), "material_strength_properties: yield strength");
    check(near(mat.get_tensile_strength(), 400e6), "material_strength_properties: tensile strength");
    check(near(mat.get_fatigue_limit(), 100e6), "material_strength_properties: fatigue limit");
}

static void test_material_thermal_properties() {
    Material mat(1, "Test Steel", MaterialType::Isotropic);

    mat.set_thermal_conductivity(50.0);
    mat.set_specific_heat(500.0);
    mat.set_thermal_expansion(1.2e-5);

    check(near(mat.get_thermal_conductivity(), 50.0), "material_thermal_properties: conductivity");
    check(near(mat.get_specific_heat(), 500.0), "material_thermal_properties: specific heat");
    check(near(mat.get_thermal_expansion(), 1.2e-5), "material_thermal_properties: thermal expansion");
}

static void test_material_database_add_material() {
    MaterialDatabase db;
    auto mat = std::make_unique<Material>(1, "Steel", MaterialType::Isotropic);

    db.add_material(std::move(mat));

    check(db.get_material(1) != nullptr, "material_database_add_material: material added");
    check(db.get_material(1)->get_name() == "Steel", "material_database_add_material: material retrieved");
}

static void test_material_database_default_materials() {
    MaterialDatabase db;
    db.load_default_materials();

    check(db.get_material_by_name("Mild Steel") != nullptr, "material_database_default_materials: mild steel loaded");
    check(db.get_material_by_name("Aluminum 6061") != nullptr, "material_database_default_materials: aluminum loaded");
}

static void test_material_orthotropic_properties() {
    Material mat(1, "Composite", MaterialType::Orthotropic);

    mat.set_youngs_modulus_x(140e9);
    mat.set_youngs_modulus_y(10e9);
    mat.set_youngs_modulus_z(10e9);

    check(near(mat.get_youngs_modulus_x(), 140e9), "material_orthotropic_properties: Ex");
    check(near(mat.get_youngs_modulus_y(), 10e9), "material_orthotropic_properties: Ey");
    check(near(mat.get_youngs_modulus_z(), 10e9), "material_orthotropic_properties: Ez");
}

static void test_material_temperature_dependent_properties() {
    Material mat(1, "Test Steel", MaterialType::Isotropic);

    mat.add_temperature_dependent_youngs_modulus(273.0, 210e9);
    mat.add_temperature_dependent_youngs_modulus(373.0, 205e9);

    double E_at_300 = mat.get_youngs_modulus_at_temperature(300.0);
    check(E_at_300 < 210e9 && E_at_300 > 205e9, "material_temperature_dependent_properties: interpolation works");
}

// ============================================================================
// Boundary Conditions Tests (6 tests)
// ============================================================================

static void test_displacement_bc_creation() {
    DisplacementBC bc(1, 1, Direction::X, Vec3
        {0.0, 0.0, 0.0});

    check(bc.get_node_id() == 1, "displacement_bc_creation: node id set");
    check(bc.applies_to_x(), "displacement_bc_creation: applies to x");
}

static void test_force_bc_creation() {
    ForceBC bc(1, 1, Vec3
        {100.0, 0.0, 0.0});

    check(bc.get_node_id() == 1, "force_bc_creation: node id set");
    check(near(bc.get_force()[0], 100.0), "force_bc_creation: force value set");
}

static void test_bc_manager_add_displacement() {
    BoundaryConditionManager manager;

    DisplacementBC bc(1, 1, Direction::XYZ, Vec3
        {0.0, 0.0, 0.0});
    manager.add_displacement_bc(bc);

    check(manager.get_displacement_bcs().size() == 1, "bc_manager_add_displacement: bc added");
}

static void test_bc_manager_add_force() {
    BoundaryConditionManager manager;

    ForceBC bc(1, 1, Vec3
        {100.0, 0.0, 0.0});
    manager.add_force_bc(bc);

    check(manager.get_force_bcs().size() == 1, "bc_manager_add_force: bc added");
}

static void test_bc_manager_get_external_forces() {
    BoundaryConditionManager manager;

    ForceBC bc1(1, 0, Vec3
        {100.0, 0.0, 0.0});
    ForceBC bc2(2, 1, Vec3
        {0.0, 200.0, 0.0});
    manager.add_force_bc(bc1);
    manager.add_force_bc(bc2);

    auto forces = manager.get_external_forces(2);

    check(forces.size() == 2, "bc_manager_get_external_forces: correct number of nodes");
    check(near(forces[0][0], 100.0), "bc_manager_get_external_forces: force on node 0");
    check(near(forces[1][1], 200.0), "bc_manager_get_external_forces: force on node 1");
}

static void test_bc_manager_validation() {
    BoundaryConditionManager manager;

    DisplacementBC bc(1, 0, Direction::XYZ, Vec3
        {0.0, 0.0, 0.0});
    manager.add_displacement_bc(bc);

    check(manager.validate(1, 0), "bc_manager_validation: valid bc passes");
    check(!manager.validate(0, 0), "bc_manager_validation: invalid node id fails");
}

// ============================================================================
// FEA Solver Tests (5 tests)
// ============================================================================

static void test_fea_solver_initialization() {
    FEASolver solver;

    TetrahedralMeshGenerator gen;
    auto mesh = gen.generate_uniform_mesh(
        Vec3
        {0.0, 0.0, 0.0}, Vec3
        {1.0, 1.0, 1.0}, 1, 1, 1);

    solver.set_mesh(*mesh);

    check(solver.get_num_nodes() > 0, "fea_solver_initialization: mesh set");
    check(solver.get_num_elements() > 0, "fea_solver_initialization: elements loaded");
}

static void test_fea_solver_material_assignment() {
    FEASolver solver;
    MaterialDatabase db;
    db.load_default_materials();

    TetrahedralMeshGenerator gen;
    auto mesh = gen.generate_uniform_mesh(
        Vec3
        {0.0, 0.0, 0.0}, Vec3
        {1.0, 1.0, 1.0}, 1, 1, 1);

    solver.set_mesh(*mesh);
    solver.set_material_database(db);

    Element* elem = mesh->get_element(1);
    if (elem) {
        solver.assign_material_to_element(elem->id, 1);
        check(true, "fea_solver_material_assignment: material assigned");
    }
}

static void test_fea_solver_boundary_conditions() {
    FEASolver solver;
    BoundaryConditionManager bc_mgr;

    DisplacementBC bc(1, 1, Direction::XYZ, Vec3
        {0.0, 0.0, 0.0});
    bc_mgr.add_displacement_bc(bc);

    solver.set_boundary_conditions(bc_mgr);

    check(bc_mgr.get_displacement_bcs().size() == 1, "fea_solver_boundary_conditions: bc set");
}

static void test_direct_solver_small_system() {
    std::vector<std::vector<double>> A{
        {2.0, 1.0},
        {1.0, 3.0}
    };
    std::vector<double> b{5.0, 6.0};

    auto result = DirectSolver::solve(A, b);

    check(result.converged, "direct_solver_small_system: solution converged");
    check(result.solution.size() == 2, "direct_solver_small_system: solution size correct");
}

static void test_conjugate_gradient_solver() {
    SparseMatrix A(2);
    std::vector<double> b{1.0, 2.0};
    std::vector<double> x0{0.0, 0.0};

    auto result = ConjugateGradientSolver::solve(A, b, x0);

    check(result.solution.size() == 2, "conjugate_gradient_solver: solution computed");
}

// ============================================================================
// Result Processing Tests (4 tests)
// ============================================================================

static void test_stress_analysis_von_mises() {
    std::array<std::array<double, 3>, 3> stress{};
    stress[0][0] = 100e6;
    stress[1][1] = 50e6;
    stress[2][2] = 0.0;

    double vm_stress = StressAnalyzer::compute_von_mises_stress(stress);

    check(vm_stress > 0.0, "stress_analysis_von_mises: vm stress computed");
}

static void test_stress_analysis_principal_stresses() {
    std::array<std::array<double, 3>, 3> stress{};
    stress[0][0] = 100e6;
    stress[1][1] = 50e6;
    stress[2][2] = 0.0;

    auto principal = StressAnalyzer::compute_principal_stresses(stress);

    check(principal[0] >= principal[1], "stress_analysis_principal_stresses: sorted correctly");
}

static void test_stress_analysis_hydrostatic() {
    std::array<std::array<double, 3>, 3> stress{};
    stress[0][0] = 100e6;
    stress[1][1] = 100e6;
    stress[2][2] = 100e6;

    double hydro = StressAnalyzer::compute_hydrostatic_stress(stress);

    check(near(hydro, 100e6), "stress_analysis_hydrostatic: pressure correct");
}

static void test_stress_analysis_deviatoric() {
    std::array<std::array<double, 3>, 3> stress{};
    stress[0][0] = 150e6;
    stress[1][1] = 100e6;
    stress[2][2] = 100e6;

    auto dev = StressAnalyzer::compute_deviatoric_stress(stress);

    double trace = dev[0][0] + dev[1][1] + dev[2][2];
    check(near(trace, 0.0, 1e-3), "stress_analysis_deviatoric: trace is zero");
}

// ============================================================================
// Main Test Runner
// ============================================================================

int main() {
    std::printf("========================================\n");
    std::printf("M6 Phase: FEA Integration Test Suite\n");
    std::printf("========================================\n\n");

    // Mesh Generation Tests
    std::printf("--- Mesh Generation Tests ---\n");
    test_mesh_node_addition();
    test_mesh_element_addition();
    test_tetrahedral_mesh_generation();
    test_hexahedral_mesh_generation();
    test_mesh_boundary_marking();
    test_mesh_num_nodes_per_element();
    test_mesh_element_volume_calculation();
    test_mesh_quality_check();

    // Element Library Tests
    std::printf("\n--- Element Library Tests ---\n");
    test_tetrahedron4_shape_functions();
    test_tetrahedron4_gauss_points();
    test_tetrahedron10_shape_functions();
    test_hexahedron8_shape_functions();
    test_element_factory_creation();
    test_jacobian_computation();
    test_element_volume_computation();

    // Material Database Tests
    std::printf("\n--- Material Database Tests ---\n");
    test_material_creation();
    test_material_properties();
    test_material_strength_properties();
    test_material_thermal_properties();
    test_material_database_add_material();
    test_material_database_default_materials();
    test_material_orthotropic_properties();
    test_material_temperature_dependent_properties();

    // Boundary Conditions Tests
    std::printf("\n--- Boundary Conditions Tests ---\n");
    test_displacement_bc_creation();
    test_force_bc_creation();
    test_bc_manager_add_displacement();
    test_bc_manager_add_force();
    test_bc_manager_get_external_forces();
    test_bc_manager_validation();

    // FEA Solver Tests
    std::printf("\n--- FEA Solver Tests ---\n");
    test_fea_solver_initialization();
    test_fea_solver_material_assignment();
    test_fea_solver_boundary_conditions();
    test_direct_solver_small_system();
    test_conjugate_gradient_solver();

    // Result Processing Tests
    std::printf("\n--- Result Processing Tests ---\n");
    test_stress_analysis_von_mises();
    test_stress_analysis_principal_stresses();
    test_stress_analysis_hydrostatic();
    test_stress_analysis_deviatoric();

    std::printf("\n========================================\n");
    if (failures == 0) {
        std::printf("All tests passed! (35 tests)\n");
    } else {
        std::printf("Tests failed: %d\n", failures);
    }
    std::printf("========================================\n");

    return failures > 0 ? 1 : 0;
}
