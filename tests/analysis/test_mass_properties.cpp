#include "bendcore/analysis/mass_properties.hpp"
#include <iostream>
#include <cassert>
#include <cmath>

using namespace BendCore::CadCore;

// Test utilities
int testsPassed = 0;
int testsFailed = 0;

void assertCondition(bool condition, const char* message) {
    if (condition) {
        std::cout << "[PASS] " << message << std::endl;
        testsPassed++;
    } else {
        std::cout << "[FAIL] " << message << std::endl;
        testsFailed++;
    }
}

void assertEquals(double actual, double expected, double tolerance, const char* message) {
    if (std::abs(actual - expected) <= tolerance) {
        std::cout << "[PASS] " << message << std::endl;
        testsPassed++;
    } else {
        std::cout << "[FAIL] " << message << " (expected: " << expected << ", got: " << actual << ")" << std::endl;
        testsFailed++;
    }
}

void assertEquals(const Vector3D& actual, const Vector3D& expected, double tolerance, const char* message) {
    double dx = actual.x - expected.x;
    double dy = actual.y - expected.y;
    double dz = actual.z - expected.z;
    double dist = std::sqrt(dx*dx + dy*dy + dz*dz);

    if (dist <= tolerance) {
        std::cout << "[PASS] " << message << std::endl;
        testsPassed++;
    } else {
        std::cout << "[FAIL] " << message << " (distance: " << dist << ")" << std::endl;
        testsFailed++;
    }
}

// ==================== Material Library Tests ====================

void test_material_library_create_default() {
    MaterialLibrary lib = MaterialLibrary::getDefaultLibrary();
    assertCondition(lib.hasMaterial("Steel"), "Default library has Steel");
    assertCondition(lib.hasMaterial("Aluminum"), "Default library has Aluminum");
}

void test_material_library_get_material() {
    MaterialLibrary lib = MaterialLibrary::getDefaultLibrary();
    const Material* steel = lib.getMaterial("Steel");
    assertCondition(steel != nullptr, "Get Steel material");
    assertEquals(steel->density, 7850.0, 1.0, "Steel density is 7850 kg/m^3");
}

void test_material_library_add_custom() {
    MaterialLibrary lib;
    Material custom("CustomMetal", 5000.0);
    lib.addMaterial(custom);
    assertCondition(lib.hasMaterial("CustomMetal"), "Custom material added");
}

void test_material_library_update_density() {
    MaterialLibrary lib;
    Material mat("Test", 1000.0);
    lib.addMaterial(mat);
    bool success = lib.updateDensity("Test", 2000.0);
    assertCondition(success, "Density updated");
    const Material* updated = lib.getMaterial("Test");
    assertEquals(updated->density, 2000.0, 1.0, "New density is 2000");
}

void test_material_library_remove() {
    MaterialLibrary lib;
    Material mat("Temp", 1000.0);
    lib.addMaterial(mat);
    bool removed = lib.removeMaterial("Temp");
    assertCondition(removed, "Material removed");
    assertCondition(!lib.hasMaterial("Temp"), "Material no longer exists");
}

// ==================== Matrix3x3 Tests ====================

void test_matrix3x3_create() {
    Matrix3x3 m;
    assertEquals(m.get(0, 0), 0.0, 1e-10, "Matrix initialized to zero");
}

void test_matrix3x3_set_get() {
    Matrix3x3 m;
    m.set(0, 0, 5.0);
    assertEquals(m.get(0, 0), 5.0, 1e-10, "Matrix set/get works");
}

void test_matrix3x3_add() {
    Matrix3x3 m1, m2;
    m1.set(0, 0, 1.0);
    m2.set(0, 0, 2.0);
    Matrix3x3 result = m1 + m2;
    assertEquals(result.get(0, 0), 3.0, 1e-10, "Matrix addition works");
}

