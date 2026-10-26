// Material Library - Comprehensive Unit Tests
// M8 Phase: Material Library (GAP-033)
#include <cassert>
#include <iostream>
#include <cmath>
#include "bendcore/materials/material_library.hpp"

using namespace bendcore::materials;

// Test counter
int test_count = 0;
int passed_tests = 0;

void assert_equal(bool condition, const std::string& test_name) {
    test_count++;
    if (condition) {
        passed_tests++;
        std::cout << "[PASS] " << test_name << std::endl;
    } else {
        std::cout << "[FAIL] " << test_name << std::endl;
    }
}

void assert_approx_equal(double a, double b, double tolerance, const std::string& test_name) {
    test_count++;
    bool condition = std::abs(a - b) < tolerance;
    if (condition) {
        passed_tests++;
        std::cout << "[PASS] " << test_name << std::endl;
    } else {
        std::cout << "[FAIL] " << test_name << " (expected " << a << ", got " << b << ")" << std::endl;
    }
}

// Test 1: Basic Material Creation
void test_basic_material_creation() {
    Material mat(1, "Test Steel", MaterialCategory::Steel);
    assert_equal(mat.get_id() == 1, "Material ID set correctly");
    assert_equal(mat.get_name() == "Test Steel", "Material name set correctly");
    assert_equal(mat.get_category() == MaterialCategory::Steel, "Material category set correctly");
}

// Test 2: Mechanical Properties
void test_mechanical_properties() {
    Material mat(2, "Test Material", MaterialCategory::Steel);
    mat.set_youngs_modulus(200e9);
    mat.set_poissons_ratio(0.3);
    mat.set_shear_modulus(80e9);
    mat.set_yield_strength(250e6);
    mat.set_tensile_strength(400e6);

    assert_approx_equal(mat.get_youngs_modulus(), 200e9, 1e6, "Young's modulus set correctly");
    assert_approx_equal(mat.get_poissons_ratio(), 0.3, 0.01, "Poisson's ratio set correctly");
    assert_approx_equal(mat.get_shear_modulus(), 80e9, 1e6, "Shear modulus set correctly");
    assert_approx_equal(mat.get_yield_strength(), 250e6, 1e6, "Yield strength set correctly");
    assert_approx_equal(mat.get_tensile_strength(), 400e6, 1e6, "Tensile strength set correctly");
}

// Test 3: Thermal Properties
void test_thermal_properties() {
    Material mat(3, "Test Material", MaterialCategory::Steel);
    mat.set_thermal_conductivity(50.0);
    mat.set_specific_heat(500.0);
    mat.set_thermal_expansion(12e-6);
    mat.set_melting_point(1540.0);

    assert_approx_equal(mat.get_thermal_conductivity(), 50.0, 0.1, "Thermal conductivity set");
    assert_approx_equal(mat.get_specific_heat(), 500.0, 1.0, "Specific heat set");
    assert_approx_equal(mat.get_thermal_expansion(), 12e-6, 1e-7, "Thermal expansion set");
    assert_approx_equal(mat.get_melting_point(), 1540.0, 1.0, "Melting point set");
}

// Test 4: Density and Cost
void test_density_and_cost() {
    Material mat(4, "Test Material", MaterialCategory::Steel);
    mat.set_density(7850.0);
    mat.set_cost_per_kg(0.5);

    assert_approx_equal(mat.get_density(), 7850.0, 0.1, "Density set correctly");
    assert_approx_equal(mat.get_cost_per_kg(), 0.5, 0.01, "Cost per kg set correctly");
}

// Test 5: Manufacturing Properties
void test_manufacturing_properties() {
    Material mat(5, "Test Material", MaterialCategory::Steel);
    ManufacturingProperties mfg{};
    mfg.machinability_rating = 75.0;
    mfg.weldability_rating = 90.0;
    mfg.formability_rating = 80.0;
    mfg.heat_treatability_rating = 70.0;
    mfg.primary_joining_method = "welding";
    mat.set_manufacturing_properties(mfg);

    const auto& props = mat.get_manufacturing_properties();
    assert_approx_equal(props.machinability_rating, 75.0, 0.1, "Machinability rating set");
    assert_approx_equal(props.weldability_rating, 90.0, 0.1, "Weldability rating set");
    assert_approx_equal(props.formability_rating, 80.0, 0.1, "Formability rating set");
    assert_approx_equal(props.heat_treatability_rating, 70.0, 0.1, "Heat treatability rating set");
    assert_equal(props.primary_joining_method == "welding", "Primary joining method set");
}

