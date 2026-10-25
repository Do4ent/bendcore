#pragma once

#include <vector>
#include <memory>
#include <cmath>

namespace bendcore::math {

/**
 * Point and Vector types
 */
struct Point3D {
    double x, y, z;

    Point3D() : x(0), y(0), z(0) {}
    Point3D(double x_, double y_, double z_) : x(x_), y(y_), z(z_) {}

    double distance_to(const Point3D& other) const noexcept;
    double length() const noexcept;
};

struct Vector3D {
    double x, y, z;

    Vector3D() : x(0), y(0), z(0) {}
    Vector3D(double x_, double y_, double z_) : x(x_), y(y_), z(z_) {}
    Vector3D(const Point3D& from, const Point3D& to)
        : x(to.x - from.x), y(to.y - from.y), z(to.z - from.z) {}

    double length() const noexcept;
    Vector3D normalized() const;
    double dot(const Vector3D& other) const noexcept;
    Vector3D cross(const Vector3D& other) const noexcept;
};

/**
 * Geometry Operations wrapper around CGAL library
 * Provides exact arithmetic and advanced geometric operations
 */
class GeometryOps {
public:
    /**
     * Check if point is inside polygon (2D)
     */
    static bool point_in_polygon_2d(const Point3D& point, const std::vector<Point3D>& polygon);

    /**
     * Calculate area of triangle
     */
    static double triangle_area(const Point3D& p1, const Point3D& p2, const Point3D& p3) noexcept;

    /**
     * Calculate area of polygon
     */
    static double polygon_area(const std::vector<Point3D>& polygon);

    /**
     * Convex hull 2D
     */
    static std::vector<Point3D> convex_hull_2d(const std::vector<Point3D>& points);

    /**
     * Convex hull 3D
     */
    static std::vector<Point3D> convex_hull_3d(const std::vector<Point3D>& points);

    /**
     * Delaunay triangulation 2D
     */
    static std::vector<std::vector<size_t>> delaunay_triangulation_2d(
        const std::vector<Point3D>& points);

    /**
     * Delaunay triangulation 3D
     */
    static std::vector<std::vector<size_t>> delaunay_triangulation_3d(
        const std::vector<Point3D>& points);

    /**
     * Distance from point to line segment
     */
    static double point_to_segment_distance(
        const Point3D& point,
        const Point3D& seg_start,
        const Point3D& seg_end) noexcept;

    /**
     * Find closest point on line segment to a point
     */
    static Point3D closest_point_on_segment(
        const Point3D& point,
        const Point3D& seg_start,
        const Point3D& seg_end) noexcept;

    /**
     * Check if two line segments intersect (3D)
     */
    static bool segments_intersect_3d(
        const Point3D& p1, const Point3D& p2,
        const Point3D& p3, const Point3D& p4);

    /**
     * Find intersection point of two line segments (3D)
     */
    static bool segments_intersection_3d(
        const Point3D& p1, const Point3D& p2,
        const Point3D& p3, const Point3D& p4,
        Point3D& intersection);

    /**
     * Polygon offset (Minkowski sum approach)
     */
    static std::vector<Point3D> polygon_offset(
        const std::vector<Point3D>& polygon,
        double offset_distance);

    /**
     * Boolean union of two polygons
     */
    static std::vector<Point3D> polygon_union(
        const std::vector<Point3D>& poly1,
        const std::vector<Point3D>& poly2);

    /**
     * Boolean intersection of two polygons
     */
    static std::vector<Point3D> polygon_intersection(
        const std::vector<Point3D>& poly1,
        const std::vector<Point3D>& poly2);

    /**
     * Boolean difference of two polygons
     */
    static std::vector<Point3D> polygon_difference(
        const std::vector<Point3D>& poly1,
        const std::vector<Point3D>& poly2);

    /**
     * Check if polygon is simple (non-self-intersecting)
     */
    static bool is_polygon_simple(const std::vector<Point3D>& polygon);

    /**
     * Calculate centroid of polygon
     */
    static Point3D polygon_centroid(const std::vector<Point3D>& polygon) noexcept;

    /**
     * Check orientation (CCW/CW) of 2D polygon
     */
    static bool is_counterclockwise(const std::vector<Point3D>& polygon) noexcept;

    /**
     * Triangulate polygon (fan triangulation)
     */
    static std::vector<std::vector<size_t>> triangulate_polygon(
        const std::vector<Point3D>& polygon);
};

} // namespace bendcore::math
