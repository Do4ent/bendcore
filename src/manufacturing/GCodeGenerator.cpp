#include "GCodeGenerator.hpp"
#include <sstream>
#include <fstream>
#include <iomanip>
#include <cmath>
#include <algorithm>

namespace bendcore::manufacturing {

GCodeGenerator::GCodeGenerator() {
  default_config_.controller = MachineControllerType::GENERIC_ISO;
  default_config_.coordinate_system = CoordinateSystem::ABSOLUTE;
}

std::string GCodeGenerator::generateGCode(
    const Tube& tube,
    const BendingMachine& machine,
    const GCodeConfig& config) {

  std::ostringstream gcode;
  gcode << std::fixed << std::setprecision(config.decimal_places);

  // Preamble
  gcode << generatePreamble(tube, machine, config);

  // Bend sequence
  gcode << generateBendSequence(tube, machine, config);

  // Postamble
  gcode << generatePostamble(config);

  return gcode.str();
}

std::string GCodeGenerator::generateCustomGCode(
    const Tube& tube,
    const std::vector<ToolDefinition>& tools,
    const GCodeConfig& config) {

  std::ostringstream gcode;
  gcode << std::fixed << std::setprecision(config.decimal_places);

  gcode << generatePreamble(tube, BendingMachine(), config);

  if (config.include_tool_changes) {
    gcode << generateToolChanges(tools, config);
  }

  gcode << generateBendSequence(tube, BendingMachine(), config);
  gcode << generatePostamble(config);

  return gcode.str();
}

bool GCodeGenerator::exportToFile(
    const std::string& gcode,
    const std::string& filename) {

  std::ofstream file(filename);
  if (!file.is_open()) {
    return false;
  }

  file << gcode;
  file.close();
  return true;
}

bool GCodeGenerator::validateGCode(const std::string& gcode) {
  // Basic validation
  if (gcode.empty()) {
    return false;
  }

  // Check for G-code patterns
  bool has_g00_or_g01 = (gcode.find("G00") != std::string::npos ||
                         gcode.find("G01") != std::string::npos);

  return has_g00_or_g01;
}

double GCodeGenerator::estimateMachiningTime(
    const Tube& tube,
    const BendingMachine& machine,
    const GCodeConfig& config) {

  double total_time = 0.0;

  // Setup time
  total_time += 5.0;  // 5 minutes setup

  // Cutting/bending time
  for (const auto& bend : tube.bends) {
    // Time per bend
    double bend_time = std::abs(bend.angle) / 60.0;  // 1 degree per minute
    total_time += bend_time + bend.dwell_time / 60.0;
  }

  // Tool change time
  if (config.include_tool_changes) {
    total_time += tube.bends.size() * 2.0;  // 2 minutes per tool change
  }

  // Cooldown/unload time
  total_time += 3.0;  // 3 minutes

  return total_time;
}

std::string GCodeGenerator::generateFanucCode(
    const Tube& tube,
    const GCodeConfig& config) {

  std::ostringstream gcode;
  gcode << std::fixed << std::setprecision(config.decimal_places);

  gcode << "(FANUC G-Code Program for tube: " << tube.id << ")\n";
  gcode << "G90 G54 G40 G17\n";  // FANUC defaults
  gcode << "S" << static_cast<int>(config.spindle_speed_rpm) << " M03\n";

  for (size_t i = 0; i < tube.bends.size(); ++i) {
    const auto& bend = tube.bends[i];
    gcode << "G01 X" << bend.position << " Y0 Z0 F" << config.feed_rate_mm_min << "\n";
    gcode << "G04 P" << bend.dwell_time << "\n";  // Dwell
  }

  gcode << "M05 M02\n";  // Stop spindle, program end
  return gcode.str();
}

std::string GCodeGenerator::generateSiemensCode(
    const Tube& tube,
    const GCodeConfig& config) {

  std::ostringstream gcode;
  gcode << std::fixed << std::setprecision(config.decimal_places);

  gcode << "; SIEMENS SINUMERIK program for tube: " << tube.id << "\n";
  gcode << "G90 DIAMOF\n";  // SIEMENS defaults
  gcode << "S" << static_cast<int>(config.spindle_speed_rpm) << " M03\n";

  for (size_t i = 0; i < tube.bends.size(); ++i) {
    const auto& bend = tube.bends[i];
    gcode << "G01 X" << bend.position << " Y0 Z0 F" << config.feed_rate_mm_min << "\n";
    gcode << "G04 F" << bend.dwell_time << "\n";  // Dwell in seconds
  }

  gcode << "M05 M02\n";
  return gcode.str();
}

std::string GCodeGenerator::generateHeidenhainCode(
    const Tube& tube,
    const GCodeConfig& config) {

  std::ostringstream gcode;
  gcode << std::fixed << std::setprecision(config.decimal_places);

  gcode << "; HEIDENHAIN iTNC program for tube: " << tube.id << "\n";
  gcode << "BEGIN PGM Tube Main\n";
  gcode << "BLK FORM 0.1 Z X+100 Y+100 Z+100\n";
  gcode << "TOOL CALL 1 Z\n";
  gcode << "SPINDLE ON\n";
  gcode << "M3 S" << static_cast<int>(config.spindle_speed_rpm) << "\n";

  for (size_t i = 0; i < tube.bends.size(); ++i) {
    const auto& bend = tube.bends[i];
    gcode << "L X" << bend.position << " Y0 Z0 F" << config.feed_rate_mm_min << "\n";
    gcode << "DWELL " << bend.dwell_time << "\n";
  }

  gcode << "SPINDLE OFF\n";
  gcode << "M02\n";
  gcode << "END PGM\n";
  return gcode.str();
}

std::string GCodeGenerator::generatePreamble(
    const Tube& tube,
    const BendingMachine& machine,
    const GCodeConfig& config) {

  std::ostringstream preamble;
  preamble << std::fixed << std::setprecision(config.decimal_places);

  if (config.include_comments) {
    preamble << "(========================================)\n";
    preamble << "(BendCore G-Code Program)\n";
    preamble << "(Tube ID: " << tube.id << ")\n";
    preamble << "(Material: " << tube.material << ")\n";
    preamble << "(Length: " << tube.length << " mm)\n";
    preamble << "(OD: " << tube.outer_diameter << " mm)\n";
    preamble << "(Wall: " << tube.wall_thickness << " mm)\n";
    preamble << "(========================================)\n";
  }

  // Machine setup
  if (config.controller == MachineControllerType::GENERIC_ISO) {
    preamble << "G90 G40 G49\n";  // Absolute, tool radius cancel, tool length cancel
    preamble << "G28\n";  // Home
    preamble << "S" << static_cast<int>(config.spindle_speed_rpm) << " M03\n";
  }

  return preamble.str();
}

std::string GCodeGenerator::generateToolChanges(
    const std::vector<ToolDefinition>& tools,
    const GCodeConfig& config) {

  std::ostringstream tool_changes;

  if (tools.empty()) {
    return tool_changes.str();
  }

  for (const auto& tool : tools) {
    tool_changes << "T" << tool.tool_number << " M06\n";  // Tool change
    tool_changes << "S" << static_cast<int>(tool.rpm) << " M03\n";  // Set speed
  }

  return tool_changes.str();
}

std::string GCodeGenerator::generateBendSequence(
    const Tube& tube,
    const BendingMachine& machine,
    const GCodeConfig& config) {

  std::ostringstream sequence;
  sequence << std::fixed << std::setprecision(config.decimal_places);

  for (size_t i = 0; i < tube.bends.size(); ++i) {
    sequence << generateBendCommand(tube.bends[i], i, config);
  }

  return sequence.str();
}

std::string GCodeGenerator::generateBendCommand(
    const Bend& bend,
    int bend_number,
    const GCodeConfig& config) {

  std::ostringstream command;
  command << std::fixed << std::setprecision(config.decimal_places);

  // Move to bend position
  command << "G00 X" << bend.position << " Y0 Z0\n";  // Rapid move

  // Actual bend (feed move)
  command << "G01 X" << bend.position << " Y" << bend.radius * std::sin(bend.angle * M_PI / 180.0)
          << " Z0 F" << config.feed_rate_mm_min << "\n";

  // Dwell time
  if (bend.dwell_time > 0) {
    command << "G04 P" << bend.dwell_time << "\n";
  }

  if (config.include_comments) {
    command << "(Bend #" << (bend_number + 1) << ": " << bend.angle << " degrees)\n";
  }

  return command.str();
}

std::string GCodeGenerator::generatePostamble(const GCodeConfig& config) {
  std::ostringstream postamble;

  // Return to home
  postamble << "G28\n";

  // Stop spindle
  postamble << "M05\n";

  // Program end
  postamble << "M02\n";

  return postamble.str();
}

void GCodeGenerator::optimizeToolPath(std::vector<Bend>& bends) {
  // Simple optimization: sort bends by position
  std::sort(bends.begin(), bends.end(),
    [](const Bend& a, const Bend& b) { return a.position < b.position; });
}

bool GCodeGenerator::validateBendForMachine(
    const Bend& bend,
    const BendingMachine& machine) {

  // Check bend radius
  if (bend.radius < machine.min_radius || bend.radius > machine.max_radius) {
    return false;
  }

  // Check bend angle
  if (std::abs(bend.angle) > machine.max_bending_angle) {
    return false;
  }

  return true;
}

}  // namespace bendcore::manufacturing
