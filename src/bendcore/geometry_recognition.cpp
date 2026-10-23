/**
 * BendCore Geometry Recognition Implementation
 */

#include "geometry_recognition.hpp"
#include "import_filter.hpp"
#include <cmath>
#include <algorithm>
#include <numeric>
#include <stdexcept>

namespace bendcore {

/**
 * Recognize tubes from imported solids
 */
std::vector<TubeCandidate> GeometryRecognition::recognize_tubes(
    const std::vector<SolidGeometry>& solids) {
  std::vector<TubeCandidate> candidates;

  for (const auto& solid : solids) {
    // Skip if not cylindrical or hollow
    if (!solid.is_cylindrical || !solid.is_hollow) {
      continue;
    }

    try {
      // Fit path through solid
      auto fitting_result = fit_path_through_solid(solid);
      if (!fitting_result.success) {
        continue;
      }

      // Create candidate
      TubeCandidate candidate;
      candidate.id = "tube-" + std::to_string(solid.id);
      candidate.name = solid.name;
      candidate.outer_diameter = solid.outer_diameter;
      candidate.inner_diameter = solid.inner_diameter;
      candidate.wall_thickness = solid.wall_thickness;
      candidate.material = solid.material;
      candidate.vertices = fitting_result.fitted_path;
      candidate.bend_angles = fitting_result.detected_angles;

      // Match profile
      candidate.bending_profile = match_profile_by_diameter(
          solid.outer_diameter, 0.5);

      // Calculate confidence
      candidate.confidence = calculate_confidence(candidate);

      // Validate
      if (validate_tube_candidate(candidate)) {
        candidates.push_back(candidate);
      }

    } catch (const std::exception& e) {
      // Skip on error
      continue;
    }
  }

  return candidates;
}

/**
 * Fit path through solid
 */
TubeFittingResult GeometryRecognition::fit_path_through_solid(
    const SolidGeometry& solid) {
  TubeFittingResult result;
  result.success = false;

  // Extract centerline
  auto centerline = extract_centerline(solid);
  if (centerline.size() < 2) {
    result.error_message = "Failed to extract centerline";
    return result;
  }

  // Simplify path
  auto simplified = simplify_path(centerline, 0.5);

  // Detect bend angles
  auto angles = detect_bend_angles(simplified, 5.0);

  // Check if path is valid
  if (simplified.size() < 2) {
    result.error_message = "Simplified path too short";
    return result;
  }

  result.fitted_path = simplified;
  result.detected_angles = angles;
  result.fit_error = 0.1;  // Simplified
  result.confidence = 85.0;  // Simplified
  result.success = true;

  return result;
}

/**
 * Detect bend angles
 */
std::vector<double> GeometryRecognition::detect_bend_angles(
    const std::vector<std::vector<double>>& vertices,
    double angle_threshold) {
  std::vector<double> angles;

  if (vertices.size() < 3) {
    return angles;
  }

  for (size_t i = 1; i < vertices.size() - 1; ++i) {
    double angle = compute_angle(vertices[i - 1], vertices[i], vertices[i + 1]);

    // 180° means straight, so deviation from 180° is the bend
    double bend_angle = std::abs(180.0 - angle);

    if (bend_angle > angle_threshold) {
      angles.push_back(bend_angle);
    }
  }

  return angles;
}

/**
 * Smooth path using Catmull-Rom splines
 */
std::vector<std::vector<double>> GeometryRecognition::smooth_path(
    const std::vector<std::vector<double>>& vertices,
    double smoothing_factor) {
  if (vertices.size() < 4) {
    return vertices;  // Can't smooth with fewer than 4 points
  }

  std::vector<std::vector<double>> smoothed;

  // Add boundary points
  auto p_minus1 = vertices[0];
  p_minus1[0] -= (vertices[1][0] - vertices[0][0]);
  p_minus1[1] -= (vertices[1][1] - vertices[0][1]);
  p_minus1[2] -= (vertices[1][2] - vertices[0][2]);

  auto p_n = vertices[vertices.size() - 1];
  const auto& p_n_minus1 = vertices[vertices.size() - 2];
  p_n[0] += (p_n[0] - p_n_minus1[0]);
  p_n[1] += (p_n[1] - p_n_minus1[1]);
  p_n[2] += (p_n[2] - p_n_minus1[2]);

  // Interpolate between points
  for (size_t i = 0; i < vertices.size() - 1; ++i) {
    auto p0 = (i == 0) ? p_minus1 : vertices[i - 1];
    auto p1 = vertices[i];
    auto p2 = vertices[i + 1];
    auto p3 = (i == vertices.size() - 2) ? p_n : vertices[i + 2];

    // Generate intermediate points
    int steps = static_cast<int>(1.0 / smoothing_factor);
    for (int step = 0; step < steps; ++step) {
      double t = step * smoothing_factor;
      auto point = catmull_rom_point(p0, p1, p2, p3, t);
      smoothed.push_back(point);
    }
  }

  // Add last point
  smoothed.push_back(vertices.back());

  return smoothed;
}

/**
 * Match diameter to standard profile
 */
std::string GeometryRecognition::match_profile_by_diameter(
    double diameter,
    double tolerance) {
  double best_match = diameter;
  double min_diff = tolerance + 1.0;

  for (double std_dia : standard_diameters_) {
    double diff = std::abs(std_dia - diameter);
    if (diff < min_diff) {
      min_diff = diff;
      best_match = std_dia;
    }
  }

  if (min_diff <= tolerance) {
    return "Profile_" + std::to_string(static_cast<int>(best_match * 100));
  }

  return "Custom_" + std::to_string(static_cast<int>(diameter * 100));
}

/**
 * Calculate confidence score
 */
double GeometryRecognition::calculate_confidence(
    const TubeCandidate& candidate) {
  double confidence = 70.0;  // Base confidence

  // Diameter match bonus
  if (!candidate.bending_profile.find("Profile_") == std::string::npos) {
    confidence += 15.0;
  }

  // Path quality bonus
  if (candidate.vertices.size() > 2) {
    confidence += 10.0;
  }

  // Material bonus
  if (!candidate.material.empty() && candidate.material != "Unknown") {
    confidence += 5.0;
  }

  return std::min(confidence, 100.0);
}

/**
 * Simplify path
 */
std::vector<std::vector<double>> GeometryRecognition::simplify_path(
    const std::vector<std::vector<double>>& vertices,
    double tolerance) {
  if (vertices.size() < 3) {
    return vertices;
  }

  std::vector<std::vector<double>> simplified;
  simplified.push_back(vertices[0]);

  for (size_t i = 1; i < vertices.size() - 1; ++i) {
    // Check distance from current point to line between prev and next
    const auto& p_prev = simplified.back();
    const auto& p_curr = vertices[i];
    const auto& p_next = vertices[i + 1];

    // Line segment vector
    double dx = p_next[0] - p_prev[0];
    double dy = p_next[1] - p_prev[1];
    double dz = p_next[2] - p_prev[2];
    double line_len_sq = dx * dx + dy * dy + dz * dz;

    if (line_len_sq > 0.0001) {
      // Point to line distance
      double t = ((p_curr[0] - p_prev[0]) * dx +
                  (p_curr[1] - p_prev[1]) * dy +
                  (p_curr[2] - p_prev[2]) * dz) /
                 line_len_sq;

      t = std::max(0.0, std::min(1.0, t));

      double closest_x = p_prev[0] + t * dx;
      double closest_y = p_prev[1] + t * dy;
      double closest_z = p_prev[2] + t * dz;

      double dist_x = p_curr[0] - closest_x;
      double dist_y = p_curr[1] - closest_y;
      double dist_z = p_curr[2] - closest_z;
      double distance = std::sqrt(dist_x * dist_x + dist_y * dist_y + dist_z * dist_z);

      if (distance > tolerance) {
        simplified.push_back(p_curr);
      }
    } else {
      simplified.push_back(p_curr);
    }
  }

  simplified.push_back(vertices.back());
  return simplified;
}

/**
 * Validate tube candidate
 */
bool GeometryRecognition::validate_tube_candidate(
    const TubeCandidate& candidate) {
  // Minimum confidence
  if (candidate.confidence < 50.0) {
    return false;
  }

  // Must have vertices
  if (candidate.vertices.size() < 2) {
    return false;
  }

  // Must have positive diameter
  if (candidate.outer_diameter <= 0.0 || candidate.inner_diameter < 0.0) {
    return false;
  }

  // Wall thickness must be positive
  if (candidate.outer_diameter <= candidate.inner_diameter) {
    return false;
  }

  return true;
}

/**
 * Get recognition report
 */
json GeometryRecognition::get_recognition_report(
    const std::vector<TubeCandidate>& candidates) {
  json report;
  report["total_candidates"] = candidates.size();

  // Group by confidence
  int high_confidence = 0;
  int medium_confidence = 0;
  int low_confidence = 0;

  for (const auto& candidate : candidates) {
    if (candidate.confidence > 80.0) {
      high_confidence++;
    } else if (candidate.confidence > 60.0) {
      medium_confidence++;
    } else {
      low_confidence++;
    }
  }

  report["high_confidence"] = high_confidence;
  report["medium_confidence"] = medium_confidence;
  report["low_confidence"] = low_confidence;

  // Candidate details
  json candidates_json = json::array();
  for (const auto& candidate : candidates) {
    candidates_json.push_back({
        {"id", candidate.id},
        {"name", candidate.name},
        {"od", candidate.outer_diameter},
        {"id_inner", candidate.inner_diameter},
        {"confidence", candidate.confidence},
        {"bends", candidate.bend_angles.size()},
    });
  }
  report["candidates"] = candidates_json;

  return report;
}

/**
 * Extract centerline from solid
 */
std::vector<std::vector<double>> GeometryRecognition::extract_centerline(
    const SolidGeometry& solid) {
  // Simplified: create synthetic centerline based on bounding box
  const auto& bbox = solid.bounding_box;

  std::vector<std::vector<double>> centerline;

  // Determine major axis
  double dx = bbox[3] - bbox[0];
  double dy = bbox[4] - bbox[1];
  double dz = bbox[5] - bbox[2];

  double cx = (bbox[0] + bbox[3]) / 2.0;
  double cy = (bbox[1] + bbox[4]) / 2.0;
  double cz = (bbox[2] + bbox[5]) / 2.0;

  // Generate centerline points along major axis
  if (dx > dy && dx > dz) {
    // X-axis
    for (double x = bbox[0]; x <= bbox[3]; x += dx / 10.0) {
      centerline.push_back({x, cy, cz});
    }
  } else if (dy > dz) {
    // Y-axis
    for (double y = bbox[1]; y <= bbox[4]; y += dy / 10.0) {
      centerline.push_back({cx, y, cz});
    }
  } else {
    // Z-axis
    for (double z = bbox[2]; z <= bbox[5]; z += dz / 10.0) {
      centerline.push_back({cx, cy, z});
    }
  }

  return centerline;
}

/**
 * Compute angle between three points
 */
double GeometryRecognition::compute_angle(
    const std::vector<double>& p1,
    const std::vector<double>& p2,
    const std::vector<double>& p3) {
  // Vectors
  std::vector<double> v1 = {p1[0] - p2[0], p1[1] - p2[1], p1[2] - p2[2]};
  std::vector<double> v2 = {p3[0] - p2[0], p3[1] - p2[1], p3[2] - p2[2]};

  // Normalize
  v1 = normalize_vector(v1);
  v2 = normalize_vector(v2);

  // Dot product
  double dot = dot_product(v1, v2);
  dot = std::max(-1.0, std::min(1.0, dot));

  // Angle in degrees
  return std::acos(dot) * 180.0 / M_PI;
}

/**
 * Compute distance
 */
double GeometryRecognition::compute_distance(
    const std::vector<double>& p1,
    const std::vector<double>& p2) {
  double dx = p2[0] - p1[0];
  double dy = p2[1] - p1[1];
  double dz = p2[2] - p1[2];
  return std::sqrt(dx * dx + dy * dy + dz * dz);
}

/**
 * Normalize vector
 */
std::vector<double> GeometryRecognition::normalize_vector(
    const std::vector<double>& v) {
  double len = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
  if (len < 1e-10) {
    return {0, 0, 0};
  }
  return {v[0] / len, v[1] / len, v[2] / len};
}

/**
 * Cross product
 */
std::vector<double> GeometryRecognition::cross_product(
    const std::vector<double>& a,
    const std::vector<double>& b) {
  return {
      a[1] * b[2] - a[2] * b[1],
      a[2] * b[0] - a[0] * b[2],
      a[0] * b[1] - a[1] * b[0],
  };
}

/**
 * Dot product
 */
double GeometryRecognition::dot_product(
    const std::vector<double>& a,
    const std::vector<double>& b) {
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

/**
 * Catmull-Rom spline point
 */
std::vector<double> GeometryRecognition::catmull_rom_point(
    const std::vector<double>& p0,
    const std::vector<double>& p1,
    const std::vector<double>& p2,
    const std::vector<double>& p3,
    double t) {
  double t2 = t * t;
  double t3 = t2 * t;

  double mt = 1.0 - t;
  double mt2 = mt * mt;
  double mt3 = mt2 * mt;

  std::vector<double> result(3);
  for (int i = 0; i < 3; ++i) {
    result[i] = 0.5 * (
        2.0 * p1[i] +
        (-p0[i] + p2[i]) * t +
        (2.0 * p0[i] - 5.0 * p1[i] + 4.0 * p2[i] - p3[i]) * t2 +
        (-p0[i] + 3.0 * p1[i] - 3.0 * p2[i] + p3[i]) * t3
    );
  }

  return result;
}

}  // namespace bendcore
