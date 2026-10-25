#include <cassert>
#include <iostream>
#include <cmath>

#include "bendcore/geometry/geometry_kernel.hpp"

using namespace bendcore::geometry;

int main() {
    std::cout << "Testing GeometryKernel...\n";

    // Test 1: Point3D creation and distance
    {
        Point3D p1(0, 0, 0);
        Point3D p2(3, 4, 0);
        assert(p1.distance_to(p2) == 5.0);
        std::cout << "✓ test_point_distance\n";
    }

    // Test 2: Point3D equality
    {
        Point3D p1(1.0, 2.0, 3.0);
        Point3D p2(1.0 + 1e-11, 2.0, 3.0);
        assert(p1.equals(p2, DEFAULT_TOLERANCE));
        std::cout << "✓ test_point_equals\n";
    }

    // Test 3: Vector3D magnitude
    {
        Vector3D v(3, 4, 0);
        assert(v.magnitude() == 5.0);
        std::cout << "✓ test_vector_magnitude\n";
    }

    // Test 4: Vector3D normalization
    {
        Vector3D v(3, 4, 0);
        Vector3D vn = v.normalized();
        assert(std::abs(vn.magnitude() - 1.0) < 1e-10);
        std::cout << "✓ test_vector_normalize\n";
    }

    // Test 5: Vector3D dot product
    {
        Vector3D v1(1, 0, 0);
        Vector3D v2(0, 1, 0);
        assert(v1.dot(v2) == 0.0);
        Vector3D v3(1, 0, 0);
        assert(v1.dot(v3) == 1.0);
        std::cout << "✓ test_vector_dot\n";
    }

    // Test 6: Vector3D cross product
    {
        Vector3D v1(1, 0, 0);
        Vector3D v2(0, 1, 0);
        Vector3D v3 = v1.cross(v2);
        assert(v3.z == 1.0);
        std::cout << "✓ test_vector_cross\n";
    }

    // Test 7: Vector3D angle
    {
        Vector3D v1(1, 0, 0);
        Vector3D v2(0, 1, 0);
        double angle = v1.angle_to(v2);
        assert(std::abs(angle - M_PI / 2.0) < 1e-10);
        std::cout << "✓ test_vector_angle\n";
    }

    // Test 8: Vector3D perpendicularity
    {
        Vector3D v1(1, 0, 0);
        Vector3D v2(0, 1, 0);
        assert(v1.is_perpendicular_to(v2));
        std::cout << "✓ test_vector_perpendicular\n";
    }

    // Test 9: Vector3D parallelism
    {
        Vector3D v1(1, 2, 3);
        Vector3D v2(2, 4, 6);
        assert(v1.is_parallel_to(v2));
        std::cout << "✓ test_vector_parallel\n";
    }

    // Test 10: Plane creation and distance to point
    {
        Plane plane(Point3D(0, 0, 0), Vector3D(0, 0, 1));
        Point3D p(0, 0, 5);
        assert(plane.distance_to_point(p) == 5.0);
        std::cout << "✓ test_plane_distance\n";
    }

    // Test 11: Plane point projection
    {
        Plane plane(Point3D(0, 0, 0), Vector3D(0, 0, 1));
        Point3D p(1, 2, 5);
        Point3D proj = plane.project_point(p);
        assert(proj.equals(Point3D(1, 2, 0)));
        std::cout << "✓ test_plane_projection\n";
    }

    // Test 12: Plane contains point
    {
        Plane plane(Point3D(0, 0, 0), Vector3D(0, 0, 1));
        Point3D p(1, 2, 0);
        assert(plane.contains_point(p));
        std::cout << "✓ test_plane_contains\n";
    }

    // Test 13: Transform identity
    {
        Transform t = Transform::identity();
        Point3D p(1, 2, 3);
        Point3D tp = t.apply_to_point(p);
        assert(p.equals(tp));
        std::cout << "✓ test_transform_identity\n";
    }

    // Test 14: Transform translation
    {
        Transform t = Transform::translation(1, 2, 3);
        Point3D p(1, 1, 1);
        Point3D tp = t.apply_to_point(p);
        assert(tp.equals(Point3D(2, 3, 4)));
        std::cout << "✓ test_transform_translation\n";
    }

    // Test 15: Transform rotation Z
    {
        Transform t = Transform::rotation_z(M_PI / 2);
        Vector3D v(1, 0, 0);
        Vector3D tv = t.apply_to_vector(v);
        assert(std::abs(tv.x) < 1e-10 && std::abs(tv.y - 1.0) < 1e-10);
        std::cout << "✓ test_transform_rotation_z\n";
    }

    // Test 16: Transform composition
    {
        Transform t1 = Transform::translation(1, 0, 0);
        Transform t2 = Transform::translation(1, 0, 0);
        Transform t3 = t1.compose(t2);
        Point3D p(0, 0, 0);
        Point3D tp = t3.apply_to_point(p);
        assert(tp.equals(Point3D(2, 0, 0)));
        std::cout << "✓ test_transform_compose\n";
    }

    // Test 17: Line evaluation
    {
        Line line(Point3D(0, 0, 0), Vector3D(1, 0, 0));
        Point3D p = line.evaluate(5);
        assert(p.equals(Point3D(5, 0, 0)));
        std::cout << "✓ test_line_evaluate\n";
    }

    // Test 18: Line closest point
    {
        Line line(Point3D(0, 0, 0), Vector3D(1, 0, 0));
        Point3D p(0, 5, 0);
        Point3D closest = line.closest_point_to(p);
        assert(closest.equals(Point3D(0, 0, 0)));
        std::cout << "✓ test_line_closest_point\n";
    }

    // Test 19: Line distance to point
    {
        Line line(Point3D(0, 0, 0), Vector3D(1, 0, 0));
        Point3D p(0, 3, 4);
        double dist = line.distance_to_point(p);
        assert(std::abs(dist - 5.0) < 1e-10);
        std::cout << "✓ test_line_distance\n";
    }

    // Test 20: Circle evaluation
    {
        Circle circle(Point3D(0, 0, 0), 1.0, Vector3D(0, 0, 1));
        Point3D p = circle.evaluate(0);
        assert(p.distance_to(Point3D(0, 0, 0)) == 1.0);
        std::cout << "✓ test_circle_evaluate\n";
    }

    // Test 21: Circle circumference
    {
        Circle circle(Point3D(0, 0, 0), 1.0, Vector3D(0, 0, 1));
        double circ = circle.circumference();
        assert(std::abs(circ - 2.0 * M_PI) < 1e-10);
        std::cout << "✓ test_circle_circumference\n";
    }

    // Test 22: Circle area
    {
        Circle circle(Point3D(0, 0, 0), 1.0, Vector3D(0, 0, 1));
        double area = circle.area();
        assert(std::abs(area - M_PI) < 1e-10);
        std::cout << "✓ test_circle_area\n";
    }

    // Test 23: Arc evaluation
    {
        Arc arc(Point3D(0, 0, 0), 1.0, Vector3D(0, 0, 1), 0, M_PI / 2);
        Point3D p = arc.evaluate(0);
        assert(p.distance_to(Point3D(0, 0, 0)) == 1.0);
        std::cout << "✓ test_arc_evaluate\n";
    }

    // Test 24: Arc length
    {
        Arc arc(Point3D(0, 0, 0), 1.0, Vector3D(0, 0, 1), 0, M_PI / 2);
        double length = arc.arc_length();
        assert(std::abs(length - M_PI / 2.0) < 1e-10);
        std::cout << "✓ test_arc_length\n";
    }

    // Test 25: GeometryKernel instance
    {
        GeometryKernel kernel(1e-10);
        Point3D p1(0, 0, 0);
        Point3D p2(1, 1, 1);
        double dist = kernel.point_distance(p1, p2);
        assert(std::abs(dist - std::sqrt(3.0)) < 1e-10);
        std::cout << "✓ test_geometry_kernel\n";
    }

    // Test 26: Line-line intersection
    {
        GeometryKernel kernel(1e-10);
        Line l1(Point3D(0, 0, 0), Vector3D(1, 0, 0));
        Line l2(Point3D(0, 1, 0), Vector3D(0, 1, 0));
        auto result = kernel.line_line_intersection(l1, l2);
        assert(!result.intersects);  // Parallel lines
        std::cout << "✓ test_line_line_intersection\n";
    }

    // Test 27: Line-circle intersection
    {
        GeometryKernel kernel(1e-10);
        Line line(Point3D(-2, 0, 0), Vector3D(1, 0, 0));
        Circle circle(Point3D(0, 0, 0), 1.0, Vector3D(0, 0, 1));
        auto result = kernel.line_circle_intersection(line, circle);
        assert(result.intersects);
        assert(result.points.size() == 2);
        std::cout << "✓ test_line_circle_intersection\n";
    }

    // Test 28: Plane-line intersection
    {
        GeometryKernel kernel(1e-10);
        Plane plane(Point3D(0, 0, 0), Vector3D(0, 0, 1));
        Line line(Point3D(0, 0, -1), Vector3D(0, 0, 1));
        auto result = kernel.plane_line_intersection(plane, line);
        assert(result.intersects);
        std::cout << "✓ test_plane_line_intersection\n";
    }

    std::cout << "\nAll tests passed!\n";
    return 0;
}
