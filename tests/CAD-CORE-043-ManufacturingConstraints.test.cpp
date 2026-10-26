#include <gtest/gtest.h>
#include "../include/bendcore/manufacturing_constraints.hpp"
#include <sstream>
#include <cmath>

using namespace bendcore::manufacturing;

// ============================================================================
// TubeBendingConstraint Tests
// ============================================================================

class TubeBendingConstraintTest : public ::testing::Test {
protected:
    TubeBendingConstraint createValidTubeConstraint() const {
        return TubeBendingConstraint{
            .min_bend_radius_mm = 10.0,
            .max_bend_radius_mm = 50.0,
            .min_wall_thickness_mm = 1.0,
            .max_wall_thickness_mm = 5.0,
            .min_diameter_mm = 6.35,
            .max_diameter_mm = 50.8,
            .diameter_to_thickness_ratio = 50.0,
            .spring_back_factor = 0.85,
            .min_bend_angle_deg = 10.0,
            .max_bend_angle_deg = 175.0,
            .material_name = "Steel"
        };
    }
};

TEST_F(TubeBendingConstraintTest, ValidConstraintPassesValidation) {
    auto constraint = createValidTubeConstraint();
    EXPECT_TRUE(constraint.validate());
}

TEST_F(TubeBendingConstraintTest, InvalidBendRadiusThrowsException) {
    auto constraint = createValidTubeConstraint();
    constraint.min_bend_radius_mm = 60.0;  // Greater than max
    constraint.max_bend_radius_mm = 50.0;
    EXPECT_THROW(constraint.validate(), std::invalid_argument);
}

TEST_F(TubeBendingConstraintTest, NegativeBendRadiusThrowsException) {
    auto constraint = createValidTubeConstraint();
    constraint.min_bend_radius_mm = -10.0;
    EXPECT_THROW(constraint.validate(), std::invalid_argument);
}

TEST_F(TubeBendingConstraintTest, InvalidSpringBackFactorThrowsException) {
    auto constraint = createValidTubeConstraint();
    constraint.spring_back_factor = 1.5;  // Greater than 1.0
    EXPECT_THROW(constraint.validate(), std::invalid_argument);
}

TEST_F(TubeBendingConstraintTest, InvalidBendAngleThrowsException) {
    auto constraint = createValidTubeConstraint();
    constraint.min_bend_angle_deg = 0.0;  // Must be > 0
    EXPECT_THROW(constraint.validate(), std::invalid_argument);
}

TEST_F(TubeBendingConstraintTest, GetDescriptionReturnsFormattedString) {
    auto constraint = createValidTubeConstraint();
    auto description = constraint.getDescription();
    EXPECT_FALSE(description.empty());
    EXPECT_TRUE(description.find("Tube Bending") != std::string::npos);
    EXPECT_TRUE(description.find("Steel") != std::string::npos);
}

// ============================================================================
// WeldingConstraint Tests
// ============================================================================

class WeldingConstraintTest : public ::testing::Test {
protected:
    WeldingConstraint createValidWeldConstraint() const {
        return WeldingConstraint{
            .min_joint_gap_mm = 0.5,
            .max_joint_gap_mm = 3.0,
            .min_weld_size_mm = 3.0,
            .max_weld_size_mm = 10.0,
            .min_weld_length_mm = 20.0,
            .requires_root_pass = true,
            .requires_stress_relief = false,
            .preheat_temperature_c = 150.0,
            .requires_surface_prep = true,
            .surface_prep_method = "Wire brush",
            .thermal_distortion_limit_mm = 2.0,
            .accessible_from_front = true,
            .accessible_from_back = true
        };
    }
};

TEST_F(WeldingConstraintTest, ValidWeldConstraintPassesValidation) {
    auto constraint = createValidWeldConstraint();
    EXPECT_TRUE(constraint.validate());
}