// Test 6: Sustainability Properties
void test_sustainability_properties() {
    Material mat(6, "Test Material", MaterialCategory::Steel);
    SustainabilityProperties sust{};
    sust.carbon_footprint = 2.0;
    sust.recyclability_percentage = 100.0;
    sust.environmental_impact = "medium";
    sust.is_recyclable = true;
    sust.recycled_content_percentage = 20.0;
    mat.set_sustainability_properties(sust);

    const auto& props = mat.get_sustainability_properties();
    assert_approx_equal(props.carbon_footprint, 2.0, 0.1, "Carbon footprint set");
    assert_approx_equal(props.recyclability_percentage, 100.0, 0.1, "Recyclability percentage set");
    assert_equal(props.is_recyclable, "Recyclability flag set");
    assert_approx_equal(props.recycled_content_percentage, 20.0, 0.1, "Recycled content set");
}

// Test 7: Material Validation
void test_material_validation() {
    Material mat(7, "Valid Material", MaterialCategory::Steel);
    mat.set_youngs_modulus(200e9);
    mat.set_poissons_ratio(0.3);

    assert_equal(mat.is_valid(), "Valid material recognized");

    Material invalid_mat(8, "Invalid Material", MaterialCategory::Steel);
    assert_equal(!invalid_mat.is_valid(), "Invalid material recognized");
}

// Test 8: Material Library Creation
void test_material_library_creation() {
    MaterialLibrary lib;
    assert_equal(lib.get_material_count() == 0, "Empty library created");
}

// Test 9: Adding and Retrieving Materials
void test_adding_and_retrieving_materials() {
    MaterialLibrary lib;
    auto mat = std::make_unique<Material>(1, "Steel", MaterialCategory::Steel);
    mat->set_youngs_modulus(200e9);
    lib.add_material(std::move(mat));

    assert_equal(lib.get_material_count() == 1, "Material added to library");
    assert_equal(lib.get_material(1) != nullptr, "Material retrieved by ID");
    assert_equal(lib.get_material_by_name("Steel") != nullptr, "Material retrieved by name");
}

// Test 10: Material Categories
void test_material_categories() {
    MaterialLibrary lib;
    auto steel = std::make_unique<Material>(1, "Steel", MaterialCategory::Steel);
    auto aluminum = std::make_unique<Material>(2, "Aluminum", MaterialCategory::Aluminum);
    auto titanium = std::make_unique<Material>(3, "Titanium", MaterialCategory::Titanium);

    lib.add_material(std::move(steel));
    lib.add_material(std::move(aluminum));
    lib.add_material(std::move(titanium));

    auto steels = lib.get_materials_by_category(MaterialCategory::Steel);
    assert_equal(steels.size() == 1, "Steel category filter works");
    assert_equal(steels[0]->get_name() == "Steel", "Correct steel returned");
}

// Test 11: Load Default Materials
void test_load_default_materials() {
    MaterialLibrary lib;
    lib.load_default_materials();
    assert_equal(lib.get_material_count() >= 51, "All default materials loaded");
    assert_equal(lib.get_material_by_name("Mild Steel") != nullptr, "Mild Steel loaded");
    assert_equal(lib.get_material_by_name("Aluminum 6061") != nullptr, "Aluminum 6061 loaded");
    assert_equal(lib.get_material_by_name("Titanium Grade 5 (Ti6Al4V)") != nullptr, "Ti6Al4V loaded");
}

// Test 12: Search by Modulus Range
void test_search_by_modulus_range() {
    MaterialLibrary lib;
    lib.load_default_materials();

    MaterialSearchCriteria criteria;
    criteria.min_youngs_modulus = 190e9;
    criteria.max_youngs_modulus = 220e9;

    auto results = lib.search(criteria);
    assert_equal(results.size() > 0, "Found materials with specified modulus range");
}

// Test 13: Search by Category
void test_search_by_category() {
    MaterialLibrary lib;
    lib.load_default_materials();

    MaterialSearchCriteria criteria;
    criteria.category = MaterialCategory::Aluminum;

    auto results = lib.search(criteria);
    assert_equal(results.size() >= 7, "Found all aluminum materials");
    for (const auto* mat : results) {
        assert_equal(mat->get_category() == MaterialCategory::Aluminum, "All results are aluminum");
    }
}

