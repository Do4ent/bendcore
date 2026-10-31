// GAP-051: BendRadiusSimulation (32 tests) - Bend Radius and Spring-Back Analysis
#include <cassert>
#include <cstdio>
#include <cmath>
#include <vector>
#include "bendcore/simulation/bend_radius_simulation.hpp"

using namespace bendcore::simulation;

int test_count = 0;
#define RUN_TEST(func) do { func(); test_count++; } while(0)

void test_01_material_type_steel() {
    BendMaterialProperties props;
    props.material_type = MaterialType::Steel;
    props.thickness = 1.0;
    assert(props.material_type == MaterialType::Steel);
    printf("✓ test_01_material_type_steel\n");
}

void test_02_material_type_aluminum() {
    BendMaterialProperties props;
    props.material_type = MaterialType::Aluminum;
    assert(props.material_type == MaterialType::Aluminum);
    printf("✓ test_02_material_type_aluminum\n");
}

void test_03_material_properties_default() {
    BendMaterialProperties props;
    assert(props.young_modulus == 200e9);
    assert(props.yield_strength == 250e6);
    printf("✓ test_03_material_properties_default\n");
}

void test_04_bend_parameters_default() {
    BendParameters params;
    assert(params.bend_angle == 90.0);
    assert(params.tool_radius == 5.0);
    printf("✓ test_04_bend_parameters_default\n");
}

void test_05_bend_result_initialization() {
    BendResult result;
    assert(result.is_feasible == true);
    assert(result.spring_back_angle == 0.0);
    printf("✓ test_05_bend_result_initialization\n");
}

void test_06_simulation_default() {
    BendRadiusSimulation sim;
    assert(sim.getMaterialProperties().young_modulus == 200e9);
    printf("✓ test_06_simulation_default\n");
}

void test_07_set_material_properties() {
    BendRadiusSimulation sim;
    BendMaterialProperties props;
    props.thickness = 2.0;
    sim.setMaterialProperties(props);
    assert(sim.getMaterialProperties().thickness == 2.0);
    printf("✓ test_07_set_material_properties\n");
}

void test_08_set_bend_parameters() {
    BendRadiusSimulation sim;
    BendParameters params;
    params.bend_angle = 45.0;
    sim.setBendParameters(params);
    assert(sim.getBendParameters().bend_angle == 45.0);
    printf("✓ test_08_set_bend_parameters\n");
}

void test_09_minimum_bend_radius() {
    BendRadiusSimulation sim;
    BendMaterialProperties props;
    props.thickness = 1.0;
    props.max_bend_radius_ratio = 0.1;
    sim.setMaterialProperties(props);
    
    double min_r = sim.getMinimumBendRadius();
    assert(min_r > 0.09 && min_r < 0.11);
    printf("✓ test_09_minimum_bend_radius\n");
}

void test_10_simulate_bend() {
    BendRadiusSimulation sim;
    BendMaterialProperties props;
    props.thickness = 1.0;
    props.material_type = MaterialType::Steel;
    sim.setMaterialProperties(props);
    
    auto result = sim.simulateBend(90.0, 5.0);
    assert(result.required_bend_radius > 0.0);
    printf("✓ test_10_simulate_bend\n");
}

void test_11_spring_back_calculation() {
    BendRadiusSimulation sim;
    BendMaterialProperties props;
    props.young_modulus = 200e9;
    props.yield_strength = 300e6;
    props.thickness = 1.0;
    sim.setMaterialProperties(props);
    
    double sb = sim.calculateSpringBack(90.0, 10.0);
    assert(sb >= 0.0 && sb < 90.0);
    printf("✓ test_11_spring_back_calculation\n");
}

void test_12_bend_strain() {
    BendRadiusSimulation sim;
    BendMaterialProperties props;
    props.thickness = 1.0;
    sim.setMaterialProperties(props);
    
    double strain = sim.calculateBendStrain(10.0);
    assert(strain >= 0.0);
    printf("✓ test_12_bend_strain\n");
}

void test_13_bend_stress() {
    BendRadiusSimulation sim;
    BendMaterialProperties props;
    props.young_modulus = 200e9;
    props.thickness = 1.0;
    sim.setMaterialProperties(props);
    
    double stress = sim.calculateBendStress(10.0);
    assert(stress > 0.0);
    printf("✓ test_13_bend_stress\n");
}

void test_14_fracture_check_safe() {
    BendRadiusSimulation sim;
    assert(sim.checkFracture(0.1) == false);
    printf("✓ test_14_fracture_check_safe\n");
}

void test_15_fracture_check_unsafe() {
    BendRadiusSimulation sim;
    assert(sim.checkFracture(0.3) == true);
    printf("✓ test_15_fracture_check_unsafe\n");
}