void test_matrix3x3_vector_multiply() {
    Matrix3x3 m;
    m.set(0, 0, 2.0);
    m.set(1, 1, 3.0);
    m.set(2, 2, 4.0);
    Vector3D v(1, 1, 1);
    Vector3D result = m * v;
    assertEquals(result.x, 2.0, 1e-10, "Matrix-vector multiply X");
    assertEquals(result.y, 3.0, 1e-10, "Matrix-vector multiply Y");
    assertEquals(result.z, 4.0, 1e-10, "Matrix-vector multiply Z");
}

// ==================== Component Mass Calculation Tests ====================

void test_component_mass_simple() {
    MaterialLibrary lib = MaterialLibrary::getDefaultLibrary();
    MassPropertiesCalculator calc(lib);

    EntityId id = EntityId::generate();
    double volume = 1.0;  // m^3
    ComponentMassData data = calc.calculateComponentMass(
        id, "Steel Bar", volume, "Steel", Vector3D(0, 0, 0)
    );

    // Steel: 7850 kg/m^3 * 1 m^3 = 7850 kg
    assertEquals(data.mass, 7850.0, 1.0, "Steel component mass calculated");
}

void test_component_mass_aluminum() {
    MaterialLibrary lib = MaterialLibrary::getDefaultLibrary();
    MassPropertiesCalculator calc(lib);

    EntityId id = EntityId::generate();
    double volume = 0.5;  // m^3
    ComponentMassData data = calc.calculateComponentMass(
        id, "Aluminum Part", volume, "Aluminum", Vector3D(1, 2, 3)
    );

    // Aluminum: 2700 kg/m^3 * 0.5 m^3 = 1350 kg
    assertEquals(data.mass, 1350.0, 1.0, "Aluminum component mass calculated");
    assertEquals(data.center_of_gravity, Vector3D(1, 2, 3), 1e-6, "COG set correctly");
}

void test_component_mass_custom_mass() {
    MaterialLibrary lib;
    MassPropertiesCalculator calc(lib);

    EntityId id = EntityId::generate();
    calc.calculateComponentMass(id, "Part", 1.0, "Generic", Vector3D(0, 0, 0));
    calc.setComponentCustomMass(id, 500.0, Vector3D(2, 3, 4));

    // Note: We need to get the component from cache to verify
    // This tests the custom mass setting mechanism
}

void test_component_mass_unknown_material() {
    MaterialLibrary lib;  // Empty library
    MassPropertiesCalculator calc(lib);

    EntityId id = EntityId::generate();
    ComponentMassData data = calc.calculateComponentMass(
        id, "Unknown Part", 1.0, "UnknownMaterial", Vector3D(0, 0, 0)
    );

    // Should create material with 0 density
    assertEquals(data.mass, 0.0, 1e-10, "Unknown material has zero mass");
}

// ==================== Center of Gravity Tests ====================

void test_cog_single_component() {
    MaterialLibrary lib = MaterialLibrary::getDefaultLibrary();
    MassPropertiesCalculator calc(lib);

    std::vector<ComponentMassData> components;
    ComponentMassData comp = calc.calculateComponentMass(
        EntityId::generate(), "Part1", 1.0, "Steel", Vector3D(5, 10, 15)
    );
    components.push_back(comp);

    Vector3D cog = calc.calculateCenterOfGravity(components);
    assertEquals(cog, Vector3D(5, 10, 15), 1e-6, "COG of single component is at its position");
}

void test_cog_two_equal_components() {
    MaterialLibrary lib = MaterialLibrary::getDefaultLibrary();
    MassPropertiesCalculator calc(lib);

    std::vector<ComponentMassData> components;

    ComponentMassData comp1 = calc.calculateComponentMass(
        EntityId::generate(), "Part1", 1.0, "Steel", Vector3D(0, 0, 0)
    );
    components.push_back(comp1);

    ComponentMassData comp2 = calc.calculateComponentMass(
        EntityId::generate(), "Part2", 1.0, "Steel", Vector3D(10, 0, 0)
    );
    components.push_back(comp2);

    Vector3D cog = calc.calculateCenterOfGravity(components);
    assertEquals(cog.x, 5.0, 1e-6, "COG X is midpoint");
    assertEquals(cog.y, 0.0, 1e-6, "COG Y is zero");
    assertEquals(cog.z, 0.0, 1e-6, "COG Z is zero");
}