TEST_F(WeldingConstraintTest, InvalidJointGapThrowsException) {
    auto constraint = createValidWeldConstraint();
    constraint.min_joint_gap_mm = 5.0;
    constraint.max_joint_gap_mm = 3.0;  // Invalid range
    EXPECT_THROW(constraint.validate(), std::invalid_argument);
}

TEST_F(WeldingConstraintTest, InvalidWeldSizeThrowsException) {
    auto constraint = createValidWeldConstraint();
    constraint.min_weld_size_mm = 10.0;
    constraint.max_weld_size_mm = 3.0;  // Invalid range
    EXPECT_THROW(constraint.validate(), std::invalid_argument);
}

TEST_F(WeldingConstraintTest, NegativePreheatTemperatureThrowsException) {
    auto constraint = createValidWeldConstraint();
    constraint.preheat_temperature_c = -100.0;
    EXPECT_THROW(constraint.validate(), std::invalid_argument);
}

TEST_F(WeldingConstraintTest, GetDescriptionReturnsFormattedString) {
    auto constraint = createValidWeldConstraint();
    auto description = constraint.getDescription();
    EXPECT_FALSE(description.empty());
    EXPECT_TRUE(description.find("Welding") != std::string::npos);
}

// ============================================================================
// MachiningConstraint Tests
// ============================================================================

class MachiningConstraintTest : public ::testing::Test {
protected:
    MachiningConstraint createValidMachiningConstraint() const {
        return MachiningConstraint{
            .tool_diameter_mm = 10.0,
            .min_tool_clearance_mm = 2.0,
            .max_tool_engagement_mm = 5.0,
            .surface_finish_ra_um = 3.2,
            .tolerance_grade_um = 25.0,
            .min_feature_size_mm = 0.5,
            .max_feature_size_mm = 100.0,
            .requires_special_coolant = true,
            .coolant_type = "Flood",
            .spindle_speed_rpm = 2000,
            .feed_rate_mm_per_min = 150,
            .tool_changes_required = 3
        };
    }
};

TEST_F(MachiningConstraintTest, ValidMachiningConstraintPassesValidation) {
    auto constraint = createValidMachiningConstraint();
    EXPECT_TRUE(constraint.validate());
}

TEST_F(MachiningConstraintTest, ZeroToolDiameterThrowsException) {
    auto constraint = createValidMachiningConstraint();
    constraint.tool_diameter_mm = 0.0;
    EXPECT_THROW(constraint.validate(), std::invalid_argument);
}

TEST_F(MachiningConstraintTest, InvalidToleranceThrowsException) {
    auto constraint = createValidMachiningConstraint();
    constraint.tolerance_grade_um = 0.0;
    EXPECT_THROW(constraint.validate(), std::invalid_argument);
}

TEST_F(MachiningConstraintTest, NegativeSpindleSpeedThrowsException) {
    auto constraint = createValidMachiningConstraint();
    constraint.spindle_speed_rpm = -1000;
    EXPECT_THROW(constraint.validate(), std::invalid_argument);
}

TEST_F(MachiningConstraintTest, GetDescriptionReturnsFormattedString) {
    auto constraint = createValidMachiningConstraint();
    auto description = constraint.getDescription();
    EXPECT_FALSE(description.empty());
    EXPECT_TRUE(description.find("Machining") != std::string::npos);
}

// ============================================================================
// AssemblyConstraint Tests
// ============================================================================

class AssemblyConstraintTest : public ::testing::Test {
protected:
    AssemblyConstraint createValidAssemblyConstraint() const {
        return AssemblyConstraint{
            .interference_tolerance_mm = 0.1,
            .clearance_min_mm = 0.5,
            .fastener_access_diameter_mm = 8.0,
            .maintenance_clearance_mm = 50.0,
            .service_access_width_mm = 200.0,
            .requires_sequence_verification = true,
            .assembly_steps_minimum = 5,
            .assembly_steps_maximum = 20,
            .disassembly_required_for_maintenance = false,
            .estimated_assembly_time_hours = 2.5
        };
    }
};

