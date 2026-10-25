#include <gtest/gtest.h>
#include "bendcore/sketch/constraint_diagnostics.hpp"
#include "bendcore/sketch/primitives.hpp"
#include "bendcore/sketch/edit_tools.hpp"
#include <cmath>
#include <memory>

namespace bendcore::sketch {
namespace test {

// ============= Constraint Diagnostics Tests =============

class ConstraintDiagnosticsTest : public ::testing::Test {
protected:
    ConstraintDiagnosticsEngine engine;
};

TEST_F(ConstraintDiagnosticsTest, AddConstraint) {
    engine.addConstraint(1, "distance");
    engine.addConstraint(2, "parallel");
    EXPECT_EQ(engine.getConstraints().size(), 2);
}

TEST_F(ConstraintDiagnosticsTest, RemoveConstraint) {
    engine.addConstraint(1, "distance");
    engine.removeConstraint(1);
    EXPECT_EQ(engine.getConstraints().size(), 0);
}

TEST_F(ConstraintDiagnosticsTest, CalculateDegreesOfFreedom) {
    // 2 points (4 DOF) - 1 distance constraint = 3 DOF remaining
    engine.addEntity(1, "point");
    engine.addEntity(2, "point");
    engine.addConstraint(1, "distance");

    int dof = engine.calculateDegreesOfFreedom(2, 1);
    EXPECT_EQ(dof, 3);
}

TEST_F(ConstraintDiagnosticsTest, FullyConstrainedSketch) {
    // 1 point fixed = 0 DOF
    engine.addEntity(1, "point");
    engine.addConstraint(1, "fixed_x");
    engine.addConstraint(2, "fixed_y");

    auto report = engine.diagnose();
    EXPECT_TRUE(report.is_fully_constrained);
    EXPECT_EQ(report.degrees_of_freedom, 0);
}

TEST_F(ConstraintDiagnosticsTest, UnderconstrainedSketch) {
    // 1 point with no constraints = 2 DOF
    engine.addEntity(1, "point");

    auto report = engine.diagnose();
    EXPECT_TRUE(report.is_underconstrained);
    EXPECT_GT(report.degrees_of_freedom, 0);
}

TEST_F(ConstraintDiagnosticsTest, DiagnoseReport) {
    engine.addEntity(1, "point");
    engine.addConstraint(1, "distance");

    auto report = engine.diagnose();
    EXPECT_TRUE(!report.status_message.empty());
    EXPECT_GT(report.constraint_diagnostics.size(), 0);
}

TEST_F(ConstraintDiagnosticsTest, ClearDiagnostics) {
    engine.addConstraint(1, "distance");
    engine.addEntity(1, "point");
    engine.clear();

    EXPECT_EQ(engine.getConstraints().size(), 0);
}

TEST_F(ConstraintDiagnosticsTest, SuggestConstraints) {
    engine.addEntity(1, "point");

    auto suggestions = engine.suggestConstraints();
    EXPECT_GT(suggestions.size(), 0);
}

TEST_F(ConstraintDiagnosticsTest, GetConstraintDescription) {
    engine.addConstraint(1, "distance");

    std::string desc = engine.getConstraintDescription(1);
    EXPECT_TRUE(desc.find("distance") != std::string::npos ||
               desc.find("Constraint") != std::string::npos);
}

// ============= Primitives Tests =============

class PointTest : public ::testing::Test {
protected:
    Point p1{0, 0};
    Point p2{3, 4};
};

TEST_F(PointTest, PointCreation) {
    EXPECT_EQ(p1.getX(), 0);
    EXPECT_EQ(p1.getY(), 0);
    EXPECT_EQ(p2.getX(), 3);
    EXPECT_EQ(p2.getY(), 4);
}

TEST_F(PointTest, PointDistance) {
    double dist = p1.distance(p2);
    EXPECT_NEAR(dist, 5.0, 1e-6);
}

TEST_F(PointTest, PointSetPosition) {
    p1.setPosition(1, 1);
    EXPECT_EQ(p1.getX(), 1);
    EXPECT_EQ(p1.getY(), 1);
}

class LineTest : public ::testing::Test {
protected:
    Point start{0, 0};
    Point end{4, 0};
    Line horizontal_line{start, end};