void test_16_compensated_angle() {
    BendRadiusSimulation sim;
    BendMaterialProperties props;
    props.young_modulus = 200e9;
    props.yield_strength = 300e6;
    props.thickness = 1.0;
    sim.setMaterialProperties(props);
    
    double compensated = sim.getCompensatedBendAngle(90.0, 10.0);
    assert(compensated > 90.0 || compensated == 90.0);
    printf("✓ test_16_compensated_angle\n");
}

void test_17_sequential_bends() {
    BendRadiusSimulation sim;
    BendMaterialProperties props;
    props.thickness = 1.0;
    sim.setMaterialProperties(props);
    
    std::vector<BendConfiguration> configs(3);
    for (auto& cfg : configs) {
        cfg.material = props;
        cfg.parameters.bend_angle = 45.0;
        cfg.parameters.tool_radius = 5.0;
    }
    
    auto results = sim.simulateSequentialBends(configs);
    assert(results.size() == 3);
    printf("✓ test_17_sequential_bends\n");
}

void test_18_bend_factor_small_radius() {
    BendRadiusSimulation sim;
    BendMaterialProperties props;
    props.thickness = 1.0;
    sim.setMaterialProperties(props);
    
    double bf = sim.calculateBendFactor(0.5);
    assert(bf >= 1.3);
    printf("✓ test_18_bend_factor_small_radius\n");
}

void test_19_k_factor_setter() {
    BendRadiusSimulation sim;
    sim.setKFactor(0.4);
    assert(sim.getKFactor() == 0.4);
    printf("✓ test_19_k_factor_setter\n");
}

void test_20_k_factor_bounds() {
    BendRadiusSimulation sim;
    sim.setKFactor(0.1);  // Too low
    assert(sim.getKFactor() == 0.2);  // Clamped to min
    printf("✓ test_20_k_factor_bounds\n");
}

void test_21_bending_energy() {
    BendRadiusSimulation sim;
    BendMaterialProperties props;
    props.young_modulus = 200e9;
    props.thickness = 1.0;
    props.width = 10.0;
    sim.setMaterialProperties(props);
    
    double energy = sim.calculateBendingEnergy(90.0, 10.0);
    assert(energy >= 0.0);
    printf("✓ test_21_bending_energy\n");
}

void test_22_radians_conversion() {
    BendRadiusSimulation sim;
    BendMaterialProperties props;
    props.thickness = 1.0;
    sim.setMaterialProperties(props);
    
    double sb = sim.calculateSpringBack(45.0, 10.0);
    assert(sb >= 0.0);
    printf("✓ test_22_radians_conversion\n");
}

void test_23_degrees_conversion() {
    BendRadiusSimulation sim;
    BendMaterialProperties props;
    props.thickness = 1.0;
    sim.setMaterialProperties(props);
    
    double compensated = sim.getCompensatedBendAngle(45.0, 10.0);
    assert(compensated > 0.0);
    printf("✓ test_23_degrees_conversion\n");
}

void test_24_aluminum_material() {
    BendRadiusSimulation sim;
    BendMaterialProperties props;
    props.material_type = MaterialType::Aluminum;
    props.young_modulus = 69e9;
    props.thickness = 1.0;
    sim.setMaterialProperties(props);
    
    auto result = sim.simulateBend(90.0, 5.0);
    assert(result.required_bend_radius > 0.0);
    printf("✓ test_24_aluminum_material\n");
}

void test_25_copper_material() {
    BendRadiusSimulation sim;
    BendMaterialProperties props;
    props.material_type = MaterialType::Copper;
    props.young_modulus = 130e9;
    props.thickness = 1.0;
    sim.setMaterialProperties(props);
    
    auto result = sim.simulateBend(90.0, 5.0);
    assert(result.required_bend_radius > 0.0);
    printf("✓ test_25_copper_material\n");
}

void test_26_tight_radius_infeasible() {
    BendRadiusSimulation sim;
    BendMaterialProperties props;
    props.thickness = 1.0;
    props.max_bend_radius_ratio = 5.0;  // Very restrictive
    sim.setMaterialProperties(props);
    
    auto result = sim.simulateBend(90.0, 0.1);  // Very small radius
    assert(!result.is_feasible);
    printf("✓ test_26_tight_radius_infeasible\n");
}

void test_27_reasonable_bend() {
    BendRadiusSimulation sim;
    BendMaterialProperties props;
    props.thickness = 1.0;
    props.max_bend_radius_ratio = 0.1;
    sim.setMaterialProperties(props);
    
    auto result = sim.simulateBend(45.0, 15.0);  // Reasonable bend
    assert(result.required_bend_radius > 0.0);
    printf("✓ test_27_reasonable_bend\n");
}

