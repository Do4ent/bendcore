#include <gtest/gtest.h>
#include "bendcore/simulation/stress_analysis_framework.hpp"
#include <cmath>

using namespace bendcore::simulation;

// Test StressTensor class
class StressTensorTest : public ::testing::Test {
protected:
    StressTensor stress;
};

TEST_F(StressTensorTest, DefaultConstructor) {
    EXPECT_EQ(stress.xx(), 0.0);
    EXPECT_EQ(stress.yy(), 0.0);
    EXPECT_EQ(stress.zz(), 0.0);
    EXPECT_EQ(stress.xy(), 0.0);
    EXPECT_EQ(stress.yz(), 0.0);
    EXPECT_EQ(stress.xz(), 0.0);
}

TEST_F(StressTensorTest, ParameterizedConstructor) {
    StressTensor t(100e6, 50e6, 25e6, 10e6, 5e6, 2e6);
    EXPECT_EQ(t.xx(), 100e6);
    EXPECT_EQ(t.yy(), 50e6);
    EXPECT_EQ(t.zz(), 25e6);
    EXPECT_EQ(t.xy(), 10e6);
    EXPECT_EQ(t.yz(), 5e6);
    EXPECT_EQ(t.xz(), 2e6);
}

TEST_F(StressTensorTest, SettersAndGetters) {
    stress.set_xx(100e6);
    stress.set_yy(80e6);
    stress.set_zz(60e6);
    stress.set_xy(40e6);
    stress.set_yz(20e6);
    stress.set_xz(10e6);

    EXPECT_EQ(stress.xx(), 100e6);
    EXPECT_EQ(stress.yy(), 80e6);
    EXPECT_EQ(stress.zz(), 60e6);
    EXPECT_EQ(stress.xy(), 40e6);
    EXPECT_EQ(stress.yz(), 20e6);
    EXPECT_EQ(stress.xz(), 10e6);
}

TEST_F(StressTensorTest, VonMisesStress) {
    StressTensor t(100e6, 50e6, 0.0);
    double vm = t.vonMisesStress();
    EXPECT_GT(vm, 0.0);
    EXPECT_TRUE(!std::isnan(vm));
}

TEST_F(StressTensorTest, VonMisesStressZero) {
    StressTensor t;
    EXPECT_EQ(t.vonMisesStress(), 0.0);
}

TEST_F(StressTensorTest, MaxPrincipalStress) {
    StressTensor t(150e6, 100e6, 50e6);
    EXPECT_EQ(t.maxPrincipalStress(), 150e6);
}

TEST_F(StressTensorTest, MeanStress) {
    StressTensor t(90e6, 60e6, 30e6);
    EXPECT_DOUBLE_EQ(t.meanStress(), 60e6);
}

TEST_F(StressTensorTest, DeviatoricStress) {
    StressTensor t(100e6, 60e6, 20e6);
    double dev = t.deviatoricStress();
    EXPECT_GT(dev, 0.0);
    EXPECT_TRUE(!std::isnan(dev));
}

// Test StressAnalysisFramework
class StressAnalysisFrameworkTest : public ::testing::Test {
protected:
    StressAnalysisFramework framework;

    void SetUp() override {
        MaterialProperties mat;
        mat.young_modulus = 200e9;
        mat.yield_strength = 250e6;

        GeometricProperties geom;
        geom.length = 1.0;
        geom.cross_section_area = 0.01;
        geom.moment_of_inertia = 1e-5;

        framework.setMaterialProperties(mat);
        framework.setGeometricProperties(geom);
    }
};

TEST_F(StressAnalysisFrameworkTest, SetMaterialProperties) {
    auto props = framework.getMaterialProperties();
    EXPECT_EQ(props.young_modulus, 200e9);
    EXPECT_EQ(props.yield_strength, 250e6);
}

TEST_F(StressAnalysisFrameworkTest, SetGeometricProperties) {
    auto props = framework.getGeometricProperties();
    EXPECT_EQ(props.length, 1.0);
    EXPECT_EQ(props.cross_section_area, 0.01);
}

TEST_F(StressAnalysisFrameworkTest, CalculateAxialStress) {
    double stress = framework.calculateAxialStress(1000.0);
    EXPECT_DOUBLE_EQ(stress, 100000.0);
}

TEST_F(StressAnalysisFrameworkTest, CalculateAxialStressZeroArea) {
    GeometricProperties geom;
    geom.cross_section_area = 0.0;
    framework.setGeometricProperties(geom);
    double stress = framework.calculateAxialStress(1000.0);
    EXPECT_EQ(stress, 0.0);
}

