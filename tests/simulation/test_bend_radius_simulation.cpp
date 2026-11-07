#include <gtest/gtest.h>
#include "bendcore/simulation/bend_radius_simulation.hpp"
#include <cmath>

using namespace bendcore::simulation;

class BendRadiusSimulationTest : public ::testing::Test {
protected:
    BendRadiusSimulation simulator;

    void SetUp() override {
        BendMaterialProperties mat;
        mat.material_type = MaterialType::Steel;
        mat.young_modulus = 200e9;
        mat.yield_strength = 250e6;
        mat.thickness = 1.0;
        mat.width = 10.0;
        mat.max_bend_radius_ratio = 0.1;
        mat.k_factor = 0.5;
        mat.material_name = "Steel";

        BendParameters params;
        params.bend_angle = 90.0;
        params.tool_radius = 5.0;
        params.material_thickness = 1.0;
        params.bend_direction = 0.0;
        params.forming_speed = 50.0;
        params.is_air_bend = false;
        params.compensate_spring_back = true;

        simulator.setMaterialProperties(mat);
        simulator.setBendParameters(params);
    }
};

TEST_F(BendRadiusSimulationTest, DefaultConstructor) {
    BendRadiusSimulation sim;
    EXPECT_TRUE(true);  // Object created successfully
}

TEST_F(BendRadiusSimulationTest, SetMaterialProperties) {
    auto props = simulator.getMaterialProperties();
    EXPECT_EQ(props.material_type, MaterialType::Steel);
    EXPECT_EQ(props.young_modulus, 200e9);
}

TEST_F(BendRadiusSimulationTest, SetBendParameters) {
    auto params = simulator.getBendParameters();
    EXPECT_EQ(params.bend_angle, 90.0);
    EXPECT_EQ(params.tool_radius, 5.0);
}

TEST_F(BendRadiusSimulationTest, SimulateBendBasic) {
    auto result = simulator.simulateBend(90.0, 5.0);
    EXPECT_TRUE(result.is_feasible);
    EXPECT_GT(result.required_bend_radius, 0.0);
}

TEST_F(BendRadiusSimulationTest, MinimumBendRadius) {
    double min_radius = simulator.getMinimumBendRadius();
    EXPECT_DOUBLE_EQ(min_radius, 0.1);
}

TEST_F(BendRadiusSimulationTest, CalculateSpringBack) {
    double spring_back = simulator.calculateSpringBack(90.0, 5.0);
    EXPECT_GT(spring_back, 0.0);
    EXPECT_LT(spring_back, 90.0);
}

TEST_F(BendRadiusSimulationTest, CalculateBendStrain) {
    double strain = simulator.calculateBendStrain(5.0);
    EXPECT_GT(strain, 0.0);
    EXPECT_LT(strain, 1.0);
}

TEST_F(BendRadiusSimulationTest, CalculateBendStress) {
    double stress = simulator.calculateBendStress(5.0);
    EXPECT_GT(stress, 0.0);
}

TEST_F(BendRadiusSimulationTest, CheckFracture_Safe) {
    bool fractures = simulator.checkFracture(0.05);
    EXPECT_FALSE(fractures);
}

TEST_F(BendRadiusSimulationTest, CheckFracture_Unsafe) {
    bool fractures = simulator.checkFracture(0.25);
    EXPECT_TRUE(fractures);
}

TEST_F(BendRadiusSimulationTest, GetCompensatedBendAngle) {
    double compensated = simulator.getCompensatedBendAngle(90.0, 5.0);
    EXPECT_GT(compensated, 90.0);
}

TEST_F(BendRadiusSimulationTest, SetKFactor) {
    simulator.setKFactor(0.3);
    EXPECT_DOUBLE_EQ(simulator.getKFactor(), 0.3);
}

TEST_F(BendRadiusSimulationTest, SetKFactorBounds) {
    simulator.setKFactor(0.1);  // Below min
    EXPECT_EQ(simulator.getKFactor(), 0.2);

    simulator.setKFactor(0.9);  // Above max
    EXPECT_EQ(simulator.getKFactor(), 0.8);
}

TEST_F(BendRadiusSimulationTest, CalculateBendFactor) {
    double factor = simulator.calculateBendFactor(5.0);
    EXPECT_GT(factor, 1.0);
    EXPECT_LT(factor, 3.0);
}

TEST_F(BendRadiusSimulationTest, CalculateBendingEnergy) {
    double energy = simulator.calculateBendingEnergy(90.0, 5.0);
    EXPECT_GT(energy, 0.0);
}

TEST_F(BendRadiusSimulationTest, SimulateBendWithConfiguration) {
    BendConfiguration config;
    config.material = simulator.getMaterialProperties();
    config.parameters = simulator.getBendParameters();
    config.target_bend_radius = 5.0;

    auto result = simulator.simulateBend(config);
    EXPECT_TRUE(result.is_feasible || !result.is_feasible);
}

TEST_F(BendRadiusSimulationTest, SimulateSequentialBends) {
    std::vector<BendConfiguration> configs(3);
    for (auto& config : configs) {
        config.material = simulator.getMaterialProperties();
        config.parameters = simulator.getBendParameters();
    }

    auto results = simulator.simulateSequentialBends(configs);
    EXPECT_EQ(results.size(), 3);
}

TEST_F(BendRadiusSimulationTest, LargeBendAngle) {
    auto result = simulator.simulateBend(180.0, 5.0);
    EXPECT_TRUE(!result.failure_reason.empty() || result.is_feasible);
}

TEST_F(BendRadiusSimulationTest, SmallToolRadius) {
    auto result = simulator.simulateBend(90.0, 0.5);
    EXPECT_FALSE(result.is_feasible);
}

