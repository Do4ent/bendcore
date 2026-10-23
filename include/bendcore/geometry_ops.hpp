/**
 * BendCore Phase 35C: Geometry Operations
 * Solid geometry utilities: merging, collision detection, volume calculation
 */

#pragma once

#include "bendcore/tube_builder.hpp"
#include <vector>
#include <optional>

namespace bendcore::tube {

// ============ GEOMETRY OPERATIONS ============

class GeometryOps {
 public:
  /**
   * Bounding box
   */
  struct BoundingBox {
    Vector3 min;
    Vector3 max;

    bool contains(const Vector3& p) const {
      return p.x >= min.x && p.x <= max.x &&
             p.y >= min.y && p.y <= max.y &&
             p.z >= min.z && p.z <= max.z;
    }

    bool intersects(const BoundingBox& other) const {
      return min.x <= other.max.x && max.x >= other.min.x &&
             min.y <= other.max.y && max.y >= other.min.y &&
             min.z <= other.max.z && max.z >= other.min.z;
    }
  };

  // ========== Bounding Box Operations ==========

  /**
   * Calculate bounding box for cylinder
   */
  static BoundingBox get_cylinder_bbox(const Cylinder& cyl);

  /**
   * Calculate bounding box for torus
   */
  static BoundingBox get_torus_bbox(const Torus& torus);

  /**
   * Calculate bounding box for complete tube
   */
  static BoundingBox get_tube_bbox(const TubeBody& tube);

  // ========== Volume Calculations ==========

  /**
   * Calculate volume of cylinder in cm³
   */
  static double cylinder_volume(const Cylinder& cyl);

  /**
   * Calculate volume of torus in cm³
   */
  static double torus_volume(const Torus& torus);

  /**
   * Calculate total volume of tube
   */
  static double tube_volume(const TubeBody& tube);

  /**
   * Calculate surface area of cylinder in cm²
   */
  static double cylinder_surface_area(const Cylinder& cyl);

  /**
   * Calculate surface area of torus in cm²
   */
  static double torus_surface_area(const Torus& torus);

  // ========== Collision Detection ==========

  /**
   * Check if two cylinders intersect (sphere-swept line segment method)
   */
  static bool cylinder_cylinder_intersect(const Cylinder& c1, const Cylinder& c2);

  /**
   * Check if two toruses intersect
   */
  static bool torus_torus_intersect(const Torus& t1, const Torus& t2);

  /**
   * Check if cylinder and torus intersect
   */
  static bool cylinder_torus_intersect(const Cylinder& cyl, const Torus& torus);

  /**
   * Check if point is inside cylinder
   */
  static bool point_in_cylinder(const Vector3& p, const Cylinder& cyl);

  /**
   * Check if point is inside torus
   */
  static bool point_in_torus(const Vector3& p, const Torus& torus);

  /**
   * Distance from point to cylinder axis
   */
  static double point_to_cylinder_distance(const Vector3& p, const Cylinder& cyl);

  /**
   * Distance from point to torus center
   */
  static double point_to_torus_distance(const Vector3& p, const Torus& torus);

  // ========== Geometric Calculations ==========

  /**
   * Closest point on line segment to a given point
   */
  static Vector3 closest_point_on_segment(
    const Vector3& p,
    const Vector3& seg_start,
    const Vector3& seg_end
  );

  /**
   * Distance between two line segments
   */
  static double segment_segment_distance(
    const Vector3& a1, const Vector3& a2,
    const Vector3& b1, const Vector3& b2
  );

  /**
   * Check if two cylinders share the same axis (within tolerance)
   */
  static bool same_axis(const Cylinder& c1, const Cylinder& c2, double tol = 1e-6);

  /**
   * Merge two parallel cylinders (when aligned)
   */
  static std::optional<Cylinder> merge_cylinders(const Cylinder& c1, const Cylinder& c2);

  // ========== Transformation ==========

  /**
   * Translate geometry by offset vector
   */
  static Cylinder translate_cylinder(const Cylinder& cyl, const Vector3& offset);
  static Torus translate_torus(const Torus& torus, const Vector3& offset);

  /**
   * Rotate geometry around axis
   */
  static Vector3 rotate_vector(const Vector3& v, const Vector3& axis, double angle);
  static Cylinder rotate_cylinder(const Cylinder& cyl, const Vector3& axis, double angle);
  static Torus rotate_torus(const Torus& torus, const Vector3& axis, double angle);

  // ========== Projection ==========

  /**
   * Project cylinder onto plane (for 2D visualization)
   */
  static std::vector<Vector3> project_cylinder_2d(
    const Cylinder& cyl,
    const Vector3& plane_normal
  );

  /**
   * Project torus onto plane
   */
  static std::vector<Vector3> project_torus_2d(
    const Torus& torus,
    const Vector3& plane_normal
  );
};

}  // namespace bendcore::tube
