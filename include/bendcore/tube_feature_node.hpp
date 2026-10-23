/**
 * BendCore Phase 35C: Tube Feature Node
 * Integration of TubeBody into FeatureTree design history
 */

#pragma once

#include "bendcore/tube_builder.hpp"
#include "bendcore/feature_tree.hpp"
#include <nlohmann/json.hpp>

namespace bendcore::tube {

using json = nlohmann::json;

/**
 * TubeFeatureNode: Represents a tube in the design tree
 * Extends FeatureNode with tube-specific geometry and properties
 */
class TubeFeatureNode : public FeatureNode {
 public:
  // ========== Constructor ==========

  TubeFeatureNode() {
    type = "tube";
    icon_color = "#FF6B35";  // Orange for tubes
  }

  // ========== Tube-Specific Properties ==========

  // Core geometry
  TubeBody geometry;

  // Design inputs
  std::vector<PathSegment> path;
  BendingProfile profile;
  TubeSpecification spec;

  // Calculated properties
  double length = 0.0;        // mm (computed)
  double mass = 0.0;          // grams (computed)
  double volume = 0.0;        // cm³ (computed)

  // Manufacturing designation
  std::string designation;    // e.g., "Трубка медная ∅12×1, 90°, 250мм"

  // Quality and validation
  bool is_valid = true;
  std::vector<std::string> validation_errors;
  std::vector<std::string> validation_warnings;

  // Bending information
  int num_bends = 0;
  double total_bend_angle = 0.0;  // Sum of all bend angles in degrees

  // ========== Serialization ==========

  json to_json() const override;
  static std::shared_ptr<TubeFeatureNode> from_json(const json& j);

  // ========== Utility Methods ==========

  /**
   * Get human-readable tube description
   */
  std::string get_description() const;

  /**
   * Get manufacturing specifications as JSON
   */
  json get_manufacturing_spec() const;

  /**
   * Get bending details
   */
  json get_bending_details() const;

  /**
   * Get material and weight information
   */
  json get_material_info() const;

  /**
   * Get validation status
   */
  json get_validation_status() const;

  /**
   * Update calculated properties after geometry change
   */
  void update_properties();

  /**
   * Create feature tree nodes for each bend section
   */
  std::vector<std::shared_ptr<FeatureNode>> create_bend_features();

  /**
   * Create feature tree nodes for each straight section
   */
  std::vector<std::shared_ptr<FeatureNode>> create_straight_features();
};

/**
 * Factory for creating TubeFeatureNode from build results
 */
class TubeFeatureFactory {
 public:
  /**
   * Create TubeFeatureNode from TubeBody
   */
  static std::shared_ptr<TubeFeatureNode> create_from_body(
    const TubeBody& body,
    const std::vector<PathSegment>& path,
    const BendingProfile& profile,
    const TubeSpecification& spec
  );

  /**
   * Add tube feature to design history
   */
  static bool add_to_history(
    std::shared_ptr<TubeFeatureNode> tube_node,
    const std::string& parent_id = ""
  );
};

}  // namespace bendcore::tube