TEST_F(AssemblyConstraintTest, ValidAssemblyConstraintPassesValidation) {
    auto constraint = createValidAssemblyConstraint();
    EXPECT_TRUE(constraint.validate());
}

TEST_F(AssemblyConstraintTest, InvalidAssemblyStepsThrowsException) {
    auto constraint = createValidAssemblyConstraint();
    constraint.assembly_steps_minimum = 20;
    constraint.assembly_steps_maximum = 5;  // Invalid range
    EXPECT_THROW(constraint.validate(), std::invalid_argument);
}

TEST_F(AssemblyConstraintTest, NegativeMaintenanceClearanceThrowsException) {
    auto constraint = createValidAssemblyConstraint();
    constraint.maintenance_clearance_mm = -10.0;
    EXPECT_THROW(constraint.validate(), std::invalid_argument);
}

TEST_F(AssemblyConstraintTest, GetDescriptionReturnsFormattedString) {
    auto constraint = createValidAssemblyConstraint();
    auto description = constraint.getDescription();
    EXPECT_FALSE(description.empty());
    EXPECT_TRUE(description.find("Assembly") != std::string::npos);
}

// ============================================================================
// MaterialConstraint Tests
// ============================================================================

class MaterialConstraintTest : public ::testing::Test {
protected:
    MaterialConstraint createValidMaterialConstraint() const {
        return MaterialConstraint{
            .material_type = MaterialType::STEEL,
            .density_g_per_cc = 7.85,
            .yield_strength_mpa = 250.0,
            .tensile_strength_mpa = 400.0,
            .elongation_percent = 20.0,
            .certified_material = true,
            .certification_standard = "ASTM A276",
            .material_cost_per_kg = 5.0,
            .max_material_cost_usd = 1000.0,
            .material_in_stock = true,
            .lead_time_days = 7,
            .thermal_conductivity_w_per_mk = 50.0,
            .max_operating_temperature_c = 400.0
        };
    }
};

TEST_F(MaterialConstraintTest, ValidMaterialConstraintPassesValidation) {
    auto constraint = createValidMaterialConstraint();
    EXPECT_TRUE(constraint.validate());
}

TEST_F(MaterialConstraintTest, InvalidStrengthValuesThrowsException) {
    auto constraint = createValidMaterialConstraint();
    constraint.yield_strength_mpa = 450.0;
    constraint.tensile_strength_mpa = 400.0;  // Yield > tensile is invalid
    EXPECT_THROW(constraint.validate(), std::invalid_argument);
}

TEST_F(MaterialConstraintTest, NegativeDensityThrowsException) {
    auto constraint = createValidMaterialConstraint();
    constraint.density_g_per_cc = -7.85;
    EXPECT_THROW(constraint.validate(), std::invalid_argument);
}

TEST_F(MaterialConstraintTest, InvalidElongationThrowsException) {
    auto constraint = createValidMaterialConstraint();
    constraint.elongation_percent = -10.0;
    EXPECT_THROW(constraint.validate(), std::invalid_argument);
}

TEST_F(MaterialConstraintTest, GetDescriptionReturnsFormattedString) {
    auto constraint = createValidMaterialConstraint();
    auto description = constraint.getDescription();
    EXPECT_FALSE(description.empty());
    EXPECT_TRUE(description.find("Material") != std::string::npos);
}

// ============================================================================
// ConstraintViolation Tests
// ============================================================================

class ConstraintViolationTest : public ::testing::Test {
protected:
    ConstraintViolation createValidViolation() const {
        return ConstraintViolation(
            "Test Constraint",
            "entity_123",
            ConstraintSeverity::ERROR,
            "Test violation description",
            "Test recommendation",
            1.5,
            "mm"
        );
    }
};

TEST_F(ConstraintViolationTest, ViolationSeverityINFO) {
    auto violation = ConstraintViolation(
        "Info", "e1", ConstraintSeverity::INFO, "desc", "rec", 0.0, "mm"
    );
    EXPECT_EQ(violation.getSeverityString(), "INFO");
}