// Test 14: Search by Yield Strength
void test_search_by_yield_strength() {
    MaterialLibrary lib;
    lib.load_default_materials();

    MaterialSearchCriteria criteria;
    criteria.min_yield_strength = 500e6;

    auto results = lib.search(criteria);
    assert_equal(results.size() > 0, "Found high-strength materials");
}

// Test 15: Search by Density
void test_search_by_density() {
    MaterialLibrary lib;
    lib.load_default_materials();

    MaterialSearchCriteria criteria;
    criteria.max_density = 3000.0;

    auto results = lib.search(criteria);
    assert_equal(results.size() > 0, "Found light materials");
}

// Test 16: Search by Cost
void test_search_by_cost() {
    MaterialLibrary lib;
    lib.load_default_materials();

    MaterialSearchCriteria criteria;
    criteria.max_cost_per_kg = 5.0;

    auto results = lib.search(criteria);
    assert_equal(results.size() > 0, "Found affordable materials");
}

// Test 17: Search by Name Substring
void test_search_by_name_substring() {
    MaterialLibrary lib;
    lib.load_default_materials();

    MaterialSearchCriteria criteria;
    criteria.name_contains = "Steel";

    auto results = lib.search(criteria);
    assert_equal(results.size() >= 9, "Found all steel materials");
}

// Test 18: Material Comparison
void test_material_comparison() {
    MaterialLibrary lib;
    auto steel = std::make_unique<Material>(1, "Steel", MaterialCategory::Steel);
    steel->set_youngs_modulus(200e9);
    steel->set_yield_strength(250e6);
    steel->set_density(7850.0);
    steel->set_cost_per_kg(0.5);

    auto aluminum = std::make_unique<Material>(2, "Aluminum", MaterialCategory::Aluminum);
    aluminum->set_youngs_modulus(69e9);
    aluminum->set_yield_strength(275e6);
    aluminum->set_density(2700.0);
    aluminum->set_cost_per_kg(3.0);

    lib.add_material(std::move(steel));
    lib.add_material(std::move(aluminum));

    auto comp = lib.compare(1, 2);
    assert_equal(comp.material1 != nullptr && comp.material2 != nullptr, "Comparison created");
    assert_approx_equal(comp.weight_ratio, 7850.0 / 2700.0, 0.1, "Weight ratio calculated");
    assert_approx_equal(comp.stiffness_ratio, 200e9 / 69e9, 0.1, "Stiffness ratio calculated");
}

// Test 19: Cost Performance Ranking
void test_cost_performance_ranking() {
    MaterialLibrary lib;
    lib.load_default_materials();

    MaterialSearchCriteria criteria;
    criteria.category = MaterialCategory::Steel;

    auto ranking = lib.rank_by_cost_performance(criteria);
    assert_equal(ranking.size() > 0, "Cost performance ranking created");

    // Verify that rankings are in descending order
    if (ranking.size() > 1) {
        assert_equal(ranking[0].second >= ranking[1].second, "Rankings are in order");
    }
}

// Test 20: Strength-to-Weight Ranking
void test_strength_to_weight_ranking() {
    MaterialLibrary lib;
    lib.load_default_materials();

    MaterialSearchCriteria criteria;
    criteria.category = MaterialCategory::Titanium;

    auto ranking = lib.rank_by_strength_to_weight(criteria);
    assert_equal(ranking.size() > 0, "Strength-to-weight ranking created");
}

// Test 21: Stiffness-to-Weight Ranking
void test_stiffness_to_weight_ranking() {
    MaterialLibrary lib;
    lib.load_default_materials();

    MaterialSearchCriteria criteria;

    auto ranking = lib.rank_by_stiffness_to_weight(criteria);
    assert_equal(ranking.size() > 0, "Stiffness-to-weight ranking created");
}

// Test 22: Custom Material Creation
void test_custom_material_creation() {
    MaterialLibrary lib;
    int32_t id = lib.create_custom_material("My Custom Material", MaterialCategory::Steel);

    assert_equal(id > 9999, "Custom material ID is unique");
    assert_equal(lib.get_material(id) != nullptr, "Custom material retrieved");
    assert_equal(lib.is_custom_material(id), "Custom material marked as custom");
}