void test_cog_two_different_mass_components() {
    MaterialLibrary lib = MaterialLibrary::getDefaultLibrary();
    MassPropertiesCalculator calc(lib);

    std::vector<ComponentMassData> components;

    // Heavy component at origin
    ComponentMassData comp1 = calc.calculateComponentMass(
        EntityId::generate(), "Heavy", 2.0, "Steel", Vector3D(0, 0, 0)
    );
    components.push_back(comp1);

    // Light component at (6, 0, 0)
    ComponentMassData comp2 = calc.calculateComponentMass(
        EntityId::generate(), "Light", 1.0, "Aluminum", Vector3D(6, 0, 0)
    );
    components.push_back(comp2);

    Vector3D cog = calc.calculateCenterOfGravity(components);
    // m1 = 2.0*7850 = 15700 kg at (0,0,0)
    // m2 = 1.0*2700 = 2700 kg at (6,0,0)
    // COG_x = (0*15700 + 6*2700) / (15700+2700) = 16200/18400 ≈ 0.8804
    assertEquals(cog.x, 0.8804, 0.01, "COG weighted towards heavier component");
}

void test_cog_empty_assembly() {
    MaterialLibrary lib;
    MassPropertiesCalculator calc(lib);

    std::vector<ComponentMassData> components;
    Vector3D cog = calc.calculateCenterOfGravity(components);
    assertEquals(cog, Vector3D(0, 0, 0), 1e-10, "Empty assembly COG is at origin");
}

void test_cog_three_components() {
    MaterialLibrary lib = MaterialLibrary::getDefaultLibrary();
    MassPropertiesCalculator calc(lib);

    std::vector<ComponentMassData> components;

    ComponentMassData comp1 = calc.calculateComponentMass(
        EntityId::generate(), "Part1", 1.0, "Steel", Vector3D(0, 0, 0)
    );
    components.push_back(comp1);

    ComponentMassData comp2 = calc.calculateComponentMass(
        EntityId::generate(), "Part2", 1.0, "Steel", Vector3D(3, 0, 0)
    );
    components.push_back(comp2);

    ComponentMassData comp3 = calc.calculateComponentMass(
        EntityId::generate(), "Part3", 1.0, "Steel", Vector3D(0, 3, 0)
    );
    components.push_back(comp3);

    Vector3D cog = calc.calculateCenterOfGravity(components);
    assertEquals(cog.x, 1.0, 1e-6, "COG X coordinate");
    assertEquals(cog.y, 1.0, 1e-6, "COG Y coordinate");
    assertEquals(cog.z, 0.0, 1e-6, "COG Z coordinate");
}

// ==================== Inertia Tensor Tests ====================

void test_inertia_single_point_mass() {
    MaterialLibrary lib;
    MassPropertiesCalculator calc(lib);

    std::vector<ComponentMassData> components;
    ComponentMassData comp;
    comp.mass = 1.0;
    comp.center_of_gravity = Vector3D(0, 0, 0);
    components.push_back(comp);

    Matrix3x3 I = calc.calculateInertiaTensor(components, Vector3D(0, 0, 0));
    assertEquals(I.get(0, 0), 0.0, 1e-10, "Inertia Ixx for mass at origin");
    assertEquals(I.get(1, 1), 0.0, 1e-10, "Inertia Iyy for mass at origin");
    assertEquals(I.get(2, 2), 0.0, 1e-10, "Inertia Izz for mass at origin");
}

