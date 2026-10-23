/**
 * BendCore Geometry Recognition
 * Auto-detect and fit tubes from imported geometry
 *
 * Algorithm:
 * 1. Check if solid is cylindrical
 * 2. Match diameter to known profiles
 * 3. Fit path through solid
 * 4. Detect bend angles
 * 5. Match against bending profiles
 * 6. Create TubeCandidate
 */

#pragma once

#include <vector>
#include <string>
#include <memory>
#include <nlohmann/json.hpp>

namespace bendcore {

using json = nlohmann::json;

/**
 * Forward declarations
 */
struct SolidGeometry;
struct BendingProfile;

/**
 * Recognized tube candidate
 */
struct TubeCandidate {
  std::string id;
  std::string name;
  double outer_diameter;
  double inner_diameter;
  double wall_thickness;
  std::string material;
  std::string bending_profile;
  double confidence;  // 0-100
  std::vector<std::vector<double>> vertices;  // 3D path
  std::vector<double> bend_angles;
  std::vector<double> segment_lengths;
  json metadata;
};

/**
 * Tube fitting result
 */
struct TubeFittingResult {
  bool success;
  std::string error_message;
  std::vector<std::vector<double>> fitted_path;
  std::vector<double> detected_angles;
  double fit_error;
  double confidence;
};

/**
 * Geometry recognition engine
 */
class GeometryRecognition {
 public:
  GeometryRecognition() = default;
  ~GeometryRecognition() = default;

  /**
   * Auto-recognize tubes from solids
   * @param solids Vector of imported solids
   * @return List of recognized tube candidates
   */
  std::vector<TubeCandidate> recognize_tubes(
      const std::vector<SolidGeometry>& solids);

  /**
   * Fit a path through a cylindrical solid
   * @param solid Input solid
   * @return Fitting result with path and angles
   */
  TubeFittingResult fit_path_through_solid(
      const SolidGeometry& solid);

  /**
   * Detect bend angles in a path
   * @param vertices 3D path vertices
   * @param angle_threshold Minimum angle to detect (degrees)
   * @return List of bend angles
   */
  std::vector<double> detect_bend_angles(
      const std::vector<std::vector<double>>& vertices,
      double angle_threshold = 5.0);

  /**
   * Smooth a path using curve fitting
   * @param vertices Raw path vertices
   * @param smoothing_factor Smoothing strength (0-1)
   * @return Smoothed vertices
   */
  std::vector<std::vector<double>> smooth_path(
      const std::vector<std::vector<double>>& vertices,
      double smoothing_factor = 0.5);

  /**
   * Match diameter to standard profile
   * @param diameter Measured diameter (mm)
   * @param tolerance Tolerance (mm)
   * @return Best matching profile name
   */
  std::string match_profile_by_diameter(
      double diameter,
      double tolerance = 0.5);

  /**
   * Get confidence score for a tube candidate
   * @param candidate Tube candidate
   * @return Confidence score (0-100)
   */
  double calculate_confidence(const TubeCandidate& candidate);

  /**
   * Simplify a path (remove unnecessary points)
   * @param vertices Input vertices
   * @param tolerance Simplification tolerance
   * @return Simplified vertices
   */
  std::vector<std::vector<double>> simplify_path(
      const std::vector<std::vector<double>>& vertices,
      double tolerance = 0.1);

  /**
   * Validate a tube candidate
   * @param candidate Tube to validate
   * @return Validation result
   */
  bool validate_tube_candidate(const TubeCandidate& candidate);

  /**
   * Get recognition analysis report
   */
  json get_recognition_report(
      const std::vector<TubeCandidate>& candidates);

 private:
  /**
   * Extract centerline from cylindrical solid
   */
  std::vector<std::vector<double>> extract_centerline(
      const SolidGeometry& solid);

  /**
   * Compute angle between three points
   */
  double compute_angle(const std::vector<double>& p1,
                       const std::vector<double>& p2,
                       const std::vector<double>& p3);

  /**
   * Compute distance between two points
   */
  double compute_distance(const std::vector<double>& p1,
                          const std::vector<double>& p2);

  /**
   * Normalize vector
   */
  std::vector<double> normalize_vector(const std::vector<double>& v);

  /**
   * Cross product of two 3D vectors
   */
  std::vector<double> cross_product(const std::vector<double>& a,
                                     const std::vector<double>& b);

  /**
   * Dot product of two vectors
   */
  double dot_product(const std::vector<double>& a,
                     const std::vector<double>& b);

  /**
   * Catmull-Rom spline interpolation
   */
  std::vector<double> catmull_rom_point(
      const std::vector<double>& p0,
      const std::vector<double>& p1,
      const std::vector<double>& p2,
      const std::vector<double>& p3,
      double t);

  /**
   * Fit a circle to a set of 2D points
   */
  std::vector<double> fit_circle(
      const std::vector<std::vector<double>>& points);

  /**
   * Check if path has bends (not straight line)
   */
  bool has_bends(const std::vector<std::vector<double>>& path,
                 double angle_threshold = 5.0);

  // Standard tube diameters (mm)
  std::vector<double> standard_diameters_ = {
      3.175,   // 1/8"
      4.763,   // 3/16"
      6.35,    // 1/4"
      7.938,   // 5/16"
      9.525,   // 3/8"
      11.113,  // 7/16"
      12.7,    // 1/2"
      14.288,  // 9/16"
      15.875,  // 5/8"
      17.463,  // 11/16"
      19.05,   // 3/4"
      22.225,  // 7/8"
      25.4,    // 1"
      28.575,  // 1 1/8"
      31.75,   // 1 1/4"
      38.1,    // 1 1/2"
      44.45,   // 1 3/4"
      50.8,    // 2"
  };
};

}  // namespace bendcore