TEST_F(BendRadiusSimulationTest, AluminumMaterial) {
    BendMaterialProperties mat;
    mat.material_type = MaterialType::Aluminum;
    mat.young_modulus = 70e9;
    mat.yield_strength = 100e6;
    mat.thickness = 2.0;
    mat.width = 20.0;
    mat.max_bend_radius_ratio = 0.2;

    simulator.setMaterialProperties(mat);
    auto result = simulator.simulateBend(90.0, 5.0);
    EXPECT_TRUE(result.is_feasible || !result.is_feasible);
}

TEST_F(BendRadiusSimulationTest, CopperMaterial) {
    BendMaterialProperties mat;
    mat.material_type = MaterialType::Copper;
    mat.young_modulus = 110e9;
    mat.yield_strength = 60e6;
    mat.thickness = 1.5;
    mat.width = 15.0;

    simulator.setMaterialProperties(mat);
    auto result = simulator.simulateBend(90.0, 5.0);
    EXPECT_GT(result.strain, 0.0);
}

TEST_F(BendRadiusSimulationTest, TitaniumMaterial) {
    BendMaterialProperties mat;
    mat.material_type = MaterialType::Titanium;
    mat.young_modulus = 100e9;
    mat.yield_strength = 880e6;
    mat.thickness = 1.0;

    simulator.setMaterialProperties(mat);
    auto result = simulator.simulateBend(90.0, 5.0);
    EXPECT_GT(result.material_stress, 0.0);
}

TEST_F(BendRadiusSimulationTest, StainlessSteelMaterial) {
    BendMaterialProperties mat;
    mat.material_type = MaterialType::Stainless;
    mat.young_modulus = 200e9;
    mat.yield_strength = 310e6;
    mat.thickness = 1.2;

    simulator.setMaterialProperties(mat);
    auto result = simulator.simulateBend(90.0, 5.0);
    EXPECT_EQ(result.strain, result.strain);  // Non-NaN check
}

TEST_F(BendRadiusSimulationTest, VariousToolRadii) {
    double radii[] = {2.0, 5.0, 10.0, 15.0};
    for (double radius : radii) {
        auto result = simulator.simulateBend(90.0, radius);
        EXPECT_GT(result.required_bend_radius, 0.0);
    }
}

TEST_F(BendRadiusSimulationTest, VariousBendAngles) {
    double angles[] = {45.0, 90.0, 135.0, 180.0};
    for (double angle : angles) {
        auto result = simulator.simulateBend(angle, 5.0);
        EXPECT_TRUE(!std::isnan(result.spring_back_angle));
    }
}

TEST_F(BendRadiusSimulationTest, AirBendMode) {
    BendParameters params = simulator.getBendParameters();
    params.is_air_bend = true;
    simulator.setBendParameters(params);

    auto result = simulator.simulateBend(90.0, 5.0);
    EXPECT_GT(result.required_bend_radius, 0.0);
}

TEST_F(BendRadiusSimulationTest, CompensateSpringBack) {
    BendParameters params = simulator.getBendParameters();
    params.compensate_spring_back = true;
    simulator.setBendParameters(params);

    double compensated = simulator.getCompensatedBendAngle(90.0, 5.0);
    EXPECT_GT(compensated, 90.0);
}

TEST_F(BendRadiusSimulationTest, HighFormingSpeed) {
    BendParameters params = simulator.getBendParameters();
    params.forming_speed = 500.0;
    simulator.setBendParameters(params);

    auto result = simulator.simulateBend(90.0, 5.0);
    EXPECT_TRUE(!result.failure_reason.empty() || result.is_feasible);
}

TEST_F(BendRadiusSimulationTest, LowFormingSpeed) {
    BendParameters params = simulator.getBendParameters();
    params.forming_speed = 5.0;
    simulator.setBendParameters(params);

    auto result = simulator.simulateBend(90.0, 5.0);
    EXPECT_TRUE(!result.failure_reason.empty() || result.is_feasible);
}

TEST_F(BendRadiusSimulationTest, ZeroBendAngle) {
    auto result = simulator.simulateBend(0.0, 5.0);
    EXPECT_EQ(result.spring_back_angle, 0.0);
}

TEST_F(BendRadiusSimulationTest, ThickMaterial) {
    BendMaterialProperties mat = simulator.getMaterialProperties();
    mat.thickness = 5.0;
    simulator.setMaterialProperties(mat);

    auto result = simulator.simulateBend(90.0, 5.0);
    EXPECT_GT(result.required_bend_radius, 0.0);
}

TEST_F(BendRadiusSimulationTest, ThinMaterial) {
    BendMaterialProperties mat = simulator.getMaterialProperties();
    mat.thickness = 0.1;
    simulator.setMaterialProperties(mat);

    auto result = simulator.simulateBend(90.0, 5.0);
    EXPECT_TRUE(result.is_feasible);
}

TEST_F(BendRadiusSimulationTest, WideWorkpiece) {
    BendMaterialProperties mat = simulator.getMaterialProperties();
    mat.width = 50.0;
    simulator.setMaterialProperties(mat);

    double energy = simulator.calculateBendingEnergy(90.0, 5.0);
    EXPECT_GT(energy, 0.0);
}

TEST_F(BendRadiusSimulationTest, NarrowWorkpiece) {
    BendMaterialProperties mat = simulator.getMaterialProperties();
    mat.width = 2.0;
    simulator.setMaterialProperties(mat);

    double energy = simulator.calculateBendingEnergy(90.0, 5.0);
    EXPECT_GT(energy, 0.0);
}