void test_inertia_point_mass_offset() {
    MaterialLibrary lib;
    MassPropertiesCalculator calc(lib);

    std::vector<ComponentMassData> components;
    ComponentMassData comp;
    comp.mass = 1.0;
    comp.center_of_gravity = Vector3D(1, 0, 0);
    components.push_back(comp);

    Matrix3x3 I = calc.calculateInertiaTensor(components, Vector3D(0, 0, 0));
    // For mass m at (x,y,z): I_yy = m*(x^2 + z^2) = 1*(1+0) = 1
    assertEquals(I.get(1, 1), 1.0, 1e-6, "Inertia Iyy correct for offset mass");
}

void test_inertia_symmetry() {
    MaterialLibrary lib;
    MassPropertiesCalculator calc(lib);

    std::vector<ComponentMassData> components;
    ComponentMassData comp;
    comp.mass = 5.0;
    comp.center_of_gravity = Vector3D(2, 3, 4);
    components.push_back(comp);

    Matrix3x3 I = calc.calculateInertiaTensor(components, Vector3D(0, 0, 0));
    // Matrix should be symmetric
    assertEquals(I.get(0, 1), I.get(1, 0), 1e-6, "Inertia matrix symmetric Ixy=Iyx");
    assertEquals(I.get(0, 2), I.get(2, 0), 1e-6, "Inertia matrix symmetric Ixz=Izx");
    assertEquals(I.get(1, 2), I.get(2, 1), 1e-6, "Inertia matrix symmetric Iyz=Izy");
}

// ==================== Principal Moments Tests ====================

void test_principal_moments_diagonal_matrix() {
    MaterialLibrary lib;
    MassPropertiesCalculator calc(lib);

    Matrix3x3 I;
    I.set(0, 0, 1.0);
    I.set(1, 1, 2.0);
    I.set(2, 2, 3.0);

    std::array<double, 3> moments = calc.calculatePrincipalMoments(I);
    // Should return sorted eigenvalues
    assertCondition(moments[0] <= moments[1], "Principal moments sorted ascending");
    assertCondition(moments[1] <= moments[2], "Principal moments sorted ascending");
}

// ==================== Radius of Gyration Tests ====================

void test_radius_of_gyration() {
    MaterialLibrary lib;
    MassPropertiesCalculator calc(lib);

    std::array<double, 3> moments = {1.0, 2.0, 3.0};
    auto radius = calc.calculateRadiusOfGyration(10.0, moments);

    // R = sqrt(I/m)
    assertEquals(radius[0], std::sqrt(0.1), 1e-6, "Radius of gyration X");
    assertEquals(radius[1], std::sqrt(0.2), 1e-6, "Radius of gyration Y");
    assertEquals(radius[2], std::sqrt(0.3), 1e-6, "Radius of gyration Z");
}

void test_radius_of_gyration_zero_mass() {
    MaterialLibrary lib;
    MassPropertiesCalculator calc(lib);

    std::array<double, 3> moments = {1.0, 2.0, 3.0};
    auto radius = calc.calculateRadiusOfGyration(0.0, moments);

    assertEquals(radius[0], 0.0, 1e-10, "Zero mass gives zero radius");
}

// ==================== Bounding Box Tests ====================

void test_bounding_box_single_point() {
    MaterialLibrary lib;
    MassPropertiesCalculator calc(lib);

    std::vector<ComponentMassData> components;
    ComponentMassData comp;
    comp.center_of_gravity = Vector3D(5, 5, 5);
    components.push_back(comp);

    auto [min, max] = calc.calculateBoundingBox(components);
    assertEquals(min, Vector3D(5, 5, 5), 1e-6, "Bounding box min for single point");
    assertEquals(max, Vector3D(5, 5, 5), 1e-6, "Bounding box max for single point");
}

void test_bounding_box_multiple_points() {
    MaterialLibrary lib;
    MassPropertiesCalculator calc(lib);

    std::vector<ComponentMassData> components;

    ComponentMassData comp1;
    comp1.center_of_gravity = Vector3D(0, 0, 0);
    components.push_back(comp1);

    ComponentMassData comp2;
    comp2.center_of_gravity = Vector3D(10, 10, 10);
    components.push_back(comp2);

    auto [min, max] = calc.calculateBoundingBox(components);
    assertEquals(min, Vector3D(0, 0, 0), 1e-6, "Bounding box minimum");
    assertEquals(max, Vector3D(10, 10, 10), 1e-6, "Bounding box maximum");
}

