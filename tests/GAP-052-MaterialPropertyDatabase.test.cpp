// GAP-052: MaterialPropertyDatabase (38 tests) - Material Property Management
#include <cassert>
#include <cstdio>
#include <string>
#include <vector>
#include "bendcore/materials/material_property_database.hpp"

using namespace bendcore::materials;

int test_count = 0;
#define RUN_TEST(func) do { func(); test_count++; } while(0)

void test_01_database_creation() {
    MaterialPropertyDatabase db;
    assert(db.getMaterialCount() > 0);
    printf("✓ test_01_database_creation\n");
}

void test_02_standard_materials_loaded() {
    MaterialPropertyDatabase db;
    assert(db.getMaterialCount() >= 6);  // At least 6 standard materials
    printf("✓ test_02_standard_materials_loaded\n");
}

void test_03_get_steel_aisi1045() {
    MaterialPropertyDatabase db;
    auto opt = db.getMaterial("steel_aisi1045");
    assert(opt.has_value());
    assert(opt->material_name == "AISI 1045 Steel");
    printf("✓ test_03_get_steel_aisi1045\n");
}

void test_04_get_stainless() {
    MaterialPropertyDatabase db;
    auto opt = db.getMaterial("steel_stainless304");
    assert(opt.has_value());
    assert(opt->material_class == MaterialClass::Stainless);
    printf("✓ test_04_get_stainless\n");
}

void test_05_get_aluminum_6061() {
    MaterialPropertyDatabase db;
    auto opt = db.getMaterial("aluminum_6061");
    assert(opt.has_value());
    assert(opt->material_class == MaterialClass::Aluminum);
    printf("✓ test_05_get_aluminum_6061\n");
}

void test_06_get_copper() {
    MaterialPropertyDatabase db;
    auto opt = db.getMaterial("copper_pure");
    assert(opt.has_value());
    assert(opt->material_class == MaterialClass::Copper);
    printf("✓ test_06_get_copper\n");
}

void test_07_get_titanium() {
    MaterialPropertyDatabase db;
    auto opt = db.getMaterial("titanium_grade5");
    assert(opt.has_value());
    assert(opt->material_class == MaterialClass::Titanium);
    printf("✓ test_07_get_titanium\n");
}

void test_08_get_nonexistent() {
    MaterialPropertyDatabase db;
    auto opt = db.getMaterial("nonexistent");
    assert(!opt.has_value());
    printf("✓ test_08_get_nonexistent\n");
}

void test_09_get_by_class_steel() {
    MaterialPropertyDatabase db;
    auto steels = db.getMaterialsByClass(MaterialClass::Steel);
    assert(steels.size() >= 1);
    printf("✓ test_09_get_by_class_steel\n");
}

void test_10_get_by_class_aluminum() {
    MaterialPropertyDatabase db;
    auto alus = db.getMaterialsByClass(MaterialClass::Aluminum);
    assert(alus.size() >= 2);
    printf("✓ test_10_get_by_class_aluminum\n");
}

void test_11_get_by_name_pattern() {
    MaterialPropertyDatabase db;
    auto results = db.getMaterialsByName("Steel");
    assert(results.size() >= 1);
    printf("✓ test_11_get_by_name_pattern\n");
}

void test_12_get_all_materials() {
    MaterialPropertyDatabase db;
    auto all = db.getAllMaterials();
    assert(all.size() >= 6);
    printf("✓ test_12_get_all_materials\n");
}

void test_13_add_custom_material() {
    MaterialPropertyDatabase db;
    MaterialProperty custom;
    custom.material_id = "custom_bronze";
    custom.material_name = "Custom Bronze";
    custom.material_class = MaterialClass::Copper;
    custom.density = 8900.0;
    
    bool added = db.addMaterial(custom);
    assert(added);
    
    auto opt = db.getMaterial("custom_bronze");
    assert(opt.has_value());
    printf("✓ test_13_add_custom_material\n");
}

