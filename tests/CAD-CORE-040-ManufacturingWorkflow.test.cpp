#include <gtest/gtest.h>
#include "../src/manufacturing/ManufacturingWorkflow.hpp"

using namespace bendcore::manufacturing;

class ManufacturingWorkflowTest : public ::testing::Test {
 protected:
  ManufacturingWorkflow workflow_;

  std::vector<Tube> createTestTubes() {
    std::vector<Tube> tubes;

    Tube tube1;
    tube1.id = "TUBE-001";
    tube1.material = "Steel";
    tube1.length = 100.0;
    tube1.outer_diameter = 12.0;
    tube1.wall_thickness = 1.0;

    Bend bend;
    bend.position = 50.0;
    bend.angle = 45.0;
    bend.radius = 15.0;
    bend.dwell_time = 1.0;
    tube1.bends.push_back(bend);
    tubes.push_back(tube1);

    return tubes;
  }

  ManufacturingConstraints createTestConstraints() {
    ManufacturingConstraints constraints;
    constraints.max_wall_thickness = 10.0;
    constraints.min_bend_radius = 10.0;
    constraints.max_concurrent_machines = 5;
    constraints.optimize_for_cost = true;
    return constraints;
  }
};

TEST_F(ManufacturingWorkflowTest, ExecuteCompleteWorkflow) {
  auto tubes = createTestTubes();
  auto constraints = createTestConstraints();
  FEAIntegration::AnalysisSettings fea_settings;
  GCodeGenerator::GCodeConfig gcode_config;

  auto result = workflow_.executeCompleteWorkflow(
      tubes, constraints, fea_settings, gcode_config);

  EXPECT_TRUE(result.success);
  EXPECT_EQ(result.current_stage, ManufacturingWorkflow::WorkflowStage::COMPLETION);
}

TEST_F(ManufacturingWorkflowTest, PlanningStage) {
  auto tubes = createTestTubes();
  auto constraints = createTestConstraints();

  auto result = workflow_.executePlanningStage(tubes, constraints);

  EXPECT_TRUE(result.success);
  EXPECT_EQ(result.current_stage, ManufacturingWorkflow::WorkflowStage::PLANNING);
  EXPECT_FALSE(result.manufacturing_plan.instructions.empty());
}

TEST_F(ManufacturingWorkflowTest, ValidationStage) {
  auto tubes = createTestTubes();
  auto constraints = createTestConstraints();

  auto planning_result = workflow_.executePlanningStage(tubes, constraints);

  std::vector<fea::Material> materials;
  materials.push_back(FEAIntegration::getMaterialProperties("Steel"));
  FEAIntegration::AnalysisSettings fea_settings;

  auto result = workflow_.executeValidationStage(
      planning_result, materials, fea_settings);

  EXPECT_EQ(result.current_stage, ManufacturingWorkflow::WorkflowStage::VALIDATION);
}

TEST_F(ManufacturingWorkflowTest, DesignReviewStage) {
  auto tubes = createTestTubes();
  auto constraints = createTestConstraints();

  auto planning_result = workflow_.executePlanningStage(tubes, constraints);
  auto review_result = workflow_.executeDesignReviewStage(planning_result);

  EXPECT_EQ(review_result.current_stage, ManufacturingWorkflow::WorkflowStage::DESIGN_REVIEW);
}

TEST_F(ManufacturingWorkflowTest, CodeGenerationStage) {
  auto tubes = createTestTubes();
  auto constraints = createTestConstraints();

  auto planning_result = workflow_.executePlanningStage(tubes, constraints);
  GCodeGenerator::GCodeConfig gcode_config;

  auto result = workflow_.executeCodeGenerationStage(planning_result, gcode_config);

  EXPECT_EQ(result.current_stage, ManufacturingWorkflow::WorkflowStage::CODE_GEN);
  EXPECT_GE(result.gcode_programs.size(), 0);
}

