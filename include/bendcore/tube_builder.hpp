/**
 * BendCore Phase 35C: Tube Body Generation
 * TubeBuilder: Core geometry generation from path sketches and bending profiles
 *
 * Algorithm (Р. Сафаров):
 * For each segment:
 *   T = R × tan(B/2)     [tangent offset]
 *   S = L - T_left - T_right  [straight segment]
 *   Create cylinder on S, torus on bend arc
 * Validate entire tube, merge solids, calculate mass
 */

#pragma once

#include <vector>
#include <string>
#include <memory>
#include <optional>
#include <cmath>
#include <nlohmann/json.hpp>

namespace bendcore::tube {

using json = nlohmann::json;

// ============ DATA STRUCTURES ============

/**
 * 3D Vector type
 */
struct Vector3 {
  double x, y, z;

  Vector3(double x = 0, double y = 0, double z = 0) : x(x), y(y), z(z) {}

  Vector3 operator+(const Vector3& v) const {
    return Vector3(x + v.x, y + v.y, z + v.z);
  }

  Vector3 operator-(const Vector3& v) const {
    return Vector3(x - v.x, y - v.y, z - v.z);
  }

  Vector3 operator*(double s) const {
    return Vector3(x * s, y * s, z * s);
  }

  double dot(const Vector3& v) const {
    return x * v.x + y * v.y + z * v.z;
  }

  Vector3 cross(const Vector3& v) const {
    return Vector3(
      y * v.z - z * v.y,
      z * v.x - x * v.z,
      x * v.y - y * v.x
    );
  }

  double length() const {
    return std::sqrt(x * x + y * y + z * z);
  }

  Vector3 normalized() const {
    double len = length();
    return len > 0 ? Vector3(x / len, y / len, z / len) : Vector3(0, 0, 0);
  }
};

/**
 * Path segment from Phase 35B PathSketch
 */
struct PathSegment {
  Vector3 start;
  Vector3 end;
  double bend_angle;      // B in radians
  std::string segment_id;
  bool is_straight;
};

/**
 * Bending profile constraint from Phase 35A
 */
struct BendingProfile {
  std::string profile_id;
  std::string name;
  double radius;          // R (major radius for torus)
  double min_bend_angle;  // B_min
  double max_bend_angle;  // B_max
  double min_straight;    // S_min (minimum straight segment)
  double min_degenerate;  // Minimum bend angle to avoid degeneracy
  json metadata;
};

/**
 * Tube specification (diameter, wall thickness, material)
 */
struct TubeSpecification {
  std::string tube_id;
  double outer_diameter;  // OD in mm
  double inner_diameter;  // ID in mm
  std::string material_id;
  double density;         // g/cm³
  std::string designation; // e.g., "Трубка медная ∅12×1, 90°, 250мм"
};

/**
 * Hollow cylinder geometry
 */
struct Cylinder {
  Vector3 center;
  Vector3 axis;
  double length;
  double outer_radius;
  double inner_radius;
  std::string segment_id;

  double volume() const {
    double r_o = outer_radius;
    double r_i = inner_radius;
    return M_PI * (r_o * r_o - r_i * r_i) * length;
  }
};

/**
 * Hollow torus geometry
 */
struct Torus {
  Vector3 center;
  Vector3 axis;           // Normal to torus plane
  double major_radius;    // R
  double minor_radius;    // (OD - ID) / 2
  double start_angle;     // In radians
  double end_angle;       // In radians
  std::string segment_id;

  double sweep_angle() const {
    return std::abs(end_angle - start_angle);
  }

  double volume() const {
    double r_m = minor_radius;
    double sweep = sweep_angle();
    return 2 * M_PI * r_m * r_m * major_radius * (sweep / (2 * M_PI));
  }
};

/**
 * Complete tube body ready for rendering/export
 */
struct TubeBody {
  std::string tube_id;
  std::vector<Cylinder> straight_sections;
  std::vector<Torus> bend_sections;

  double total_length;    // Sum of straight lengths + arc lengths
  double total_volume;    // cm³
  double total_mass;      // grams

  bool valid;
  std::vector<std::string> validation_errors;
  std::vector<std::string> validation_warnings;

  std::vector<std::string> feature_ids;  // FeatureTree node IDs

  json to_json() const;
};

// ============ TUBE BUILDER CLASS ============

class TubeBuilder {
 public:
  TubeBuilder() = default;
  ~TubeBuilder() = default;

  /**
   * Main entry point: Build complete tube from path, profile, and spec
   */
  TubeBody build(
    const std::vector<PathSegment>& path,
    const BendingProfile& profile,
    const TubeSpecification& spec
  );

 private:
  // ========== Core Building Blocks ==========

  /**
   * Create hollow cylinder
   */
  Cylinder create_hollow_cylinder(
    Vector3 center,
    Vector3 axis,
    double length,
    double outer_diameter,
    double inner_diameter,
    const std::string& segment_id
  );

  /**
   * Create hollow torus
   */
  Torus create_hollow_torus(
    Vector3 center,
    Vector3 axis,
    double major_radius,
    double minor_radius,
    double start_angle,
    double end_angle,
    const std::string& segment_id
  );

  // ========== Calculations ==========

  /**
   * Calculate tangent offset: T = R × tan(B/2)
   */
  double calculate_tangent(double radius, double angle);

  /**
   * Calculate straight segment: S = L - T_left - T_right
   */
  double calculate_straight_segment(
    double total_length,
    double tangent_left,
    double tangent_right
  );

  /**
   * Calculate arc length: arc = R × B (in radians)
   */
  double calculate_arc_length(double radius, double angle);

  /**
   * Get segment direction vector
   */
  Vector3 get_segment_direction(const PathSegment& seg);

  /**
   * Get perpendicular vector to segment
   */
  Vector3 get_perpendicular(const Vector3& direction);

  // ========== Validation ==========

  /**
   * Validate entire tube geometry
   */
  void validate_tube(
    TubeBody& tube,
    const std::vector<PathSegment>& path,
    const BendingProfile& profile,
    const TubeSpecification& spec
  );

  /**
   * Check straight segments meet minimum requirement
   */
  bool check_straight_segments(
    const std::vector<PathSegment>& path,
    const BendingProfile& profile,
    const TubeSpecification& spec,
    std::vector<std::string>& errors
  );

  /**
   * Check bend angles within limits
   */
  bool check_bend_angles(
    const std::vector<PathSegment>& path,
    const BendingProfile& profile,
    std::vector<std::string>& errors
  );

  /**
   * Check for arc overlap at tangent points
   */
  bool check_arc_overlap(
    const std::vector<PathSegment>& path,
    const TubeSpecification& spec,
    std::vector<std::string>& errors
  );

  /**
   * Check for degenerate bends
   */
  bool check_degenerate_bends(
    const std::vector<PathSegment>& path,
    const BendingProfile& profile,
    std::vector<std::string>& warnings
  );

  // ========== Properties Calculation ==========

  /**
   * Calculate total tube length
   */
  double calculate_total_length(
    const std::vector<Cylinder>& straights,
    const std::vector<Torus>& bends,
    const BendingProfile& profile
  );

  /**
   * Calculate total volume in cm³
   */
  double calculate_total_volume(
    const std::vector<Cylinder>& straights,
    const std::vector<Torus>& bends
  );

  /**
   * Calculate total mass
   */
  double calculate_mass(double volume, double density);
};

}  // namespace bendcore::tube
