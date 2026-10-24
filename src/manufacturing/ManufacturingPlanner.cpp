#include "ManufacturingPlanner.hpp"
#include <algorithm>
#include <numeric>
#include <sstream>
#include <iomanip>

namespace bendcore::manufacturing {

ManufacturingPlanner::ManufacturingPlanner() {}

ManufacturingPlan ManufacturingPlanner::generatePlan(
    const std::vector<Tube>& tubes,
    const ManufacturingConstraints& constraints) {

  ManufacturingPlan plan;

  // 1. Sort tubes by diameter (largest first)
  auto sorted_tubes = tubes;
  std::sort(sorted_tubes.begin(), sorted_tubes.end(),
    [](const Tube& a, const Tube& b) {
      return a.outer_diameter > b.outer_diameter;
    }
  );

  // 2. Select available bending machines
  std::vector<BendingMachine> machines =
    selectBendingMachines(constraints);

  // 3. Assign tubes to machines
  std::map<std::string, std::vector<const Tube*>> machine_schedule;

  for (const auto& tube : sorted_tubes) {
    BendingMachine* best_machine = nullptr;
    double best_score = std::numeric_limits<double>::infinity();

    for (auto& machine : machines) {
      // Check if tube fits machine constraints
      if (tube.outer_diameter < machine.tube_diameter_range[0] ||
          tube.outer_diameter > machine.tube_diameter_range[1]) {
        continue;
      }

      // Calculate scheduling score
      double score = estimateCost(machine, tube);

      if (score < best_score) {
        best_score = score;
        best_machine = &machine;
      }
    }

    if (best_machine) {
      machine_schedule[best_machine->name].push_back(&tube);
    }
  }

  // 4. Generate work instructions and NC programs
  for (auto& [machine_name, tubes_for_machine] : machine_schedule) {
    auto it = std::find_if(machines.begin(), machines.end(),
      [&machine_name](const BendingMachine& m) {
        return m.name == machine_name;
      }
    );

    if (it == machines.end()) continue;
    const BendingMachine& machine = *it;

    for (const auto* tube : tubes_for_machine) {
      // Create work instruction
      WorkInstruction instr{
        .tube_id = tube->id,
        .machine = machine.name,
        .material = tube->material,
        .length = tube->length,
        .outer_diameter = tube->outer_diameter,
        .wall_thickness = tube->wall_thickness,
        .bending_sequence = generateBendSequence(*tube),
        .estimated_time = estimateTime(*tube),
        .quality_checks = generateQualityChecks(*tube),
        .safety_notes = generateSafetyNotes(machine)
      };
      plan.instructions.push_back(instr);

      // Create NC program
      NCProgram nc = generateNCProgram(*tube, machine);
      plan.nc_programs.push_back(nc);

      // Update plan metrics
      plan.estimated_time += instr.estimated_time;
      plan.estimated_cost += estimateCost(machine, *tube);
    }
  }

  // 5. Generate material list
  generateMaterialList(sorted_tubes, plan);

  // 6. Optimize scheduling
  optimizeSequence(plan);

  return plan;
}

NCProgram ManufacturingPlanner::generateNCProgram(
    const Tube& tube,
    const BendingMachine& machine) {

  NCProgram program;
  program.machine = machine.name;
  program.material = tube.material;
  program.tube_id = tube.id;

  // Initialize G-code header
  std::ostringstream gcode;
  gcode << std::fixed << std::setprecision(3);

  // Header
  gcode << "G17 G20 G40 G49 G54 G80 G90 G94\n";
  gcode << "(Tube ID: " << tube.id << ")\n";
  gcode << "(Material: " << tube.material << ")\n";
  gcode << "(Length: " << tube.length << " mm)\n";
  gcode << "(Diameter: " << tube.outer_diameter << " mm)\n";
  gcode << "\n";

  // Tool change and spindle startup
  gcode << "M06 T01 (Tool change to bender)\n";
  gcode << "M03 S" << machine.spindle_speed << " (Start spindle)\n";
  gcode << "\n";

  // Initial positioning
  gcode << "G00 X0 Y0 Z0 (Position to start)\n";
  gcode << "G01 Z-" << (tube.outer_diameter / 2.0)
        << " F100 (Approach workpiece)\n";
  gcode << "\n";

  // Generate bending operations
  double current_position = 0.0;

  for (size_t i = 0; i < tube.bends.size(); i++) {
    const auto& bend = tube.bends[i];

    gcode << "(Bend " << (i + 1) << ")\n";
    gcode << "G00 X" << bend.position << " (Move to bend position)\n";

    // Bending command (machine-specific)
    if (machine.type == MachineType::CNC_PIPE_BENDER) {
      gcode << "G65 P9001 R" << bend.angle
            << " P" << bend.radius
            << " (Bend operation)\n";
    } else if (machine.type == MachineType::TUBE_BENDER_MANUAL) {
      program.manual_notes.push_back(
        "Position " + std::to_string(i + 1) +
        ": Bend " + std::to_string(static_cast<int>(bend.angle)) +
        "° at X=" + std::to_string(static_cast<int>(bend.position)) +
        " (Radius: " + std::to_string(static_cast<int>(bend.radius)) + "mm)"
      );
    }

    gcode << "G04 P" << (bend.dwell_time * 1000) << " (Dwell time)\n";
    gcode << "\n";

    current_position = bend.position;
  }

  // Cleanup
  gcode << "(End of program)\n";
  gcode << "G00 Z10 (Retract)\n";
  gcode << "M05 (Stop spindle)\n";
  gcode << "M30 (End program)\n";
  gcode << "%\n";

  program.gcode = gcode.str();

  return program;
}

std::vector<BendingMachine> ManufacturingPlanner::selectBendingMachines(
    const ManufacturingConstraints& constraints) {

  std::vector<BendingMachine> machines;

  // Popular CNC pipe bender
  machines.push_back(BendingMachine{
    .name = "AMOB BC8060",
    .type = MachineType::CNC_PIPE_BENDER,
    .min_radius = 25.0,
    .max_radius = 8000.0,
    .max_bending_angle = 180.0,
    .tube_diameter_range = {3.0, 60.0},
    .spindle_speed = 3000,
    .cost_per_hour = 150.0,
    .availability = 1.0
  });

  // Manual tube bender
  machines.push_back(BendingMachine{
    .name = "Hilpert RB67",
    .type = MachineType::TUBE_BENDER_MANUAL,
    .min_radius = 50.0,
    .max_radius = 5000.0,
    .max_bending_angle = 160.0,
    .tube_diameter_range = {6.0, 50.0},
    .spindle_speed = 2500,
    .cost_per_hour = 120.0,
    .availability = 0.9
  });

  // Compact bender
  machines.push_back(BendingMachine{
    .name = "Triton TBM-40",
    .type = MachineType::CNC_PIPE_BENDER,
    .min_radius = 40.0,
    .max_radius = 4000.0,
    .max_bending_angle = 150.0,
    .tube_diameter_range = {8.0, 40.0},
    .spindle_speed = 2800,
    .cost_per_hour = 100.0,
    .availability = 0.95
  });

  return machines;
}

std::string ManufacturingPlanner::generateBendSequence(
    const Tube& tube) {

  std::ostringstream sequence;

  for (size_t i = 0; i < tube.bends.size(); i++) {
    const auto& bend = tube.bends[i];
    sequence << "Bend " << (i + 1) << ": ";
    sequence << std::fixed << std::setprecision(1);
    sequence << "Position " << bend.position << "mm, ";
    sequence << "Angle " << bend.angle << "°, ";
    sequence << "Radius " << bend.radius << "mm";

    if (i < tube.bends.size() - 1) {
      sequence << "\n";
    }
  }

  return sequence.str();
}

double ManufacturingPlanner::estimateTime(const Tube& tube) {
  // Base time: 1 second per millimeter of tube
  double base_time = tube.length / 1000.0;

  // Add time for each bend: 0.5 minutes per bend
  double bend_time = tube.bends.size() * 30.0;

  // Add setup and cleanup: 5 minutes
  double setup_time = 300.0;

  return base_time + bend_time + setup_time;
}

double ManufacturingPlanner::estimateCost(
    const BendingMachine& machine,
    const Tube& tube) {

  double time = estimateTime(tube);
  double machine_cost = (time / 3600.0) * machine.cost_per_hour;

  // Material cost
  double material_volume = (tube.outer_diameter / 2.0) *
                          (tube.outer_diameter / 2.0) *
                          tube.length * 3.14159;
  double material_cost = material_volume * 0.000001 * 8.0;  // $/mm^3

  return machine_cost + material_cost;
}

std::string ManufacturingPlanner::generateQualityChecks(
    const Tube& tube) {

  std::ostringstream checks;
  checks << "1. Visual inspection for cracks\n";
  checks << "2. Measure bend angles (±2°)\n";
  checks << "3. Verify bend radii (±2mm)\n";
  checks << "4. Check tube straightness (±3mm over length)\n";
  checks << "5. Surface finish inspection\n";
  checks << "6. Dimensional verification";

  return checks.str();
}

std::string ManufacturingPlanner::generateSafetyNotes(
    const BendingMachine& machine) {

  std::ostringstream notes;
  notes << "Safety Requirements for " << machine.name << ":\n";
  notes << "- Ensure proper machine guarding\n";
  notes << "- Wear safety glasses and gloves\n";
  notes << "- Check workpiece clamping before operation\n";
  notes << "- Clear all obstacles from work area\n";
  notes << "- Emergency stop button must be accessible\n";
  notes << "- Do not reach into bending area during operation";

  return notes.str();
}

void ManufacturingPlanner::generateMaterialList(
    const std::vector<Tube>& tubes,
    ManufacturingPlan& plan) {

  std::map<std::string, Material> materials;

  for (const auto& tube : tubes) {
    if (materials.find(tube.material) == materials.end()) {
      materials[tube.material] = Material{
        .type = tube.material,
        .quantity = 1,
        .unit = "piece"
      };
    } else {
      materials[tube.material].quantity++;
    }
  }

  for (const auto& [name, material] : materials) {
    plan.materials.push_back(material);
  }
}

void ManufacturingPlanner::optimizeSequence(ManufacturingPlan& plan) {
  // Sort work instructions by machine to minimize changeovers
  std::stable_sort(plan.instructions.begin(), plan.instructions.end(),
    [](const WorkInstruction& a, const WorkInstruction& b) {
      return a.machine < b.machine;
    }
  );

  // Calculate cumulative time
  double cumulative_time = 0.0;
  for (auto& instr : plan.instructions) {
    cumulative_time += instr.estimated_time;
    instr.scheduled_start_time = cumulative_time - instr.estimated_time;
    instr.scheduled_end_time = cumulative_time;
  }
}

}  // namespace bendcore::manufacturing
