// GAP-050: StressAnalysisFramework (35 tests) - Comprehensive Stress Analysis
#include <cassert>
#include <cstdio>
#include <cmath>
#include <vector>
#include "bendcore/simulation/stress_analysis_framework.hpp"

using namespace bendcore::simulation;

// Test counter
int test_count = 0;

#define RUN_TEST(func) do { func(); test_count++; } while(0)

void test_01_stress_tensor_default() {
    StressTensor st;
    assert(st.xx() == 0.0);
    assert(st.yy() == 0.0);
    assert(st.zz() == 0.0);
    printf("✓ test_01_stress_tensor_default\n");
}

void test_02_stress_tensor_initialization() {
    StressTensor st(100e6, 50e6, 25e6, 10e6, 5e6, 2e6);
    assert(st.xx() == 100e6);
    assert(st.yy() == 50e6);
    assert(st.zz() == 25e6);
    assert(st.xy() == 10e6);
    printf("✓ test_02_stress_tensor_initialization\n");
}

void test_03_von_mises_simple() {
    StressTensor st(100e6, 0, 0);
    double vm = st.vonMisesStress();
    assert(vm > 99e6 && vm < 101e6);
    printf("✓ test_03_von_mises_simple\n");
}

void test_04_von_mises_biaxial() {
    StressTensor st(100e6, 50e6, 0);
    double vm = st.vonMisesStress();
    assert(vm > 85e6 && vm < 95e6);
    printf("✓ test_04_von_mises_biaxial\n");
}

void test_05_mean_stress_calculation() {
    StressTensor st(90e6, 60e6, 30e6);
    double mean = st.meanStress();
    assert(std::abs(mean - 60e6) < 1e3);
    printf("✓ test_05_mean_stress_calculation\n");
}

void test_06_deviatoric_stress() {
    StressTensor st(100e6, 50e6, 25e6);
    double dev = st.deviatoricStress();
    assert(dev > 0.0);
    printf("✓ test_06_deviatoric_stress\n");
}

void test_07_principal_stress() {
    StressTensor st(150e6, 100e6, 50e6);
    double max_p = st.maxPrincipalStress();
    assert(max_p == 150e6);
    printf("✓ test_07_principal_stress\n");
}

void test_08_stress_tensor_setters() {
    StressTensor st;
    st.set_xx(100e6);
    st.set_yy(50e6);
    st.set_xy(25e6);
    assert(st.xx() == 100e6);
    assert(st.yy() == 50e6);
    assert(st.xy() == 25e6);
    printf("✓ test_08_stress_tensor_setters\n");
}

void test_09_material_properties_default() {
    MaterialProperties props;
    assert(props.young_modulus == 200e9);
    assert(props.poisson_ratio == 0.3);
    assert(props.yield_strength == 250e6);
    assert(props.density == 7850.0);
    printf("✓ test_09_material_properties_default\n");
}

void test_10_material_properties_modification() {
    MaterialProperties props;
    props.young_modulus = 100e9;
    props.yield_strength = 500e6;
    assert(props.young_modulus == 100e9);
    assert(props.yield_strength == 500e6);
    printf("✓ test_10_material_properties_modification\n");
}

void test_11_geometric_properties() {
    GeometricProperties geo;
    geo.length = 2.0;
    geo.cross_section_area = 0.01;
    assert(geo.length == 2.0);
    assert(geo.cross_section_area == 0.01);
    printf("✓ test_11_geometric_properties\n");
}

void test_12_load_case_default() {
    LoadCase lc;
    assert(lc.axial_force == 0.0);
    assert(lc.temperature_change == 0.0);
    printf("✓ test_12_load_case_default\n");
}

void test_13_load_case_initialization() {
    LoadCase lc;
    lc.name = "TestLoad";
    lc.axial_force = 1000.0;
    lc.shear_force_y = 500.0;
    assert(lc.name == "TestLoad");
    assert(lc.axial_force == 1000.0);
    printf("✓ test_13_load_case_initialization\n");
}

void test_14_framework_default() {
    StressAnalysisFramework framework;
    MaterialProperties mat = framework.getMaterialProperties();
    assert(mat.young_modulus > 0);
    printf("✓ test_14_framework_default\n");
}

void test_15_framework_set_material() {
    StressAnalysisFramework framework;
    MaterialProperties props;
    props.yield_strength = 500e6;
    framework.setMaterialProperties(props);
    assert(framework.getMaterialProperties().yield_strength == 500e6);
    printf("✓ test_15_framework_set_material\n");
}

void test_16_framework_set_geometry() {
    StressAnalysisFramework framework;
    GeometricProperties geo;
    geo.length = 3.0;
    framework.setGeometricProperties(geo);
    assert(framework.getGeometricProperties().length == 3.0);
    printf("✓ test_16_framework_set_geometry\n");
}

void test_17_axial_stress() {
    StressAnalysisFramework framework;
    GeometricProperties geo;
    geo.cross_section_area = 0.01;
    framework.setGeometricProperties(geo);
    
    double stress = framework.calculateAxialStress(100000.0);
    assert(stress > 9.99e6 && stress < 10.01e6);
    printf("✓ test_17_axial_stress\n");
}

