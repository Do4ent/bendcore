/**
 * BendCore Phase 35C: Tube Analyzer
 * Runtime validation and geometric checking of tube bodies
 */

#pragma once

#include "bendcore/tube_builder.hpp"
#include <vector>
#include <string>
#include <map>

namespace bendcore::tube {

// ============ VALIDATION RESULT ==========

struct ValidationResult {
  bool valid;
  std::vector<std::string> errors;     // Critical issues
  std::vector<std::string> warnings;   // Non-critical issues

  std::map<std::string, double> metrics;  // Calculated metrics
};

// ============ TUBE ANALYZER CLASS ==========

class TubeAnalyzer {
 public:
  /**
   * Full validation of tube geometry
   */
  static ValidationResult validate(
    const TubeBody& tube,
    const std::vector<PathSegment>& path,
    const BendingProfile& profile,
    const TubeSpecification& spec
  );

  // ========== Straight Segment Validation ==========

  /**
   * Check all straight segments meet minimum requirement S >= S_min
   */
  static ValidationResult check_straight_segments(
    const TubeBody& tube,
    const std::vector<PathSegment>& path,
    const BendingProfile& profile,
    const TubeSpecification& spec
  );

  /**
   * Calculate actual straight segment length for each bend
   */
  static std::vector<double> calculate_straight_lengths(
    const std::vector<PathSegment>& path,
    const BendingProfile& profile,
    const TubeSpecification& spec
  );

  // ========== Bend Angle Validation ==========

  /**
   * Check all bend angles within B_min and B_max
   */
  static ValidationResult check_bend_angles(
    const TubeBody& tube,
    const std::vector<PathSegment>& path,
    const BendingProfile& profile
  );

  /**
   * Calculate statistics for bend angles
   */
  struct BendStatistics {
    double min_angle;
    double max_angle;
    double avg_angle;
    int total_bends;
  };

  static BendStatistics get_bend_statistics(const std::vector<PathSegment>& path);

  // ========== Arc Overlap Detection ==========

  /**
   * Check for tangent point overlap between adjacent bends
   */
  static ValidationResult check_arc_overlap(
    const TubeBody& tube,
    const std::vector<PathSegment>& path,
    const BendingProfile& profile,
    const TubeSpecification& spec
  );

  /**
   * Calculate tangent offset for a bend
   */
  static double calculate_tangent_offset(double radius, double angle);

  /**
   * Calculate tangent points for a segment
   */
  struct TangentPoints {
    Vector3 left;
    Vector3 right;
    double left_offset;
    double right_offset;
  };

  static TangentPoints calculate_tangent_points(
    const PathSegment& segment,
    const BendingProfile& profile
  );

  // ========== Degenerate Bend Detection ==========

  /**
   * Check for degenerate bends (too small angles)
   */
  static ValidationResult check_degenerate_bends(
    const TubeBody& tube,
    const std::vector<PathSegment>& path,
    const BendingProfile& profile
  );

  /**
   * Is angle too small to be a valid bend?
   */
  static bool is_degenerate_angle(double angle, double min_degenerate = 0.5 * M_PI / 180.0);

  // ========== Diameter and Wall Validation ==========

  /**
   * Check diameter and wall thickness specifications
   */
  static ValidationResult check_diameter_wall(
    const TubeBody& tube,
    const TubeSpecification& spec
  );

  /**
   * Calculate wall thickness in mm
   */
  static double calculate_wall_thickness(const TubeSpecification& spec);

  /**
   * Check wall thickness is reasonable
   */
  static bool is_valid_wall_thickness(double od, double id);

  // ========== Collision Detection ==========

  /**
   * Check for self-intersections in the tube
   */
  static ValidationResult check_self_collision(const TubeBody& tube);

  /**
   * Check for collisions with other tubes/assembly
   * (Requires geometry comparison with other objects)
   */
  static ValidationResult check_collision_with_object(
    const TubeBody& tube,
    const TubeBody& other
  );

  // ========== Material Properties ==========

  /**
   * Verify material density is reasonable
   */
  static ValidationResult check_material(
    const TubeBody& tube,
    const TubeSpecification& spec
  );

  /**
   * Check mass calculation is consistent
   */
  static bool verify_mass_calculation(
    double calculated_mass,
    double volume,
    double density
  );

  // ========== Comprehensive Analysis ==========

  struct TubeAnalysis {
    ValidationResult validation;
    BendStatistics bend_stats;
    double total_length;
    double total_volume;
    double total_mass;
    std::vector<double> segment_lengths;
    std::vector<double> bend_angles;
  };

  /**
   * Perform complete analysis of tube
   */
  static TubeAnalysis analyze(
    const TubeBody& tube,
    const std::vector<PathSegment>& path,
    const BendingProfile& profile,
    const TubeSpecification& spec
  );

  // ========== Report Generation ==========

  /**
   * Generate JSON report of validation results
   */
  static json validation_to_json(const ValidationResult& result);

  /**
   * Generate human-readable validation report
   */
  static std::string generate_report(const TubeAnalysis& analysis);
};

}  // namespace bendcore::tube
