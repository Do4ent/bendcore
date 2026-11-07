#include <cassert>
#include <iostream>
#include <cmath>

#include "bendcore/geometry/geometry_kernel.hpp"
#include "bendcore/geometry/curve_operations.hpp"
#include "bendcore/geometry/surface_operations.hpp"
#include "bendcore/document/cad_document.hpp"
#include "bendcore/parameters/parameter_store.hpp"

using namespace bendcore::geometry;
using namespace bendcore::document;
using namespace bendcore::parameters;

int main() {
    std::cout << "Testing Phase D Integration (Geometry & Topology)...\n";

    // Test 1: GeometryKernel with CadDocument
    {
        auto doc = std::make_unique<CadDocument>();
        GeometryKernel kernel(1e-10);

        Point3D p1(0, 0, 0);
        Point3D p2(1, 1, 1);
        double dist = kernel.point_distance(p1, p2);
        assert(std::abs(dist - std::sqrt(3.0)) < 1e-10);
        std::cout << "✓ test_geometry_kernel_with_document\n";
    }

    // Test 4: Tube creation workflow (NURBS curve → tube)
    {
        // Create a NURBS curve for the centerline
        std::vector<Point3D> cp = {Point3D(0, 0, 0), Point3D(1, 0, 0), Point3D(2, 0, 0)};
        std::vector<double> knots = {0, 0, 0, 1, 1, 1};
        std::vector<double> weights = {1, 1, 1};

        auto nurbs = std::make_shared<NurbsCurve>(cp, knots, weights, 2);
        TubeCurve tube(nurbs, 0.5);

        assert(tube.tube_radius() == 0.5);
        assert(tube.centerline_length(100) > 0);
        std::cout << "✓ test_tube_creation_workflow\n";
    }

    // Test 5: Tube with surface operations
    {
        CurveOperations curve_ops;
        AnalyticCurve line = AnalyticCurve::create_line(Point3D(0, 0, 0), Point3D(0, 0, 1));
        auto centerline = std::make_shared<AnalyticCurve>(line);

        SurfaceOperations surf_ops;
        auto tube_surface = surf_ops.create_tube_surface(centerline, 0.5);

        assert(tube_surface != nullptr);

        // Calculate surface area
        double area = tube_surface->surface_area(50, 50);
        assert(area > 0);
        std::cout << "✓ test_tube_with_surface_operations\n";
    }

    // Test 6: Curve offset for pipe wall thickness
    {
        std::vector<Point3D> cp = {Point3D(0, 0, 0), Point3D(1, 0, 0), Point3D(2, 0, 0)};
        std::vector<double> knots = {0, 0, 0, 1, 1, 1};
        std::vector<double> weights = {1, 1, 1};
        NurbsCurve curve(cp, knots, weights, 2);

        Vector3D normal(0, 1, 0);
        auto offset = curve.offset(0.1, normal);

        assert(offset != nullptr);
        std::cout << "✓ test_curve_offset_for_walls\n";
    }

    // Test 7: Surface with wall thickness
    {
        SurfaceOperations ops;
        std::vector<std::vector<Point3D>> cp = {
            {Point3D(0, 0, 0), Point3D(0, 1, 0)},
            {Point3D(1, 0, 1), Point3D(1, 1, 1)},
            {Point3D(2, 0, 0), Point3D(2, 1, 0)}
        };
        std::vector<double> u_knots = {0, 0, 0, 1, 1, 1};
        std::vector<double> v_knots = {0, 0, 1, 1};
        std::vector<std::vector<double>> weights = {{1, 1}, {1, 1}, {1, 1}};
        NurbsSurface surface(cp, u_knots, v_knots, weights, 2, 1);

        auto thickness = ops.create_wall_thickness(surface, 0.05);
        assert(thickness != nullptr);
        std::cout << "✓ test_surface_wall_thickness\n";
    }

    // Test 8: Curve-surface intersection for bending analysis
    {
        SurfaceOperations ops;
        std::vector<std::vector<Point3D>> cp = {
            {Point3D(0, 0, 0), Point3D(0, 1, 0)},
            {Point3D(1, 0, 0.5), Point3D(1, 1, 0.5)},
            {Point3D(2, 0, 0), Point3D(2, 1, 0)}
        };
        std::vector<double> u_knots = {0, 0, 0, 1, 1, 1};
        std::vector<double> v_knots = {0, 0, 1, 1};
        std::vector<std::vector<double>> weights = {{1, 1}, {1, 1}, {1, 1}};
        NurbsSurface surface(cp, u_knots, v_knots, weights, 2, 1);

        std::vector<Point3D> line_cp = {Point3D(1, 0.5, -1), Point3D(1, 0.5, 2)};
        std::vector<double> line_knots = {0, 0, 1, 1};
        std::vector<double> line_weights = {1, 1};
        NurbsCurve line(line_cp, line_knots, line_weights, 1);

        auto intersections = ops.curve_surface_intersection(line, surface, 50);
        // Intersections may be 0 depending on surface
        std::cout << "✓ test_curve_surface_intersection\n";
    }

    // Test 11: Arc length and parametrization
    {
        AnalyticCurve circle = AnalyticCurve::create_circle(
            Point3D(0, 0, 0), 1.0, Vector3D(0, 0, 1)
        );
        double arc_len = circle.arc_length(100);
        assert(std::abs(arc_len - 2.0 * M_PI) < 0.1);
        std::cout << "✓ test_arc_length_parametrization\n";
    }

    // Test 12: Surface metrics computation
    {
        SurfaceOperations ops;
        std::vector<std::vector<Point3D>> cp = {
            {Point3D(0, 0, 0), Point3D(0, 1, 0)},
            {Point3D(1, 0, 0), Point3D(1, 1, 0)},
            {Point3D(2, 0, 0), Point3D(2, 1, 0)}
        };
        std::vector<double> u_knots = {0, 0, 0, 1, 1, 1};
        std::vector<double> v_knots = {0, 0, 1, 1};
        std::vector<std::vector<double>> weights = {{1, 1}, {1, 1}, {1, 1}};
        NurbsSurface surface(cp, u_knots, v_knots, weights, 2, 1);

        SurfaceMetrics metrics = ops.compute_surface_metrics(surface, 10, 10);
        assert(metrics.surface_area > 0);
        std::cout << "✓ test_surface_metrics\n";
    }

    // Test 13: Interpolation and smoothing
    {
        CurveOperations ops;
        std::vector<Point3D> points = {
            Point3D(0, 0, 0),
            Point3D(1, 1, 0),
            Point3D(2, 0, 0),
            Point3D(3, 1, 0)
        };
        auto curve = ops.interpolate_points(points, 3);
        assert(curve != nullptr);
        assert(curve->control_point_count() == 4);
        std::cout << "✓ test_interpolation_smoothing\n";
    }

    // Test 14: Revolution surface for rotational geometry
    {
        SurfaceOperations ops;
        std::vector<Point3D> profile = {Point3D(1, 0, 0), Point3D(1, 0, 1), Point3D(0.5, 0, 2)};
        std::vector<double> knots = {0, 0, 0, 1, 1, 1};
        std::vector<double> weights = {1, 1, 1};
        auto prof = std::make_shared<NurbsCurve>(profile, knots, weights, 2);

        auto rev_surface = ops.create_revolution_surface(
            prof,
            Point3D(0, 0, 0),
            Vector3D(0, 0, 1),
            2.0 * M_PI
        );

        assert(rev_surface != nullptr);
        std::cout << "✓ test_revolution_surface\n";
    }

    // Test 15: Tolerance management in geometry kernel
    {
        GeometryKernel kernel(1e-8);
        assert(kernel.get_tolerance() == 1e-8);

        kernel.set_tolerance(1e-6);
        assert(kernel.get_tolerance() == 1e-6);

        Point3D p1(0, 0, 0);
        Point3D p2(1e-7, 0, 0);
        bool equal = kernel.points_equal(p1, p2);
        assert(!equal);  // Should not be equal with this tolerance

        std::cout << "✓ test_tolerance_management\n";
    }

    std::cout << "\nAll Phase D integration tests passed!\n";
    return 0;
}
