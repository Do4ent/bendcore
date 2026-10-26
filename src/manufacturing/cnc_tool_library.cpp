#include "cnc_tool_library.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>

namespace bendcore::manufacturing::cnc {

// ============================================================================
// ToolDefinition Implementation
// ============================================================================

double ToolDefinition::getRigidityScore() const {
  // Rigidity calculation based on diameter and length ratio
  if (flute_length_mm <= 0 || diameter_mm <= 0) return 0.0;

  double ratio = flute_length_mm / diameter_mm;
  // Shorter tools are more rigid (less deflection)
  // Score from 0-100, with ideal ratio < 4
  double score = 100.0 - (ratio / 10.0 * 100.0);
  return std::max(0.0, std::min(100.0, score));
}

std::map<std::string, double> ToolDefinition::getRecommendedSpeeds(
    const std::string& material) const {
  std::map<std::string, double> speeds;

  // Base cutting speeds (m/min) for different tool materials and workpiece materials
  if (material == "steel") {
    if (this->material == ToolMaterial::HSS) {
      speeds["cutting_speed"] = 30.0;
    } else if (this->material == ToolMaterial::CARBIDE) {
      speeds["cutting_speed"] = 150.0;
    } else if (this->material == ToolMaterial::CERAMIC) {
      speeds["cutting_speed"] = 300.0;
    }
  } else if (material == "aluminum") {
    if (this->material == ToolMaterial::HSS) {
      speeds["cutting_speed"] = 100.0;
    } else if (this->material == ToolMaterial::CARBIDE) {
      speeds["cutting_speed"] = 300.0;
    }
  } else if (material == "copper") {
    if (this->material == ToolMaterial::HSS) {
      speeds["cutting_speed"] = 50.0;
    } else if (this->material == ToolMaterial::CARBIDE) {
      speeds["cutting_speed"] = 200.0;
    }
  } else if (material == "titanium") {
    if (this->material == ToolMaterial::CARBIDE) {
      speeds["cutting_speed"] = 80.0;
    } else if (this->material == ToolMaterial::CERAMIC) {
      speeds["cutting_speed"] = 200.0;
    }
  }

  // Default fallback
  if (speeds.empty()) {
    speeds["cutting_speed"] = 100.0;
  }

  return speeds;
}

// ============================================================================
// CNCToolLibrary Implementation
// ============================================================================

CNCToolLibrary::CNCToolLibrary() {
  initializeMaterialDatabase();
}

bool CNCToolLibrary::addTool(const ToolDefinition& tool) {
  ToolDefinition new_tool = tool;
  if (new_tool.tool_id == 0) {
    new_tool.tool_id = next_tool_id_++;
  } else {
    next_tool_id_ = std::max(next_tool_id_, new_tool.tool_id + 1);
  }

  tool_database_[new_tool.tool_id] = new_tool;
  return true;
}

bool CNCToolLibrary::removeTool(int tool_id) {
  return tool_database_.erase(tool_id) > 0;
}

std::optional<ToolDefinition> CNCToolLibrary::getTool(int tool_id) const {
  auto it = tool_database_.find(tool_id);
  if (it != tool_database_.end()) {
    return it->second;
  }
  return std::nullopt;
}

std::vector<ToolDefinition> CNCToolLibrary::getToolsByType(ToolType type) const {
  std::vector<ToolDefinition> result;
  for (const auto& [id, tool] : tool_database_) {
    if (tool.type == type) {
      result.push_back(tool);
    }
  }
  return result;
}

std::vector<ToolDefinition> CNCToolLibrary::getToolsByMaterial(
    ToolMaterial material) const {
  std::vector<ToolDefinition> result;
  for (const auto& [id, tool] : tool_database_) {
    if (tool.material == material) {
      result.push_back(tool);
    }
  }
  return result;
}

std::vector<ToolDefinition> CNCToolLibrary::getAllTools() const {
  std::vector<ToolDefinition> result;
  for (const auto& [id, tool] : tool_database_) {
    result.push_back(tool);
  }
  return result;
}

SpeedFeedData CNCToolLibrary::getSpeedFeedData(
    int tool_id,
    const std::string& workpiece_material,
    double desired_surface_finish_um) const {
  SpeedFeedData data;
  data.material = workpiece_material;
  data.surface_finish_ra_um = static_cast<int>(desired_surface_finish_um);

  auto tool_opt = getTool(tool_id);
  if (!tool_opt) return data;

  const auto& tool = tool_opt.value();

  // Get recommended cutting speeds
  auto speeds = tool.getRecommendedSpeeds(workpiece_material);
  if (speeds.count("cutting_speed")) {
    data.cutting_speed_m_min = speeds["cutting_speed"];
  }

  // Set feed per tooth based on tool type and material
  if (workpiece_material == "aluminum") {
    data.feed_per_tooth_mm = tool.type == ToolType::END_MILL ? 0.15 : 0.10;
  } else if (workpiece_material == "steel") {
    data.feed_per_tooth_mm = tool.type == ToolType::END_MILL ? 0.10 : 0.06;
  } else {
    data.feed_per_tooth_mm = 0.08;
  }

  // Recommended depth of cut
  data.depth_of_cut_mm = std::min(5.0, tool.diameter_mm * 0.5);

  // Calculate spindle speed
  data.calculateSpindleSpeed(tool.diameter_mm);

  // Calculate feed rate
  data.calculateFeedRate(tool.num_flutes);

  return data;
}

double CNCToolLibrary::estimateToolLife(
    int tool_id,
    double cutting_length_mm,
    const std::string& material,
    double spindle_speed_rpm,
    double feed_rate_mm_min) const {
  auto tool_opt = getTool(tool_id);
  if (!tool_opt) return 0.0;

  const auto& tool = tool_opt.value();

  // Estimate tool wear based on cutting parameters
  // Using Taylor's tool life equation: VT^n = C
  // Where V = cutting speed, T = tool life, n = exponent (typically 0.2-0.5)

  if (spindle_speed_rpm <= 0 || feed_rate_mm_min <= 0) return tool.estimated_life_mm;

  // Basic tool life degradation factor
  double degradation_factor = 1.0;

  if (material == "steel") {
    degradation_factor = 1.0;
  } else if (material == "aluminum") {
    degradation_factor = 1.5;  // Aluminum wears tools faster
  } else if (material == "titanium") {
    degradation_factor = 0.5;  // Titanium is hard on tools
  }

  double cutting_speed_m_min =
      (spindle_speed_rpm * M_PI * tool.diameter_mm) / 1000.0;
  double taylor_life = calculateTaylorFactorLife(spindle_speed_rpm,
                                                 cutting_speed_m_min, material);

  return tool.estimated_life_mm * degradation_factor * taylor_life;
}

std::vector<int> CNCToolLibrary::getToolChangeRecommendations(
    const std::vector<int>& current_tools_used,
    double remaining_tool_life_percent) const {
  std::vector<int> recommendations;

  for (int tool_id : current_tools_used) {
    // Tools with less than the specified remaining life should be changed
    // In a real system, this would track actual tool wear
    if (remaining_tool_life_percent < 20.0) {
      recommendations.push_back(tool_id);
    }
  }

  return recommendations;
}

std::optional<int> CNCToolLibrary::findOptimalTool(
    ToolType operation_type,
    double required_diameter_mm,
    const std::string& workpiece_material,
    bool must_be_coolant_capable) const {
  std::vector<ToolDefinition> candidates;

  // Find tools matching operation type
  auto tools_by_type = getToolsByType(operation_type);

  for (const auto& tool : tools_by_type) {
    // Check diameter match (within 10% tolerance)
    double diameter_tolerance = required_diameter_mm * 0.1;
    if (std::abs(tool.diameter_mm - required_diameter_mm) > diameter_tolerance) {
      continue;
    }

    // Check coolant capability if required
    if (must_be_coolant_capable && !tool.coolant_capable) {
      continue;
    }

    // Check material compatibility
    if (!tool.compatible_materials.empty()) {
      auto it = std::find(tool.compatible_materials.begin(),
                          tool.compatible_materials.end(), workpiece_material);
      if (it == tool.compatible_materials.end()) {
        continue;
      }
    }

    candidates.push_back(tool);
  }

  // Return the first suitable tool
  if (!candidates.empty()) {
    // Prefer carbide over HSS for better performance
    for (const auto& tool : candidates) {
      if (tool.material == ToolMaterial::CARBIDE) {
        return tool.tool_id;
      }
    }
    return candidates[0].tool_id;
  }

  return std::nullopt;
}

CNCToolLibrary::ToolInventory CNCToolLibrary::getInventoryStatus() const {
  ToolInventory status;
  status.total_tools = tool_database_.size();

  std::map<ToolType, int> type_count;
  for (const auto& [id, tool] : tool_database_) {
    type_count[tool.type]++;
    status.total_inventory_value += tool.cost_dollars;
  }

  status.tools_by_type = type_count;
  status.tools_available = status.total_tools;  // In real system, track usage
  status.tools_in_use = 0;

  return status;
}

double CNCToolLibrary::calculateToolingCost(
    const std::vector<int>& tool_ids) const {
  double total_cost = 0.0;
  for (int tool_id : tool_ids) {
    auto tool_opt = getTool(tool_id);
    if (tool_opt) {
      total_cost += tool_opt.value().cost_dollars;
    }
  }
  return total_cost;
}

void CNCToolLibrary::loadStandardLibrary() {
  // Load commonly used end mills
  ToolDefinition end_mill_2mm;
  end_mill_2mm.tool_id = 1;
  end_mill_2mm.name = "2mm Carbide End Mill";
  end_mill_2mm.type = ToolType::END_MILL;
  end_mill_2mm.material = ToolMaterial::CARBIDE;
  end_mill_2mm.diameter_mm = 2.0;
  end_mill_2mm.length_mm = 38.0;
  end_mill_2mm.flute_length_mm = 6.0;
  end_mill_2mm.num_flutes = 2;
  end_mill_2mm.cost_dollars = 15.0;
  end_mill_2mm.compatible_materials = {"aluminum", "steel", "copper"};
  addTool(end_mill_2mm);

  ToolDefinition end_mill_4mm;
  end_mill_4mm.tool_id = 2;
  end_mill_4mm.name = "4mm Carbide End Mill";
  end_mill_4mm.type = ToolType::END_MILL;
  end_mill_4mm.material = ToolMaterial::CARBIDE;
  end_mill_4mm.diameter_mm = 4.0;
  end_mill_4mm.length_mm = 40.0;
  end_mill_4mm.flute_length_mm = 10.0;
  end_mill_4mm.num_flutes = 2;
  end_mill_4mm.cost_dollars = 20.0;
  end_mill_4mm.compatible_materials = {"aluminum", "steel", "copper"};
  addTool(end_mill_4mm);

  // Drills
  ToolDefinition drill_3mm;
  drill_3mm.tool_id = 3;
  drill_3mm.name = "3mm Carbide Drill";
  drill_3mm.type = ToolType::DRILL;
  drill_3mm.material = ToolMaterial::CARBIDE;
  drill_3mm.diameter_mm = 3.0;
  drill_3mm.length_mm = 45.0;
  drill_3mm.flute_length_mm = 8.0;
  drill_3mm.num_flutes = 2;
  drill_3mm.cost_dollars = 10.0;
  drill_3mm.compatible_materials = {"aluminum", "steel", "copper"};
  addTool(drill_3mm);

  // Reamers
  ToolDefinition reamer_5mm;
  reamer_5mm.tool_id = 4;
  reamer_5mm.name = "5mm Carbide Reamer";
  reamer_5mm.type = ToolType::REAMER;
  reamer_5mm.material = ToolMaterial::CARBIDE;
  reamer_5mm.diameter_mm = 5.0;
  reamer_5mm.length_mm = 50.0;
  reamer_5mm.flute_length_mm = 12.0;
  reamer_5mm.num_flutes = 4;
  reamer_5mm.cost_dollars = 25.0;
  reamer_5mm.compatible_materials = {"aluminum", "steel", "copper"};
  addTool(reamer_5mm);

  // Taps
  ToolDefinition tap_m4;
  tap_m4.tool_id = 5;
  tap_m4.name = "M4 Carbide Tap";
  tap_m4.type = ToolType::TAP;
  tap_m4.material = ToolMaterial::CARBIDE;
  tap_m4.diameter_mm = 4.0;
  tap_m4.length_mm = 50.0;
  tap_m4.flute_length_mm = 10.0;
  tap_m4.num_flutes = 3;
  tap_m4.cost_dollars = 12.0;
  tap_m4.compatible_materials = {"aluminum", "steel"};
  addTool(tap_m4);

  // Boring tools
  ToolDefinition boring_tool_6mm;
  boring_tool_6mm.tool_id = 6;
  boring_tool_6mm.name = "6mm Boring Tool";
  boring_tool_6mm.type = ToolType::BORING_TOOL;
  boring_tool_6mm.material = ToolMaterial::CARBIDE;
  boring_tool_6mm.diameter_mm = 6.0;
  boring_tool_6mm.length_mm = 60.0;
  boring_tool_6mm.flute_length_mm = 15.0;
  boring_tool_6mm.num_flutes = 1;
  boring_tool_6mm.cost_dollars = 40.0;
  boring_tool_6mm.compatible_materials = {"aluminum", "steel"};
  addTool(boring_tool_6mm);

  // Ball mills for curved surfaces
  ToolDefinition ball_mill_3mm;
  ball_mill_3mm.tool_id = 7;
  ball_mill_3mm.name = "3mm Ball Mill";
  ball_mill_3mm.type = ToolType::BALL_MILL;
  ball_mill_3mm.material = ToolMaterial::CARBIDE;
  ball_mill_3mm.diameter_mm = 3.0;
  ball_mill_3mm.length_mm = 38.0;
  ball_mill_3mm.flute_length_mm = 8.0;
  ball_mill_3mm.corner_radius_mm = 1.5;
  ball_mill_3mm.num_flutes = 2;
  ball_mill_3mm.cost_dollars = 22.0;
  ball_mill_3mm.compatible_materials = {"aluminum", "steel", "copper"};
  addTool(ball_mill_3mm);
}

bool CNCToolLibrary::saveLibrary(const std::string& filename) const {
  std::ofstream file(filename);
  if (!file.is_open()) {
    return false;
  }

  // Simple CSV format
  file << "tool_id,name,type,material,diameter_mm,length_mm,flute_length_mm,"
          "num_flutes,cost_dollars\n";

  for (const auto& [id, tool] : tool_database_) {
    file << tool.tool_id << "," << tool.name << "," << static_cast<int>(tool.type)
         << "," << static_cast<int>(tool.material) << "," << tool.diameter_mm
         << "," << tool.length_mm << "," << tool.flute_length_mm << ","
         << tool.num_flutes << "," << tool.cost_dollars << "\n";
  }

  return true;
}

bool CNCToolLibrary::loadLibrary(const std::string& filename) {
  std::ifstream file(filename);
  if (!file.is_open()) {
    return false;
  }

  std::string line;
  std::getline(file, line);  // Skip header

  while (std::getline(file, line)) {
    if (line.empty()) continue;

    std::istringstream iss(line);
    ToolDefinition tool;
    int type_val, material_val;

    char comma;
    iss >> tool.tool_id >> comma >> tool.name >> comma >> type_val >> comma
        >> material_val >> comma >> tool.diameter_mm >> comma >> tool.length_mm
        >> comma >> tool.flute_length_mm >> comma >> tool.num_flutes >> comma
        >> tool.cost_dollars;

    tool.type = static_cast<ToolType>(type_val);
    tool.material = static_cast<ToolMaterial>(material_val);

    addTool(tool);
  }

  return true;
}

void CNCToolLibrary::initializeMaterialDatabase() {
  // Initialize speed/feed data for common materials
  MaterialSpeedDatabase steel_db;
  SpeedFeedData steel_speeds;
  steel_speeds.material = "steel";
  steel_speeds.cutting_speed_m_min = 100.0;
  steel_speeds.feed_per_tooth_mm = 0.10;
  steel_speeds.depth_of_cut_mm = 3.0;
  steel_db.speeds_by_material["standard"] = steel_speeds;

  MaterialSpeedDatabase aluminum_db;
  SpeedFeedData aluminum_speeds;
  aluminum_speeds.material = "aluminum";
  aluminum_speeds.cutting_speed_m_min = 200.0;
  aluminum_speeds.feed_per_tooth_mm = 0.15;
  aluminum_speeds.depth_of_cut_mm = 5.0;
  aluminum_db.speeds_by_material["standard"] = aluminum_speeds;

  material_database_["steel"] = steel_db;
  material_database_["aluminum"] = aluminum_db;
}

double CNCToolLibrary::calculateTaylorFactorLife(
    double spindle_rpm,
    double cutting_speed,
    const std::string& material) const {
  // Simplified Taylor's tool life equation
  // VT^n = C, where n is typically 0.2-0.5 depending on material
  double n = 0.3;  // Default exponent

  if (material == "steel") {
    n = 0.25;
  } else if (material == "aluminum") {
    n = 0.35;
  } else if (material == "titanium") {
    n = 0.15;
  }

  // Simplified calculation returning a factor
  if (cutting_speed <= 0) return 1.0;
  return std::pow(100.0 / cutting_speed, n);
}

}  // namespace bendcore::manufacturing::cnc
