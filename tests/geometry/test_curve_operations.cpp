#include <cassert>
#include <iostream>
#include <cmath>

#include "bendcore/geometry/curve_operations.hpp"

using namespace bendcore::geometry;

int main() {
    std::cout << "Testing CurveOperations...\n";

    // Test 1: Create NURBS curve
    {
        std::vector<Point3D> cp = {Point3D(0, 0, 0), Point3D(1, 1, 0), Point3D(2, 0, 0)};
        std::vector<double> knots = {0, 0, 0, 1, 1, 1};
        std::vector<double> weights = {1, 1, 1};
        NurbsCurve curve(cp, knots, weights, 2);
        assert(curve.control_point_count() == 3);
        assert(curve.degree() == 2);
        std::cout << "✓ test_create_nurbs_curve\n";
    }

    // Test 2: NURBS curve evaluation
    {
        std::vector<Point3D> cp = {Point3D(0, 0, 0), Point3D(1, 1, 0), Point3D(2, 0, 0)};
        std::vector<double> knots = {0, 0, 0, 1, 1, 1};
        std::vector<double> weights = {1, 1, 1};
        NurbsCurve curve(cp, knots, weights, 2);
        Point3D p = curve.evaluate(0.5);
        // Check that evaluation gives a reasonable point
        assert(p.x >= 0 && p.x <= 2);
        std::cout << "✓ test_nurbs_evaluate\n";
    }

    // Test 3: NURBS curve tangent
    {
        std::vector<Point3D> cp = {Point3D(0, 0, 0), Point3D(1, 0, 0), Point3D(2, 0, 0)};
        std::vector<double> knots = {0, 0, 0, 1, 1, 1};
        std::vector<double> weights = {1, 1, 1};
        NurbsCurve curve(cp, knots, weights, 2);
        Vector3D t = curve.tangent(0.5);
        // Tangent should be normalized
        assert(std::abs(t.magnitude() - 1.0) < 1e-6 || t.magnitude() < 1e-10);
        std::cout << "✓ test_nurbs_tangent\n";
    }

    // Test 4: NURBS curve arc length
    {
        std::vector<Point3D> cp = {Point3D(0, 0, 0), Point3D(1, 0, 0), Point3D(2, 0, 0)};
        std::vector<double> knots = {0, 0, 0, 1, 1, 1};
        std::vector<double> weights = {1, 1, 1};
        NurbsCurve curve(cp, knots, weights, 2);
        double length = curve.total_arc_length(100);
        // Length should be positive and reasonable
        assert(length > 0 && length < 10);
        std::cout << "✓ test_nurbs_arc_length\n";
    }

    // Test 5: NURBS curve validity
    {
        std::vector<Point3D> cp = {Point3D(0, 0, 0), Point3D(1, 0, 0)};
        std::vector<double> knots = {0, 0, 1, 1};
        std::vector<double> weights = {1, 1};
        NurbsCurve curve(cp, knots, weights, 1);
        assert(curve.is_valid());
        std::cout << "✓ test_nurbs_validity\n";
    }

    // Test 6: Bezier curve creation
    {
        std::vector<Point3D> cp = {Point3D(0, 0, 0), Point3D(1, 1, 0), Point3D(2, 0, 0)};
        BezierCurve curve(cp);
        assert(curve.control_point_count() == 3);
        std::cout << "✓ test_bezier_creation\n";
    }

    // Test 7: Bezier curve evaluation
    {
        std::vector<Point3D> cp = {Point3D(0, 0, 0), Point3D(1, 1, 0), Point3D(2, 0, 0)};
        BezierCurve curve(cp);
        Point3D p0 = curve.evaluate(0);
        Point3D p1 = curve.evaluate(1);
        assert(p0.equals(cp[0]));
        assert(p1.equals(cp[2]));
        std::cout << "✓ test_bezier_evaluate\n";
    }

    // Test 8: Bezier curve tangent
    {
        std::vector<Point3D> cp = {Point3D(0, 0, 0), Point3D(1, 0, 0), Point3D(2, 0, 0)};
        BezierCurve curve(cp);
        Vector3D t = curve.tangent(0.5);
        assert(std::abs(t.magnitude() - 1.0) < 1e-6 || t.magnitude() < 1e-10);
        std::cout << "✓ test_bezier_tangent\n";
    }

    // Test 9: Bezier curve arc length
    {
        std::vector<Point3D> cp = {Point3D(0, 0, 0), Point3D(1, 0, 0), Point3D(2, 0, 0)};
        BezierCurve curve(cp);
        double length = curve.arc_length(100);
        assert(length > 0 && length <= 2.5);
        std::cout << "✓ test_bezier_arc_length\n";
    }

    // Test 10: Create line curve
    {
        AnalyticCurve line = AnalyticCurve::create_line(Point3D(0, 0, 0), Point3D(1, 0, 0));
        assert(line.get_type() == AnalyticCurve::Type::Line);
        std::cout << "✓ test_analytic_line\n";
    }

    // Test 11: Create circle curve
    {
        AnalyticCurve circle = AnalyticCurve::create_circle(Point3D(0, 0, 0), 1.0, Vector3D(0, 0, 1));
        assert(circle.get_type() == AnalyticCurve::Type::Circle);
        assert(circle.is_closed());
        std::cout << "✓ test_analytic_circle\n";
    }

    // Test 12: Create arc curve
    {
        AnalyticCurve arc = AnalyticCurve::create_arc(Point3D(0, 0, 0), 1.0, Vector3D(0, 0, 1), 0, M_PI/2);
        assert(arc.get_type() == AnalyticCurve::Type::Arc);
        assert(!arc.is_closed());
        std::cout << "✓ test_analytic_arc\n";
    }

    // Test 13: Create ellipse curve
    {
        AnalyticCurve ellipse = AnalyticCurve::create_ellipse(Point3D(0, 0, 0), 2.0, 1.0, Vector3D(0, 0, 1));
        assert(ellipse.get_type() == AnalyticCurve::Type::Ellipse);
        assert(ellipse.is_closed());
        std::cout << "✓ test_analytic_ellipse\n";
    }

    // Test 14: Create spiral curve
    {
        AnalyticCurve spiral = AnalyticCurve::create_spiral(Point3D(0, 0, 0), Vector3D(0, 0, 1), 1.0, 0.5, 3);
        assert(spiral.get_type() == AnalyticCurve::Type::Spiral);
        std::cout << "✓ test_analytic_spiral\n";
    }

    // Test 15: Analytic curve evaluation
    {
        AnalyticCurve line = AnalyticCurve::create_line(Point3D(0, 0, 0), Point3D(1, 0, 0));
        Point3D p = line.evaluate(0.5);
        assert(std::abs(p.x - 0.5) < 1e-10);
        std::cout << "✓ test_analytic_evaluate\n";
    }

    // Test 16: Analytic curve tangent
    {
        AnalyticCurve line = AnalyticCurve::create_line(Point3D(0, 0, 0), Point3D(1, 0, 0));
        Vector3D t = line.tangent(0.5);
        assert(std::abs(t.magnitude() - 1.0) < 1e-6 || t.magnitude() < 1e-10);
        std::cout << "✓ test_analytic_tangent\n";
    }

    // Test 17: Analytic curve arc length
    {
        AnalyticCurve circle = AnalyticCurve::create_circle(Point3D(0, 0, 0), 1.0, Vector3D(0, 0, 1));
        double length = circle.arc_length(100);
        assert(std::abs(length - 2.0 * M_PI) < 0.1);
        std::cout << "✓ test_analytic_arc_length\n";
    }

    // Test 18: Create tube from NURBS
    {
        std::vector<Point3D> cp = {Point3D(0, 0, 0), Point3D(1, 0, 0), Point3D(2, 0, 0)};
        std::vector<double> knots = {0, 0, 0, 1, 1, 1};
        std::vector<double> weights = {1, 1, 1};
        auto nurbs = std::make_shared<NurbsCurve>(cp, knots, weights, 2);
        TubeCurve tube(nurbs, 0.5);
        assert(tube.tube_radius() == 0.5);
        std::cout << "✓ test_tube_from_nurbs\n";
    }

    // Test 19: Create tube from analytic
    {
        AnalyticCurve line = AnalyticCurve::create_line(Point3D(0, 0, 0), Point3D(1, 0, 0));
        auto analytic = std::make_shared<AnalyticCurve>(line);
        TubeCurve tube(analytic, 0.25);
        assert(tube.tube_radius() == 0.25);
        std::cout << "✓ test_tube_from_analytic\n";
    }

    // Test 20: Tube surface evaluation
    {
        AnalyticCurve line = AnalyticCurve::create_line(Point3D(0, 0, 0), Point3D(1, 0, 0));
        auto analytic = std::make_shared<AnalyticCurve>(line);
        TubeCurve tube(analytic, 0.5);
        Point3D p = tube.surface_point(0.5, 0);
        // Should give a point on the tube surface
        assert(p.y >= -0.5 && p.y <= 0.5);
        std::cout << "✓ test_tube_surface_point\n";
    }

    // Test 21: Tube offset
    {
        AnalyticCurve line = AnalyticCurve::create_line(Point3D(0, 0, 0), Point3D(1, 0, 0));
        auto analytic = std::make_shared<AnalyticCurve>(line);
        TubeCurve tube(analytic, 0.5);
        auto offset_tube = tube.offset(0.1);
        assert(offset_tube != nullptr);
        assert(offset_tube->tube_radius() == 0.6);
        std::cout << "✓ test_tube_offset\n";
    }

    // Test 22: CurveOperations creation
    {
        CurveOperations ops;
        AnalyticCurve line = AnalyticCurve::create_line(Point3D(0, 0, 0), Point3D(1, 0, 0));
        auto arc_curve = ops.create_line(Point3D(0, 0, 0), Point3D(1, 0, 0));
        assert(arc_curve != nullptr);
        std::cout << "✓ test_curve_operations\n";
    }

    // Test 23: Interpolate points
    {
        CurveOperations ops;
        std::vector<Point3D> points = {Point3D(0, 0, 0), Point3D(1, 1, 0), Point3D(2, 0, 0)};
        auto curve = ops.interpolate_points(points, 2);
        assert(curve != nullptr);
        std::cout << "✓ test_interpolate_points\n";
    }

    // Test 24: Curve length calculation
    {
        CurveOperations ops;
        std::vector<Point3D> cp = {Point3D(0, 0, 0), Point3D(1, 0, 0), Point3D(2, 0, 0)};
        std::vector<double> knots = {0, 0, 0, 1, 1, 1};
        std::vector<double> weights = {1, 1, 1};
        NurbsCurve curve(cp, knots, weights, 2);
        double length = ops.curve_length(curve, 100);
        assert(length > 0);
        std::cout << "✓ test_curve_length\n";
    }

    // Test 25: NURBS offset curve
    {
        std::vector<Point3D> cp = {Point3D(0, 0, 0), Point3D(1, 0, 0), Point3D(2, 0, 0)};
        std::vector<double> knots = {0, 0, 0, 1, 1, 1};
        std::vector<double> weights = {1, 1, 1};
        NurbsCurve curve(cp, knots, weights, 2);
        Vector3D normal(0, 1, 0);
        auto offset = curve.offset(0.5, normal);
        assert(offset != nullptr);
        std::cout << "✓ test_nurbs_offset\n";
    }

    std::cout << "\nAll tests passed!\n";
    return 0;
}