void test_bounding_box_negative_coordinates() {
    MaterialLibrary lib;
    MassPropertiesCalculator calc(lib);

    std::vector<ComponentMassData> components;

    ComponentMassData comp1;
    comp1.center_of_gravity = Vector3D(-5, -5, -5);
    components.push_back(comp1);

    ComponentMassData comp2;
    comp2.center_of_gravity = Vector3D(5, 5, 5);
    components.push_back(comp2);

    auto [min, max] = calc.calculateBoundingBox(components);
    assertEquals(min, Vector3D(-5, -5, -5), 1e-6, "Bounding box with negatives min");
    assertEquals(max, Vector3D(5, 5, 5), 1e-6, "Bounding box with negatives max");
}

// ==================== Material Composition Tests ====================

void test_material_composition_single() {
    MaterialLibrary lib = MaterialLibrary::getDefaultLibrary();
    MassPropertiesCalculator calc(lib);

    std::vector<ComponentMassData> components;
    ComponentMassData comp = calc.calculateComponentMass(
        EntityId::generate(), "Steel Part", 1.0, "Steel", Vector3D(0, 0, 0)
    );
    components.push_back(comp);

    auto composition = calc.getMaterialComposition(components);
    assertCondition(composition.count("Steel") > 0, "Material composition has Steel");
    assertEquals(composition["Steel"], 7850.0, 1.0, "Steel mass in composition");
}

void test_material_composition_mixed() {
    MaterialLibrary lib = MaterialLibrary::getDefaultLibrary();
    MassPropertiesCalculator calc(lib);

    std::vector<ComponentMassData> components;

    ComponentMassData comp1 = calc.calculateComponentMass(
        EntityId::generate(), "Steel", 1.0, "Steel", Vector3D(0, 0, 0)
    );
    components.push_back(comp1);

    ComponentMassData comp2 = calc.calculateComponentMass(
        EntityId::generate(), "Aluminum", 1.0, "Aluminum", Vector3D(0, 0, 0)
    );
    components.push_back(comp2);

    auto composition = calc.getMaterialComposition(components);
    assertCondition(composition.size() == 2, "Mixed composition has 2 materials");
}

// ==================== Assembly Mass Properties Tests ====================

void test_assembly_mass_properties_single() {
    MaterialLibrary lib = MaterialLibrary::getDefaultLibrary();
    MassPropertiesCalculator calc(lib);

    std::vector<ComponentMassData> components;
    ComponentMassData comp = calc.calculateComponentMass(
        EntityId::generate(), "Part1", 1.0, "Steel", Vector3D(0, 0, 0)
    );
    components.push_back(comp);

    MassProperties props = calc.calculateAssemblyMassProperties(components);
    assertEquals(props.total_mass, 7850.0, 1.0, "Assembly total mass");
    assertEquals(props.center_of_gravity, Vector3D(0, 0, 0), 1e-6, "Assembly COG");
}

void test_assembly_mass_properties_empty() {
    MaterialLibrary lib;
    MassPropertiesCalculator calc(lib);

    std::vector<ComponentMassData> components;
    MassProperties props = calc.calculateAssemblyMassProperties(components);

    assertEquals(props.total_mass, 0.0, 1e-10, "Empty assembly has zero mass");
    assertEquals(props.center_of_gravity, Vector3D(0, 0, 0), 1e-10, "Empty assembly COG at origin");
}

