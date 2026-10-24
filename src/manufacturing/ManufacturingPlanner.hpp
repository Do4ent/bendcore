#pragma once

#include <vector>
#include <map>
#include <string>
#include <array>

namespace bendcore::manufacturing {

// Enums

enum class MachineType {
  CNC_PIPE_BENDER,
  TUBE_BENDER_MANUAL,
  PLASMA_CUTTER,
  GRINDING_MACHINE
};

// Structures

struct Bend {
  double position;    // Position along tube (mm)
  double angle;       // Bending angle (degrees)
  double radius;      // Bending radius (mm)
  double dwell_time;  // Time to hold bend (seconds)
};

struct Tube {
  std::string id;
  std::string material;
  double length;
  double outer_diameter;
  double wall_thickness;
  std::vector<Bend> bends;
};

struct BendingMachine {
  std::string name;
  MachineType type;
  double min_radius;
  double max_radius;
  double max_bending_angle;
  std::array<double, 2> tube_diameter_range;  // [min, max]
  int spindle_speed;
  double cost_per_hour;
  double availability;
};

struct WorkInstruction {
  std::string tube_id;
  std::string machine;
  std::string material;
  double length;
  double outer_diameter;
  double wall_thickness;
  std::string bending_sequence;
  double estimated_time;
  std::string quality_checks;
  std::string safety_notes;
  double scheduled_start_time = 0.0;
  double scheduled_end_time = 0.0;
};

struct NCProgram {
  std::string machine;
  std::string tube_id;
  std::string material;
  std::string gcode;
  std::vector<std::string> manual_notes;
};

struct Material {
  std::string type;
  int quantity;
  std::string unit;
};

struct ToolChange {
  std::string machine;
  int old_tool;
  int new_tool;
  std::string reason;
};

struct ManufacturingConstraints {
  double max_wall_thickness = 10.0;
  double min_bend_radius = 25.0;
  int max_concurrent_machines = 5;
  bool optimize_for_cost = true;
  bool optimize_for_time = false;
};

struct ManufacturingPlan {
  std::vector<WorkInstruction> instructions;
  std::vector<NCProgram> nc_programs;
  std::vector<Material> materials;
  std::vector<ToolChange> tool_changes;
  double estimated_time = 0.0;  // Total hours
  double estimated_cost = 0.0;  // Total cost
};

// Main Manufacturing Planner class

class ManufacturingPlanner {
 public:
  ManufacturingPlanner();

  // Generate complete manufacturing plan
  ManufacturingPlan generatePlan(
    const std::vector<Tube>& tubes,
    const ManufacturingConstraints& constraints
  );

  // Individual plan components
  NCProgram generateNCProgram(
    const Tube& tube,
    const BendingMachine& machine
  );

 private:
  // Machine selection
  std::vector<BendingMachine> selectBendingMachines(
    const ManufacturingConstraints& constraints
  );

  // Plan generation helpers
  std::string generateBendSequence(const Tube& tube);
  double estimateTime(const Tube& tube);
  double estimateCost(const BendingMachine& machine, const Tube& tube);

  std::string generateQualityChecks(const Tube& tube);
  std::string generateSafetyNotes(const BendingMachine& machine);

  void generateMaterialList(
    const std::vector<Tube>& tubes,
    ManufacturingPlan& plan
  );

  // Optimization
  void optimizeSequence(ManufacturingPlan& plan);
};

}  // namespace bendcore::manufacturing