TEST_F(ConstraintViolationTest, ViolationSeverityWARNING) {
    auto violation = ConstraintViolation(
        "Warn", "e1", ConstraintSeverity::WARNING, "desc", "rec", 0.0, "mm"
    );
    EXPECT_EQ(violation.getSeverityString(), "WARNING");
}

TEST_F(ConstraintViolationTest, ViolationSeverityERROR) {
    auto violation = ConstraintViolation(
        "Error", "e1", ConstraintSeverity::ERROR, "desc", "rec", 0.0, "mm"
    );
    EXPECT_EQ(violation.getSeverityString(), "ERROR");
}

TEST_F(ConstraintViolationTest, ViolationSeverityCRITICAL) {
    auto violation = ConstraintViolation(
        "Critical", "e1", ConstraintSeverity::CRITICAL, "desc", "rec", 0.0, "mm"
    );
    EXPECT_EQ(violation.getSeverityString(), "CRITICAL");
}

TEST_F(ConstraintViolationTest, ToStringFormatsViolationCorrectly) {
    auto violation = createValidViolation();
    auto str = violation.toString();
    EXPECT_FALSE(str.empty());
    EXPECT_TRUE(str.find("[ERROR]") != std::string::npos);
    EXPECT_TRUE(str.find("Test Constraint") != std::string::npos);
}

// ============================================================================
// ManufacturingConstraintsManager Tests
// ============================================================================

class ManufacturingConstraintsManagerTest : public ::testing::Test {
protected:
    ManufacturingConstraintsManager manager_;
    
    TubeBendingConstraint createValidTubeConstraint() const {
        return TubeBendingConstraint{
            .min_bend_radius_mm = 10.0,
            .max_bend_radius_mm = 50.0,
            .min_wall_thickness_mm = 1.0,
            .max_wall_thickness_mm = 5.0,
            .min_diameter_mm = 6.35,
            .max_diameter_mm = 50.8,
            .diameter_to_thickness_ratio = 50.0,
            .spring_back_factor = 0.85,
            .min_bend_angle_deg = 10.0,
            .max_bend_angle_deg = 175.0,
            .material_name = "Steel"
        };
    }
    
    WeldingConstraint createValidWeldConstraint() const {
        return WeldingConstraint{
            .min_joint_gap_mm = 0.5,
            .max_joint_gap_mm = 3.0,
            .min_weld_size_mm = 3.0,
            .max_weld_size_mm = 10.0,
            .min_weld_length_mm = 20.0,
            .requires_root_pass = true,
            .requires_stress_relief = false,
            .preheat_temperature_c = 150.0,
            .requires_surface_prep = true,
            .surface_prep_method = "Wire brush",
            .thermal_distortion_limit_mm = 2.0,
            .accessible_from_front = true,
            .accessible_from_back = true
        };
    }
    
    MaterialConstraint createValidMaterialConstraint() const {
        return MaterialConstraint{
            .material_type = MaterialType::STEEL,
            .density_g_per_cc = 7.85,
            .yield_strength_mpa = 250.0,
            .tensile_strength_mpa = 400.0,
            .elongation_percent = 20.0,
            .certified_material = true,
            .certification_standard = "ASTM A276",
            .material_cost_per_kg = 5.0,
            .max_material_cost_usd = 1000.0,
            .material_in_stock = true,
            .lead_time_days = 7,
            .thermal_conductivity_w_per_mk = 50.0,
            .max_operating_temperature_c = 400.0
        };
    }
};

TEST_F(ManufacturingConstraintsManagerTest, AddTubeBendingConstraint) {
    auto constraint = createValidTubeConstraint();
    manager_.addTubeBendingConstraint("tube_001", constraint);
    
    auto retrieved = manager_.getTubeBendingConstraint("tube_001");
    EXPECT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved.value().min_bend_radius_mm, 10.0);
}

