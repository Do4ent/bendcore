/**
 * BendCore Phase 35C: Geometry Operations Implementation
 * Solid geometry utilities: bounding boxes, volume, collision detection
 */

#include "bendcore/geometry_ops.hpp"
#include <cmath>
#include <algorithm>

namespace bendcore::tube {

// ============ BOUNDING BOX OPERATIONS ============

GeometryOps::BoundingBox GeometryOps::get_cylinder_bbox(const Cylinder& cyl) {
  BoundingBox bbox;

  // Get cylinder endpoints
  Vector3 p1 = cyl.center - cyl.axis * (cyl.length / 2.0);
  Vector3 p2 = cyl.center + cyl.axis * (cyl.length / 2.0);

  // Initialize with endpoints
  bbox.min.x = std::min(p1.x, p2.x) - cyl.outer_radius;
  bbox.max.x = std::max(p1.x, p2.x) + cyl.outer_radius;

  bbox.min.y = std::min(p1.y, p2.y) - cyl.outer_radius;
  bbox.max.y = std::max(p1.y, p2.y) + cyl.outer_radius;

  bbox.min.z = std::min(p1.z, p2.z) - cyl.outer_radius;
  bbox.max.z = std::max(p1.z, p2.z) + cyl.outer_radius;

  return bbox;
}

GeometryOps::BoundingBox GeometryOps::get_torus_bbox(const Torus& torus) {
  BoundingBox bbox;

  double radius_extent = torus.major_radius + torus.minor_radius;

  bbox.min.x = torus.center.x - radius_extent;
  bbox.max.x = torus.center.x + radius_extent;

  bbox.min.y = torus.center.y - radius_extent;
  bbox.max.y = torus.center.y + radius_extent;

  bbox.min.z = torus.center.z - radius_extent;
  bbox.max.z = torus.center.z + radius_extent;

  return bbox;
}

GeometryOps::BoundingBox GeometryOps::get_tube_bbox(const TubeBody& tube) {
  BoundingBox bbox;

  if (tube.straight_sections.empty() && tube.bend_sections.empty()) {
    bbox.min = bbox.max = Vector3(0, 0, 0);
    return bbox;
  }

  // Initialize with first element
  if (!tube.straight_sections.empty()) {
    bbox = get_cylinder_bbox(tube.straight_sections[0]);
  } else if (!tube.bend_sections.empty()) {
    bbox = get_torus_bbox(tube.bend_sections[0]);
  }

  // Expand to include all cylinders
  for (const auto& cyl : tube.straight_sections) {
    auto cyl_bbox = get_cylinder_bbox(cyl);
    bbox.min.x = std::min(bbox.min.x, cyl_bbox.min.x);
    bbox.min.y = std::min(bbox.min.y, cyl_bbox.min.y);
    bbox.min.z = std::min(bbox.min.z, cyl_bbox.min.z);
    bbox.max.x = std::max(bbox.max.x, cyl_bbox.max.x);
    bbox.max.y = std::max(bbox.max.y, cyl_bbox.max.y);
    bbox.max.z = std::max(bbox.max.z, cyl_bbox.max.z);
  }

  // Expand to include all toruses
  for (const auto& torus : tube.bend_sections) {
    auto torus_bbox = get_torus_bbox(torus);
    bbox.min.x = std::min(bbox.min.x, torus_bbox.min.x);
    bbox.min.y = std::min(bbox.min.y, torus_bbox.min.y);
    bbox.min.z = std::min(bbox.min.z, torus_bbox.min.z);
    bbox.max.x = std::max(bbox.max.x, torus_bbox.max.x);
    bbox.max.y = std::max(bbox.max.y, torus_bbox.max.y);
    bbox.max.z = std::max(bbox.max.z, torus_bbox.max.z);
  }

  return bbox;
}

// ============ VOLUME CALCULATIONS ============

double GeometryOps::cylinder_volume(const Cylinder& cyl) {
  return cyl.volume();
}

double GeometryOps::torus_volume(const Torus& torus) {
  return torus.volume();
}

double GeometryOps::tube_volume(const TubeBody& tube) {
  double total = 0.0;

  for (const auto& cyl : tube.straight_sections) {
    total += cylinder_volume(cyl);
  }

  for (const auto& torus : tube.bend_sections) {
    total += torus_volume(torus);
  }

  return total;
}

double GeometryOps::cylinder_surface_area(const Cylinder& cyl) {
  double r_o = cyl.outer_radius;
  double r_i = cyl.inner_radius;
  double l = cyl.length;

  // Outer surface
  double outer = 2 * M_PI * r_o * l;

  // Inner surface
  double inner = 2 * M_PI * r_i * l;

  // Top and bottom annuli
  double ends = 2 * M_PI * (r_o * r_o - r_i * r_i);

  return outer + inner + ends;
}

double GeometryOps::torus_surface_area(const Torus& torus) {
  double R = torus.major_radius;
  double r = torus.minor_radius;
  double sweep = torus.sweep_angle();

  // Surface area of full torus is 4π²Rr
  // For partial sweep, multiply by sweep/(2π)
  double full_area = 4 * M_PI * M_PI * R * r;
  return full_area * (sweep / (2 * M_PI));
}

// ============ COLLISION DETECTION ============

bool GeometryOps::cylinder_cylinder_intersect(const Cylinder& c1, const Cylinder& c2) {
  // Get closest points on the two cylinder axes
  Vector3 p1_start = c1.center - c1.axis * (c1.length / 2.0);
  Vector3 p1_end = c1.center + c1.axis * (c1.length / 2.0);
  Vector3 p2_start = c2.center - c2.axis * (c2.length / 2.0);
  Vector3 p2_end = c2.center + c2.axis * (c2.length / 2.0);

  // Compute distance between axes
  double dist = segment_segment_distance(p1_start, p1_end, p2_start, p2_end);

  // If distance <= sum of radii, they intersect
  double min_dist = c1.outer_radius + c2.outer_radius;
  return dist <= min_dist;
}

bool GeometryOps::torus_torus_intersect(const Torus& t1, const Torus& t2) {
  // Simplified check: if centers are close and toruses are in same plane
  double dist = (t1.center - t2.center).length();
  double max_dist = (t1.major_radius + t1.minor_radius) +
                    (t2.major_radius + t2.minor_radius);

  return dist < max_dist;
}

bool GeometryOps::cylinder_torus_intersect(const Cylinder& cyl, const Torus& torus) {
  // Check if cylinder axis is close to torus center
  Vector3 cyl_start = cyl.center - cyl.axis * (cyl.length / 2.0);
  Vector3 cyl_end = cyl.center + cyl.axis * (cyl.length / 2.0);

  Vector3 closest = closest_point_on_segment(torus.center, cyl_start, cyl_end);
  double dist = (closest - torus.center).length();

  double torus_extent = torus.major_radius + torus.minor_radius;
  double min_dist = cyl.outer_radius + torus.minor_radius;

  return dist < min_dist;
}

bool GeometryOps::point_in_cylinder(const Vector3& p, const Cylinder& cyl) {
  // Project point onto cylinder axis
  Vector3 cyl_start = cyl.center - cyl.axis * (cyl.length / 2.0);
  Vector3 to_p = p - cyl_start;

  double t = to_p.dot(cyl.axis);

  // Check if projection is within cylinder bounds
  if (t < 0 || t > cyl.length) return false;

  // Check radial distance
  Vector3 proj = cyl_start + cyl.axis * t;
  double radial_dist = (p - proj).length();

  return radial_dist <= cyl.outer_radius && radial_dist >= cyl.inner_radius;
}

bool GeometryOps::point_in_torus(const Vector3& p, const Torus& torus) {
  // Project point onto torus plane
  Vector3 to_p = p - torus.center;
  double dist_from_center = to_p.length();

  // Check if within major radius extent
  if (dist_from_center > torus.major_radius + torus.minor_radius) {
    return false;
  }

  if (dist_from_center < torus.major_radius - torus.minor_radius) {
    return false;
  }

  return true;
}

double GeometryOps::point_to_cylinder_distance(const Vector3& p, const Cylinder& cyl) {
  Vector3 cyl_start = cyl.center - cyl.axis * (cyl.length / 2.0);
  Vector3 cyl_end = cyl.center + cyl.axis * (cyl.length / 2.0);

  Vector3 closest = closest_point_on_segment(p, cyl_start, cyl_end);
  return (p - closest).length() - cyl.outer_radius;
}

double GeometryOps::point_to_torus_distance(const Vector3& p, const Torus& torus) {
  Vector3 to_p = p - torus.center;
  double dist = to_p.length();

  // Distance to major radius circle
  double to_major = std::abs(dist - torus.major_radius);

  return to_major - torus.minor_radius;
}

// ============ GEOMETRIC CALCULATIONS ============

Vector3 GeometryOps::closest_point_on_segment(
    const Vector3& p,
    const Vector3& seg_start,
    const Vector3& seg_end) {

  Vector3 seg = seg_end - seg_start;
  Vector3 to_p = p - seg_start;

  double seg_len_sq = seg.dot(seg);
  if (seg_len_sq < 1e-10) {
    return seg_start;
  }

  double t = to_p.dot(seg) / seg_len_sq;
  t = std::max(0.0, std::min(1.0, t));

  return seg_start + seg * t;
}

double GeometryOps::segment_segment_distance(
    const Vector3& a1, const Vector3& a2,
    const Vector3& b1, const Vector3& b2) {

  Vector3 da = a2 - a1;
  Vector3 db = b2 - b1;
  Vector3 w = b1 - a1;

  double a = da.dot(da);
  double b = da.dot(db);
  double c = db.dot(db);
  double d = da.dot(w);
  double e = db.dot(w);

  double denom = a * c - b * b;

  double t = 0.0, s = 0.0;

  if (denom > 1e-10) {
    t = (b * e - c * d) / denom;
    s = (a * e - b * d) / denom;
  }

  t = std::max(0.0, std::min(1.0, t));
  s = std::max(0.0, std::min(1.0, s));

  Vector3 p1 = a1 + da * t;
  Vector3 p2 = b1 + db * s;

  return (p1 - p2).length();
}

bool GeometryOps::same_axis(const Cylinder& c1, const Cylinder& c2, double tol) {
  // Check if axes are parallel (within tolerance)
  Vector3 cross = c1.axis.cross(c2.axis);
  if (cross.length() > tol) return false;

  // Check if axes are aligned (same direction)
  Vector3 start_diff = c2.center - c1.center;

  // Project onto first axis
  double proj = start_diff.dot(c1.axis);

  // If projection is close to axis alignment
  return std::abs(proj) < tol;
}

std::optional<Cylinder> GeometryOps::merge_cylinders(const Cylinder& c1, const Cylinder& c2) {
  if (!same_axis(c1, c2)) {
    return std::nullopt;
  }

  // Calculate new merged cylinder
  Vector3 c1_start = c1.center - c1.axis * (c1.length / 2.0);
  Vector3 c1_end = c1.center + c1.axis * (c1.length / 2.0);

  Vector3 c2_start = c2.center - c2.axis * (c2.length / 2.0);
  Vector3 c2_end = c2.center + c2.axis * (c2.length / 2.0);

  Vector3 new_start = c1_start;
  Vector3 new_end = c1_end;

  // Extend to include c2
  double t = c2_start.dot(c1.axis);
  if (t < new_start.dot(c1.axis)) new_start = c2_start;

  t = c2_end.dot(c1.axis);
  if (t > new_end.dot(c1.axis)) new_end = c2_end;

  Cylinder merged;
  merged.center = (new_start + new_end) * 0.5;
  merged.axis = c1.axis;
  merged.length = (new_end - new_start).length();
  merged.outer_radius = c1.outer_radius;
  merged.inner_radius = c1.inner_radius;

  return merged;
}

// ============ TRANSFORMATION ============

Cylinder GeometryOps::translate_cylinder(const Cylinder& cyl, const Vector3& offset) {
  Cylinder result = cyl;
  result.center = result.center + offset;
  return result;
}

Torus GeometryOps::translate_torus(const Torus& torus, const Vector3& offset) {
  Torus result = torus;
  result.center = result.center + offset;
  return result;
}

Vector3 GeometryOps::rotate_vector(const Vector3& v, const Vector3& axis, double angle) {
  Vector3 a = axis.normalized();
  double c = std::cos(angle);
  double s = std::sin(angle);

  // Rodrigues' rotation formula
  return v * c + a.cross(v) * s + a * a.dot(v) * (1 - c);
}

Cylinder GeometryOps::rotate_cylinder(const Cylinder& cyl, const Vector3& axis, double angle) {
  Cylinder result = cyl;
  result.center = rotate_vector(cyl.center, axis, angle);
  result.axis = rotate_vector(cyl.axis, axis, angle);
  return result;
}

Torus GeometryOps::rotate_torus(const Torus& torus, const Vector3& axis, double angle) {
  Torus result = torus;
  result.center = rotate_vector(torus.center, axis, angle);
  result.axis = rotate_vector(torus.axis, axis, angle);
  return result;
}

// ============ PROJECTION ============

std::vector<Vector3> GeometryOps::project_cylinder_2d(
    const Cylinder& cyl,
    const Vector3& plane_normal) {

  std::vector<Vector3> projected;

  Vector3 p_normal = plane_normal.normalized();

  // Project cylinder endpoints
  Vector3 start = cyl.center - cyl.axis * (cyl.length / 2.0);
  Vector3 end = cyl.center + cyl.axis * (cyl.length / 2.0);

  // Simple projection (not full 2D, but 3D with plane component)
  Vector3 p_start = start - p_normal * start.dot(p_normal);
  Vector3 p_end = end - p_normal * end.dot(p_normal);

  projected.push_back(p_start);
  projected.push_back(p_end);

  return projected;
}

std::vector<Vector3> GeometryOps::project_torus_2d(
    const Torus& torus,
    const Vector3& plane_normal) {

  std::vector<Vector3> projected;

  Vector3 p_normal = plane_normal.normalized();

  // Project center
  Vector3 center = torus.center - p_normal * torus.center.dot(p_normal);
  projected.push_back(center);

  // Project major radius extent
  Vector3 extent = center + torus.axis * (torus.major_radius + torus.minor_radius);
  extent = extent - p_normal * extent.dot(p_normal);
  projected.push_back(extent);

  return projected;
}

}  // namespace bendcore::tube