    Point v_start{0, 0};
    Point v_end{0, 3};
    Line vertical_line{v_start, v_end};
};

TEST_F(LineTest, LineCreation) {
    EXPECT_EQ(horizontal_line.getStart().getX(), 0);
    EXPECT_EQ(horizontal_line.getEnd().getX(), 4);
}

TEST_F(LineTest, LineLength) {
    double len = horizontal_line.getLength();
    EXPECT_NEAR(len, 4.0, 1e-6);
}

TEST_F(LineTest, LineMidpoint) {
    Point mid = horizontal_line.getMidpoint();
    EXPECT_NEAR(mid.getX(), 2.0, 1e-6);
    EXPECT_NEAR(mid.getY(), 0.0, 1e-6);
}

TEST_F(LineTest, LineIsHorizontal) {
    EXPECT_TRUE(horizontal_line.isHorizontal());
    EXPECT_FALSE(vertical_line.isHorizontal());
}

TEST_F(LineTest, LineIsVertical) {
    EXPECT_TRUE(vertical_line.isVertical());
    EXPECT_FALSE(horizontal_line.isVertical());
}

class CircleTest : public ::testing::Test {
protected:
    Point center{0, 0};
    Circle circle{center, 5.0};
};

TEST_F(CircleTest, CircleCreation) {
    EXPECT_NEAR(circle.getRadius(), 5.0, 1e-6);
}

TEST_F(CircleTest, CircleProperties) {
    double circumference = circle.getCircumference();
    double area = circle.getArea();

    EXPECT_NEAR(circumference, 2.0 * M_PI * 5.0, 1e-6);
    EXPECT_NEAR(area, M_PI * 5.0 * 5.0, 1e-6);
}

TEST_F(CircleTest, CirclePoint) {
    Point p = circle.getPoint(0);  // 0 degrees
    EXPECT_NEAR(p.getX(), 5.0, 1e-6);
    EXPECT_NEAR(p.getY(), 0.0, 1e-6);
}

TEST_F(CircleTest, CircleSetRadius) {
    circle.setRadius(10.0);
    EXPECT_NEAR(circle.getRadius(), 10.0, 1e-6);
}

class ArcTest : public ::testing::Test {
protected:
    Point center{0, 0};
    Arc arc{center, 5.0, 0.0, 90.0};
};

TEST_F(ArcTest, ArcCreation) {
    EXPECT_NEAR(arc.getRadius(), 5.0, 1e-6);
    EXPECT_NEAR(arc.getStartAngle(), 0.0, 1e-6);
    EXPECT_NEAR(arc.getEndAngle(), 90.0, 1e-6);
}

TEST_F(ArcTest, ArcLength) {
    double length = arc.getArcLength();
    double expected = 5.0 * (M_PI / 2.0);  // 90 degrees = PI/2 radians
    EXPECT_NEAR(length, expected, 1e-6);
}

TEST_F(ArcTest, ArcPoints) {
    Point start = arc.getStartPoint();
    Point end = arc.getEndPoint();

    EXPECT_NEAR(start.getX(), 5.0, 1e-6);
    EXPECT_NEAR(end.getY(), 5.0, 1e-6);
}

class EllipseTest : public ::testing::Test {
protected:
    Point center{0, 0};
    Ellipse ellipse{center, 6.0, 4.0};
};

TEST_F(EllipseTest, EllipseCreation) {
    EXPECT_NEAR(ellipse.getMajorAxis(), 6.0, 1e-6);
    EXPECT_NEAR(ellipse.getMinorAxis(), 4.0, 1e-6);
}

TEST_F(EllipseTest, EllipseArea) {
    double area = ellipse.getArea();
    double expected = M_PI * 6.0 * 4.0;
    EXPECT_NEAR(area, expected, 1e-6);
}

TEST_F(EllipseTest, EllipseEccentricity) {
    double ecc = ellipse.getEccentricity();
    EXPECT_GT(ecc, 0.0);
    EXPECT_LT(ecc, 1.0);
}

class RectangleTest : public ::testing::Test {
protected:
    Point origin{0, 0};
    Rectangle rect{origin, 4.0, 3.0};
};

TEST_F(RectangleTest, RectangleCreation) {
    EXPECT_NEAR(rect.getWidth(), 4.0, 1e-6);
    EXPECT_NEAR(rect.getHeight(), 3.0, 1e-6);
}

TEST_F(RectangleTest, RectangleProperties) {
    EXPECT_NEAR(rect.getPerimeter(), 14.0, 1e-6);
    EXPECT_NEAR(rect.getArea(), 12.0, 1e-6);
}

TEST_F(RectangleTest, RectangleCorners) {
    std::vector<Point> corners = rect.getCorners();
    EXPECT_EQ(corners.size(), 4);
}

class PolygonTest : public ::testing::Test {
protected:
    Point center{0, 0};
    Polygon polygon{center, 6, 1.0};  // Regular hexagon
};

TEST_F(PolygonTest, PolygonCreation) {
    EXPECT_EQ(polygon.getNumberOfSides(), 6);
}

TEST_F(PolygonTest, PolygonVertices) {
    const auto& vertices = polygon.getVertices();
    EXPECT_EQ(vertices.size(), 6);
}

TEST_F(PolygonTest, PolygonArea) {
    double area = polygon.getArea();
    EXPECT_GT(area, 0.0);
}

class SplineTest : public ::testing::Test {
protected:
    Spline bezier{Spline::SplineType::BEZIER};
};

TEST_F(SplineTest, SplineControlPoints) {
    bezier.addControlPoint(Point(0, 0));
    bezier.addControlPoint(Point(1, 2));
    bezier.addControlPoint(Point(3, 2));
    bezier.addControlPoint(Point(4, 0));

    const auto& pts = bezier.getControlPoints();
    EXPECT_EQ(pts.size(), 4);
}

TEST_F(SplineTest, SplineEvaluation) {
    bezier.addControlPoint(Point(0, 0));
    bezier.addControlPoint(Point(1, 1));
    bezier.addControlPoint(Point(2, 1));
    bezier.addControlPoint(Point(3, 0));

    Point p0 = bezier.evaluate(0.0);
    EXPECT_NEAR(p0.getX(), 0.0, 1e-5);
    EXPECT_NEAR(p0.getY(), 0.0, 1e-5);
}

// ============= Edit Tools Tests =============

class EditToolsCoordinatorTest : public ::testing::Test {
protected:
    EditToolsCoordinator coordinator;
};

TEST_F(EditToolsCoordinatorTest, Undo) {
    EXPECT_FALSE(coordinator.canUndo());
    EXPECT_FALSE(coordinator.canRedo());
}

TEST_F(EditToolsCoordinatorTest, OperationHistory) {
    auto history = coordinator.getOperationHistory();
    EXPECT_EQ(history.size(), 0);
}

TEST_F(EditToolsCoordinatorTest, ClearHistory) {
    coordinator.clearHistory();
    EXPECT_FALSE(coordinator.canUndo());
    EXPECT_FALSE(coordinator.canRedo());
}

class MirrorTest : public ::testing::Test {
protected:
    Point p1{1, 0};
    Point axis_start{0, 0};
    Point axis_end{0, 1};
    Line mirror_axis{axis_start, axis_end};
};

TEST_F(MirrorTest, MirrorPointAboutVerticalAxis) {
    Point mirrored = MirrorTool::mirrorPoint(p1, mirror_axis);
    EXPECT_NEAR(mirrored.getX(), -1.0, 1e-6);
}

class RotateTest : public ::testing::Test {
protected:
    Point p{1, 0};
    Point center{0, 0};
};

TEST_F(RotateTest, RotatePoint90Degrees) {
    Point rotated = RotateTool::rotatePoint(p, center, 90.0);
    EXPECT_NEAR(rotated.getX(), 0.0, 1e-6);
    EXPECT_NEAR(rotated.getY(), 1.0, 1e-6);
}

TEST_F(RotateTest, RotatePoint180Degrees) {
    Point rotated = RotateTool::rotatePoint(p, center, 180.0);
    EXPECT_NEAR(rotated.getX(), -1.0, 1e-6);
    EXPECT_NEAR(rotated.getY(), 0.0, 1e-6);
}

// ============= Integration Tests =============

TEST(SketchIntegration, CompleteWorkflow) {
    // Create sketch elements
    Point p1{0, 0};
    Point p2{4, 0};
    Line line1{p1, p2};

    // Test diagnostics
    ConstraintDiagnosticsEngine engine;
    engine.addEntity(1, "point");
    engine.addEntity(2, "point");
    engine.addConstraint(1, "distance");

    auto report = engine.diagnose();
    EXPECT_GE(report.degrees_of_freedom, 0);

    // Test primitives
    Circle circle{p1, 2.5};
    EXPECT_NEAR(circle.getRadius(), 2.5, 1e-6);

    // Test tools
    EditToolsCoordinator coordinator;
    EXPECT_FALSE(coordinator.canUndo());
    EXPECT_FALSE(coordinator.canRedo());
}

}  // namespace test
}  // namespace bendcore::sketch