TEST_F(ManufacturingConstraintsManagerTest, GetNonExistentConstraintReturnsEmpty) {
    auto retrieved = manager_.getTubeBendingConstraint("nonexistent");
    EXPECT_FALSE(retrieved.has_value());
}

TEST_F(ManufacturingConstraintsManagerTest, ValidateTubeBendingSuccess) {
    auto constraint = createValidTubeConstraint();
    manager_.addTubeBendingConstraint("tube_001", constraint);
    
    bool valid = manager_.validateTubeBending(
        "tube_001",
        30.0,   // bend radius (within 10-50 range)
        2.0,    // wall thickness (within 1-5 range)
        25.4,   // diameter (within 6.35-50.8 range)
        90.0    // bend angle (within 10-175 range)
    );
    
    EXPECT_TRUE(valid);
}

TEST_F(ManufacturingConstraintsManagerTest, ValidateTubeBendingFailsBendRadiusTooSmall) {
    auto constraint = createValidTubeConstraint();
    manager_.addTubeBendingConstraint("tube_001", constraint);
    
    bool valid = manager_.validateTubeBending(
        "tube_001",
        5.0,    // bend radius too small (< 10)
        2.0,
        25.4,
        90.0
    );
    
    EXPECT_FALSE(valid);
    EXPECT_GT(manager_.getViolations().size(), 0);
}

TEST_F(ManufacturingConstraintsManagerTest, ValidateWeldingSuccess) {
    auto constraint = createValidWeldConstraint();
    manager_.addWeldingConstraint("weld_001", constraint);
    
    bool valid = manager_.validateWelding(
        "weld_001",
        1.5,        // gap (within 0.5-3.0 range)
        5.0,        // weld size (within 3.0-10.0 range)
        true,       // has root pass
        "Wire brush"
    );
    
    EXPECT_TRUE(valid);
}

TEST_F(ManufacturingConstraintsManagerTest, ValidateWeldingFailsGapTooLarge) {
    auto constraint = createValidWeldConstraint();
    manager_.addWeldingConstraint("weld_001", constraint);
    
    bool valid = manager_.validateWelding(
        "weld_001",
        5.0,        // gap too large (> 3.0)
        5.0,
        true,
        "Wire brush"
    );
    
    EXPECT_FALSE(valid);
    EXPECT_GT(manager_.getViolations().size(), 0);
}

TEST_F(ManufacturingConstraintsManagerTest, ValidateMaterialSuccess) {
    auto constraint = createValidMaterialConstraint();
    manager_.addMaterialConstraint("part_001", constraint);
    
    bool valid = manager_.validateMaterial(
        "part_001",
        500.0,  // cost (within max of 1000)
        true,   // material available
        5       // lead time (5 days)
    );
    
    EXPECT_TRUE(valid);
}

TEST_F(ManufacturingConstraintsManagerTest, ValidateMaterialFailsCostTooHigh) {
    auto constraint = createValidMaterialConstraint();
    manager_.addMaterialConstraint("part_001", constraint);
    
    bool valid = manager_.validateMaterial(
        "part_001",
        1500.0,  // cost exceeds max of 1000
        true,
        5
    );
    
    EXPECT_FALSE(valid);
    EXPECT_GT(manager_.getViolations().size(), 0);
}

TEST_F(ManufacturingConstraintsManagerTest, GetViolationCount) {
    auto constraint = createValidTubeConstraint();
    manager_.addTubeBendingConstraint("tube_001", constraint);
    
    manager_.validateTubeBending("tube_001", 5.0, 2.0, 25.4, 90.0);
    
    EXPECT_GT(manager_.getViolationCount(), 0);
}

TEST_F(ManufacturingConstraintsManagerTest, ClearViolations) {
    auto constraint = createValidTubeConstraint();
    manager_.addTubeBendingConstraint("tube_001", constraint);
    
    manager_.validateTubeBending("tube_001", 5.0, 2.0, 25.4, 90.0);
    EXPECT_GT(manager_.getViolationCount(), 0);
    
    manager_.clearViolations();
    EXPECT_EQ(manager_.getViolationCount(), 0);
}

