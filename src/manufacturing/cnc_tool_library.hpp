#pragma once

#include <vector>
#include <string>
#include <map>
#include <memory>
#include <optional>
#include <cmath>

namespace bendcore::manufacturing::cnc {

// ============================================================================
// Tool Types and Definitions
// ============================================================================

enum class ToolType {
  END_MILL,
  DRILL,
  TAP,
  REAMER,
  BORING_TOOL,
  FACE_MILL,
  SLOT_DRILL,
  BALL_MILL,
  CORNER_RADIUS_MILL,
  THREAD_MILL,
  CHAMFER_MILL,
  CORE_DRILL,
  STEP_DRILL
};

enum class ToolMaterial {
  HSS,          // High Speed Steel
  CARBIDE,      // Tungsten Carbide
  CERAMIC,      // Ceramic
  CBN,          // Cubic Boron Nitride
  DIAMOND       // Polycrystalline Diamond
};

enum class CuttingFluid {
  NONE,
  AIR,
  OIL,
  WATER_SOLUBLE,
  SEMI_SYNTHETIC,
  SYNTHETIC
};

// ============================================================================
// Tool Definition Structure
// ============================================================================

struct ToolDefinition {
  int tool_id = 0;
  std::string name;
  ToolType type = ToolType::END_MILL;
  ToolMaterial material = ToolMaterial::CARBIDE;
  double diameter_mm = 0.0;
  double length_mm = 0.0;
  double flute_length_mm = 0.0;
  double shank_diameter_mm = 0.0;
  int num_flutes = 2;
  double corner_radius_mm = 0.0;
  double helix_angle_degrees = 0.0;
  std::string manufacturer;
  std::string part_number;
  bool coolant_capable = true;
  CuttingFluid preferred_coolant = CuttingFluid::OIL;
  double estimated_life_mm = 1000.0;  // Tool life in mm of cutting
  double cost_dollars = 50.0;
  std::vector<std::string> compatible_materials;

  // Calculate tool rigidity score (0-100)
  double getRigidityScore() const;

  // Get recommended speeds for a material
  std::map<std::string, double> getRecommendedSpeeds(const std::string& material) const;
};

// ============================================================================
// Speeds and Feeds Database
// ============================================================================

struct SpeedFeedData {
  std::string material;
  double cutting_speed_m_min = 0.0;  // Recommended cutting speed
  double feed_per_tooth_mm = 0.0;    // Feed per tooth
  double depth_of_cut_mm = 0.0;      // Recommended depth of cut
  double spindle_speed_rpm = 0.0;    // Calculated spindle speed
  double feed_rate_mm_min = 0.0;     // Calculated feed rate
  int surface_finish_ra_um = 3;      // Surface finish in micrometers Ra

  // Calculate spindle speed from cutting speed
  void calculateSpindleSpeed(double tool_diameter_mm) {
    if (tool_diameter_mm > 0) {
      spindle_speed_rpm = (cutting_speed_m_min * 1000) / (M_PI * tool_diameter_mm);
    }
  }

  // Calculate feed rate from feed per tooth
  void calculateFeedRate(int num_flutes) {
    if (num_flutes > 0) {
      feed_rate_mm_min = feed_per_tooth_mm * spindle_speed_rpm * num_flutes;
    }
  }
};

// ============================================================================
// Tool Library Class
// ============================================================================

class CNCToolLibrary {
 public:
  CNCToolLibrary();

  // Tool management
  bool addTool(const ToolDefinition& tool);
  bool removeTool(int tool_id);
  std::optional<ToolDefinition> getTool(int tool_id) const;
  std::vector<ToolDefinition> getToolsByType(ToolType type) const;
  std::vector<ToolDefinition> getToolsByMaterial(ToolMaterial material) const;
  std::vector<ToolDefinition> getAllTools() const;

  // Speed and feed recommendations
  SpeedFeedData getSpeedFeedData(
    int tool_id,
    const std::string& workpiece_material,
    double desired_surface_finish_um = 3.2
  ) const;

  // Tool life estimation
  double estimateToolLife(
    int tool_id,
    double cutting_length_mm,
    const std::string& material,
    double spindle_speed_rpm,
    double feed_rate_mm_min
  ) const;

  // Tool change recommendations
  std::vector<int> getToolChangeRecommendations(
    const std::vector<int>& current_tools_used,
    double remaining_tool_life_percent = 20.0
  ) const;

  // Find optimal tool for operation
  std::optional<int> findOptimalTool(
    ToolType operation_type,
    double required_diameter_mm,
    const std::string& workpiece_material,
    bool must_be_coolant_capable = false
  ) const;

  // Get tool inventory status
  struct ToolInventory {
    int total_tools = 0;
    int tools_in_use = 0;
    int tools_available = 0;
    std::map<ToolType, int> tools_by_type;
    double total_inventory_value = 0.0;
  };

  ToolInventory getInventoryStatus() const;

  // Tool cost analysis
  double calculateToolingCost(const std::vector<int>& tool_ids) const;

  // Pre-load standard tool library
  void loadStandardLibrary();

  // Persistence
  bool saveLibrary(const std::string& filename) const;
  bool loadLibrary(const std::string& filename);

 private:
  std::map<int, ToolDefinition> tool_database_;
  int next_tool_id_ = 1;

  // Material-specific speed/feed database
  struct MaterialSpeedDatabase {
    std::map<std::string, SpeedFeedData> speeds_by_material;
  };

  std::map<std::string, MaterialSpeedDatabase> material_database_;

  // Initialize material database
  void initializeMaterialDatabase();

  // Helper methods
  double calculateTaylorFactorLife(
    double spindle_rpm,
    double cutting_speed,
    const std::string& material
  ) const;
};

}  // namespace bendcore::manufacturing::cnc
