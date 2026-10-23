/**
 * BendCore Profile Manager
 * Manages bending profiles, diameters, and tube specifications
 * Phase 35A: Profile Database and New Tube Dialog
 */

#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace bendcore {

using json = nlohmann::json;

/**
 * ProfileDiameter: Bending constraints for a specific diameter
 */
struct ProfileDiameter {
  std::string id;
  double OD;                           // Outer diameter (inches)
  double R;                            // Bend radius (inches)
  double S_min;                        // Minimum straight length (inches)
  int N;                               // Number of dies/stations
  double B_min;                        // Minimum bend angle (degrees)
  double B_max;                        // Maximum bend angle (degrees)
  std::string discrete_type;           // "continuous" or "step"
  std::vector<double> discrete_values; // For step type: allowed bend angles
};

/**
 * BendingProfile: Complete bending tool profile
 */
struct BendingProfile {
  std::string id;
  std::string name;
  std::string category;           // Manual, Hydraulic, CNC, Orbital, Pipe
  std::string manufacturer;
  std::string type;               // manual, hydraulic, cnc, orbital
  std::string description;
  double capacity_max_OD = 0.0;   // Optional: max OD capacity
  double capacity_max_wall = 0.0; // Optional: max wall thickness
  std::vector<ProfileDiameter> diameters;
};

/**
 * TubeSpecification: User-entered tube parameters
 */
struct TubeSpecification {
  std::string standard;            // e.g., "ANSI/ASME", "DIN", "ISO"
  std::string material;            // Material ID (e.g., "steel-1020")
  std::string type;                // Tube, Pipe, Structural
  std::string description;         // Optional user description
  std::string profile_id;          // Selected bending profile
  double diameter;                 // Selected diameter (OD)
  double wall_thickness;           // Wall thickness
  double first_segment_allowance;  // First segment allowance
  double last_segment_allowance;   // Last segment allowance
  bool calculate_allowance;        // Auto-calculate trimming
};

/**
 * ProfileManager: Singleton manager for bending profiles
 * Loads profiles from data/bending_profiles.json
 * Validates tube specifications against profile constraints
 */
class ProfileManager {
 public:
  static ProfileManager& instance();

  /**
   * Load profiles from data/bending_profiles.json
   * Returns true on success, false on file not found or parse error
   */
  bool load_profiles(const std::string& data_dir = "data");

  /**
   * Get profile by ID
   * Returns std::nullopt if profile not found
   */
  std::optional<BendingProfile> get_profile(const std::string& id);

  /**
   * List all profile IDs
   */
  std::vector<std::string> list_profile_ids();

  /**
   * Get available diameters for a profile
   * Returns empty vector if profile not found
   */
  std::vector<ProfileDiameter> get_diameters(const std::string& profile_id);

  /**
   * Get constraints for a specific diameter in a profile
   * Returns std::nullopt if diameter not found in profile
   */
  std::optional<ProfileDiameter> get_diameter_constraints(
      const std::string& profile_id, double diameter);

  /**
   * Validate a tube specification against profile constraints
   * Returns vector of validation error messages (empty = valid)
   */
  std::vector<std::string> validate_tube_spec(
      const TubeSpecification& spec);

  /**
   * Check if a profile exists
   */
  bool profile_exists(const std::string& id);

  /**
   * Get profile metadata (name, category, description)
   */
  json get_profile_metadata(const std::string& id);

 private:
  ProfileManager() = default;
  ProfileManager(const ProfileManager&) = delete;
  ProfileManager& operator=(const ProfileManager&) = delete;

  std::vector<BendingProfile> profiles_;
  bool profiles_loaded_ = false;

  // Parse profile from JSON
  BendingProfile parse_profile(const json& j);
  ProfileDiameter parse_diameter(const json& j);
};

} // namespace bendcore