// Test 23: Custom Material Deletion
void test_custom_material_deletion() {
    MaterialLibrary lib;
    int32_t id = lib.create_custom_material("Temp Material", MaterialCategory::Steel);
    assert_equal(lib.get_material(id) != nullptr, "Custom material created");

    bool deleted = lib.delete_custom_material(id);
    assert_equal(deleted, "Custom material deleted successfully");
    assert_equal(lib.get_material(id) == nullptr, "Custom material no longer exists");
}

// Test 24: Total Cost Calculation
void test_total_cost_calculation() {
    MaterialLibrary lib;
    auto mat = std::make_unique<Material>(1, "Steel", MaterialCategory::Steel);
    mat->set_cost_per_kg(0.5);
    lib.add_material(std::move(mat));

    double total_cost = lib.calculate_total_cost(1, 1000.0);
    assert_approx_equal(total_cost, 500.0, 1.0, "Total cost calculated correctly");
}

// Test 25: Mild Steel Properties
void test_mild_steel_properties() {
    MaterialLibrary lib;
    lib.load_default_materials();

    const auto* mild_steel = lib.get_material_by_name("Mild Steel");
    assert_equal(mild_steel != nullptr, "Mild Steel loaded");
    assert_equal(mild_steel->get_category() == MaterialCategory::Steel, "Mild Steel is in Steel category");
    assert_approx_equal(mild_steel->get_youngs_modulus(), 200e9, 1e9, "Mild Steel E correct");
    assert_approx_equal(mild_steel->get_yield_strength(), 250e6, 1e6, "Mild Steel yield strength correct");
}

// Test 26: Stainless Steel 304 Properties
void test_stainless_steel_304_properties() {
    MaterialLibrary lib;
    lib.load_default_materials();

    const auto* ss304 = lib.get_material_by_name("Stainless Steel 304");
    assert_equal(ss304 != nullptr, "Stainless Steel 304 loaded");
    assert_approx_equal(ss304->get_density(), 8000.0, 10.0, "SS304 density correct");
    assert_approx_equal(ss304->get_cost_per_kg(), 5.0, 0.5, "SS304 cost correct");
}

// Test 27: Aluminum 6061 Properties
void test_aluminum_6061_properties() {
    MaterialLibrary lib;
    lib.load_default_materials();

    const auto* al6061 = lib.get_material_by_name("Aluminum 6061");
    assert_equal(al6061 != nullptr, "Aluminum 6061 loaded");
    assert_approx_equal(al6061->get_density(), 2700.0, 10.0, "Al6061 density correct");
    assert_approx_equal(al6061->get_thermal_conductivity(), 167.0, 1.0, "Al6061 thermal conductivity correct");
}

// Test 28: Titanium Grade 5 Properties
void test_titanium_grade_5_properties() {
    MaterialLibrary lib;
    lib.load_default_materials();

    const auto* ti6al4v = lib.get_material_by_name("Titanium Grade 5 (Ti6Al4V)");
    assert_equal(ti6al4v != nullptr, "Titanium Grade 5 loaded");
    assert_approx_equal(ti6al4v->get_yield_strength(), 880e6, 1e6, "Ti6Al4V yield strength correct");
}

// Test 29: Copper Material Properties
void test_copper_properties() {
    MaterialLibrary lib;
    lib.load_default_materials();

    const auto* copper = lib.get_material_by_name("Copper");
    assert_equal(copper != nullptr, "Copper loaded");
    assert_approx_equal(copper->get_thermal_conductivity(), 398.0, 1.0, "Copper conductivity correct");
}

// Test 30: Carbon Fiber Epoxy Composite
void test_carbon_fiber_epoxy_properties() {
    MaterialLibrary lib;
    lib.load_default_materials();

    const auto* cfe = lib.get_material_by_name("Carbon Fiber Epoxy");
    assert_equal(cfe != nullptr, "Carbon Fiber Epoxy loaded");
    assert_equal(cfe->get_category() == MaterialCategory::Composite, "CFE is composite");
    assert_approx_equal(cfe->get_density(), 1550.0, 10.0, "CFE density correct");
}