void test_assembly_mass_properties_complex() {
    MaterialLibrary lib = MaterialLibrary::getDefaultLibrary();
    MassPropertiesCalculator calc(lib);

    std::vector<ComponentMassData> components;

    // 3 components made of different materials
    ComponentMassData comp1 = calc.calculateComponentMass(
        EntityId::generate(), "Steel Tube", 0.5, "Steel", Vector3D(0, 0, 0)
    );
    components.push_back(comp1);

    ComponentMassData comp2 = calc.calculateComponentMass(
        EntityId::generate(), "Aluminum Plate", 0.3, "Aluminum", Vector3D(1, 0, 0)
    );
    components.push_back(comp2);

    ComponentMassData comp3 = calc.calculateComponentMass(
        EntityId::generate(), "Copper Wire", 0.1, "Copper", Vector3D(0, 1, 0)
    );
    components.push_back(comp3);

    MassProperties props = calc.calculateAssemblyMassProperties(components);

    // Calculate expected mass
    double expected_mass = 0.5 * 7850.0 + 0.3 * 2700.0 + 0.1 * 8960.0;
    assertEquals(props.total_mass, expected_mass, 1.0, "Complex assembly total mass");
    assertCondition(props.material_composition.size() == 3, "Complex assembly has 3 materials");
}

// ==================== Reporter Tests ====================

void test_reporter_generate_report() {
    MassPropertiesReporter reporter;
    MaterialLibrary lib = MaterialLibrary::getDefaultLibrary();
    MassPropertiesCalculator calc(lib);

    std::vector<ComponentMassData> components;
    ComponentMassData comp = calc.calculateComponentMass(
        EntityId::generate(), "Test Part", 1.0, "Steel", Vector3D(0, 0, 0)
    );
    components.push_back(comp);

    MassProperties props = calc.calculateAssemblyMassProperties(components);
    std::string report = reporter.generateReport(props);

    assertCondition(report.find("Mass Properties Report") != std::string::npos, "Report contains title");
    assertCondition(report.find("Total Mass") != std::string::npos, "Report contains mass");
}

void test_reporter_generate_csv() {
    MassPropertiesReporter reporter;
    MaterialLibrary lib = MaterialLibrary::getDefaultLibrary();
    MassPropertiesCalculator calc(lib);

    std::vector<ComponentMassData> components;
    ComponentMassData comp = calc.calculateComponentMass(
        EntityId::generate(), "Part1", 1.0, "Steel", Vector3D(0, 0, 0)
    );
    components.push_back(comp);

    MassProperties props = calc.calculateAssemblyMassProperties(components);
    std::string csv = reporter.generateCSV(props, components);

    assertCondition(csv.find("Component") != std::string::npos, "CSV has header");
    assertCondition(csv.find("Part1") != std::string::npos, "CSV has component");
}

void test_reporter_generate_json() {
    MassPropertiesReporter reporter;
    MaterialLibrary lib = MaterialLibrary::getDefaultLibrary();
    MassPropertiesCalculator calc(lib);

    std::vector<ComponentMassData> components;
    ComponentMassData comp = calc.calculateComponentMass(
        EntityId::generate(), "Part1", 1.0, "Steel", Vector3D(0, 0, 0)
    );
    components.push_back(comp);

    MassProperties props = calc.calculateAssemblyMassProperties(components);
    std::string json = reporter.generateJSON(props, components);

    assertCondition(json.find("total_mass") != std::string::npos, "JSON has total_mass");
    assertCondition(json.find("center_of_gravity") != std::string::npos, "JSON has COG");
}

void test_reporter_weight_breakdown() {
    MassPropertiesReporter reporter;
    MaterialLibrary lib = MaterialLibrary::getDefaultLibrary();
    MassPropertiesCalculator calc(lib);

    std::vector<ComponentMassData> components;
    ComponentMassData comp1 = calc.calculateComponentMass(
        EntityId::generate(), "Heavy", 1.0, "Steel", Vector3D(0, 0, 0)
    );
    components.push_back(comp1);

    ComponentMassData comp2 = calc.calculateComponentMass(
        EntityId::generate(), "Light", 0.1, "Aluminum", Vector3D(0, 0, 0)
    );
    components.push_back(comp2);

    std::string breakdown = reporter.generateWeightBreakdown(components);
    assertCondition(breakdown.find("Heavy") != std::string::npos, "Breakdown has Heavy component");
    assertCondition(breakdown.find("Light") != std::string::npos, "Breakdown has Light component");
}

