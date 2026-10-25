#pragma once

#include <vector>
#include <memory>
#include <string>
#include <optional>
#include <cmath>
#include <limits>

namespace bendcore::sketch {

/**
 * @brief Primitive types in sketch
 */
enum class PrimitiveType {
    POINT,
    LINE,
    CIRCLE,
    ARC,
    ELLIPSE,
    SPLINE,
    RECTANGLE,
    POLYGON,
    CONSTRUCTION_POINT,
    CONSTRUCTION_LINE,
    CONSTRUCTION_CIRCLE,
};

/**
 * @brief 2D Point primitive
 */
class Point {
public:
    Point(double x = 0.0, double y = 0.0) : x_(x), y_(y) {}

    double getX() const { return x_; }
    double getY() const { return y_; }
    void setPosition(double x, double y) { x_ = x; y_ = y; }
    double distance(const Point& other) const;

    PrimitiveType getType() const { return PrimitiveType::POINT; }

private:
    double x_, y_;
};

/**
 * @brief 2D Line primitive
 */
class Line {
public:
    Line() : start_(Point()), end_(Point()) {}
    Line(const Point& start, const Point& end) : start_(start), end_(end) {}

    const Point& getStart() const { return start_; }
    const Point& getEnd() const { return end_; }
    void setStart(const Point& p) { start_ = p; }
    void setEnd(const Point& p) { end_ = p; }

    double getLength() const { return start_.distance(end_); }
    Point getMidpoint() const;
    double getSlope() const;  // dy/dx
    bool isHorizontal(double tolerance = 1e-6) const;
    bool isVertical(double tolerance = 1e-6) const;

    PrimitiveType getType() const { return PrimitiveType::LINE; }

private:
    Point start_, end_;
};

/**
 * @brief 2D Circle primitive
 */
class Circle {
public:
    Circle(const Point& center, double radius) : center_(center), radius_(radius) {}

    const Point& getCenter() const { return center_; }
    double getRadius() const { return radius_; }
    void setCenter(const Point& p) { center_ = p; }
    void setRadius(double r) { radius_ = std::abs(r); }

    double getCircumference() const { return 2.0 * M_PI * radius_; }
    double getArea() const { return M_PI * radius_ * radius_; }
    Point getPoint(double angle_deg) const;  // Get point on circle at angle

    PrimitiveType getType() const { return PrimitiveType::CIRCLE; }

private:
    Point center_;
    double radius_;
};

/**
 * @brief 2D Arc primitive
 */
class Arc {
public:
    Arc() : center_(Point()), radius_(0.0), start_angle_(0.0), end_angle_(0.0) {}
    Arc(const Point& center, double radius, double start_angle_deg, double end_angle_deg)
        : center_(center), radius_(radius), start_angle_(start_angle_deg), end_angle_(end_angle_deg) {}

    const Point& getCenter() const { return center_; }
    double getRadius() const { return radius_; }
    double getStartAngle() const { return start_angle_; }
    double getEndAngle() const { return end_angle_; }

    void setCenter(const Point& p) { center_ = p; }
    void setRadius(double r) { radius_ = std::abs(r); }
    void setAngles(double start_deg, double end_deg) { start_angle_ = start_deg; end_angle_ = end_deg; }

    Point getStartPoint() const;
    Point getEndPoint() const;
    Point getMidPoint() const;
    double getArcLength() const;

    PrimitiveType getType() const { return PrimitiveType::ARC; }

private:
    Point center_;
    double radius_;
    double start_angle_;  // degrees
    double end_angle_;    // degrees
};

/**
 * @brief 2D Ellipse primitive
 */
class Ellipse {
public:
    Ellipse(const Point& center, double major_axis, double minor_axis, double rotation_deg = 0.0)
        : center_(center), major_axis_(major_axis), minor_axis_(minor_axis), rotation_(rotation_deg) {}

    const Point& getCenter() const { return center_; }
    double getMajorAxis() const { return major_axis_; }
    double getMinorAxis() const { return minor_axis_; }
    double getRotation() const { return rotation_; }

    void setCenter(const Point& p) { center_ = p; }
    void setAxes(double major, double minor) { major_axis_ = major; minor_axis_ = minor; }
    void setRotation(double angle_deg) { rotation_ = angle_deg; }

    double getEccentricity() const;
    double getPerimeter() const;  // Approximate
    double getArea() const { return M_PI * major_axis_ * minor_axis_; }
    Point getPoint(double angle_deg) const;

    PrimitiveType getType() const { return PrimitiveType::ELLIPSE; }

private:
    Point center_;
    double major_axis_;
    double minor_axis_;
    double rotation_;  // degrees
};

/**
 * @brief 2D B-Spline curve primitive
 */
class Spline {
public:
    enum class SplineType {
        BEZIER,
        BSPLINE,
        INTERPOLATED
    };