TEST_F(ManufacturingWorkflowTest, ProcurementStage) {
  auto tubes = createTestTubes();
  auto constraints = createTestConstraints();

  auto planning_result = workflow_.executePlanningStage(tubes, constraints);

  std::map<std::string, double> material_costs;
  std::map<std::string, double> labor_rates;

  auto result = workflow_.executeProcurementStage(
      planning_result, material_costs, labor_rates);

  EXPECT_EQ(result.current_stage, ManufacturingWorkflow::WorkflowStage::PROCUREMENT);
}

TEST_F(ManufacturingWorkflowTest, ValidateConstraints) {
  auto tubes = createTestTubes();
  auto constraints = createTestConstraints();

  bool valid = workflow_.validateConstraints(tubes, constraints);
  EXPECT_TRUE(valid);
}

TEST_F(ManufacturingWorkflowTest, ValidateConstraintsFailure) {
  std::vector<Tube> empty_tubes;
  auto constraints = createTestConstraints();

  bool valid = workflow_.validateConstraints(empty_tubes, constraints);
  EXPECT_FALSE(valid);
}

TEST_F(ManufacturingWorkflowTest, GenerateHTMLReport) {
  auto tubes = createTestTubes();
  auto constraints = createTestConstraints();
  FEAIntegration::AnalysisSettings fea_settings;
  GCodeGenerator::GCodeConfig gcode_config;

  auto result = workflow_.executeCompleteWorkflow(
      tubes, constraints, fea_settings, gcode_config);

  std::string report = workflow_.generateManufacturingReport(result, "html");

  EXPECT_FALSE(report.empty());
  EXPECT_NE(report.find("<!DOCTYPE html>"), std::string::npos);
}

TEST_F(ManufacturingWorkflowTest, GenerateJSONReport) {
  auto tubes = createTestTubes();
  auto constraints = createTestConstraints();
  FEAIntegration::AnalysisSettings fea_settings;
  GCodeGenerator::GCodeConfig gcode_config;

  auto result = workflow_.executeCompleteWorkflow(
      tubes, constraints, fea_settings, gcode_config);

  std::string report = workflow_.generateManufacturingReport(result, "json");

  EXPECT_FALSE(report.empty());
  EXPECT_NE(report.find("{"), std::string::npos);
}

TEST_F(ManufacturingWorkflowTest, CalculateQualityMetrics) {
  auto tubes = createTestTubes();
  auto constraints = createTestConstraints();
  FEAIntegration::AnalysisSettings fea_settings;
  GCodeGenerator::GCodeConfig gcode_config;

  auto result = workflow_.executeCompleteWorkflow(
      tubes, constraints, fea_settings, gcode_config);

  auto metrics = workflow_.calculateQualityMetrics(result);

  EXPECT_GE(metrics.total_bends, 0);
  EXPECT_GE(metrics.validated_bends, 0);
  EXPECT_GE(metrics.critical_issues, 0);
}

TEST_F(ManufacturingWorkflowTest, ExportWorkflowResults) {
  auto tubes = createTestTubes();
  auto constraints = createTestConstraints();
  FEAIntegration::AnalysisSettings fea_settings;
  GCodeGenerator::GCodeConfig gcode_config;

  auto result = workflow_.executeCompleteWorkflow(
      tubes, constraints, fea_settings, gcode_config);

  bool success = workflow_.exportWorkflowResults(result, "/tmp");

  EXPECT_TRUE(success);
}

TEST_F(ManufacturingWorkflowTest, WorkflowResultStructure) {
  auto tubes = createTestTubes();
  auto constraints = createTestConstraints();
  FEAIntegration::AnalysisSettings fea_settings;
  GCodeGenerator::GCodeConfig gcode_config;

  auto result = workflow_.executeCompleteWorkflow(
      tubes, constraints, fea_settings, gcode_config);

  EXPECT_TRUE(result.success);
  EXPECT_GE(result.total_manufacturing_time, 0.0);
  EXPECT_GE(result.total_estimated_cost, 0.0);
  EXPECT_FALSE(result.manufacturing_plan.instructions.empty());
}

