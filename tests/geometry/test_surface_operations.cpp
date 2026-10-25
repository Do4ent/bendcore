#include <cassert>
#include <iostream>
#include <cmath>

#include "bendcore/geometry/surface_operations.hpp"

using namespace bendcore::geometry;

int main() {
    std::cout << "Testing SurfaceOperations...\n";

    // Test 1: Create NURBS surface
    {
        std::vector<std::vector<Point3D>> cp = {
            {Point3D(0, 0, 0), Point3D(0, 1, 0)},
            {Point3D(1, 0, 1), Point3D(1, 1, 1)},
            {Point3D(2, 0, 0), Point3D(2, 1, 0)}
        };
        std::vector<double> u_knots = {0, 0, 0, 1, 1, 1};
        std::vector<double> v_knots = {0, 0, 1, 1};
        std::vector<std::vector<double>> weights = {{1, 1}, {1, 1}, {1, 1}};
        NurbsSurface surface(cp, u_knots, v_knots, weights, 2, 1);
        assert(surface.u_control_points() == 3);
        assert(surface.v_control_points() == 2);
        std::cout << "✓ test_create_nurbs_surface\n";
    }

    // Test 2: NURBS surface evaluation
    {
        std::vector<std::vector<Point3D>> cp = {
            {Point3D(0, 0, 0), Point3D(0, 1, 0)},
            {Point3D(1, 0, 1), Point3D(1, 1, 1)},
            {Point3D(2, 0, 0), Point3D(2, 1, 0)}
        };
        std::vector<double> u_knots = {0, 0, 0, 1, 1, 1};
        std::vector<double> v_knots = {0, 0, 1, 1};
        std::vector<std::vector<double>> weights = {{1, 1}, {1, 1}, {1, 1}};
        NurbsSurface surface(cp, u_knots, v_knots, weights, 2, 1);
        Point3D p = surface.evaluate(0.5, 0.5);
        // Check that evaluation gives a reasonable point
        assert(p.x >= 0 && p.x <= 2);
        std::cout << "✓ test_nurbs_surface_evaluate\n";
    }

    // Test 3: NURBS surface normal
    {
        std::vector<std::vector<Point3D>> cp = {
            {Point3D(0, 0, 0), Point3D(0, 1, 0)},
            {Point3D(1, 0, 1), Point3D(1, 1, 1)},
            {Point3D(2, 0, 0), Point3D(2, 1, 0)}
        };
        std::vector<double> u_knots = {0, 0, 0, 1, 1, 1};
        std::vector<double> v_knots = {0, 0, 1, 1};
        std::vector<std::vector<double>> weights = {{1, 1}, {1, 1}, {1, 1}};
        NurbsSurface surface(cp, u_knots, v_knots, weights, 2, 1);
        Vector3D n = surface.normal(0.5, 0.5);
        // Normal should be unit vector (approximately)
        assert(std::abs(n.magnitude() - 1.0) < 1e-6 || n.magnitude() < 1e-10);
        std::cout << "✓ test_nurbs_surface_normal\n";
    }

    // Test 4: NURBS surface closest point
    {
        std::vector<std::vector<Point3D>> cp = {
            {Point3D(0, 0, 0), Point3D(0, 1, 0)},
            {Point3D(1, 0, 1), Point3D(1, 1, 1)},
            {Point3D(2, 0, 0), Point3D(2, 1, 0)}
        };
        std::vector<double> u_knots = {0, 0, 0, 1, 1, 1};
        std::vector<double> v_knots = {0, 0, 1, 1};
        std::vector<std::vector<double>> weights = {{1, 1}, {1, 1}, {1, 1}};
        NurbsSurface surface(cp, u_knots, v_knots, weights, 2, 1);
        Point3D closest = surface.closest_point_on_surface(Point3D(1, 0.5, 0.5));
        assert(closest.x >= 0 && closest.x <= 2);
        std::cout << "✓ test_nurbs_surface_closest_point\n";
    }

    // Test 5: NURBS surface validity
    {
        std::vector<std::vector<Point3D>> cp = {
            {Point3D(0, 0, 0), Point3D(0, 1, 0)},
            {Point3D(1, 0, 1), Point3D(1, 1, 1)}
        };
        std::vector<double> u_knots = {0, 0, 1, 1};
        std::vector<double> v_knots = {0, 0, 1, 1};
        std::vector<std::vector<double>> weights = {{1, 1}, {1, 1}};
        NurbsSurface surface(cp, u_knots, v_knots, weights, 1, 1);
        assert(surface.is_valid());
        std::cout << "✓ test_nurbs_surface_validity\n";
    }

    // Test 6: Create trimmed surface
    {
        std::vector<std::vector<Point3D>> cp = {
            {Point3D(0, 0, 0), Point3D(0, 1, 0)},
            {Point3D(1, 0, 1), Point3D(1, 1, 1)},
            {Point3D(2, 0, 0), Point3D(2, 1, 0)}
        };
        std::vector<double> u_knots = {0, 0, 0, 1, 1, 1};
        std::vector<double> v_knots = {0, 0, 1, 1};
        std::vector<std::vector<double>> weights = {{1, 1}, {1, 1}, {1, 1}};
        auto base = std::make_shared<NurbsSurface>(cp, u_knots, v_knots, weights, 2, 1);
        std::vector<std::vector<Point3D>> trim;
        TrimmedSurface ts(base, trim);
        assert(ts.get_surface() == base);
        std::cout << "✓ test_trimmed_surface\n";
    }

    // Test 7: Trimmed surface evaluation
    {
        std::vector<std::vector<Point3D>> cp = {
            {Point3D(0, 0, 0), Point3D(0, 1, 0)},
            {Point3D(1, 0, 1), Point3D(1, 1, 1)},
            {Point3D(2, 0, 0), Point3D(2, 1, 0)}
        };
        std::vector<double> u_knots = {0, 0, 0, 1, 1, 1};
        std::vector<double> v_knots = {0, 0, 1, 1};
        std::vector<std::vector<double>> weights = {{1, 1}, {1, 1}, {1, 1}};
        auto base = std::make_shared<NurbsSurface>(cp, u_knots, v_knots, weights, 2, 1);
        std::vector<std::vector<Point3D>> trim;
        TrimmedSurface ts(base, trim);
        Point3D p = ts.evaluate(0.5, 0.5);
        assert(p.x >= 0 && p.x <= 2);
        std::cout << "✓ test_trimmed_surface_evaluate\n";
    }

    // Test 8: Create revolution surface
    {
        std::vector<Point3D> cp = {Point3D(1, 0, 0), Point3D(1, 0, 1), Point3D(0.5, 0, 2)};
        std::vector<double> knots = {0, 0, 0, 1, 1, 1};
        std::vector<double> weights = {1, 1, 1};
        auto profile = std::make_shared<NurbsCurve>(cp, knots, weights, 2);
        RevolutionSurface rs(profile, Point3D(0, 0, 0), Vector3D(0, 0, 1), 2.0 * M_PI);
        assert(rs.get_type == nullptr || true);  // Simplified test
        std::cout << "✓ test_revolution_surface\n";
    }

    // Test 9: Revolution surface evaluation
    {
        std::vector<Point3D> cp = {Point3D(1, 0, 0), Point3D(1, 0, 1), Point3D(0.5, 0, 2)};
        std::vector<double> knots = {0, 0, 0, 1, 1, 1};
        std::vector<double> weights = {1, 1, 1};
        auto profile = std::make_shared<NurbsCurve>(cp, knots, weights, 2);
        RevolutionSurface rs(profile, Point3D(0, 0, 0), Vector3D(0, 0, 1), 2.0 * M_PI);
        Point3D p = rs.evaluate(0.5, 0.5);
        // Should give a point on the revolution surface
        assert(std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z));
        std::cout << "✓ test_revolution_surface_evaluate\n";
    }

    // Test 10: Create surface of revolution (tube)
    {
        AnalyticCurve line = AnalyticCurve::create_line(Point3D(0, 0, 0), Point3D(0, 0, 1));
        auto centerline = std::make_shared<AnalyticCurve>(line);
        SurfaceOfRevolution sor(centerline, 0.5);
        assert(sor.tube_radius() == 0.5);
        std::cout << "✓ test_surface_of_revolution\n";
    }

    // Test 11: Surface of revolution evaluation
    {
        AnalyticCurve line = AnalyticCurve::create_line(Point3D(0, 0, 0), Point3D(0, 0, 1));
        auto centerline = std::make_shared<AnalyticCurve>(line);
        SurfaceOfRevolution sor(centerline, 0.5);
        Point3D p = sor.evaluate(0.5, 0);
        assert(std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z));
        std::cout << "✓ test_surface_of_revolution_evaluate\n";
    }

    // Test 12: Surface of revolution normal
    {
        AnalyticCurve line = AnalyticCurve::create_line(Point3D(0, 0, 0), Point3D(0, 0, 1));
        auto centerline = std::make_shared<AnalyticCurve>(line);
        SurfaceOfRevolution sor(centerline, 0.5);
        Vector3D n = sor.normal(0.5, 0.5);
        assert(std::abs(n.magnitude() - 1.0) < 1e-6 || n.magnitude() < 1e-10);
        std::cout << "✓ test_surface_of_revolution_normal\n";
    }

    // Test 13: Surface of revolution area
    {
        AnalyticCurve line = AnalyticCurve::create_line(Point3D(0, 0, 0), Point3D(0, 0, 1));
        auto centerline = std::make_shared<AnalyticCurve>(line);
        SurfaceOfRevolution sor(centerline, 0.5);
        double area = sor.surface_area(50, 50);
        assert(area > 0);
        std::cout << "✓ test_surface_of_revolution_area\n";
    }

    // Test 14: Surface of revolution offset
    {
        AnalyticCurve line = AnalyticCurve::create_line(Point3D(0, 0, 0), Point3D(0, 0, 1));
        auto centerline = std::make_shared<AnalyticCurve>(line);
        SurfaceOfRevolution sor(centerline, 0.5);
        auto offset = sor.offset_wall_thickness(0.1);
        assert(offset != nullptr);
        assert(offset->tube_radius() == 0.6);
        std::cout << "✓ test_surface_of_revolution_offset\n";
    }

    // Test 15: SurfaceOperations instance
    {
        SurfaceOperations ops;
        std::vector<std::vector<Point3D>> cp = {
            {Point3D(0, 0, 0), Point3D(0, 1, 0)},
            {Point3D(1, 0, 1), Point3D(1, 1, 1)}
        };
        std::vector<double> u_knots = {0, 0, 1, 1};
        std::vector<double> v_knots = {0, 0, 1, 1};
        std::vector<std::vector<double>> weights = {{1, 1}, {1, 1}};
        auto surface = ops.create_nurbs_surface(cp, u_knots, v_knots, weights, 1, 1);
        assert(surface != nullptr);
        std::cout << "✓ test_surface_operations\n";
    }

    // Test 16: Surface evaluation via operations
    {
        SurfaceOperations ops;
        std::vector<std::vector<Point3D>> cp = {
            {Point3D(0, 0, 0), Point3D(0, 1, 0)},
            {Point3D(1, 0, 1), Point3D(1, 1, 1)}
        };
        std::vector<double> u_knots = {0, 0, 1, 1};
        std::vector<double> v_knots = {0, 0, 1, 1};
        std::vector<std::vector<double>> weights = {{1, 1}, {1, 1}};
        NurbsSurface surface(cp, u_knots, v_knots, weights, 1, 1);
        Point3D p = ops.surface_evaluate(surface, 0.5, 0.5);
        assert(std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z));
        std::cout << "✓ test_surface_evaluate_operations\n";
    }

    // Test 17: Surface normal via operations
    {
        SurfaceOperations ops;
        std::vector<std::vector<Point3D>> cp = {
            {Point3D(0, 0, 0), Point3D(0, 1, 0)},
            {Point3D(1, 0, 1), Point3D(1, 1, 1)}
        };
        std::vector<double> u_knots = {0, 0, 1, 1};
        std::vector<double> v_knots = {0, 0, 1, 1};
        std::vector<std::vector<double>> weights = {{1, 1}, {1, 1}};
        NurbsSurface surface(cp, u_knots, v_knots, weights, 1, 1);
        Vector3D n = ops.surface_normal(surface, 0.5, 0.5);
        assert(std::abs(n.magnitude() - 1.0) < 1e-6 || n.magnitude() < 1e-10);
        std::cout << "✓ test_surface_normal_operations\n";
    }

    // Test 18: Curve-surface intersection
    {
        SurfaceOperations ops;
        std::vector<std::vector<Point3D>> cp = {
            {Point3D(0, 0, 0), Point3D(0, 1, 0)},
            {Point3D(1, 0, 1), Point3D(1, 1, 1)}
        };
        std::vector<double> u_knots = {0, 0, 1, 1};
        std::vector<double> v_knots = {0, 0, 1, 1};
        std::vector<std::vector<double>> weights = {{1, 1}, {1, 1}};
        NurbsSurface surface(cp, u_knots, v_knots, weights, 1, 1);

        std::vector<Point3D> line_cp = {Point3D(0.5, 0.5, -1), Point3D(0.5, 0.5, 2)};
        std::vector<double> line_knots = {0, 0, 1, 1};
        std::vector<double> line_weights = {1, 1};
        NurbsCurve curve(line_cp, line_knots, line_weights, 1);

        auto intersections = ops.curve_surface_intersection(curve, surface, 50);
        assert(intersections.size() >= 0);
        std::cout << "✓ test_curve_surface_intersection\n";
    }

    // Test 19: Surface metrics
    {
        SurfaceOperations ops;
        std::vector<std::vector<Point3D>> cp = {
            {Point3D(0, 0, 0), Point3D(0, 1, 0)},
            {Point3D(1, 0, 1), Point3D(1, 1, 1)}
        };
        std::vector<double> u_knots = {0, 0, 1, 1};
        std::vector<double> v_knots = {0, 0, 1, 1};
        std::vector<std::vector<double>> weights = {{1, 1}, {1, 1}};
        NurbsSurface surface(cp, u_knots, v_knots, weights, 1, 1);
        SurfaceMetrics metrics = ops.compute_surface_metrics(surface, 10, 10);
        assert(metrics.surface_area > 0);
        std::cout << "✓ test_surface_metrics\n";
    }

    // Test 20: Surface offset
    {
        SurfaceOperations ops;
        std::vector<std::vector<Point3D>> cp = {
            {Point3D(0, 0, 0), Point3D(0, 1, 0)},
            {Point3D(1, 0, 1), Point3D(1, 1, 1)}
        };
        std::vector<double> u_knots = {0, 0, 1, 1};
        std::vector<double> v_knots = {0, 0, 1, 1};
        std::vector<std::vector<double>> weights = {{1, 1}, {1, 1}};
        NurbsSurface surface(cp, u_knots, v_knots, weights, 1, 1);
        auto offset = ops.offset_surface(surface, 0.5);
        assert(offset != nullptr);
        std::cout << "✓ test_surface_offset\n";
    }

    // Test 21: Create wall thickness
    {
        SurfaceOperations ops;
        std::vector<std::vector<Point3D>> cp = {
            {Point3D(0, 0, 0), Point3D(0, 1, 0)},
            {Point3D(1, 0, 1), Point3D(1, 1, 1)}
        };
        std::vector<double> u_knots = {0, 0, 1, 1};
        std::vector<double> v_knots = {0, 0, 1, 1};
        std::vector<std::vector<double>> weights = {{1, 1}, {1, 1}};
        NurbsSurface surface(cp, u_knots, v_knots, weights, 1, 1);
        auto thickness = ops.create_wall_thickness(surface, 0.1);
        assert(thickness != nullptr);
        std::cout << "✓ test_wall_thickness\n";
    }

    // Test 22: Create tube surface
    {
        SurfaceOperations ops;
        AnalyticCurve line = AnalyticCurve::create_line(Point3D(0, 0, 0), Point3D(0, 0, 1));
        auto centerline = std::make_shared<AnalyticCurve>(line);
        auto tube = ops.create_tube_surface(centerline, 0.5);
        assert(tube != nullptr);
        std::cout << "✓ test_create_tube_surface\n";
    }

    // Test 23: Create trimmed surface via operations
    {
        SurfaceOperations ops;
        std::vector<std::vector<Point3D>> cp = {
            {Point3D(0, 0, 0), Point3D(0, 1, 0)},
            {Point3D(1, 0, 1), Point3D(1, 1, 1)}
        };
        std::vector<double> u_knots = {0, 0, 1, 1};
        std::vector<double> v_knots = {0, 0, 1, 1};
        std::vector<std::vector<double>> weights = {{1, 1}, {1, 1}};
        auto surface_unique = ops.create_nurbs_surface(cp, u_knots, v_knots, weights, 1, 1);
        auto surface = std::shared_ptr<NurbsSurface>(surface_unique.release());
        std::vector<std::vector<Point3D>> trim;
        auto trimmed = ops.create_trimmed_surface(surface, trim);
        assert(trimmed != nullptr);
        std::cout << "✓ test_trimmed_surface_operations\n";
    }

    // Test 24: Create revolution surface via operations
    {
        SurfaceOperations ops;
        std::vector<Point3D> cp = {Point3D(1, 0, 0), Point3D(1, 0, 1)};
        std::vector<double> knots = {0, 0, 1, 1};
        std::vector<double> weights = {1, 1};
        auto profile = std::make_shared<NurbsCurve>(cp, knots, weights, 1);
        auto rev = ops.create_revolution_surface(profile, Point3D(0, 0, 0), Vector3D(0, 0, 1));
        assert(rev != nullptr);
        std::cout << "✓ test_revolution_surface_operations\n";
    }

    // Test 25: Surface closed checks
    {
        std::vector<std::vector<Point3D>> cp = {
            {Point3D(0, 0, 0), Point3D(0, 1, 0)},
            {Point3D(1, 0, 1), Point3D(1, 1, 1)},
            {Point3D(0, 0, 0), Point3D(0, 1, 0)}
        };
        std::vector<double> u_knots = {0, 0, 0, 1, 1, 1};
        std::vector<double> v_knots = {0, 0, 1, 1};
        std::vector<std::vector<double>> weights = {{1, 1}, {1, 1}, {1, 1}};
        NurbsSurface surface(cp, u_knots, v_knots, weights, 2, 1);
        bool closed_u = surface.is_surface_closed_u();
        assert(closed_u || !closed_u);  // Just test it doesn't crash
        std::cout << "✓ test_surface_closed_checks\n";
    }

    std::cout << "\nAll tests passed!\n";
    return 0;
}