// Test 31: Ceramic Material Properties
void test_ceramic_material_properties() {
    MaterialLibrary lib;
    lib.load_default_materials();

    const auto* ceramic = lib.get_material_by_name("Aluminum Oxide (Al2O3) Ceramic");
    assert_equal(ceramic != nullptr, "Ceramic loaded");
    assert_equal(ceramic->get_category() == MaterialCategory::Ceramic, "Material is ceramic");
    assert_approx_equal(ceramic->get_youngs_modulus(), 345e9, 1e9, "Ceramic modulus correct");
}

// Test 32: Polymer Material Properties
void test_polymer_material_properties() {
    MaterialLibrary lib;
    lib.load_default_materials();

    const auto* polymer = lib.get_material_by_name("Polycarbonate");
    assert_equal(polymer != nullptr, "Polymer loaded");
    assert_equal(polymer->get_category() == MaterialCategory::Polymer, "Material is polymer");
}

// Test 33: Nickel Alloy Properties
void test_nickel_alloy_properties() {
    MaterialLibrary lib;
    lib.load_default_materials();

    const auto* inconel = lib.get_material_by_name("Inconel 625");
    assert_equal(inconel != nullptr, "Inconel 625 loaded");
    assert_equal(inconel->get_category() == MaterialCategory::Nickel, "Inconel is nickel-based");
}

// Test 34: Magnesium Alloy Properties
void test_magnesium_alloy_properties() {
    MaterialLibrary lib;
    lib.load_default_materials();

    const auto* magnesium = lib.get_material_by_name("Magnesium AZ91D");
    assert_equal(magnesium != nullptr, "Magnesium loaded");
    assert_equal(magnesium->get_category() == MaterialCategory::Magnesium, "Material is magnesium");
}

// Test 35: Manufacturing Properties - Machinability
void test_manufacturing_machinability() {
    MaterialLibrary lib;
    lib.load_default_materials();

    MaterialSearchCriteria criteria;
    criteria.min_machinability = 80.0;

    auto results = lib.search(criteria);
    assert_equal(results.size() > 0, "Found highly machinable materials");
}

// Test 36: Manufacturing Properties - Weldability
void test_manufacturing_weldability() {
    MaterialLibrary lib;
    lib.load_default_materials();

    MaterialSearchCriteria criteria;
    criteria.min_weldability = 90.0;

    auto results = lib.search(criteria);
    assert_equal(results.size() > 0, "Found highly weldable materials");
}

// Test 37: Thermal Conductivity Search
void test_thermal_conductivity_search() {
    MaterialLibrary lib;
    lib.load_default_materials();

    MaterialSearchCriteria criteria;
    criteria.min_thermal_conductivity = 150.0;

    auto results = lib.search(criteria);
    assert_equal(results.size() > 0, "Found good thermal conductors");
}

// Test 38: Complex Multi-Criteria Search
void test_complex_multi_criteria_search() {
    MaterialLibrary lib;
    lib.load_default_materials();

    MaterialSearchCriteria criteria;
    criteria.category = MaterialCategory::Steel;
    criteria.min_yield_strength = 300e6;
    criteria.max_density = 8100.0;
    criteria.max_cost_per_kg = 10.0;

    auto results = lib.search(criteria);
    assert_equal(results.size() > 0, "Complex search criteria works");

    for (const auto* mat : results) {
        assert_equal(mat->get_category() == MaterialCategory::Steel, "Category matches");
        assert_equal(mat->get_yield_strength() >= 300e6, "Yield strength matches");
        assert_equal(mat->get_density() <= 8100.0, "Density matches");
        assert_equal(mat->get_cost_per_kg() <= 10.0, "Cost matches");
    }
}

// Test 39: Cast Iron Properties
void test_cast_iron_properties() {
    MaterialLibrary lib;
    lib.load_default_materials();

    const auto* cast_iron = lib.get_material_by_name("Cast Iron (Gray)");
    assert_equal(cast_iron != nullptr, "Cast Iron loaded");
    assert_approx_equal(cast_iron->get_thermal_conductivity(), 55.0, 1.0, "Cast Iron conductivity correct");
}

// Test 40: Material Library Clear and Rebuild
void test_library_clear_and_rebuild() {
    MaterialLibrary lib;
    lib.load_default_materials();
    size_t initial_count = lib.get_material_count();
    assert_equal(initial_count > 0, "Materials loaded");

    lib.clear();
    assert_equal(lib.get_material_count() == 0, "Library cleared");

    lib.load_default_materials();
    assert_equal(lib.get_material_count() == initial_count, "Library rebuilt");
}