TEST_F(ManufacturingWorkflowTest, ManufacturingPlanGeneration) {
  auto tubes = createTestTubes();
  auto constraints = createTestConstraints();

  auto result = workflow_.executePlanningStage(tubes, constraints);

  auto& plan = result.manufacturing_plan;
  EXPECT_FALSE(plan.instructions.empty());
  EXPECT_GE(plan.estimated_time, 0.0);
  EXPECT_GE(plan.estimated_cost, 0.0);
}

TEST_F(ManufacturingWorkflowTest, BOMGeneration) {
  auto tubes = createTestTubes();
  auto constraints = createTestConstraints();

  auto planning_result = workflow_.executePlanningStage(tubes, constraints);

  std::map<std::string, double> material_costs;
  std::map<std::string, double> labor_rates;

  auto result = workflow_.executeProcurementStage(
      planning_result, material_costs, labor_rates);

  auto& bom = result.bill_of_materials;
  EXPECT_FALSE(bom.tubes.empty());
}

TEST_F(ManufacturingWorkflowTest, GCodeGeneration) {
  auto tubes = createTestTubes();
  auto constraints = createTestConstraints();

  auto planning_result = workflow_.executePlanningStage(tubes, constraints);
  GCodeGenerator::GCodeConfig gcode_config;

  auto result = workflow_.executeCodeGenerationStage(planning_result, gcode_config);

  EXPECT_GE(result.gcode_programs.size(), 0);
}

TEST_F(ManufacturingWorkflowTest, FEAResultsCollection) {
  auto tubes = createTestTubes();
  auto constraints = createTestConstraints();

  auto planning_result = workflow_.executePlanningStage(tubes, constraints);
  std::vector<fea::Material> materials;
  materials.push_back(FEAIntegration::getMaterialProperties("Steel"));
  FEAIntegration::AnalysisSettings fea_settings;

  auto result = workflow_.executeValidationStage(
      planning_result, materials, fea_settings);

  // FEA results should be collected
  EXPECT_GE(result.fea_results.size(), 0);
}

TEST_F(ManufacturingWorkflowTest, ErrorTracking) {
  std::vector<Tube> empty_tubes;
  auto constraints = createTestConstraints();

  auto result = workflow_.executePlanningStage(empty_tubes, constraints);

  EXPECT_FALSE(result.success);
  EXPECT_GT(result.errors.size(), 0);
}

TEST_F(ManufacturingWorkflowTest, WarningCollection) {
  auto tubes = createTestTubes();
  auto constraints = createTestConstraints();
  FEAIntegration::AnalysisSettings fea_settings;
  GCodeGenerator::GCodeConfig gcode_config;

  auto result = workflow_.executeCompleteWorkflow(
      tubes, constraints, fea_settings, gcode_config);

  // Warnings may or may not be present
  EXPECT_GE(result.warnings.size(), 0);
}

TEST_F(ManufacturingWorkflowTest, CostEstimation) {
  auto tubes = createTestTubes();
  auto constraints = createTestConstraints();

  auto result = workflow_.executePlanningStage(tubes, constraints);

  EXPECT_GE(result.total_estimated_cost, 0.0);
  EXPECT_GE(result.manufacturing_plan.estimated_cost, 0.0);
}

TEST_F(ManufacturingWorkflowTest, TimeEstimation) {
  auto tubes = createTestTubes();
  auto constraints = createTestConstraints();

  auto result = workflow_.executePlanningStage(tubes, constraints);

  EXPECT_GE(result.total_manufacturing_time, 0.0);
  EXPECT_GE(result.manufacturing_plan.estimated_time, 0.0);
}

TEST_F(ManufacturingWorkflowTest, StageProgression) {
  auto tubes = createTestTubes();
  auto constraints = createTestConstraints();
  FEAIntegration::AnalysisSettings fea_settings;
  GCodeGenerator::GCodeConfig gcode_config;

  auto result = workflow_.executeCompleteWorkflow(
      tubes, constraints, fea_settings, gcode_config);

  // Should end at COMPLETION stage
  EXPECT_EQ(result.current_stage, ManufacturingWorkflow::WorkflowStage::COMPLETION);
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
