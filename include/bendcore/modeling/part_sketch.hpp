#pragma once

// Data of the sketch and datum nodes of the part document (docs/modeling/PART_DOCUMENT.md,
// section "Эскиз"). A sketch node stores points, curves and PlaneGCS constraints whose
// numbers are expressions (bendcore::formula); at rebuild the document evaluates them, the
// sketch is solved with PlaneGCS (BoundSketch), turned into a SketchProfile and the selected
// regions become a NamedShape whose elements are named after the sketch entities.

#include <bendcore/modeling/naming.hpp>
#include <bendcore/modeling/sketch_profile.hpp>

#include <gp_Ax1.hxx>
#include <gp_Pln.hxx>

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace bendcore::modeling {

// Sketch point: initial position (expressions, mm). A fixed point is pinned at that position,
// so its coordinates act as dimensions; a free point only starts the solver there.
struct SketchPointDef {
    std::string x = "0";
    std::string y = "0";
    bool fixed = false;
};

enum class SketchGeometryType { Line, Circle, Arc };

const char* toString(SketchGeometryType type) noexcept;
std::optional<SketchGeometryType> sketchGeometryTypeFromString(std::string_view text) noexcept;

// Sketch curve; its entity id is its index in SketchData::geometry plus one.
//   Line:   start -> end (point indices)
//   Circle: center (point index), radius (expression: initial radius)
//   Arc:    center, start, end (point indices), counterClockwise
// projection >= 0: the curve is the projection of model edge #projection of the sketch's
// "projected" reference slot; its points are pinned at the projection (a projected line is
// oriented so that its start has the smaller x, then the smaller y).
struct SketchGeometryDef {
    SketchGeometryType type = SketchGeometryType::Line;
    int start = -1;
    int end = -1;
    int center = -1;
    std::string radius = "1";
    bool counterClockwise = true;
    bool construction = false;
    int projection = -1;
};

enum class SketchConstraintType {
    Coincident,     // points a, b
    Horizontal,     // points a, b
    Vertical,       // points a, b
    Distance,       // points a, b; value mm
    Radius,         // circle/arc entity a; value mm
    Parallel,       // line entities a, b
    Perpendicular,  // line entities a, b
    EqualLength,    // line entities a, b
    Angle,          // line entities a, b; value degrees
    PointOnLine,    // point a, line entity b
    PointOnCircle,  // point a, circle/arc entity b
    EqualRadius,    // circle/arc entities a, b
    Symmetric,      // points a, b about line entity c
};

const char* toString(SketchConstraintType type) noexcept;
std::optional<SketchConstraintType> sketchConstraintTypeFromString(std::string_view text) noexcept;

// Operands: point indices (0-based) or entity ids (1-based), as listed above.
struct SketchConstraintDef {
    SketchConstraintType type = SketchConstraintType::Coincident;
    int a = -1;
    int b = -1;
    int c = -1;
    std::string value = "0";
};

struct SketchData {
    std::vector<SketchPointDef> points;
    std::vector<SketchGeometryDef> geometry;
    std::vector<SketchConstraintDef> constraints;
    // Profile regions to use. Empty: the material regions (outer contours and islands).
    // Otherwise each entry selects the region whose outer loop contains that entity (when two
    // regions qualify — an edge shared by adjacent regions — the smaller one).
    std::vector<SketchEntityId> regions;
};

// Expressions of a sketch in evaluation order: points (x, y), geometry (radius), constraints
// (value). The document evaluates them; the sketch feature receives the values in this order.
std::vector<const std::string*> sketchExpressions(const SketchData& data);

// Convenience builder for code and tests.
class SketchBuilder {
public:
    int point(std::string x, std::string y, bool fixed = false);
    SketchEntityId line(int start, int end, bool construction = false);
    SketchEntityId circle(int center, std::string radius, bool construction = false);
    SketchEntityId arc(int center, int start, int end, bool counterClockwise = true, bool construction = false);
    SketchEntityId projected(SketchGeometryType type, int referenceIndex, bool construction = true);
    int constrain(SketchConstraintType type, int a, int b, std::string value = "0", int c = -1);

    // Axis-aligned rectangle with the fixed corner (x, y) and dimensions w, h driven by
    // Distance constraints (expressions). Entities: bottom, right, top, left.
    std::vector<SketchEntityId> rectangle(std::string x, std::string y, std::string w, std::string h);
    // Circle with a fixed centre and a Radius constraint.
    SketchEntityId fixedCircle(std::string cx, std::string cy, std::string r);
    // Closed polyline through fixed points (each point is a dimension).
    std::vector<SketchEntityId> fixedPolygon(const std::vector<std::pair<std::string, std::string>>& corners);

    SketchData& data() noexcept { return data_; }
    SketchData build() const { return data_; }

private:
    SketchData data_;
};

// Result of a sketch node.
struct SketchResult {
    explicit SketchResult(const SketchPlacement& p) : placement(p) {}

    SketchPlacement placement;
    std::vector<SketchCurve> curves;   // solved curves, construction ones included
    std::vector<Point2> points;        // solved point positions, by point index
    std::vector<std::size_t> standalonePoints;  // points used by no curve (hole centres)
    std::shared_ptr<const SketchProfile> profile;  // null when there are no profile curves
    std::vector<std::size_t> regions;  // selected regions of the profile
    // Faces of the selected regions (a face or a compound of faces) with names
    // "<sketch>:R<id>" (region, smallest entity of its outer loop), "<sketch>:E<id>" (edge of
    // entity id), "<sketch>:V<id>|<id>" (vertex shared by entities). Null without regions.
    std::shared_ptr<const NamedShape> faces;
    int dof = 0;
    std::string solverStatus;  // sketch2d status
};

// Result of a datum plane or datum axis node.
struct DatumResult {
    std::optional<gp_Pln> plane;
    std::optional<gp_Ax1> axis;
};

}  // namespace bendcore::modeling
