/**
 * BendCore Geometry Rounding Implementation
 */

#include "geometry_rounding.hpp"
#include <cmath>
#include <algorithm>

namespace bendcore {

/**
 * Round diameter to nearest standard
 */
double GeometryRounder::round_diameter(double od, double tolerance) {
  return find_nearest_standard(od, standard_diameters_);
}

/**
 * Round radius to nearest standard
 */
double GeometryRounder::round_radius(double radius, double tolerance) {
  return find_nearest_standard(radius, standard_radii_);
}

/**
 * Round length to precision
 */
double GeometryRounder::round_length(double length, double precision) {
  if (precision <= 0.0) return length;
  return std::round(length / precision) * precision;
}

/**
 * Round angle to increments
 */
double GeometryRounder::round_angle(double angle_deg, double step) {
  if (step <= 0.0) return angle_deg;

  // Normalize angle to 0-360
  double normalized = std::fmod(angle_deg, 360.0);
  if (normalized < 0.0) normalized += 360.0;

  // Round to nearest step
  return std::round(normalized / step) * step;
}

/**
 * Round a tube
 */
RoundingResult GeometryRounder::round_tube(
    const TubeBody& tube,
    const RoundingPreferences& preferences) {
  RoundingResult result;
  result.success = true;
  result.rounded_tube = tube;

  json changes = json::object();

  // Round diameter
  if (preferences.round_diameters) {
    double original_od = tube.outer_diameter;
    double rounded_od = round_diameter(original_od);

    if (std::abs(rounded_od - original_od) > 0.01) {
      result.rounded_tube.outer_diameter = rounded_od;
      changes["outer_diameter"] = {
          {"original", original_od},
          {"rounded", rounded_od},
          {"difference", rounded_od - original_od}
      };
    }
  }

  // Round radius
  if (preferences.round_radii && result.rounded_tube.bend_radius > 0.0) {
    double original_r = tube.bend_radius;
    double rounded_r = round_radius(original_r);

    if (std::abs(rounded_r - original_r) > 0.01) {
      result.rounded_tube.bend_radius = rounded_r;
      changes["bend_radius"] = {
          {"original", original_r},
          {"rounded", rounded_r},
          {"difference", rounded_r - original_r}
      };
    }
  }

  // Round segment lengths
  if (preferences.round_lengths) {
    json length_changes = json::array();

    for (size_t i = 0; i < result.rounded_tube.segment_lengths.size(); ++i) {
      double original_len = tube.segment_lengths[i];
      double rounded_len = round_length(original_len, preferences.length_precision);

      if (std::abs(rounded_len - original_len) > 0.01) {
        result.rounded_tube.segment_lengths[i] = rounded_len;
        length_changes.push_back({
            {"index", static_cast<int>(i)},
            {"original", original_len},
            {"rounded", rounded_len}
        });
      }
    }

    if (!length_changes.empty()) {
      changes["segment_lengths"] = length_changes;
    }
  }

  // Round angles
  if (preferences.round_angles) {
    json angle_changes = json::array();

    for (size_t i = 0; i < result.rounded_tube.bend_angles.size(); ++i) {
      double original_angle = tube.bend_angles[i];
      double rounded_angle = round_angle(original_angle, preferences.angle_step);

      if (std::abs(rounded_angle - original_angle) > 0.01) {
        result.rounded_tube.bend_angles[i] = rounded_angle;
        angle_changes.push_back({
            {"index", static_cast<int>(i)},
            {"original", original_angle},
            {"rounded", rounded_angle}
        });
      }
    }

    if (!angle_changes.empty()) {
      changes["bend_angles"] = angle_changes;
    }
  }

  result.changes = changes;
  result.message = "Geometry rounding complete";

  return result;
}

/**
 * Round multiple tubes
 */
std::vector<RoundingResult> GeometryRounder::round_tubes(
    const std::vector<TubeBody>& tubes,
    const RoundingPreferences& preferences) {
  std::vector<RoundingResult> results;

  for (const auto& tube : tubes) {
    auto result = round_tube(tube, preferences);
    results.push_back(result);
  }

  return results;
}

/**
 * Get standard diameters
 */
std::vector<double> GeometryRounder::get_standard_diameters() const {
  return standard_diameters_;
}

/**
 * Get standard radii
 */
std::vector<double> GeometryRounder::get_standard_radii() const {
  return standard_radii_;
}

/**
 * Check if value matches standard
 */
bool GeometryRounder::matches_standard(
    double value,
    const std::vector<double>& standards,
    double tolerance) {
  for (double std_value : standards) {
    if (std::abs(value - std_value) <= tolerance) {
      return true;
    }
  }
  return false;
}

/**
 * Find nearest standard value
 */
double GeometryRounder::find_nearest_standard(
    double value,
    const std::vector<double>& standards) {
  if (standards.empty()) {
    return value;
  }

  double nearest = standards[0];
  double min_distance = std::abs(value - nearest);

  for (double std_value : standards) {
    double distance = std::abs(value - std_value);
    if (distance < min_distance) {
      min_distance = distance;
      nearest = std_value;
    }
  }

  return nearest;
}

/**
 * Get rounding report
 */
json GeometryRounder::get_rounding_report(
    const std::vector<RoundingResult>& results) {
  json report;

  int tubes_modified = 0;
  int total_changes = 0;
  json tubes_json = json::array();

  for (const auto& result : results) {
    if (!result.changes.empty()) {
      tubes_modified++;
      total_changes += result.changes.size();
    }

    tubes_json.push_back({
        {"id", result.rounded_tube.id},
        {"name", result.rounded_tube.name},
        {"od", result.rounded_tube.outer_diameter},
        {"changes", result.changes.size()},
        {"success", result.success}
    });
  }

  report["total_tubes"] = results.size();
  report["tubes_modified"] = tubes_modified;
  report["total_changes"] = total_changes;
  report["tubes"] = tubes_json;

  return report;
}

}  // namespace bendcore