TEST_F(StressAnalysisFrameworkTest, CalculateBendingStress) {
    double stress = framework.calculateBendingStress(100.0);
    EXPECT_GT(stress, 0.0);
}

TEST_F(StressAnalysisFrameworkTest, CalculateShearStress) {
    double stress = framework.calculateShearStress(500.0);
    EXPECT_DOUBLE_EQ(stress, 50000.0);
}

TEST_F(StressAnalysisFrameworkTest, AnalyzeSimpleAxialLoad) {
    LoadCase load;
    load.name = "Axial";
    load.axial_force = 1000.0;

    auto result = framework.analyzeLoadCase(load);
    EXPECT_GT(result.von_mises_equivalent, 0.0);
    EXPECT_EQ(result.is_safe, true);
}

TEST_F(StressAnalysisFrameworkTest, AnalyzeBendingLoad) {
    LoadCase load;
    load.name = "Bending";
    load.bending_moment_y = 100.0;

    auto result = framework.analyzeLoadCase(load);
    EXPECT_GT(result.von_mises_equivalent, 0.0);
}

TEST_F(StressAnalysisFrameworkTest, AnalyzeShearLoad) {
    LoadCase load;
    load.name = "Shear";
    load.shear_force_y = 500.0;

    auto result = framework.analyzeLoadCase(load);
    EXPECT_GT(result.deformation, 0.0);
}

TEST_F(StressAnalysisFrameworkTest, CombinedLoadAnalysis) {
    LoadCase load;
    load.axial_force = 1000.0;
    load.bending_moment_y = 100.0;
    load.shear_force_y = 500.0;

    auto result = framework.analyzeLoadCase(load);
    EXPECT_GT(result.von_mises_equivalent, 0.0);
    EXPECT_GT(result.strain_energy, 0.0);
    EXPECT_GT(result.safety_factor, 0.0);
}

TEST_F(StressAnalysisFrameworkTest, CalculateDeformation) {
    LoadCase load;
    load.axial_force = 1000.0;
    load.bending_moment_y = 100.0;

    double deformation = framework.calculateDeformation(load);
    EXPECT_GT(deformation, 0.0);
}

TEST_F(StressAnalysisFrameworkTest, CalculateSafetyFactor) {
    StressTensor tensor(100e6, 50e6, 25e6);
    double sf = framework.calculateSafetyFactor(tensor);
    EXPECT_GT(sf, 1.0);
}

TEST_F(StressAnalysisFrameworkTest, SafetyFactorLowStress) {
    StressTensor tensor(10e6, 5e6, 2e6);
    double sf = framework.calculateSafetyFactor(tensor);
    EXPECT_GT(sf, 10.0);
}

TEST_F(StressAnalysisFrameworkTest, AnalyzeMultipleLoadCases) {
    std::vector<LoadCase> loads(3);
    loads[0].name = "Case1";
    loads[0].axial_force = 1000.0;
    loads[1].name = "Case2";
    loads[1].bending_moment_y = 100.0;
    loads[2].name = "Case3";
    loads[2].shear_force_y = 500.0;

    auto results = framework.analyzeMultipleLoadCases(loads);
    EXPECT_EQ(results.size(), 3);
    for (const auto& result : results) {
        EXPECT_GT(result.safety_factor, 0.0);
    }
}

TEST_F(StressAnalysisFrameworkTest, ThermalLoad) {
    LoadCase load;
    load.temperature_change = 100.0;

    auto result = framework.analyzeLoadCase(load);
    EXPECT_TRUE(!std::isnan(result.strain_energy));
}

TEST_F(StressAnalysisFrameworkTest, TorsionLoad) {
    LoadCase load;
    load.torsion = 50.0;

    auto result = framework.analyzeLoadCase(load);
    EXPECT_TRUE(!std::isnan(result.von_mises_equivalent));
}

TEST_F(StressAnalysisFrameworkTest, ZeroLoadAnalysis) {
    LoadCase load;
    auto result = framework.analyzeLoadCase(load);
    EXPECT_EQ(result.von_mises_equivalent, 0.0);
    EXPECT_EQ(result.deformation, 0.0);
}

TEST_F(StressAnalysisFrameworkTest, HighStressOverYield) {
    LoadCase load;
    load.axial_force = 100000.0;  // Very high force

    auto result = framework.analyzeLoadCase(load);
    EXPECT_LT(result.safety_factor, 1.5);  // May be unsafe
}