TEST_F(ManufacturingConstraintsManagerTest, GetTotalConstraintCount) {
    manager_.addTubeBendingConstraint("tube_001", createValidTubeConstraint());
    manager_.addWeldingConstraint("weld_001", createValidWeldConstraint());
    manager_.addMaterialConstraint("mat_001", createValidMaterialConstraint());
    
    EXPECT_EQ(manager_.getTotalConstraintCount(), 3);
}

TEST_F(ManufacturingConstraintsManagerTest, GenerateConstraintReport) {
    manager_.addTubeBendingConstraint("tube_001", createValidTubeConstraint());
    
    auto report = manager_.generateConstraintReport();
    EXPECT_FALSE(report.empty());
    EXPECT_TRUE(report.find("MANUFACTURING CONSTRAINTS REPORT") != std::string::npos);
}

TEST_F(ManufacturingConstraintsManagerTest, GenerateViolationReportWithNoViolations) {
    auto report = manager_.generateViolationReport();
    EXPECT_FALSE(report.empty());
    EXPECT_TRUE(report.find("No violations") != std::string::npos);
}

TEST_F(ManufacturingConstraintsManagerTest, GenerateViolationReportWithViolations) {
    auto constraint = createValidTubeConstraint();
    manager_.addTubeBendingConstraint("tube_001", constraint);
    
    manager_.validateTubeBending("tube_001", 5.0, 2.0, 25.4, 90.0);
    
    auto report = manager_.generateViolationReport();
    EXPECT_FALSE(report.empty());
    EXPECT_TRUE(report.find("Total Violations") != std::string::npos);
}

TEST_F(ManufacturingConstraintsManagerTest, ClearAllConstraints) {
    manager_.addTubeBendingConstraint("tube_001", createValidTubeConstraint());
    manager_.addWeldingConstraint("weld_001", createValidWeldConstraint());
    
    EXPECT_EQ(manager_.getTotalConstraintCount(), 2);
    
    manager_.clearAllConstraints();
    EXPECT_EQ(manager_.getTotalConstraintCount(), 0);
}

// ============================================================================
// Helper Functions Tests
// ============================================================================

class HelpersTest : public ::testing::Test {};

TEST_F(HelpersTest, CalculateMinBendRadius) {
    double radius = helpers::calculateMinBendRadius(20.0, 2.0);
    EXPECT_GT(radius, 0);
    EXPECT_NEAR(radius, 50.0, 1.0);  // Should be ~2.5x diameter
}

TEST_F(HelpersTest, CalculateMinBendRadiusWithZeroDiameter) {
    double radius = helpers::calculateMinBendRadius(0.0, 2.0);
    EXPECT_EQ(radius, 0);
}

TEST_F(HelpersTest, CalculateSpringBackCompensation) {
    double compensation = helpers::calculateSpringBackCompensation(250.0, 30.0, 20.0);
    EXPECT_GE(compensation, 0.0);
    EXPECT_LE(compensation, 1.0);
}

TEST_F(HelpersTest, CheckThermalDistortion) {
    bool ok = helpers::checkThermalDistortion(100.0, 5.0, 5.0);
    EXPECT_TRUE(ok);
}

TEST_F(HelpersTest, CalculateToolClearance) {
    double clearance = helpers::calculateToolClearance(10.0, 50.0);
    EXPECT_GE(clearance, 2.0);  // Minimum 2mm
}

TEST_F(HelpersTest, EstimateAssemblyTime) {
    double time = helpers::estimateAssemblyTime(5, 10);
    EXPECT_GT(time, 0);
    EXPECT_NEAR(time, 2.25, 0.1);  // 5*0.25 + 10*0.1
}

// ============================================================================
// Integration Tests
// ============================================================================

class ManufacturingConstraintsIntegrationTest : public ::testing::Test {
protected:
    ManufacturingConstraintsManager manager_;
};