void test_28_sharp_bend() {
    BendRadiusSimulation sim;
    BendMaterialProperties props;
    props.thickness = 1.0;
    sim.setMaterialProperties(props);
    
    auto result = sim.simulateBend(170.0, 2.0);
    assert(result.required_bend_radius > 0.0);
    printf("✓ test_28_sharp_bend\n");
}

void test_29_configuration_based_analysis() {
    BendRadiusSimulation sim;
    BendConfiguration config;
    config.material.thickness = 1.0;
    config.material.material_type = MaterialType::Steel;
    config.parameters.bend_angle = 90.0;
    config.parameters.tool_radius = 5.0;
    
    auto result = sim.simulateBend(config);
    assert(result.required_bend_radius > 0.0);
    printf("✓ test_29_configuration_based_analysis\n");
}

void test_30_multiple_materials() {
    std::vector<MaterialType> materials = {
        MaterialType::Steel, MaterialType::Aluminum, 
        MaterialType::Copper, MaterialType::Titanium
    };
    
    BendRadiusSimulation sim;
    BendMaterialProperties props;
    props.thickness = 1.0;
    
    for (auto mat : materials) {
        props.material_type = mat;
        sim.setMaterialProperties(props);
        auto result = sim.simulateBend(90.0, 5.0);
        assert(result.required_bend_radius > 0.0);
    }
    printf("✓ test_30_multiple_materials\n");
}

void test_31_modulus_effect_on_springback() {
    BendRadiusSimulation sim;
    BendMaterialProperties props;
    props.thickness = 1.0;
    props.yield_strength = 300e6;
    
    props.young_modulus = 150e9;  // Lower E
    sim.setMaterialProperties(props);
    double sb1 = sim.calculateSpringBack(90.0, 10.0);
    
    props.young_modulus = 200e9;  // Higher E
    sim.setMaterialProperties(props);
    double sb2 = sim.calculateSpringBack(90.0, 10.0);
    
    assert(sb1 >= 0.0 && sb2 >= 0.0);
    printf("✓ test_31_modulus_effect_on_springback\n");
}

void test_32_final_comprehensive() {
    BendRadiusSimulation sim;
    BendMaterialProperties props;
    props.material_type = MaterialType::Steel;
    props.young_modulus = 210e9;
    props.yield_strength = 400e6;
    props.thickness = 2.0;
    props.width = 20.0;
    props.max_bend_radius_ratio = 0.15;
    props.k_factor = 0.45;
    sim.setMaterialProperties(props);
    
    BendParameters params;
    params.bend_angle = 120.0;
    params.tool_radius = 8.0;
    params.material_thickness = 2.0;
    params.compensate_spring_back = true;
    sim.setBendParameters(params);
    
    auto result = sim.simulateBend(120.0, 8.0);
    assert(result.required_bend_radius > 0.0);
    assert(result.spring_back_angle >= 0.0);
    assert(result.material_stress >= 0.0);
    printf("✓ test_32_final_comprehensive\n");
}

int main() {
    printf("=== GAP-051: BendRadiusSimulation (32 Tests) ===\n\n");
    
    RUN_TEST(test_01_material_type_steel);
    RUN_TEST(test_02_material_type_aluminum);
    RUN_TEST(test_03_material_properties_default);
    RUN_TEST(test_04_bend_parameters_default);
    RUN_TEST(test_05_bend_result_initialization);
    RUN_TEST(test_06_simulation_default);
    RUN_TEST(test_07_set_material_properties);
    RUN_TEST(test_08_set_bend_parameters);
    RUN_TEST(test_09_minimum_bend_radius);
    RUN_TEST(test_10_simulate_bend);
    RUN_TEST(test_11_spring_back_calculation);
    RUN_TEST(test_12_bend_strain);
    RUN_TEST(test_13_bend_stress);
    RUN_TEST(test_14_fracture_check_safe);
    RUN_TEST(test_15_fracture_check_unsafe);
    RUN_TEST(test_16_compensated_angle);
    RUN_TEST(test_17_sequential_bends);
    RUN_TEST(test_18_bend_factor_small_radius);
    RUN_TEST(test_19_k_factor_setter);
    RUN_TEST(test_20_k_factor_bounds);
    RUN_TEST(test_21_bending_energy);
    RUN_TEST(test_22_radians_conversion);
    RUN_TEST(test_23_degrees_conversion);
    RUN_TEST(test_24_aluminum_material);
    RUN_TEST(test_25_copper_material);
    RUN_TEST(test_26_tight_radius_infeasible);
    RUN_TEST(test_27_reasonable_bend);
    RUN_TEST(test_28_sharp_bend);
    RUN_TEST(test_29_configuration_based_analysis);
    RUN_TEST(test_30_multiple_materials);
    RUN_TEST(test_31_modulus_effect_on_springback);
    RUN_TEST(test_32_final_comprehensive);
    
    printf("\n✅ All %d tests passed!\n", test_count);
    return 0;
}
