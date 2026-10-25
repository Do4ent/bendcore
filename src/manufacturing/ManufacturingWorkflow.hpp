#pragma once

#include <vector>
#include <string>
#include <memory>
#include <map>
#include "FEAIntegration.hpp"
#include "GCodeGenerator.hpp"
#include "BOMGenerator.hpp"
#include "ManufacturingPlanner.hpp"

namespace bendcore::manufacturing {

// Complete Manufacturing Workflow Orchestration
class ManufacturingWorkflow {
 public:
  enum class WorkflowStage {
    PLANNING,       // Planning and constraints definition
    VALIDATION,     // FEA validation
    DESIGN_REVIEW,  // Design review before manufacturing
    CODE_GEN,       // G-code generation
    PROCUREMENT,    // BOM and procurement planning
    MANUFACTURING,  // Manufacturing execution
    QUALITY_CHECK,  // Quality assurance
    COMPLETION      // Workflow completion
  };

  struct WorkflowResult {
    bool success = true;
    WorkflowStage current_stage = WorkflowStage::PLANNING;
    ManufacturingPlan manufacturing_plan;
    std::vector<FEAIntegration::TubeAnalysisResult> fea_results;
    BOMGenerator::BOM bill_of_materials;
    std::map<std::string, std::string> gcode_programs;  // tube_id -> G-code
    std::vector<std::string> warnings;
    std::vector<std::string> errors;
    double total_manufacturing_time = 0.0;
    double total_estimated_cost = 0.0;
  };

  ManufacturingWorkflow();

  // Execute complete workflow (all stages)
  WorkflowResult executeCompleteWorkflow(
    const std::vector<Tube>& tubes,
    const ManufacturingConstraints& constraints,
    const FEAIntegration::AnalysisSettings& fea_settings,
    const GCodeGenerator::GCodeConfig& gcode_config
  );

  // Execute specific workflow stages
  WorkflowResult executePlanningStage(
    const std::vector<Tube>& tubes,
    const ManufacturingConstraints& constraints
  );

  WorkflowResult executeValidationStage(
    WorkflowResult& current_result,
    const std::vector<fea::Material>& materials,
    const FEAIntegration::AnalysisSettings& fea_settings
  );

  WorkflowResult executeDesignReviewStage(
    const WorkflowResult& current_result
  );

  WorkflowResult executeCodeGenerationStage(
    WorkflowResult& current_result,
    const GCodeGenerator::GCodeConfig& gcode_config
  );

  WorkflowResult executeProcurementStage(
    WorkflowResult& current_result,
    const std::map<std::string, double>& material_costs,
    const std::map<std::string, double>& labor_rates
  );

  // Generate complete manufacturing report
  std::string generateManufacturingReport(
    const WorkflowResult& result,
    const std::string& format = "html"  // html, pdf, json
  );

  // Export all artifacts
  bool exportWorkflowResults(
    const WorkflowResult& result,
    const std::string& output_directory
  );

  // Check workflow constraints and rules
  bool validateConstraints(
    const std::vector<Tube>& tubes,
    const ManufacturingConstraints& constraints
  );

  // Get quality metrics
  struct QualityMetrics {
    int total_bends = 0;
    int validated_bends = 0;
    double average_stress_utilization = 0.0;
    int critical_issues = 0;
    int warnings_count = 0;
  };

  QualityMetrics calculateQualityMetrics(const WorkflowResult& result);

 private:
  // Stage execution helpers
  void performDesignChecks(WorkflowResult& result);
  void performSafetyValidation(WorkflowResult& result);
  void performCostOptimization(WorkflowResult& result);

  // Report generation
  std::string generateHTMLReport(const WorkflowResult& result);
  std::string generatePDFReport(const WorkflowResult& result);
  std::string generateJSONReport(const WorkflowResult& result);

  // Component instances
  std::unique_ptr<ManufacturingPlanner> planner_;
  std::unique_ptr<FEAIntegration> fea_integration_;
  std::unique_ptr<GCodeGenerator> gcode_generator_;
  std::unique_ptr<BOMGenerator> bom_generator_;

  // Workflow state
  WorkflowStage current_stage_ = WorkflowStage::PLANNING;
  std::map<WorkflowStage, bool> stage_completed_;
};

}  // namespace bendcore::manufacturing
