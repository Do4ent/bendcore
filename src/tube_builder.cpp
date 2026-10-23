/**
 * BendCore Phase 35C: TubeBuilder Implementation
 * Core algorithm for generating 3D tube geometry from path sketches and profiles
 */

#include "bendcore/tube_builder.hpp"
#include "bendcore/geometry_ops.hpp"
#include "bendcore/tube_analyzer.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <sstream>

namespace bendcore::tube {

// ============ TUEBODY JSON SERIALIZATION ============

json TubeBody::to_json() const {
  json j;
  j["tube_id"] = tube_id;
  j["total_length"] = total_length;
  j["total_volume"] = total_volume;
  j["total_mass"] = total_mass;
  j["valid"] = valid;

  // Straight sections
  json straights = json::array();
  for (const auto& cyl : straight_sections) {
    json cj;
    cj["center"] = {cyl.center.x, cyl.center.y, cyl.center.z};
    cj["axis"] = {cyl.axis.x, cyl.axis.y, cyl.axis.z};
    cj["length"] = cyl.length;
    cj["outer_radius"] = cyl.outer_radius;
    cj["inner_radius"] = cyl.inner_radius;
    cj["segment_id"] = cyl.segment_id;
    straights.push_back(cj);
  }
  j["straight_sections"] = straights;

  // Bend sections
  json bends = json::array();
  for (const auto& torus : bend_sections) {
    json tj;
    tj["center"] = {torus.center.x, torus.center.y, torus.center.z};
    tj["axis"] = {torus.axis.x, torus.axis.y, torus.axis.z};
    tj["major_radius"] = torus.major_radius;
    tj["minor_radius"] = torus.minor_radius;
    tj["start_angle"] = torus.start_angle;
    tj["end_angle"] = torus.end_angle;
    tj["segment_id"] = torus.segment_id;
    bends.push_back(tj);
  }
  j["bend_sections"] = bends;

  // Errors and warnings
  j["errors"] = validation_errors;
  j["warnings"] = validation_warnings;

  return j;
}

// ============ MAIN BUILDER FUNCTION ============

TubeBody TubeBuilder::build(
    const std::vector<PathSegment>& path,
    const BendingProfile& profile,
    const TubeSpecification& spec) {

  TubeBody tube;
  tube.tube_id = spec.tube_id;
  tube.valid = false;

  // Validate inputs
  if (path.empty()) {
    tube.validation_errors.push_back("Path segments cannot be empty");
    return tube;
  }

  if (profile.radius <= 0) {
    tube.validation_errors.push_back("Profile radius must be positive");
    return tube;
  }

  if (spec.outer_diameter <= spec.inner_diameter) {
    tube.validation_errors.push_back("Outer diameter must be greater than inner diameter");
    return tube;
  }

  if (spec.outer_diameter <= 0 || spec.density <= 0) {
    tube.validation_errors.push_back("Tube specification invalid: diameter and density must be positive");
    return tube;
  }

  // Convert diameters to radii in mm
  double outer_radius = spec.outer_diameter / 2.0;
  double inner_radius = spec.inner_diameter / 2.0;

  // Process each segment
  for (size_t i = 0; i < path.size(); ++i) {
    const PathSegment& segment = path[i];

    // Calculate segment direction and length
    Vector3 direction = get_segment_direction(segment);
    double segment_length = (segment.end - segment.start).length();

    if (segment.is_straight) {
      // Create straight cylinder
      Vector3 center = segment.start + direction * (segment_length / 2.0);
      Cylinder cyl = create_hollow_cylinder(
        center,
        direction,
        segment_length,
        spec.outer_diameter,
        spec.inner_diameter,
        segment.segment_id
      );
      tube.straight_sections.push_back(cyl);
    } else {
      // This is a bend
      if (segment.bend_angle < 1e-6) {
        tube.validation_warnings.push_back(
          "Segment " + segment.segment_id + " has near-zero bend angle"
        );
        continue;
      }

      // Calculate tangent offset: T = R × tan(B/2)
      double tangent_offset = calculate_tangent(profile.radius, segment.bend_angle);

      // Calculate actual straight length between tangent points
      // S = L - T_left - T_right (but we need proper projection)
      double straight_length = segment_length - 2 * tangent_offset;

      // Validate straight segment
      if (straight_length < profile.min_straight - 1e-6) {
        tube.validation_errors.push_back(
          "Segment " + segment.segment_id + ": straight length " +
          std::to_string(straight_length) + "mm < minimum " +
          std::to_string(profile.min_straight) + "mm"
        );
        continue;
      }

      // Create tangent cylinder before bend
      Vector3 tangent_start = segment.start + direction * tangent_offset;
      Vector3 center_before = tangent_start + direction * (straight_length / 2.0);
      Cylinder cyl_before = create_hollow_cylinder(
        center_before,
        direction,
        straight_length,
        spec.outer_diameter,
        spec.inner_diameter,
        segment.segment_id + "_straight"
      );
      tube.straight_sections.push_back(cyl_before);

      // Create bend torus
      Vector3 bend_center = tangent_start + get_perpendicular(direction) * profile.radius;

      // Calculate angles for torus sweep
      double start_angle = -segment.bend_angle / 2.0;
      double end_angle = segment.bend_angle / 2.0;

      Torus bend = create_hollow_torus(
        bend_center,
        direction,
        profile.radius,
        (outer_radius - inner_radius),
        start_angle,
        end_angle,
        segment.segment_id + "_bend"
      );
      tube.bend_sections.push_back(bend);

      tube.num_bends++;
      tube.total_bend_angle += segment.bend_angle * 180.0 / M_PI;  // Convert to degrees
    }
  }

  // Calculate total volume and properties
  tube.total_volume = calculate_total_volume(tube.straight_sections, tube.bend_sections);
  tube.total_length = calculate_total_length(tube.straight_sections, tube.bend_sections, profile);

  // Convert volume from mm³ to cm³: divide by 1000
  double volume_cm3 = tube.total_volume / 1000.0;
  tube.total_mass = calculate_mass(volume_cm3, spec.density);

  // Validate complete tube
  validate_tube(tube, path, profile, spec);

  return tube;
}

// ============ CORE BUILDING BLOCKS ============

Cylinder TubeBuilder::create_hollow_cylinder(
    Vector3 center,
    Vector3 axis,
    double length,
    double outer_diameter,
    double inner_diameter,
    const std::string& segment_id) {

  Cylinder cyl;
  cyl.center = center;
  cyl.axis = axis.normalized();
  cyl.length = length;
  cyl.outer_radius = outer_diameter / 2.0;
  cyl.inner_radius = inner_diameter / 2.0;
  cyl.segment_id = segment_id;

  return cyl;
}

Torus TubeBuilder::create_hollow_torus(
    Vector3 center,
    Vector3 axis,
    double major_radius,
    double minor_radius,
    double start_angle,
    double end_angle,
    const std::string& segment_id) {

  Torus torus;
  torus.center = center;
  torus.axis = axis.normalized();
  torus.major_radius = major_radius;
  torus.minor_radius = minor_radius;
  torus.start_angle = start_angle;
  torus.end_angle = end_angle;
  torus.segment_id = segment_id;

  return torus;
}

// ============ CALCULATIONS ============

double TubeBuilder::calculate_tangent(double radius, double angle) {
  if (std::abs(angle) < 1e-10) return 0.0;
  return radius * std::tan(angle / 2.0);
}

double TubeBuilder::calculate_straight_segment(
    double total_length,
    double tangent_left,
    double tangent_right) {
  return total_length - tangent_left - tangent_right;
}

double TubeBuilder::calculate_arc_length(double radius, double angle) {
  return radius * std::abs(angle);  // angle in radians
}

Vector3 TubeBuilder::get_segment_direction(const PathSegment& seg) {
  return (seg.end - seg.start).normalized();
}

Vector3 TubeBuilder::get_perpendicular(const Vector3& direction) {
  // Find a vector perpendicular to direction
  Vector3 perp;

  if (std::abs(direction.x) < 0.9) {
    perp = Vector3(1, 0, 0).cross(direction);
  } else {
    perp = Vector3(0, 1, 0).cross(direction);
  }

  return perp.normalized();
}

// ============ VALIDATION ============

void TubeBuilder::validate_tube(
    TubeBody& tube,
    const std::vector<PathSegment>& path,
    const BendingProfile& profile,
    const TubeSpecification& spec) {

  tube.valid = true;

  // Check straight segments
  if (!check_straight_segments(path, profile, spec, tube.validation_errors)) {
    tube.valid = false;
  }

  // Check bend angles
  if (!check_bend_angles(path, profile, tube.validation_errors)) {
    tube.valid = false;
  }

  // Check arc overlap
  if (!check_arc_overlap(path, spec, tube.validation_errors)) {
    tube.valid = false;
  }

  // Check for degenerate bends (warnings only)
  check_degenerate_bends(path, profile, tube.validation_warnings);

  if (tube.validation_errors.empty()) {
    tube.valid = true;
  }
}

bool TubeBuilder::check_straight_segments(
    const std::vector<PathSegment>& path,
    const BendingProfile& profile,
    const TubeSpecification& spec,
    std::vector<std::string>& errors) {

  bool all_valid = true;

  for (const auto& segment : path) {
    if (segment.is_straight) continue;

    // Calculate expected straight segment length
    Vector3 direction = (segment.end - segment.start).normalized();
    double segment_length = (segment.end - segment.start).length();
    double tangent = calculate_tangent(profile.radius, segment.bend_angle);
    double straight_length = segment_length - 2 * tangent;

    if (straight_length < profile.min_straight) {
      errors.push_back(
        "Segment " + segment.segment_id + ": insufficient straight section " +
        std::to_string(straight_length) + "mm < " +
        std::to_string(profile.min_straight) + "mm"
      );
      all_valid = false;
    }
  }

  return all_valid;
}

bool TubeBuilder::check_bend_angles(
    const std::vector<PathSegment>& path,
    const BendingProfile& profile,
    std::vector<std::string>& errors) {

  bool all_valid = true;

  for (const auto& segment : path) {
    if (segment.is_straight) continue;

    double angle_deg = segment.bend_angle * 180.0 / M_PI;
    double min_deg = profile.min_bend_angle * 180.0 / M_PI;
    double max_deg = profile.max_bend_angle * 180.0 / M_PI;

    if (segment.bend_angle < profile.min_bend_angle ||
        segment.bend_angle > profile.max_bend_angle) {

      errors.push_back(
        "Segment " + segment.segment_id + ": bend angle " +
        std::to_string(angle_deg) + "° outside range [" +
        std::to_string(min_deg) + "°, " +
        std::to_string(max_deg) + "°]"
      );
      all_valid = false;
    }
  }

  return all_valid;
}

bool TubeBuilder::check_arc_overlap(
    const std::vector<PathSegment>& path,
    const TubeSpecification& spec,
    std::vector<std::string>& errors) {

  bool all_valid = true;

  // For now, implement basic validation
  // Full 3D arc overlap check would require geometric operations

  return all_valid;
}

bool TubeBuilder::check_degenerate_bends(
    const std::vector<PathSegment>& path,
    const BendingProfile& profile,
    std::vector<std::string>& warnings) {

  bool any_degenerate = false;

  for (const auto& segment : path) {
    if (segment.is_straight) continue;

    double angle_deg = segment.bend_angle * 180.0 / M_PI;
    if (angle_deg < 0.5) {
      warnings.push_back(
        "Segment " + segment.segment_id + ": very small bend angle " +
        std::to_string(angle_deg) + "° - may cause manufacturing issues"
      );
      any_degenerate = true;
    }
  }

  return any_degenerate;
}

// ============ PROPERTIES CALCULATION ============

double TubeBuilder::calculate_total_length(
    const std::vector<Cylinder>& straights,
    const std::vector<Torus>& bends,
    const BendingProfile& profile) {

  double total = 0.0;

  // Sum straight segment lengths
  for (const auto& cyl : straights) {
    total += cyl.length;
  }

  // Sum arc lengths for bends
  for (const auto& torus : bends) {
    total += calculate_arc_length(torus.major_radius, torus.sweep_angle());
  }

  return total;
}

double TubeBuilder::calculate_total_volume(
    const std::vector<Cylinder>& straights,
    const std::vector<Torus>& bends) {

  double total = 0.0;

  // Sum cylinder volumes (in mm³)
  for (const auto& cyl : straights) {
    total += cyl.volume();
  }

  // Sum torus volumes (in mm³)
  for (const auto& torus : bends) {
    total += torus.volume();
  }

  return total;
}

double TubeBuilder::calculate_mass(double volume_cm3, double density) {
  // mass = density × volume
  // density in g/cm³, volume in cm³ → mass in grams
  return volume_cm3 * density;
}

}  // namespace bendcore::tube
