/**
 * BendCore Import Filter
 * STEP/IGES import with grouping rules and filtering
 *
 * 5 Grouping Rules:
 * 1. Tubes by diameter
 * 2. Tubes by material
 * 3. Tubes by bending profile
 * 4. Contact bodies (touching tubes)
 * 5. Isolated tubes (no contact)
 */

#pragma once

#include <vector>
#include <string>
#include <map>
#include <memory>
#include <nlohmann/json.hpp>

namespace bendcore {

using json = nlohmann::json;

/**
 * Grouping rules for import filtering
 */
enum class GroupingRule {
  RULE_1,  // Tubes by diameter
  RULE_2,  // Tubes by material
  RULE_3,  // Tubes by bending profile
  RULE_4,  // Contact bodies (touching tubes)
  RULE_5,  // Isolated tubes (no contact)
};

/**
 * Contact relationship between two bodies
 */
struct Contact {
  int body1_id;
  int body2_id;
  double contact_area;
  std::vector<double> contact_points;
};

/**
 * Geometric properties of a solid
 */
struct SolidGeometry {
  int id;
  std::string name;
  double outer_diameter = 0.0;
  double inner_diameter = 0.0;
  double wall_thickness = 0.0;
  std::vector<double> bounding_box;  // [xmin, ymin, zmin, xmax, ymax, zmax]
  double length = 0.0;
  std::string material;
  std::vector<double> density;
  bool is_cylindrical = false;
  bool is_hollow = false;
};

/**
 * Grouping result
 */
struct Group {
  GroupingRule rule;
  std::string group_name;
  std::vector<SolidGeometry> solids;
  std::string description;
};

/**
 * Import filter for STEP/IGES files
 */
class ImportFilter {
 public:
  ImportFilter() = default;
  ~ImportFilter() = default;

  /**
   * Apply grouping rules to imported solids
   * @param solids Vector of geometries from STEP/IGES file
   * @param rules Rules to apply (in priority order)
   * @return Grouped solids
   */
  std::vector<Group> apply_rules(
      const std::vector<SolidGeometry>& solids,
      const std::vector<GroupingRule>& rules);

  /**
   * Detect round tubes in assembly
   * @param solids Vector of geometries
   * @return Tubes that match standard profiles
   */
  std::vector<SolidGeometry> detect_tubes(
      const std::vector<SolidGeometry>& solids);

  /**
   * Filter solids by outer diameter range
   * @param solids Input solids
   * @param min_od Minimum outer diameter (mm)
   * @param max_od Maximum outer diameter (mm)
   * @return Filtered solids
   */
  std::vector<SolidGeometry> filter_by_diameter(
      const std::vector<SolidGeometry>& solids,
      double min_od,
      double max_od);

  /**
   * Filter solids by material
   * @param solids Input solids
   * @param material Material name to match
   * @return Filtered solids
   */
  std::vector<SolidGeometry> filter_by_material(
      const std::vector<SolidGeometry>& solids,
      const std::string& material);

  /**
   * Find touching/contacting bodies
   * @param solids Input solids
   * @param tolerance Contact tolerance (mm)
   * @return List of contacts
   */
  std::vector<Contact> detect_contacts(
      const std::vector<SolidGeometry>& solids,
      double tolerance = 0.01);

  /**
   * Check if two solids are in contact
   * @param solid1 First solid
   * @param solid2 Second solid
   * @param tolerance Contact tolerance (mm)
   * @return Contact information if in contact
   */
  bool are_in_contact(
      const SolidGeometry& solid1,
      const SolidGeometry& solid2,
      double tolerance = 0.01,
      Contact* out_contact = nullptr);

  /**
   * Get analysis summary
   */
  json get_analysis_summary(const std::vector<SolidGeometry>& solids);

 private:
  /**
   * Check if geometry is cylindrical
   */
  bool is_cylindrical_geometry(const SolidGeometry& solid);

  /**
   * Check if geometry is hollow (tube)
   */
  bool is_hollow_geometry(const SolidGeometry& solid);

  /**
   * Calculate axis-aligned bounding box
   */
  std::vector<double> compute_bounding_box(const SolidGeometry& solid);

  /**
   * Extract material from solid properties
   */
  std::string extract_material(const SolidGeometry& solid);

  /**
   * Group by diameter
   */
  Group group_by_diameter(const std::vector<SolidGeometry>& solids);

  /**
   * Group by material
   */
  std::vector<Group> group_by_material(const std::vector<SolidGeometry>& solids);

  /**
   * Group by bending profile
   */
  std::vector<Group> group_by_bending_profile(
      const std::vector<SolidGeometry>& solids);

  /**
   * Group contact bodies
   */
  std::vector<Group> group_contact_bodies(
      const std::vector<SolidGeometry>& solids);

  /**
   * Find isolated tubes
   */
  Group find_isolated_tubes(const std::vector<SolidGeometry>& solids);
};

}  // namespace bendcore