void test_18_bending_stress() {
    StressAnalysisFramework framework;
    GeometricProperties geo;
    geo.moment_of_inertia = 1e-5;
    framework.setGeometricProperties(geo);
    
    double stress = framework.calculateBendingStress(1000.0);
    assert(stress > 0.0);
    printf("✓ test_18_bending_stress\n");
}

void test_19_shear_stress() {
    StressAnalysisFramework framework;
    GeometricProperties geo;
    geo.cross_section_area = 0.01;
    framework.setGeometricProperties(geo);
    
    double stress = framework.calculateShearStress(10000.0);
    assert(stress > 0.0);
    printf("✓ test_19_shear_stress\n");
}

void test_20_deformation_calculation() {
    StressAnalysisFramework framework;
    MaterialProperties mat;
    GeometricProperties geo;
    geo.length = 1.0;
    geo.cross_section_area = 0.01;
    framework.setMaterialProperties(mat);
    framework.setGeometricProperties(geo);
    
    LoadCase lc;
    lc.axial_force = 10000.0;
    double deform = framework.calculateDeformation(lc);
    assert(deform >= 0.0);
    printf("✓ test_20_deformation_calculation\n");
}

void test_21_strain_energy() {
    StressAnalysisFramework framework;
    MaterialProperties mat;
    GeometricProperties geo;
    geo.cross_section_area = 0.01;
    geo.length = 1.0;
    framework.setMaterialProperties(mat);
    framework.setGeometricProperties(geo);
    
    StressTensor st(100e6, 50e6, 0.0);
    double energy = framework.calculateStrainEnergy(st);
    assert(energy >= 0.0);
    printf("✓ test_21_strain_energy\n");
}

void test_22_safety_factor() {
    StressAnalysisFramework framework;
    MaterialProperties mat;
    mat.yield_strength = 250e6;
    framework.setMaterialProperties(mat);
    
    StressTensor st(100e6, 50e6, 0.0);
    double sf = framework.calculateSafetyFactor(st);
    assert(sf > 1.0);
    printf("✓ test_22_safety_factor\n");
}

void test_23_analyze_load_case() {
    StressAnalysisFramework framework;
    MaterialProperties mat;
    GeometricProperties geo;
    geo.cross_section_area = 0.01;
    geo.length = 1.0;
    framework.setMaterialProperties(mat);
    framework.setGeometricProperties(geo);
    
    LoadCase lc;
    lc.axial_force = 10000.0;
    
    auto result = framework.analyzeLoadCase(lc);
    assert(result.von_mises_equivalent >= 0.0);
    assert(result.safety_factor > 0.0);
    printf("✓ test_23_analyze_load_case\n");
}

void test_24_analyze_multiple_cases() {
    StressAnalysisFramework framework;
    MaterialProperties mat;
    GeometricProperties geo;
    geo.cross_section_area = 0.01;
    geo.length = 1.0;
    framework.setMaterialProperties(mat);
    framework.setGeometricProperties(geo);
    
    std::vector<LoadCase> loads(5);
    for (int i = 0; i < 5; i++) {
        loads[i].axial_force = 1000.0 * (i + 1);
    }
    
    auto results = framework.analyzeMultipleLoadCases(loads);
    assert(results.size() == 5);
    printf("✓ test_24_analyze_multiple_cases\n");
}

void test_25_combined_loading() {
    StressAnalysisFramework framework;
    MaterialProperties mat;
    GeometricProperties geo;
    geo.cross_section_area = 0.01;
    geo.length = 1.0;
    geo.moment_of_inertia = 1e-5;
    framework.setMaterialProperties(mat);
    framework.setGeometricProperties(geo);
    
    LoadCase lc;
    lc.axial_force = 5000.0;
    lc.shear_force_y = 2000.0;
    lc.bending_moment_z = 50.0;
    
    auto result = framework.analyzeLoadCase(lc);
    assert(result.von_mises_equivalent > 0.0);
    printf("✓ test_25_combined_loading\n");
}

void test_26_zero_stress() {
    StressTensor st;
    double vm = st.vonMisesStress();
    assert(vm == 0.0);
    printf("✓ test_26_zero_stress\n");
}

void test_27_shear_only() {
    StressTensor st(0, 0, 0, 100e6, 0, 0);
    double vm = st.vonMisesStress();
    assert(vm > 170e6);
    printf("✓ test_27_shear_only\n");
}

void test_28_hydrostatic() {
    StressTensor st(100e6, 100e6, 100e6);
    double vm = st.vonMisesStress();
    assert(vm < 1e3);
    printf("✓ test_28_hydrostatic\n");
}

void test_29_negative_stress() {
    StressTensor st(-100e6, -50e6, 0);
    double vm = st.vonMisesStress();
    assert(vm >= 0.0);
    printf("✓ test_29_negative_stress\n");
}