    Spline(SplineType type = SplineType::BEZIER) : type_(type) {}

    void addControlPoint(const Point& p) { control_points_.push_back(p); }
    void setControlPoints(const std::vector<Point>& points) { control_points_ = points; }
    const std::vector<Point>& getControlPoints() const { return control_points_; }

    void setKnots(const std::vector<double>& knots) { knots_ = knots; }
    const std::vector<double>& getKnots() const { return knots_; }

    void setDegree(int degree) { degree_ = degree; }
    int getDegree() const { return degree_; }

    // Evaluate spline at parameter t (0..1)
    Point evaluate(double t) const;

    // Get tangent at parameter t
    Point getTangent(double t) const;

    // Get total curve length
    double getLength() const;

    SplineType getType() const { return type_; }
    PrimitiveType getPrimitiveType() const { return PrimitiveType::SPLINE; }

private:
    SplineType type_;
    std::vector<Point> control_points_;
    std::vector<double> knots_;
    int degree_ = 3;

    // Internal calculation methods
    double calculateBezierLength() const;
    Point evaluateBezier(double t) const;
    Point evaluateBSpline(double t) const;
};

/**
 * @brief 2D Rectangle primitive
 */
class Rectangle {
public:
    Rectangle(const Point& origin, double width, double height, double rotation_deg = 0.0)
        : origin_(origin), width_(width), height_(height), rotation_(rotation_deg) {}

    const Point& getOrigin() const { return origin_; }
    double getWidth() const { return width_; }
    double getHeight() const { return height_; }
    double getRotation() const { return rotation_; }

    void setOrigin(const Point& p) { origin_ = p; }
    void setDimensions(double w, double h) { width_ = w; height_ = h; }
    void setRotation(double angle_deg) { rotation_ = angle_deg; }

    Point getCenter() const;
    std::vector<Point> getCorners() const;
    double getPerimeter() const { return 2.0 * (width_ + height_); }
    double getArea() const { return width_ * height_; }

    PrimitiveType getType() const { return PrimitiveType::RECTANGLE; }

private:
    Point origin_;
    double width_;
    double height_;
    double rotation_;  // degrees
};

/**
 * @brief 2D Polygon primitive
 */
class Polygon {
public:
    Polygon(const Point& center = Point(), int sides = 4, double radius = 1.0, double rotation_deg = 0.0)
        : center_(center), num_sides_(sides), radius_(radius), rotation_(rotation_deg) {
        regenerateVertices();
    }

    void addVertex(const Point& p) { vertices_.push_back(p); }
    void setVertices(const std::vector<Point>& vertices) { vertices_ = vertices; }
    const std::vector<Point>& getVertices() const { return vertices_; }

    void setCenter(const Point& p) { center_ = p; regenerateVertices(); }
    void setNumberOfSides(int n) { num_sides_ = n; regenerateVertices(); }
    void setRadius(double r) { radius_ = r; regenerateVertices(); }
    void setRotation(double angle_deg) { rotation_ = angle_deg; regenerateVertices(); }

    double getPerimeter() const;
    double getArea() const;
    int getNumberOfSides() const { return num_sides_; }

    PrimitiveType getType() const { return PrimitiveType::POLYGON; }

private:
    Point center_;
    std::vector<Point> vertices_;
    int num_sides_;
    double radius_;
    double rotation_;  // degrees

    void regenerateVertices();
};

/**
 * @brief Construction geometry (reference only, non-printing)
 */
class ConstructionGeometry {
public:
    enum class ConstructionType {
        POINT,
        LINE,
        CIRCLE
    };

    ConstructionGeometry(ConstructionType type) : type_(type), is_visible_(true) {}

    ConstructionType getConstructionType() const { return type_; }
    bool isVisible() const { return is_visible_; }
    void setVisible(bool visible) { is_visible_ = visible; }

    PrimitiveType getPrimitiveType() const {
        switch (type_) {
            case ConstructionType::POINT: return PrimitiveType::CONSTRUCTION_POINT;
            case ConstructionType::LINE: return PrimitiveType::CONSTRUCTION_LINE;
            case ConstructionType::CIRCLE: return PrimitiveType::CONSTRUCTION_CIRCLE;
        }
        return PrimitiveType::POINT;
    }

private:
    ConstructionType type_;
    bool is_visible_;
};

/**
 * @brief Base class for all primitives with persistence
 */
class PrimitivePersistence {
public:
    virtual ~PrimitivePersistence() = default;

    /**
     * @brief Serialize primitive to string
     */
    virtual std::string serialize() const = 0;

    /**
     * @brief Deserialize primitive from string
     */
    virtual bool deserialize(const std::string& data) = 0;

    /**
     * @brief Get primitive type
     */
    virtual PrimitiveType getType() const = 0;
};

} // namespace bendcore::sketch
