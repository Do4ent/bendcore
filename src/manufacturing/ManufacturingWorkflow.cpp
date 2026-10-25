#include "ManufacturingWorkflow.hpp"
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace bendcore::manufacturing {

ManufacturingWorkflow::ManufacturingWorkflow() {
  planner_ = std::make_unique<ManufacturingPlanner>();
  fea_integration_ = std::make_unique<FEAIntegration>();
  gcode_generator_ = std::make_unique<GCodeGenerator>();
  bom_generator_ = std::make_unique<BOMGenerator>();

  // Initialize stage tracking
  for (int i = static_cast<int>(WorkflowStage::PLANNING);
       i <= static_cast<int>(WorkflowStage::COMPLETION); ++i) {
    stage_completed_[static_cast<WorkflowStage>(i)] = false;
  }
}

ManufacturingWorkflow::WorkflowResult ManufacturingWorkflow::executeCompleteWorkflow(
    const std::vector<Tube>& tubes,
    const ManufacturingConstraints& constraints,
    const FEAIntegration::AnalysisSettings& fea_settings,
    const GCodeGenerator::GCodeConfig& gcode_config) {

  WorkflowResult result;

  // Stage 1: Planning
  result = executePlanningStage(tubes, constraints);
  if (!result.success) {
    result.errors.push_back("Planning stage failed");
    return result;
  }
  stage_completed_[WorkflowStage::PLANNING] = true;

  // Stage 2: Validation (FEA)
  std::vector<fea::Material> materials;
  for (const auto& tube : tubes) {
    materials.push_back(FEAIntegration::getMaterialProperties(tube.material));
  }
  result = executeValidationStage(result, materials, fea_settings);
  if (!result.success) {
    result.errors.push_back("Validation stage failed");
  }
  stage_completed_[WorkflowStage::VALIDATION] = true;

  // Stage 3: Design Review
  result = executeDesignReviewStage(result);
  stage_completed_[WorkflowStage::DESIGN_REVIEW] = true;

  // Check if we can proceed to manufacturing
  if (!result.warnings.empty() || !result.success) {
    result.warnings.push_back("Proceeding with caution due to design issues");
  }

  // Stage 4: Code Generation
  result = executeCodeGenerationStage(result, gcode_config);
  stage_completed_[WorkflowStage::CODE_GEN] = true;

  // Stage 5: Procurement
  std::map<std::string, double> material_costs;
  std::map<std::string, double> labor_rates;
  result = executeProcurementStage(result, material_costs, labor_rates);
  stage_completed_[WorkflowStage::PROCUREMENT] = true;

  // Stage 6-7: Quality and Completion
  stage_completed_[WorkflowStage::QUALITY_CHECK] = true;
  stage_completed_[WorkflowStage::COMPLETION] = true;

  result.current_stage = WorkflowStage::COMPLETION;
  return result;
}

ManufacturingWorkflow::WorkflowResult ManufacturingWorkflow::executePlanningStage(
    const std::vector<Tube>& tubes,
    const ManufacturingConstraints& constraints) {

  WorkflowResult result;
  result.current_stage = WorkflowStage::PLANNING;

  // Validate constraints
  if (!validateConstraints(tubes, constraints)) {
    result.success = false;
    result.errors.push_back("Constraints validation failed");
    return result;
  }

  // Generate manufacturing plan
  result.manufacturing_plan = planner_->generatePlan(tubes, constraints);
  result.total_manufacturing_time = result.manufacturing_plan.estimated_time;
  result.total_estimated_cost = result.manufacturing_plan.estimated_cost;

  if (result.manufacturing_plan.instructions.empty()) {
    result.success = false;
    result.errors.push_back("No valid manufacturing plan could be generated");
  }

  return result;
}

ManufacturingWorkflow::WorkflowResult ManufacturingWorkflow::executeValidationStage(
    WorkflowResult& current_result,
    const std::vector<fea::Material>& materials,
    const FEAIntegration::AnalysisSettings& fea_settings) {

  WorkflowResult result = current_result;
  result.current_stage = WorkflowStage::VALIDATION;

  // Get tubes from manufacturing plan instructions
  std::vector<Tube> tubes;
  for (const auto& instruction : result.manufacturing_plan.instructions) {
    Tube tube;
    tube.id = instruction.tube_id;
    tube.material = instruction.material;
    tube.length = instruction.length;
    tube.outer_diameter = instruction.outer_diameter;
    tube.wall_thickness = instruction.wall_thickness;
    tubes.push_back(tube);
  }

  // Perform FEA analysis
  if (tubes.size() <= materials.size()) {
    result.fea_results = fea_integration_->analyzeAssembly(
        tubes, materials, fea_settings);

    // Check results
    for (const auto& fea_result : result.fea_results) {
      if (!fea_result.is_valid) {
        result.errors.push_back("FEA validation failed: " + fea_result.failure_mode);
        result.success = false;
      }
    }
  } else {
    result.warnings.push_back("Material count mismatch");
  }

  return result;
}

ManufacturingWorkflow::WorkflowResult ManufacturingWorkflow::executeDesignReviewStage(
    const WorkflowResult& current_result) {

  WorkflowResult result = current_result;
  result.current_stage = WorkflowStage::DESIGN_REVIEW;

  performDesignChecks(const_cast<WorkflowResult&>(result));
  performSafetyValidation(const_cast<WorkflowResult&>(result));

  return result;
}

ManufacturingWorkflow::WorkflowResult ManufacturingWorkflow::executeCodeGenerationStage(
    WorkflowResult& current_result,
    const GCodeGenerator::GCodeConfig& gcode_config) {

  WorkflowResult result = current_result;
  result.current_stage = WorkflowStage::CODE_GEN;

  // Get tubes from manufacturing plan
  std::vector<Tube> tubes;
  for (const auto& instruction : result.manufacturing_plan.instructions) {
    Tube tube;
    tube.id = instruction.tube_id;
    tube.material = instruction.material;
    tube.length = instruction.length;
    tube.outer_diameter = instruction.outer_diameter;
    tube.wall_thickness = instruction.wall_thickness;
    tubes.push_back(tube);
  }

  // Generate G-code for each tube
  for (size_t i = 0; i < tubes.size() && i < result.manufacturing_plan.nc_programs.size(); ++i) {
    const auto& tube = tubes[i];
    const auto& nc_prog = result.manufacturing_plan.nc_programs[i];

    std::string gcode = gcode_generator_->generateGCode(
        tube, BendingMachine(), gcode_config);

    if (!gcode_generator_->validateGCode(gcode)) {
      result.warnings.push_back("Generated G-code validation failed for tube: " + tube.id);
    }

    result.gcode_programs[tube.id] = gcode;
  }

  return result;
}

ManufacturingWorkflow::WorkflowResult ManufacturingWorkflow::executeProcurementStage(
    WorkflowResult& current_result,
    const std::map<std::string, double>& material_costs,
    const std::map<std::string, double>& labor_rates) {

  WorkflowResult result = current_result;
  result.current_stage = WorkflowStage::PROCUREMENT;

  // Get tubes from manufacturing plan
  std::vector<Tube> tubes;
  for (const auto& instruction : result.manufacturing_plan.instructions) {
    Tube tube;
    tube.id = instruction.tube_id;
    tube.material = instruction.material;
    tube.length = instruction.length;
    tube.outer_diameter = instruction.outer_diameter;
    tube.wall_thickness = instruction.wall_thickness;
    tubes.push_back(tube);
  }

  // Generate BOM
  result.bill_of_materials = bom_generator_->generateBOM(
      tubes, std::vector<BendingMachine>(), "Manufacturing Project");

  // Calculate costs
  bom_generator_->calculateCosts(
      result.bill_of_materials, material_costs, labor_rates);

  result.total_estimated_cost = result.bill_of_materials.estimated_total_cost;

  return result;
}

std::string ManufacturingWorkflow::generateManufacturingReport(
    const WorkflowResult& result,
    const std::string& format) {

  if (format == "json") {
    return generateJSONReport(result);
  } else if (format == "pdf") {
    return generatePDFReport(result);
  } else {
    return generateHTMLReport(result);  // Default to HTML
  }
}

bool ManufacturingWorkflow::exportWorkflowResults(
    const WorkflowResult& result,
    const std::string& output_directory) {

  // Export manufacturing plan
  std::string plan_file = output_directory + "/manufacturing_plan.txt";
  std::ofstream plan_out(plan_file);
  if (plan_out.is_open()) {
    plan_out << "Manufacturing Plan\n";
    plan_out << "==================\n";
    plan_out << "Total Instructions: " << result.manufacturing_plan.instructions.size() << "\n";
    plan_out << "Estimated Time: " << result.manufacturing_plan.estimated_time << " hours\n";
    plan_out << "Estimated Cost: $" << result.manufacturing_plan.estimated_cost << "\n";
    plan_out.close();
  }

  // Export G-code programs
  for (const auto& pair : result.gcode_programs) {
    std::string gcode_file = output_directory + "/" + pair.first + ".gcode";
    std::ofstream gcode_out(gcode_file);
    if (gcode_out.is_open()) {
      gcode_out << pair.second;
      gcode_out.close();
    }
  }

  // Export BOM
  std::string bom_file = output_directory + "/BOM.csv";
  bom_generator_->exportBOM(
      result.bill_of_materials, bom_file, BOMGenerator::BOMFormat::CSV);

  // Export report
  std::string report_file = output_directory + "/manufacturing_report.html";
  std::ofstream report_out(report_file);
  if (report_out.is_open()) {
    report_out << generateManufacturingReport(result, "html");
    report_out.close();
  }

  return true;
}

bool ManufacturingWorkflow::validateConstraints(
    const std::vector<Tube>& tubes,
    const ManufacturingConstraints& constraints) {

  if (tubes.empty()) {
    return false;
  }

  for (const auto& tube : tubes) {
    if (tube.wall_thickness > constraints.max_wall_thickness) {
      return false;
    }

    for (const auto& bend : tube.bends) {
      if (bend.radius < constraints.min_bend_radius) {
        return false;
      }
    }
  }

  return true;
}

ManufacturingWorkflow::QualityMetrics ManufacturingWorkflow::calculateQualityMetrics(
    const WorkflowResult& result) {

  QualityMetrics metrics;

  // Count bends
  for (const auto& instruction : result.manufacturing_plan.instructions) {
    // Extract bend count from sequence
    metrics.total_bends += 2;  // Simplified: assume 2 bends per tube
  }

  // Count validated bends
  for (const auto& fea_result : result.fea_results) {
    if (fea_result.is_valid) {
      metrics.validated_bends += 2;
    }
  }

  // Calculate stress utilization
  double total_utilization = 0.0;
  for (const auto& fea_result : result.fea_results) {
    total_utilization += fea_result.stress_utilization;
  }
  if (!result.fea_results.empty()) {
    metrics.average_stress_utilization = total_utilization / result.fea_results.size();
  }

  // Count issues
  metrics.critical_issues = 0;
  for (const auto& error : result.errors) {
    metrics.critical_issues++;
  }
  metrics.warnings_count = result.warnings.size();

  return metrics;
}

void ManufacturingWorkflow::performDesignChecks(WorkflowResult& result) {
  // Check if all bends are within machine limits
  for (const auto& instruction : result.manufacturing_plan.instructions) {
    // Simplified check
    if (instruction.estimated_time > 480.0) {  // 8 hours
      result.warnings.push_back("Tube " + instruction.tube_id +
                               " estimated time exceeds 8 hours");
    }
  }
}

void ManufacturingWorkflow::performSafetyValidation(WorkflowResult& result) {
  // Perform safety checks
  if (result.fea_results.empty()) {
    result.warnings.push_back("No FEA validation performed");
  }

  for (const auto& fea_result : result.fea_results) {
    if (fea_result.stress_utilization > 0.8) {
      result.warnings.push_back("High stress utilization detected");
    }
  }
}

std::string ManufacturingWorkflow::generateHTMLReport(const WorkflowResult& result) {
  std::ostringstream html;

  html << "<!DOCTYPE html>\n<html>\n<head>\n";
  html << "<title>Manufacturing Report</title>\n";
  html << "<style>\n";
  html << "body { font-family: Arial, sans-serif; margin: 20px; }\n";
  html << "table { border-collapse: collapse; width: 100%; }\n";
  html << "th, td { border: 1px solid black; padding: 8px; text-align: left; }\n";
  html << "th { background-color: #4CAF50; color: white; }\n";
  html << "</style>\n</head>\n<body>\n";

  html << "<h1>Manufacturing Report</h1>\n";
  html << "<p>Status: " << (result.success ? "SUCCESS" : "FAILED") << "</p>\n";
  html << "<p>Total Manufacturing Time: " << result.total_manufacturing_time << " hours</p>\n";
  html << "<p>Estimated Cost: $" << result.total_estimated_cost << "</p>\n";

  html << "<h2>Manufacturing Plan</h2>\n";
  html << "<table>\n";
  html << "<tr><th>Tube ID</th><th>Machine</th><th>Time (min)</th><th>Cost</th></tr>\n";
  for (const auto& instr : result.manufacturing_plan.instructions) {
    html << "<tr><td>" << instr.tube_id << "</td>";
    html << "<td>" << instr.machine << "</td>";
    html << "<td>" << instr.estimated_time << "</td>";
    html << "<td>TBD</td></tr>\n";
  }
  html << "</table>\n";

  html << "<h2>FEA Results</h2>\n";
  html << "<table>\n";
  html << "<tr><th>Index</th><th>Valid</th><th>Max Stress (MPa)</th><th>Deflection (mm)</th></tr>\n";
  for (size_t i = 0; i < result.fea_results.size(); ++i) {
    const auto& fea = result.fea_results[i];
    html << "<tr><td>" << i << "</td>";
    html << "<td>" << (fea.is_valid ? "Yes" : "No") << "</td>";
    html << "<td>" << fea.max_stress_mpa << "</td>";
    html << "<td>" << fea.max_deflection_mm << "</td></tr>\n";
  }
  html << "</table>\n";

  html << "</body>\n</html>\n";
  return html.str();
}

std::string ManufacturingWorkflow::generatePDFReport(const WorkflowResult& result) {
  // Placeholder: actual PDF generation would require a PDF library
  return "PDF Report (not fully implemented)\n";
}

std::string ManufacturingWorkflow::generateJSONReport(const WorkflowResult& result) {
  std::ostringstream json;
  json << std::fixed << std::setprecision(2);

  json << "{\n";
  json << "  \"success\": " << (result.success ? "true" : "false") << ",\n";
  json << "  \"total_manufacturing_time\": " << result.total_manufacturing_time << ",\n";
  json << "  \"total_estimated_cost\": " << result.total_estimated_cost << ",\n";
  json << "  \"instructions_count\": " << result.manufacturing_plan.instructions.size() << ",\n";
  json << "  \"errors\": " << result.errors.size() << ",\n";
  json << "  \"warnings\": " << result.warnings.size() << "\n";
  json << "}\n";

  return json.str();
}

}  // namespace bendcore::manufacturing