TEST_F(StressAnalysisFrameworkTest, NegativeLoads) {
    LoadCase load;
    load.axial_force = -1000.0;
    load.bending_moment_y = -100.0;

    auto result = framework.analyzeLoadCase(load);
    EXPECT_GT(result.safety_factor, 0.0);
}

TEST_F(StressAnalysisFrameworkTest, ComplexMaterialProperties) {
    MaterialProperties mat;
    mat.young_modulus = 210e9;
    mat.poisson_ratio = 0.28;
    mat.yield_strength = 350e6;
    mat.density = 7800.0;
    mat.thermal_expansion = 11e-6;

    framework.setMaterialProperties(mat);
    auto props = framework.getMaterialProperties();

    EXPECT_EQ(props.young_modulus, 210e9);
    EXPECT_EQ(props.poisson_ratio, 0.28);
    EXPECT_EQ(props.yield_strength, 350e6);
}

TEST_F(StressAnalysisFrameworkTest, MultipleLoadCaseComparison) {
    std::vector<LoadCase> loads(5);
    for (int i = 0; i < 5; ++i) {
        loads[i].axial_force = (i + 1) * 1000.0;
    }

    auto results = framework.analyzeMultipleLoadCases(loads);

    // Higher loads should have higher stresses
    for (size_t i = 1; i < results.size(); ++i) {
        EXPECT_GT(results[i].von_mises_equivalent, results[i-1].von_mises_equivalent);
    }
}

TEST_F(StressAnalysisFrameworkTest, StrainEnergyCalculation) {
    StressTensor tensor(100e6, 50e6, 25e6);
    double energy = framework.calculateStrainEnergy(tensor);
    EXPECT_GT(energy, 0.0);
    EXPECT_TRUE(!std::isnan(energy));
}

TEST_F(StressAnalysisFrameworkTest, LargeGeometryScaling) {
    GeometricProperties geom;
    geom.length = 10.0;
    geom.cross_section_area = 1.0;
    geom.moment_of_inertia = 0.1;

    framework.setGeometricProperties(geom);

    LoadCase load;
    load.axial_force = 10000.0;

    auto result = framework.analyzeLoadCase(load);
    EXPECT_GT(result.deformation, 0.0);
}

TEST_F(StressAnalysisFrameworkTest, SmallGeometryScaling) {
    GeometricProperties geom;
    geom.length = 0.01;
    geom.cross_section_area = 0.0001;
    geom.moment_of_inertia = 1e-7;

    framework.setGeometricProperties(geom);

    LoadCase load;
    load.axial_force = 10.0;

    auto result = framework.analyzeLoadCase(load);
    EXPECT_GT(result.deformation, 0.0);
}

TEST_F(StressAnalysisFrameworkTest, StressComponentCalculations) {
    LoadCase load;
    load.axial_force = 5000.0;
    load.bending_moment_y = 500.0;
    load.bending_moment_z = 300.0;
    load.shear_force_y = 1000.0;
    load.shear_force_z = 800.0;

    auto result = framework.analyzeLoadCase(load);

    EXPECT_TRUE(!std::isnan(result.stress_tensor.xx()));
    EXPECT_TRUE(!std::isnan(result.stress_tensor.yy()));
    EXPECT_TRUE(!std::isnan(result.stress_tensor.zz()));
}

TEST_F(StressAnalysisFrameworkTest, ExtremeMaterialProperties) {
    MaterialProperties mat;
    mat.young_modulus = 100e9;
    mat.yield_strength = 100e6;

    framework.setMaterialProperties(mat);

    LoadCase load;
    load.axial_force = 1000.0;

    auto result = framework.analyzeLoadCase(load);
    EXPECT_GT(result.safety_factor, 0.0);
}

TEST_F(StressAnalysisFrameworkTest, ParallelLoadAnalysis) {
    std::vector<LoadCase> loads(10);
    for (int i = 0; i < 10; ++i) {
        loads[i].axial_force = (i + 1) * 100.0;
    }

    auto results = framework.analyzeMultipleLoadCases(loads);
    EXPECT_EQ(results.size(), 10);

    // Verify monotonic increase in stress
    for (size_t i = 1; i < results.size(); ++i) {
        EXPECT_GE(results[i].von_mises_equivalent, results[i-1].von_mises_equivalent);
    }
}
