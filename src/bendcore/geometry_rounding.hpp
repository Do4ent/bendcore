/**
 * BendCore Geometry Rounding
 * Snap geometry to standard values for manufacturing
 */

#pragma once

#include <vector>
#include <string>
#include <nlohmann/json.hpp>

namespace bendcore {

using json = nlohmann::json;

/**
 * Rounding preferences
 */
struct RoundingPreferences {
  bool round_diameters = true;
  bool round_radii = true;
  bool round_lengths = true;
  bool round_angles = false;
  double length_precision = 0.1;  // mm
  double angle_step = 5.0;         // degrees
};

/**
 * Tube body to be rounded
 */
struct TubeBody {
  std::string id;
  std::string name;
  double outer_diameter;
  double inner_diameter;
  double bend_radius;
  std::vector<std::vector<double>> path;
  std::vector<double> bend_angles;
  std::vector<double> segment_lengths;
  std::string material;
};

/**
 * Rounding result
 */
struct RoundingResult {
  bool success;
  std::string message;
  TubeBody rounded_tube;
  json changes;
};

/**
 * Geometry rounding engine
 */
class GeometryRounder {
 public:
  GeometryRounder() = default;
  ~GeometryRounder() = default;

  /**
   * Snap outer diameter to nearest standard size
   * @param od Outer diameter (mm)
   * @param tolerance Matching tolerance (mm)
   * @return Rounded diameter
   */
  double round_diameter(double od, double tolerance = 0.5);

  /**
   * Snap radius to nearest profile R
   * @param radius Current radius (mm)
   * @param profile Bending profile (contains allowed radii)
   * @return Rounded radius
   */
  double round_radius(double radius, double tolerance = 0.1);

  /**
   * Snap length to precision
   * @param length Length (mm)
   * @param precision Rounding precision (mm)
   * @return Rounded length
   */
  double round_length(double length, double precision = 0.1);

  /**
   * Snap angle to increments
   * @param angle_deg Angle (degrees)
   * @param step Rounding step (degrees)
   * @return Rounded angle
   */
  double round_angle(double angle_deg, double step = 5.0);

  /**
   * Round all tube properties
   * @param tube Tube to round
   * @param preferences Rounding preferences
   * @return Rounding result with changes
   */
  RoundingResult round_tube(const TubeBody& tube,
                            const RoundingPreferences& preferences);

  /**
   * Round multiple tubes
   * @param tubes Tubes to round
   * @param preferences Rounding preferences
   * @return Vector of results
   */
  std::vector<RoundingResult> round_tubes(
      const std::vector<TubeBody>& tubes,
      const RoundingPreferences& preferences);

  /**
   * Get standard diameter list
   * @return Vector of standard diameters (mm)
   */
  std::vector<double> get_standard_diameters() const;

  /**
   * Get standard radius list
   * @return Vector of standard radii (mm)
   */
  std::vector<double> get_standard_radii() const;

  /**
   * Check if value matches standard with tolerance
   * @param value Value to check
   * @param standards Vector of standard values
   * @param tolerance Matching tolerance
   * @return True if matches any standard
   */
  bool matches_standard(double value,
                       const std::vector<double>& standards,
                       double tolerance = 0.1);

  /**
   * Find nearest standard value
   * @param value Current value
   * @param standards Vector of standard values
   * @return Nearest standard value
   */
  double find_nearest_standard(double value,
                               const std::vector<double>& standards);

  /**
   * Get rounding analysis report
   */
  json get_rounding_report(const std::vector<RoundingResult>& results);

 private:
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

  // Standard bend radii (mm)
  std::vector<double> standard_radii_ = {
      4.0,    // Tight
      6.0,
      8.0,
      10.0,
      12.0,
      14.0,
      16.0,
      18.0,
      20.0,
      25.0,
      30.0,
      40.0,
      50.0,    // Gentle
  };
};

}  // namespace bendcore