void test_14_remove_material() {
    MaterialPropertyDatabase db;
    MaterialProperty custom;
    custom.material_id = "temp_material";
    custom.material_name = "Temp Material";
    custom.material_class = MaterialClass::Other;
    db.addMaterial(custom);
    
    bool removed = db.removeMaterial("temp_material");
    assert(removed);
    
    auto opt = db.getMaterial("temp_material");
    assert(!opt.has_value());
    printf("✓ test_14_remove_material\n");
}

void test_15_search_by_yield_strength() {
    MaterialPropertyDatabase db;
    auto results = db.searchByYieldStrength(200e6, 350e6, 20.0);
    assert(results.size() > 0);
    printf("✓ test_15_search_by_yield_strength\n");
}

void test_16_search_by_density_light() {
    MaterialPropertyDatabase db;
    auto results = db.searchByDensity(2000, 3000);
    assert(results.size() >= 2);  // Aluminum alloys
    printf("✓ test_16_search_by_density_light\n");
}

void test_17_search_by_density_heavy() {
    MaterialPropertyDatabase db;
    auto results = db.searchByDensity(7000, 9000);
    assert(results.size() > 0);  // Steel, Copper
    printf("✓ test_17_search_by_density_heavy\n");
}

void test_18_search_by_thermal_conductivity() {
    MaterialPropertyDatabase db;
    auto results = db.searchByThermalConductivity(100, 400, 20.0);
    assert(results.size() > 0);
    printf("✓ test_18_search_by_thermal_conductivity\n");
}

void test_19_get_at_temperature() {
    MaterialPropertyDatabase db;
    auto steel = db.getMaterialAtTemperature("steel_aisi1045", 100.0);
    assert(steel.material_id == "steel_aisi1045");
    printf("✓ test_19_get_at_temperature\n");
}

void test_20_compare_properties() {
    MaterialPropertyDatabase db;
    std::vector<std::string> ids = {"steel_aisi1045", "aluminum_6061"};
    auto comparison = db.compareProperties(ids, "density", 20.0);
    assert(comparison.size() > 0);
    printf("✓ test_20_compare_properties\n");
}

void test_21_temperature_dependent_young_modulus() {
    MaterialPropertyDatabase db;
    auto opt = db.getMaterial("steel_aisi1045");
    assert(opt.has_value());
    
    double e20 = opt->young_modulus.getValueAtTemperature(20.0);
    double e100 = opt->young_modulus.getValueAtTemperature(100.0);
    assert(e20 > 0.0 && e100 > 0.0);
    printf("✓ test_21_temperature_dependent_young_modulus\n");
}

void test_22_material_properties_structure() {
    MaterialProperty mat;
    mat.material_id = "test";
    mat.material_name = "Test Material";
    mat.density = 7850.0;
    mat.hardness_hv = 150;
    mat.elongation_percentage = 20.0;
    
    assert(mat.density == 7850.0);
    assert(mat.hardness_hv == 150);
    assert(mat.elongation_percentage == 20.0);
    printf("✓ test_22_material_properties_structure\n");
}

void test_23_temperature_dependent_property() {
    TemperatureDependentProperty prop;
    prop.reference_temperature = 20.0;
    prop.base_value = 200e9;
    prop.temperature_coefficient = -1e6;
    
    double val20 = prop.getValueAtTemperature(20.0);
    double val100 = prop.getValueAtTemperature(100.0);
    
    assert(val20 == 200e9);
    assert(val100 < val20);
    printf("✓ test_23_temperature_dependent_property\n");
}

void test_24_clear_database() {
    MaterialPropertyDatabase db;
    int count_before = db.getMaterialCount();
    
    MaterialProperty custom;
    custom.material_id = "custom";
    custom.material_name = "Custom";
    custom.material_class = MaterialClass::Other;
    db.addMaterial(custom);
    
    db.clear();
    assert(db.getMaterialCount() == 0);
    printf("✓ test_24_clear_database\n");
}