// Test 41: Composition Information
void test_composition_information() {
    Material mat(100, "Custom Alloy", MaterialCategory::Steel);
    CompositionInfo comp;
    comp.primary_element = "Iron";
    comp.elements["Fe"] = 98.5;
    comp.elements["C"] = 1.0;
    comp.elements["Mn"] = 0.5;
    mat.set_composition(comp);

    const auto& retrieved_comp = mat.get_composition();
    assert_equal(retrieved_comp.primary_element == "Iron", "Composition set correctly");
}

// Test 42: Material Grade Information
void test_material_grade_information() {
    Material mat(101, "Steel with Grades", MaterialCategory::Steel);

    MaterialGrade grade1;
    grade1.name = "Grade A";
    grade1.specification = "ASTM A36";
    grade1.id = 101;
    grade1.property_modifications["yield_strength"] = 1.1;

    mat.add_grade(grade1);

    const auto* retrieved_grade = mat.get_grade("Grade A");
    assert_equal(retrieved_grade != nullptr, "Grade retrieved correctly");
    assert_equal(retrieved_grade->name == "Grade A", "Grade name matches");
}

// Test 43: Sustainability Properties Validation
void test_sustainability_validation() {
    MaterialLibrary lib;
    lib.load_default_materials();

    const auto* steel = lib.get_material_by_name("Mild Steel");
    const auto& sust = steel->get_sustainability_properties();

    assert_equal(sust.is_recyclable, "Steel is recyclable");
    assert_equal(sust.recyclability_percentage == 100.0, "Steel fully recyclable");
    assert_equal(sust.carbon_footprint > 0, "Carbon footprint is set");
}

// Test 44: Elongation at Break
void test_elongation_at_break() {
    Material mat(102, "Ductile Material", MaterialCategory::Steel);
    mat.set_elongation_at_break(0.25);
    assert_approx_equal(mat.get_elongation_at_break(), 0.25, 0.01, "Elongation set correctly");
}

// Test 45: Fatigue Properties
void test_fatigue_properties() {
    Material mat(103, "Fatigue Resistant", MaterialCategory::Steel);
    mat.set_fatigue_limit(200e6);
    mat.set_fatigue_strength_coefficient(1000e6);

    assert_approx_equal(mat.get_fatigue_limit(), 200e6, 1e6, "Fatigue limit set");
    assert_approx_equal(mat.get_fatigue_strength_coefficient(), 1000e6, 1e6, "Fatigue coefficient set");
}

// Main test runner
int main() {
    std::cout << "=== BendCore Material Library Unit Tests ===" << std::endl;
    std::cout << std::endl;

    test_basic_material_creation();
    test_mechanical_properties();
    test_thermal_properties();
    test_density_and_cost();
    test_manufacturing_properties();
    test_sustainability_properties();
    test_material_validation();
    test_material_library_creation();
    test_adding_and_retrieving_materials();
    test_material_categories();
    test_load_default_materials();
    test_search_by_modulus_range();
    test_search_by_category();
    test_search_by_yield_strength();
    test_search_by_density();
    test_search_by_cost();
    test_search_by_name_substring();
    test_material_comparison();
    test_cost_performance_ranking();
    test_strength_to_weight_ranking();
    test_stiffness_to_weight_ranking();
    test_custom_material_creation();
    test_custom_material_deletion();
    test_total_cost_calculation();
    test_mild_steel_properties();
    test_stainless_steel_304_properties();
    test_aluminum_6061_properties();
    test_titanium_grade_5_properties();
    test_copper_properties();
    test_carbon_fiber_epoxy_properties();
    test_ceramic_material_properties();
    test_polymer_material_properties();
    test_nickel_alloy_properties();
    test_magnesium_alloy_properties();
    test_manufacturing_machinability();
    test_manufacturing_weldability();
    test_thermal_conductivity_search();
    test_complex_multi_criteria_search();
    test_cast_iron_properties();
    test_library_clear_and_rebuild();
    test_composition_information();
    test_material_grade_information();
    test_sustainability_validation();
    test_elongation_at_break();
    test_fatigue_properties();

    std::cout << std::endl;
    std::cout << "=== Test Results ===" << std::endl;
    std::cout << "Passed: " << passed_tests << "/" << test_count << std::endl;

    return (passed_tests == test_count) ? 0 : 1;
}