TEST_F(ManufacturingConstraintsIntegrationTest, FullWorkflow) {
    // Create a tube assembly with all constraint types
    TubeBendingConstraint tube{
        .min_bend_radius_mm = 15.0,
        .max_bend_radius_mm = 60.0,
        .min_wall_thickness_mm = 1.5,
        .max_wall_thickness_mm = 4.0,
        .min_diameter_mm = 10.0,
        .max_diameter_mm = 40.0,
        .diameter_to_thickness_ratio = 40.0,
        .spring_back_factor = 0.85,
        .min_bend_angle_deg = 15.0,
        .max_bend_angle_deg = 170.0,
        .material_name = "SAE 4130"
    };
    
    manager_.addTubeBendingConstraint("tube_assembly", tube);
    
    // Validate valid configuration
    bool valid = manager_.validateTubeBending(
        "tube_assembly",
        30.0,   // Valid bend radius
        2.5,    // Valid wall thickness
        25.0,   // Valid diameter
        90.0    // Valid bend angle
    );
    
    EXPECT_TRUE(valid);
    EXPECT_EQ(manager_.getViolationCount(), 0);
    
    // Now validate invalid configuration
    valid = manager_.validateTubeBending(
        "tube_assembly",
        5.0,    // Too small
        0.5,    // Too thin
        50.0,   // Too large
        5.0     // Too small angle
    );
    
    EXPECT_FALSE(valid);
    EXPECT_GT(manager_.getViolationCount(), 0);
}

TEST_F(ManufacturingConstraintsIntegrationTest, MultipleConstraintValidation) {
    // Add multiple constraints
    TubeBendingConstraint tube{
        .min_bend_radius_mm = 15.0, .max_bend_radius_mm = 60.0,
        .min_wall_thickness_mm = 1.5, .max_wall_thickness_mm = 4.0,
        .min_diameter_mm = 10.0, .max_diameter_mm = 40.0,
        .diameter_to_thickness_ratio = 40.0, .spring_back_factor = 0.85,
        .min_bend_angle_deg = 15.0, .max_bend_angle_deg = 170.0,
        .material_name = "Steel"
    };
    
    WeldingConstraint weld{
        .min_joint_gap_mm = 0.5, .max_joint_gap_mm = 2.5,
        .min_weld_size_mm = 3.0, .max_weld_size_mm = 8.0,
        .min_weld_length_mm = 25.0, .requires_root_pass = true,
        .requires_stress_relief = false, .preheat_temperature_c = 150.0,
        .requires_surface_prep = true, .surface_prep_method = "Grind",
        .thermal_distortion_limit_mm = 1.5,
        .accessible_from_front = true, .accessible_from_back = false
    };
    
    MaterialConstraint mat{
        .material_type = MaterialType::STEEL,
        .density_g_per_cc = 7.85, .yield_strength_mpa = 250.0,
        .tensile_strength_mpa = 400.0, .elongation_percent = 20.0,
        .certified_material = true, .certification_standard = "ASTM A276",
        .material_cost_per_kg = 5.0, .max_material_cost_usd = 800.0,
        .material_in_stock = true, .lead_time_days = 5,
        .thermal_conductivity_w_per_mk = 50.0, .max_operating_temperature_c = 400.0
    };
    
    manager_.addTubeBendingConstraint("tube_001", tube);
    manager_.addWeldingConstraint("joint_001", weld);
    manager_.addMaterialConstraint("part_001", mat);
    
    EXPECT_EQ(manager_.getTotalConstraintCount(), 3);
    
    // Validate all
    bool tube_ok = manager_.validateTubeBending("tube_001", 30.0, 2.0, 25.0, 90.0);
    bool weld_ok = manager_.validateWelding("joint_001", 1.5, 5.0, true, "Grind");
    bool mat_ok = manager_.validateMaterial("part_001", 600.0, true, 4);
    
    EXPECT_TRUE(tube_ok);
    EXPECT_TRUE(weld_ok);
    EXPECT_TRUE(mat_ok);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
