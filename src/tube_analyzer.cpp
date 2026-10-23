/**
 * BendCore Phase 35C: Tube Analyzer Implementation
 * Runtime validation and geometric checking of tube bodies
 */

#include "bendcore/tube_analyzer.hpp"
#include "bendcore/geometry_ops.hpp"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <iomanip>

namespace bendcore::tube {

// ============ MAIN VALIDATION ============

ValidationResult TubeAnalyzer::validate(
    const TubeBody& tube,
    const std::vector<PathSegment>& path,
    const BendingProfile& profile,
    const TubeSpecification& spec) {

  ValidationResult result;
  result.valid = true;

  // Run all validation checks
  auto straight_result = check_straight_segments(tube, path, profile, spec);
  if (!straight_result.valid) result.valid = false;
  result.errors.insert(result.errors.end(),
                       straight_result.errors.begin(),
                       straight_result.errors.end());

  auto angle_result = check_bend_angles(tube, path, profile);
  if (!angle_result.valid) result.valid = false;
  result.errors.insert(result.errors.end(),
                       angle_result.errors.begin(),
                       angle_result.errors.end());

  auto overlap_result = check_arc_overlap(tube, path, profile, spec);
  if (!overlap_result.valid) result.valid = false;
  result.errors.insert(result.errors.end(),
                       overlap_result.errors.begin(),
                       overlap_result.errors.end());

  auto degenerate_result = check_degenerate_bends(tube, path, profile);
  result.warnings.insert(result.warnings.end(),
                         degenerate_result.warnings.begin(),
                         degenerate_result.warnings.end());

  auto diameter_result = check_diameter_wall(tube, spec);
  if (!diameter_result.valid) result.valid = false;
  result.errors.insert(result.errors.end(),
                       diameter_result.errors.begin(),
                       diameter_result.errors.end());

  auto material_result = check_material(tube, spec);
  if (!material_result.valid) result.valid = false;
  result.errors.insert(result.errors.end(),
                       material_result.errors.begin(),
                       material_result.errors.end());

  auto collision_result = check_self_collision(tube);
  if (!collision_result.valid) result.valid = false;
  result.warnings.insert(result.warnings.end(),
                         collision_result.warnings.begin(),
                         collision_result.warnings.end());

  return result;
}

// ============ STRAIGHT SEGMENT VALIDATION ============

ValidationResult TubeAnalyzer::check_straight_segments(
    const TubeBody& tube,
    const std::vector<PathSegment>& path,
    const BendingProfile& profile,
    const TubeSpecification& spec) {

  ValidationResult result;
  result.valid = true;

  auto lengths = calculate_straight_lengths(path, profile, spec);

  for (size_t i = 0; i < lengths.size(); ++i) {
    if (lengths[i] < profile.min_straight) {
      result.valid = false;
      result.errors.push_back(
        "Straight section " + std::to_string(i) + ": length " +
        std::to_string(lengths[i]) + "mm < minimum " +
        std::to_string(profile.min_straight) + "mm"
      );
    }
  }

  result.metrics["min_straight_length"] = *std::min_element(lengths.begin(), lengths.end());
  result.metrics["avg_straight_length"] =
    std::accumulate(lengths.begin(), lengths.end(), 0.0) / lengths.size();

  return result;
}

std::vector<double> TubeAnalyzer::calculate_straight_lengths(
    const std::vector<PathSegment>& path,
    const BendingProfile& profile,
    const TubeSpecification& spec) {

  std::vector<double> lengths;

  for (const auto& segment : path) {
    if (segment.is_straight) {
      // For straight segments, length is full segment length
      double seg_length = (segment.end - segment.start).length();
      lengths.push_back(seg_length);
    } else {
      // For bends, straight length is between tangent points
      double seg_length = (segment.end - segment.start).length();
      double tangent = calculate_tangent_offset(profile.radius, segment.bend_angle);
      double straight_length = seg_length - 2 * tangent;
      lengths.push_back(straight_length);
    }
  }

  return lengths;
}

// ============ BEND ANGLE VALIDATION ============

ValidationResult TubeAnalyzer::check_bend_angles(
    const TubeBody& tube,
    const std::vector<PathSegment>& path,
    const BendingProfile& profile) {

  ValidationResult result;
  result.valid = true;

  for (const auto& segment : path) {
    if (segment.is_straight) continue;

    if (segment.bend_angle < profile.min_bend_angle ||
        segment.bend_angle > profile.max_bend_angle) {

      double angle_deg = segment.bend_angle * 180.0 / M_PI;
      double min_deg = profile.min_bend_angle * 180.0 / M_PI;
      double max_deg = profile.max_bend_angle * 180.0 / M_PI;

      result.valid = false;
      result.errors.push_back(
        "Segment " + segment.segment_id + ": angle " +
        std::to_string(angle_deg) + "° not in [" +
        std::to_string(min_deg) + "°, " +
        std::to_string(max_deg) + "°]"
      );
    }
  }

  auto stats = get_bend_statistics(path);
  result.metrics["min_angle"] = stats.min_angle;
  result.metrics["max_angle"] = stats.max_angle;
  result.metrics["avg_angle"] = stats.avg_angle;
  result.metrics["total_bends"] = stats.total_bends;

  return result;
}

TubeAnalyzer::BendStatistics TubeAnalyzer::get_bend_statistics(
    const std::vector<PathSegment>& path) {

  BendStatistics stats;
  std::vector<double> angles;

  for (const auto& segment : path) {
    if (!segment.is_straight) {
      angles.push_back(segment.bend_angle * 180.0 / M_PI);
    }
  }

  if (angles.empty()) {
    stats.min_angle = 0;
    stats.max_angle = 0;
    stats.avg_angle = 0;
    stats.total_bends = 0;
  } else {
    stats.min_angle = *std::min_element(angles.begin(), angles.end());
    stats.max_angle = *std::max_element(angles.begin(), angles.end());
    stats.avg_angle = std::accumulate(angles.begin(), angles.end(), 0.0) / angles.size();
    stats.total_bends = angles.size();
  }

  return stats;
}

// ============ ARC OVERLAP DETECTION ============

ValidationResult TubeAnalyzer::check_arc_overlap(
    const TubeBody& tube,
    const std::vector<PathSegment>& path,
    const BendingProfile& profile,
    const TubeSpecification& spec) {

  ValidationResult result;
  result.valid = true;

  // Check adjacent bends for tangent point overlaps
  for (size_t i = 0; i + 1 < path.size(); ++i) {
    if (path[i].is_straight || path[i+1].is_straight) continue;

    auto tp1 = calculate_tangent_points(path[i], profile);
    auto tp2 = calculate_tangent_points(path[i+1], profile);

    // Calculate distance between tangent points
    double dist = (tp1.right - tp2.left).length();

    if (dist < 0.01) {  // If closer than 0.01mm
      result.valid = false;
      result.errors.push_back(
        "Arc overlap detected between segments " + path[i].segment_id +
        " and " + path[i+1].segment_id + ": distance " +
        std::to_string(dist) + "mm"
      );
    }
  }

  return result;
}

double TubeAnalyzer::calculate_tangent_offset(double radius, double angle) {
  if (std::abs(angle) < 1e-10) return 0.0;
  return radius * std::tan(angle / 2.0);
}

TubeAnalyzer::TangentPoints TubeAnalyzer::calculate_tangent_points(
    const PathSegment& segment,
    const BendingProfile& profile) {

  TangentPoints tp;
  Vector3 direction = (segment.end - segment.start).normalized();

  double tangent = calculate_tangent_offset(profile.radius, segment.bend_angle);

  tp.left_offset = tangent;
  tp.right_offset = tangent;

  tp.left = segment.start + direction * tangent;
  tp.right = segment.end - direction * tangent;

  return tp;
}

// ============ DEGENERATE BEND DETECTION ============

ValidationResult TubeAnalyzer::check_degenerate_bends(
    const TubeBody& tube,
    const std::vector<PathSegment>& path,
    const BendingProfile& profile) {

  ValidationResult result;
  result.valid = true;

  for (const auto& segment : path) {
    if (segment.is_straight) continue;

    if (is_degenerate_angle(segment.bend_angle)) {
      double angle_deg = segment.bend_angle * 180.0 / M_PI;
      result.warnings.push_back(
        "Segment " + segment.segment_id + ": very small angle " +
        std::to_string(angle_deg) + "° may cause manufacturing issues"
      );
    }
  }

  return result;
}

bool TubeAnalyzer::is_degenerate_angle(double angle, double min_degenerate) {
  return std::abs(angle) < min_degenerate;
}

// ============ DIAMETER AND WALL VALIDATION ============

ValidationResult TubeAnalyzer::check_diameter_wall(
    const TubeBody& tube,
    const TubeSpecification& spec) {

  ValidationResult result;
  result.valid = true;

  if (spec.outer_diameter <= 0) {
    result.valid = false;
    result.errors.push_back("Outer diameter must be positive");
  }

  if (spec.inner_diameter <= 0) {
    result.valid = false;
    result.errors.push_back("Inner diameter must be positive");
  }

  if (spec.outer_diameter <= spec.inner_diameter) {
    result.valid = false;
    result.errors.push_back("Outer diameter must exceed inner diameter");
  }

  double wall = calculate_wall_thickness(spec);
  if (!is_valid_wall_thickness(spec.outer_diameter, spec.inner_diameter)) {
    result.valid = false;
    result.errors.push_back("Wall thickness " + std::to_string(wall) +
                           "mm is invalid");
  }

  result.metrics["outer_diameter"] = spec.outer_diameter;
  result.metrics["inner_diameter"] = spec.inner_diameter;
  result.metrics["wall_thickness"] = wall;

  return result;
}

double TubeAnalyzer::calculate_wall_thickness(const TubeSpecification& spec) {
  return (spec.outer_diameter - spec.inner_diameter) / 2.0;
}

bool TubeAnalyzer::is_valid_wall_thickness(double od, double id) {
  double wall = (od - id) / 2.0;

  // Wall thickness must be reasonable: > 0.1mm and < OD/2
  return wall > 0.1 && wall < od / 2.0;
}

// ============ COLLISION DETECTION ============

ValidationResult TubeAnalyzer::check_self_collision(const TubeBody& tube) {
  ValidationResult result;
  result.valid = true;

  // Check for cylinder-cylinder intersections
  for (size_t i = 0; i < tube.straight_sections.size(); ++i) {
    for (size_t j = i + 1; j < tube.straight_sections.size(); ++j) {
      if (GeometryOps::cylinder_cylinder_intersect(
          tube.straight_sections[i],
          tube.straight_sections[j])) {

        result.valid = false;
        result.errors.push_back(
          "Self-collision: straight sections " +
          tube.straight_sections[i].segment_id + " and " +
          tube.straight_sections[j].segment_id
        );
      }
    }
  }

  // Check for torus-torus intersections
  for (size_t i = 0; i < tube.bend_sections.size(); ++i) {
    for (size_t j = i + 1; j < tube.bend_sections.size(); ++j) {
      if (GeometryOps::torus_torus_intersect(
          tube.bend_sections[i],
          tube.bend_sections[j])) {

        result.warnings.push_back(
          "Potential collision: bend sections " +
          tube.bend_sections[i].segment_id + " and " +
          tube.bend_sections[j].segment_id
        );
      }
    }
  }

  return result;
}

ValidationResult TubeAnalyzer::check_collision_with_object(
    const TubeBody& tube,
    const TubeBody& other) {

  ValidationResult result;
  result.valid = true;

  // Check all pairs
  for (const auto& cyl : tube.straight_sections) {
    for (const auto& other_cyl : other.straight_sections) {
      if (GeometryOps::cylinder_cylinder_intersect(cyl, other_cyl)) {
        result.valid = false;
        result.errors.push_back(
          "Collision detected with other tube"
        );
        return result;
      }
    }
  }

  return result;
}

// ============ MATERIAL PROPERTIES ============

ValidationResult TubeAnalyzer::check_material(
    const TubeBody& tube,
    const TubeSpecification& spec) {

  ValidationResult result;
  result.valid = true;

  if (spec.density <= 0) {
    result.valid = false;
    result.errors.push_back("Material density must be positive");
  }

  if (spec.density > 50.0) {  // Tungsten is ~19, copper ~9, aluminum ~2.7
    result.warnings.push_back(
      "Material density " + std::to_string(spec.density) +
      "g/cm³ seems unusually high"
    );
  }

  if (!verify_mass_calculation(tube.total_mass, tube.total_volume, spec.density)) {
    result.warnings.push_back("Mass calculation may be inaccurate");
  }

  result.metrics["density"] = spec.density;
  result.metrics["calculated_mass"] = tube.total_mass;

  return result;
}

bool TubeAnalyzer::verify_mass_calculation(
    double calculated_mass,
    double volume,
    double density) {

  // Volume is in mm³, convert to cm³
  double volume_cm3 = volume / 1000.0;
  double expected_mass = volume_cm3 * density;

  // Allow 1% tolerance
  return std::abs(calculated_mass - expected_mass) < (expected_mass * 0.01);
}

// ============ COMPREHENSIVE ANALYSIS ============

TubeAnalyzer::TubeAnalysis TubeAnalyzer::analyze(
    const TubeBody& tube,
    const std::vector<PathSegment>& path,
    const BendingProfile& profile,
    const TubeSpecification& spec) {

  TubeAnalysis analysis;

  analysis.validation = validate(tube, path, profile, spec);
  analysis.bend_stats = get_bend_statistics(path);
  analysis.total_length = tube.total_length;
  analysis.total_volume = tube.total_volume;
  analysis.total_mass = tube.total_mass;

  analysis.segment_lengths = calculate_straight_lengths(path, profile, spec);

  for (const auto& seg : path) {
    if (!seg.is_straight) {
      analysis.bend_angles.push_back(seg.bend_angle * 180.0 / M_PI);
    }
  }

  return analysis;
}

// ============ REPORT GENERATION ============

json TubeAnalyzer::validation_to_json(const ValidationResult& result) {
  json j;
  j["valid"] = result.valid;
  j["errors"] = result.errors;
  j["warnings"] = result.warnings;
  j["metrics"] = result.metrics;
  return j;
}

std::string TubeAnalyzer::generate_report(const TubeAnalysis& analysis) {
  std::ostringstream oss;

  oss << "=== TUBE ANALYSIS REPORT ===\n\n";

  oss << "VALIDATION STATUS: " << (analysis.validation.valid ? "PASS" : "FAIL") << "\n";

  if (!analysis.validation.errors.empty()) {
    oss << "\nERRORS:\n";
    for (const auto& err : analysis.validation.errors) {
      oss << "  - " << err << "\n";
    }
  }

  if (!analysis.validation.warnings.empty()) {
    oss << "\nWARNINGS:\n";
    for (const auto& warn : analysis.validation.warnings) {
      oss << "  - " << warn << "\n";
    }
  }

  oss << "\nPROPERTIES:\n";
  oss << "  Total Length: " << std::fixed << std::setprecision(2)
      << analysis.total_length << " mm\n";
  oss << "  Total Volume: " << std::fixed << std::setprecision(2)
      << analysis.total_volume << " mm³\n";
  oss << "  Total Mass: " << std::fixed << std::setprecision(2)
      << analysis.total_mass << " g\n";

  oss << "\nBEND STATISTICS:\n";
  oss << "  Total Bends: " << analysis.bend_stats.total_bends << "\n";
  oss << "  Min Angle: " << std::fixed << std::setprecision(2)
      << analysis.bend_stats.min_angle << "°\n";
  oss << "  Max Angle: " << std::fixed << std::setprecision(2)
      << analysis.bend_stats.max_angle << "°\n";
  oss << "  Avg Angle: " << std::fixed << std::setprecision(2)
      << analysis.bend_stats.avg_angle << "°\n";

  return oss.str();
}

}  // namespace bendcore::tube