void test_25_steel_properties() {
    MaterialPropertyDatabase db;
    auto opt = db.getMaterial("steel_aisi1045");
    assert(opt.has_value());
    
    auto steel = *opt;
    assert(steel.density == 7850.0);
    assert(steel.hardness_hv == 163);
    assert(steel.machinability_rating > 0);
    printf("✓ test_25_steel_properties\n");
}

void test_26_aluminum_properties() {
    MaterialPropertyDatabase db;
    auto opt = db.getMaterial("aluminum_6061");
    assert(opt.has_value());
    
    auto alu = *opt;
    assert(alu.density == 2700.0);
    assert(alu.density < 5000.0);  // Lighter than steel
    printf("✓ test_26_aluminum_properties\n");
}

void test_27_copper_properties() {
    MaterialPropertyDatabase db;
    auto opt = db.getMaterial("copper_pure");
    assert(opt.has_value());
    
    auto copper = *opt;
    assert(copper.density > 8000.0);
    assert(copper.thermal_conductivity.base_value > 300);
    printf("✓ test_27_copper_properties\n");
}

void test_28_titanium_properties() {
    MaterialPropertyDatabase db;
    auto opt = db.getMaterial("titanium_grade5");
    assert(opt.has_value());
    
    auto ti = *opt;
    assert(ti.yield_strength.base_value > 800e6);
    assert(ti.relative_cost > 4.0);
    printf("✓ test_28_titanium_properties\n");
}

void test_29_multiple_material_search() {
    MaterialPropertyDatabase db;
    auto high_strength = db.searchByYieldStrength(300e6, 1000e6, 20.0);
    assert(high_strength.size() > 0);
    
    auto low_density = db.searchByDensity(2000, 3000);
    assert(low_density.size() > 0);
    printf("✓ test_29_multiple_material_search\n");
}

void test_30_factory_create_steel() {
    auto steel = factory::createSteel_AISI1045();
    assert(steel.material_id == "steel_aisi1045");
    assert(steel.material_class == MaterialClass::Steel);
    printf("✓ test_30_factory_create_steel\n");
}

void test_31_factory_create_aluminum() {
    auto alu = factory::createAluminum_6061();
    assert(alu.material_id == "aluminum_6061");
    assert(alu.material_class == MaterialClass::Aluminum);
    printf("✓ test_31_factory_create_aluminum\n");
}

void test_32_factory_create_titanium() {
    auto ti = factory::createTitanium_Grade5();
    assert(ti.material_id == "titanium_grade5");
    assert(ti.material_class == MaterialClass::Titanium);
    printf("✓ test_32_factory_create_titanium\n");
}

void test_33_workability_ratings() {
    MaterialPropertyDatabase db;
    auto opt = db.getMaterial("steel_aisi1045");
    assert(opt.has_value());
    
    auto steel = *opt;
    assert(steel.machinability_rating >= 0 && steel.machinability_rating <= 100);
    assert(steel.weldability_rating >= 0 && steel.weldability_rating <= 100);
    printf("✓ test_33_workability_ratings\n");
}

void test_34_corrosion_resistance() {
    MaterialPropertyDatabase db;
    auto opt = db.getMaterial("steel_stainless304");
    assert(opt.has_value());
    
    auto ss = *opt;
    assert(ss.corrosion_resistance_rating > 80);
    printf("✓ test_34_corrosion_resistance\n");
}

void test_35_cost_comparison() {
    MaterialPropertyDatabase db;
    std::vector<std::string> ids = {"aluminum_6061", "copper_pure", "titanium_grade5"};
    auto all = db.getAllMaterials();
    
    double aluminum_cost = 0;
    double copper_cost = 0;
    double titanium_cost = 0;
    
    for (const auto& mat : all) {
        if (mat.material_id == "aluminum_6061") aluminum_cost = mat.relative_cost;
        if (mat.material_id == "copper_pure") copper_cost = mat.relative_cost;
        if (mat.material_id == "titanium_grade5") titanium_cost = mat.relative_cost;
    }
    
    assert(aluminum_cost < titanium_cost);
    assert(copper_cost > aluminum_cost);
    printf("✓ test_35_cost_comparison\n");
}