void test_reporter_material_breakdown() {
    MassPropertiesReporter reporter;
    MaterialLibrary lib = MaterialLibrary::getDefaultLibrary();
    MassPropertiesCalculator calc(lib);

    std::vector<ComponentMassData> components;
    ComponentMassData comp = calc.calculateComponentMass(
        EntityId::generate(), "Part", 1.0, "Steel", Vector3D(0, 0, 0)
    );
    components.push_back(comp);

    MassProperties props = calc.calculateAssemblyMassProperties(components);
    std::string breakdown = reporter.generateMaterialBreakdown(props);

    assertCondition(breakdown.find("Steel") != std::string::npos, "Material breakdown has Steel");
}

void test_reporter_balance_analysis() {
    MassPropertiesReporter reporter;
    MaterialLibrary lib = MaterialLibrary::getDefaultLibrary();
    MassPropertiesCalculator calc(lib);

    std::vector<ComponentMassData> components;
    ComponentMassData comp = calc.calculateComponentMass(
        EntityId::generate(), "Part", 1.0, "Steel", Vector3D(1, 1, 1)
    );
    components.push_back(comp);

    MassProperties props = calc.calculateAssemblyMassProperties(components);
    Vector3D ideal_cog(0, 0, 0);
    std::string analysis = reporter.generateBalanceAnalysis(props, ideal_cog);

    assertCondition(analysis.find("BALANCE ANALYSIS") != std::string::npos, "Analysis has title");
    assertCondition(analysis.find("Offset") != std::string::npos, "Analysis has offset");
}

// ==================== Run all tests ====================

int main() {
    std::cout << "\n========== MASS PROPERTIES TESTS ==========\n\n";

    // Material Library Tests
    test_material_library_create_default();
    test_material_library_get_material();
    test_material_library_add_custom();
    test_material_library_update_density();
    test_material_library_remove();

    // Matrix3x3 Tests
    test_matrix3x3_create();
    test_matrix3x3_set_get();
    test_matrix3x3_add();
    test_matrix3x3_vector_multiply();

    // Component Mass Tests
    test_component_mass_simple();
    test_component_mass_aluminum();
    test_component_mass_custom_mass();
    test_component_mass_unknown_material();

    // Center of Gravity Tests
    test_cog_single_component();
    test_cog_two_equal_components();
    test_cog_two_different_mass_components();
    test_cog_empty_assembly();
    test_cog_three_components();

    // Inertia Tensor Tests
    test_inertia_single_point_mass();
    test_inertia_point_mass_offset();
    test_inertia_symmetry();

    // Principal Moments Tests
    test_principal_moments_diagonal_matrix();

    // Radius of Gyration Tests
    test_radius_of_gyration();
    test_radius_of_gyration_zero_mass();

    // Bounding Box Tests
    test_bounding_box_single_point();
    test_bounding_box_multiple_points();
    test_bounding_box_negative_coordinates();

    // Material Composition Tests
    test_material_composition_single();
    test_material_composition_mixed();

    // Assembly Tests
    test_assembly_mass_properties_single();
    test_assembly_mass_properties_empty();
    test_assembly_mass_properties_complex();

    // Reporter Tests
    test_reporter_generate_report();
    test_reporter_generate_csv();
    test_reporter_generate_json();
    test_reporter_weight_breakdown();
    test_reporter_material_breakdown();
    test_reporter_balance_analysis();

    std::cout << "\n========================================\n";
    std::cout << "Tests Passed: " << testsPassed << "\n";
    std::cout << "Tests Failed: " << testsFailed << "\n";
    std::cout << "========================================\n\n";

    return testsFailed > 0 ? 1 : 0;
}