void test_30_large_load_case() {
    StressAnalysisFramework framework;
    MaterialProperties mat;
    GeometricProperties geo;
    geo.cross_section_area = 0.01;
    geo.length = 10.0;
    framework.setMaterialProperties(mat);
    framework.setGeometricProperties(geo);
    
    LoadCase lc;
    lc.axial_force = 1e6;
    lc.bending_moment_y = 1e4;
    lc.temperature_change = 100.0;
    
    auto result = framework.analyzeLoadCase(lc);
    assert(result.von_mises_equivalent > 0.0);
    printf("✓ test_30_large_load_case\n");
}

void test_31_safety_margin() {
    StressAnalysisFramework framework;
    MaterialProperties mat;
    mat.yield_strength = 500e6;
    GeometricProperties geo;
    geo.cross_section_area = 1.0;
    framework.setMaterialProperties(mat);
    framework.setGeometricProperties(geo);
    
    LoadCase lc;
    lc.axial_force = 1000.0;
    
    auto result = framework.analyzeLoadCase(lc);
    assert(result.is_safe);
    printf("✓ test_31_safety_margin\n");
}

void test_32_stress_combination() {
    StressTensor st(100e6, 50e6, 25e6);
    double mean = st.meanStress();
    double dev = st.deviatoricStress();
    assert(mean > 0.0 && dev > 0.0);
    printf("✓ test_32_stress_combination\n");
}

void test_33_multiaxial_stress() {
    StressTensor st(150e6, 100e6, 50e6, 30e6, 20e6, 10e6);
    double vm = st.vonMisesStress();
    double dev = st.deviatoricStress();
    assert(vm > 0.0 && dev > 0.0);
    printf("✓ test_33_multiaxial_stress\n");
}

void test_34_strain_energy_zero() {
    StressAnalysisFramework framework;
    MaterialProperties mat;
    GeometricProperties geo;
    geo.cross_section_area = 0.01;
    geo.length = 1.0;
    framework.setMaterialProperties(mat);
    framework.setGeometricProperties(geo);
    
    StressTensor st;
    double energy = framework.calculateStrainEnergy(st);
    assert(energy == 0.0);
    printf("✓ test_34_strain_energy_zero\n");
}

void test_35_complete_analysis() {
    StressAnalysisFramework framework;
    MaterialProperties mat;
    mat.yield_strength = 400e6;
    GeometricProperties geo;
    geo.length = 2.0;
    geo.cross_section_area = 0.02;
    geo.moment_of_inertia = 2e-5;
    framework.setMaterialProperties(mat);
    framework.setGeometricProperties(geo);
    
    LoadCase lc;
    lc.axial_force = 50000.0;
    lc.shear_force_y = 10000.0;
    lc.bending_moment_z = 500.0;
    
    auto result = framework.analyzeLoadCase(lc);
    assert(result.von_mises_equivalent > 0.0);
    assert(result.safety_factor > 0.0);
    assert(result.deformation >= 0.0);
    assert(result.strain_energy >= 0.0);
    printf("✓ test_35_complete_analysis\n");
}

int main() {
    printf("=== GAP-050: StressAnalysisFramework (35 Tests) ===\n\n");
    
    RUN_TEST(test_01_stress_tensor_default);
    RUN_TEST(test_02_stress_tensor_initialization);
    RUN_TEST(test_03_von_mises_simple);
    RUN_TEST(test_04_von_mises_biaxial);
    RUN_TEST(test_05_mean_stress_calculation);
    RUN_TEST(test_06_deviatoric_stress);
    RUN_TEST(test_07_principal_stress);
    RUN_TEST(test_08_stress_tensor_setters);
    RUN_TEST(test_09_material_properties_default);
    RUN_TEST(test_10_material_properties_modification);
    RUN_TEST(test_11_geometric_properties);
    RUN_TEST(test_12_load_case_default);
    RUN_TEST(test_13_load_case_initialization);
    RUN_TEST(test_14_framework_default);
    RUN_TEST(test_15_framework_set_material);
    RUN_TEST(test_16_framework_set_geometry);
    RUN_TEST(test_17_axial_stress);
    RUN_TEST(test_18_bending_stress);
    RUN_TEST(test_19_shear_stress);
    RUN_TEST(test_20_deformation_calculation);
    RUN_TEST(test_21_strain_energy);
    RUN_TEST(test_22_safety_factor);
    RUN_TEST(test_23_analyze_load_case);
    RUN_TEST(test_24_analyze_multiple_cases);
    RUN_TEST(test_25_combined_loading);
    RUN_TEST(test_26_zero_stress);
    RUN_TEST(test_27_shear_only);
    RUN_TEST(test_28_hydrostatic);
    RUN_TEST(test_29_negative_stress);
    RUN_TEST(test_30_large_load_case);
    RUN_TEST(test_31_safety_margin);
    RUN_TEST(test_32_stress_combination);
    RUN_TEST(test_33_multiaxial_stress);
    RUN_TEST(test_34_strain_energy_zero);
    RUN_TEST(test_35_complete_analysis);
    
    printf("\n✅ All %d tests passed!\n", test_count);
    return 0;
}