void test_36_mechanical_property_ranges() {
    MaterialPropertyDatabase db;
    auto steels = db.getMaterialsByClass(MaterialClass::Steel);
    
    for (const auto& steel : steels) {
        assert(steel.young_modulus.base_value > 0);
        assert(steel.yield_strength.base_value > 0);
        assert(steel.density > 0);
    }
    printf("✓ test_36_mechanical_property_ranges\n");
}

void test_37_thermal_properties() {
    MaterialPropertyDatabase db;
    auto opt = db.getMaterial("copper_pure");
    assert(opt.has_value());
    
    auto copper = *opt;
    assert(copper.thermal_conductivity.base_value > 100);
    assert(copper.specific_heat.base_value > 0);
    printf("✓ test_37_thermal_properties\n");
}

void test_38_complete_material_info() {
    MaterialPropertyDatabase db;
    auto opt = db.getMaterial("steel_aisi1045");
    assert(opt.has_value());
    
    auto mat = *opt;
    assert(!mat.material_id.empty());
    assert(!mat.material_name.empty());
    assert(mat.young_modulus.base_value > 0);
    assert(mat.density > 0);
    assert(mat.hardness_hv > 0);
    assert(mat.elongation_percentage > 0);
    printf("✓ test_38_complete_material_info\n");
}

int main() {
    printf("=== GAP-052: MaterialPropertyDatabase (38 Tests) ===\n\n");
    
    RUN_TEST(test_01_database_creation);
    RUN_TEST(test_02_standard_materials_loaded);
    RUN_TEST(test_03_get_steel_aisi1045);
    RUN_TEST(test_04_get_stainless);
    RUN_TEST(test_05_get_aluminum_6061);
    RUN_TEST(test_06_get_copper);
    RUN_TEST(test_07_get_titanium);
    RUN_TEST(test_08_get_nonexistent);
    RUN_TEST(test_09_get_by_class_steel);
    RUN_TEST(test_10_get_by_class_aluminum);
    RUN_TEST(test_11_get_by_name_pattern);
    RUN_TEST(test_12_get_all_materials);
    RUN_TEST(test_13_add_custom_material);
    RUN_TEST(test_14_remove_material);
    RUN_TEST(test_15_search_by_yield_strength);
    RUN_TEST(test_16_search_by_density_light);
    RUN_TEST(test_17_search_by_density_heavy);
    RUN_TEST(test_18_search_by_thermal_conductivity);
    RUN_TEST(test_19_get_at_temperature);
    RUN_TEST(test_20_compare_properties);
    RUN_TEST(test_21_temperature_dependent_young_modulus);
    RUN_TEST(test_22_material_properties_structure);
    RUN_TEST(test_23_temperature_dependent_property);
    RUN_TEST(test_24_clear_database);
    RUN_TEST(test_25_steel_properties);
    RUN_TEST(test_26_aluminum_properties);
    RUN_TEST(test_27_copper_properties);
    RUN_TEST(test_28_titanium_properties);
    RUN_TEST(test_29_multiple_material_search);
    RUN_TEST(test_30_factory_create_steel);
    RUN_TEST(test_31_factory_create_aluminum);
    RUN_TEST(test_32_factory_create_titanium);
    RUN_TEST(test_33_workability_ratings);
    RUN_TEST(test_34_corrosion_resistance);
    RUN_TEST(test_35_cost_comparison);
    RUN_TEST(test_36_mechanical_property_ranges);
    RUN_TEST(test_37_thermal_properties);
    RUN_TEST(test_38_complete_material_info);
    
    printf("\n✅ All %d tests passed!\n", test_count);
    return 0;
}
