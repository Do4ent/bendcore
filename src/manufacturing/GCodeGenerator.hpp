#pragma once

#include <vector>
#include <string>
#include <map>
#include <memory>
#include "ManufacturingPlanner.hpp"

namespace bendcore::manufacturing {

// G-Code Generation for CNC Pipe Bending Machines
class GCodeGenerator {
 public:
  enum class MachineControllerType {
    FANUC,       // FANUC controllers
    SIEMENS,     // SIEMENS SINUMERIK
    HEIDENHAIN,  // HEIDENHAIN iTNC
    GENERIC_ISO  // Generic ISO G-code
  };

  enum class CoordinateSystem {
    ABSOLUTE,  // G90 absolute positioning
    INCREMENTAL  // G91 incremental positioning
  };

  struct GCodeConfig {
    MachineControllerType controller = MachineControllerType::GENERIC_ISO;
    CoordinateSystem coordinate_system = CoordinateSystem::ABSOLUTE;
    double feed_rate_mm_min = 100.0;
    double spindle_speed_rpm = 2000;
    double rapid_move_speed = 500.0;  // mm/min for G00 rapid moves
    bool include_comments = true;
    bool include_tool_changes = true;
    bool include_coolant = false;
    std::string coolant_code = "M7";  // M7=mist, M8=flood, M9=off
    int decimal_places = 4;
  };

  struct ToolDefinition {
    int tool_number = 0;
    std::string tool_name;
    double diameter_mm = 0.0;
    double flute_length_mm = 0.0;
    int flutes = 2;
    double rpm = 2000;
    double feedrate = 100.0;
  };

  GCodeGenerator();

  // Generate complete G-code program
  std::string generateGCode(
    const Tube& tube,
    const BendingMachine& machine,
    const GCodeConfig& config
  );

  // Generate G-code with custom toolpath
  std::string generateCustomGCode(
    const Tube& tube,
    const std::vector<ToolDefinition>& tools,
    const GCodeConfig& config
  );

  // Export to file
  bool exportToFile(
    const std::string& gcode,
    const std::string& filename
  );

  // Validate G-code for syntax errors
  bool validateGCode(const std::string& gcode);

  // Get estimated machining time
  double estimateMachiningTime(
    const Tube& tube,
    const BendingMachine& machine,
    const GCodeConfig& config
  );

  // Controller-specific code generation
  std::string generateFanucCode(const Tube& tube, const GCodeConfig& config);
  std::string generateSiemensCode(const Tube& tube, const GCodeConfig& config);
  std::string generateHeidenhainCode(const Tube& tube, const GCodeConfig& config);

 private:
  // Internal G-code generation helpers
  std::string generatePreamble(
    const Tube& tube,
    const BendingMachine& machine,
    const GCodeConfig& config
  );

  std::string generateToolChanges(
    const std::vector<ToolDefinition>& tools,
    const GCodeConfig& config
  );

  std::string generateBendSequence(
    const Tube& tube,
    const BendingMachine& machine,
    const GCodeConfig& config
  );

  std::string generateBendCommand(
    const Bend& bend,
    int bend_number,
    const GCodeConfig& config
  );

  std::string generatePostamble(const GCodeConfig& config);

  // Tool path optimization
  void optimizeToolPath(std::vector<Bend>& bends);

  // Validate bend parameters for machine
  bool validateBendForMachine(
    const Bend& bend,
    const BendingMachine& machine
  );

  GCodeConfig default_config_;
  std::map<std::string, int> gcode_line_counter_;
};

}  // namespace bendcore::manufacturing
