#pragma once

// Sketch placement, closed profiles (contours and regions) built from a solved 2D sketch, and
// projection of model edges onto the sketch plane (docs/modeling/SKETCH_PROFILE.md).

#include <bendcore/kernel/result.hpp>
#include <bendcore/kernel/shape.hpp>
#include <bendcore/sketch2d.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Wire.hxx>
#include <gp_Ax3.hxx>
#include <gp_Dir.hxx>
#include <gp_Pln.hxx>
#include <gp_Pnt.hxx>
#include <gp_Trsf.hxx>

namespace bendcore::modeling {

using kernel::Error;
using kernel::ErrorCode;
using kernel::Result;

// Coordinates in the sketch plane, millimetres.
struct Point2 {
    double x = 0.0;
    double y = 0.0;
};

// ---------------------------------------------------------------------------------------------
// Placement
// ---------------------------------------------------------------------------------------------

struct FacePlacementOptions {
    // Sketch origin; projected onto the face plane. Default: the location of the face plane.
    std::optional<gp_Pnt> origin;
    // Sketch X axis; projected onto the face plane. Default: X direction of the face plane.
    std::optional<gp_Dir> xDirection;
};

// Right-handed local frame of a sketch: origin, normal (local Z) and X axis; Y = Z x X.
// Sketch point (x, y) is origin + x*X + y*Y in model space.
class SketchPlacement {
public:
    // xDirection is projected onto the plane; error when it is parallel to the normal.
    static Result<SketchPlacement> fromAxes(const gp_Pnt& origin, const gp_Dir& normal,
                                            const gp_Dir& xDirection);
    // The frame must be right-handed (gp_Ax3::Direct()); a left-handed one is an error.
    static Result<SketchPlacement> fromFrame(const gp_Ax3& frame);
    // Plane position; a left-handed plane frame is made right-handed by flipping its Y axis.
    static SketchPlacement fromPlane(const gp_Pln& plane);

    // Planar face: the normal is the outward face normal (face orientation is honoured).
    static Result<SketchPlacement> onFace(const TopoDS_Face& face,
                                          const FacePlacementOptions& options = {});
    static Result<SketchPlacement> onFace(const kernel::TopologyIndex& topology,
                                          std::uint32_t faceIndex,
                                          const FacePlacementOptions& options = {});

    const gp_Ax3& frame() const noexcept { return frame_; }
    gp_Pnt origin() const { return frame_.Location(); }
    gp_Dir normal() const { return frame_.Direction(); }
    gp_Dir xDirection() const { return frame_.XDirection(); }
    gp_Dir yDirection() const { return frame_.YDirection(); }
    gp_Pln plane() const { return gp_Pln(frame_); }

    gp_Pnt toWorld(const Point2& p) const;
    // Orthogonal projection of a model point into sketch coordinates.
    Point2 toLocal(const gp_Pnt& p) const;
    // Signed distance of a model point from the sketch plane along the normal.
    double signedDistance(const gp_Pnt& p) const;
    // Transformation from sketch-local space (sketch plane = XY plane) to model space.
    gp_Trsf localToWorld() const;

private:
    explicit SketchPlacement(const gp_Ax3& frame) : frame_(frame) {}
    gp_Ax3 frame_;
};

// ---------------------------------------------------------------------------------------------
// Sketch curves
// ---------------------------------------------------------------------------------------------

using SketchEntityId = std::uint32_t;

struct LineSegment2d {
    Point2 start;
    Point2 end;
};

// Arc of a circle from start to end around center; the radius is |start - center| and the end
// point must be at the same distance (within the profile tolerance).
struct CircularArc2d {
    Point2 center;
    Point2 start;
    Point2 end;
    bool counterClockwise = true;
};

struct Circle2d {
    Point2 center;
    double radius = 0.0;
};

// Full ellipse: c + a*cos(t)*U + b*sin(t)*V, U = normalized majorDirection, V = U rotated +90 deg.
struct Ellipse2d {
    Point2 center;
    Point2 majorDirection{1.0, 0.0};
    double majorRadius = 0.0;
    double minorRadius = 0.0;
};

// Part of an ellipse for t from startParameter to endParameter (endParameter > startParameter,
// span at most 2*pi), i.e. counter-clockwise in the ellipse frame above.
struct EllipticalArc2d {
    Ellipse2d ellipse;
    double startParameter = 0.0;
    double endParameter = 0.0;
};

// B-spline in the sketch plane; weights empty = non-rational. Knots are distinct values with
// multiplicities (OCCT convention).
struct BSpline2d {
    int degree = 3;
    std::vector<Point2> poles;
    std::vector<double> weights;
    std::vector<double> knots;
    std::vector<int> multiplicities;
    bool periodic = false;
};

using Curve2d =
    std::variant<LineSegment2d, CircularArc2d, Circle2d, Ellipse2d, EllipticalArc2d, BSpline2d>;

struct SketchCurve {
    SketchEntityId id = 0;
    Curve2d geometry;
    // Construction (auxiliary) geometry never becomes part of a profile.
    bool construction = false;
};

// ---------------------------------------------------------------------------------------------
// Profile diagnostics
// ---------------------------------------------------------------------------------------------

enum class ProfileIssueKind {
    DuplicateId,       // two curves with the same id
    Degenerate,        // zero length or radius, inconsistent arc, invalid B-spline data
    OpenContour,       // chain of curves with a free end
    Branching,         // more than two curve ends meet at one point
    Intersection,      // two curves cross or touch away from a shared end point
    Overlap,           // two curves share a piece of length
    SelfIntersection,  // a curve crosses itself
    Empty,             // no profile (non-construction) curves at all
};

const char* toString(ProfileIssueKind kind) noexcept;

struct ProfileIssue {
    ProfileIssueKind kind = ProfileIssueKind::Degenerate;
    std::vector<SketchEntityId> entities;  // ascending
    Point2 location;                       // where the problem is (free end, crossing, ...)
    std::string detail;
};

struct ProfileOptions {
    // End points closer than this are joined; also the intersection tolerance (mm).
    double tolerance = 1e-6;
};

// Checks the profile curves (construction curves are ignored); empty result = valid.
std::vector<ProfileIssue> checkSketchCurves(const std::vector<SketchCurve>& curves,
                                            const ProfileOptions& options = {});

// ---------------------------------------------------------------------------------------------
// Profile
// ---------------------------------------------------------------------------------------------

struct ProfileEdge {
    SketchEntityId entity = 0;
    TopoDS_Edge edge;  // one edge per profile entity, shared by every wire that uses it
};

// Simple closed contour.
struct ProfileLoop {
    std::vector<SketchEntityId> entities;  // in traversal order, starting at the smallest id
    TopoDS_Wire wire;                      // counter-clockwise seen from the sketch normal
    double area = 0.0;                     // enclosed area, > 0
    int parent = -1;                       // innermost loop that contains this one, -1 = none
    int depth = 0;                         // number of loops that contain this one
    std::vector<std::size_t> children;     // loops whose parent is this loop
};

// Region bounded by loop `outerLoop` from outside and by its child loops from inside. Region i is
// bounded by loop i, so there are as many regions as loops.
struct ProfileRegion {
    std::size_t outerLoop = 0;
    std::vector<std::size_t> holeLoops;
    TopoDS_Face face;  // planar, normal = sketch normal
    double area = 0.0;
    int depth = 0;     // depth of the outer loop: 0 outer contour, 1 hole, 2 island, ...
};

class SketchProfile {
public:
    // Builds loops, nesting and regions; any issue from checkSketchCurves is an InvalidInput
    // error whose message lists every issue with its entity ids.
    static Result<SketchProfile> build(const SketchPlacement& placement,
                                       const std::vector<SketchCurve>& curves,
                                       const ProfileOptions& options = {});

    const SketchPlacement& placement() const noexcept { return placement_; }
    const std::vector<ProfileLoop>& loops() const noexcept { return loops_; }
    const std::vector<ProfileRegion>& regions() const noexcept { return regions_; }
    const std::vector<ProfileEdge>& edges() const noexcept { return edges_; }

    // Sketch entity an edge (or an edge sharing its TShape) was made from.
    std::optional<SketchEntityId> entityOf(const TopoDS_Shape& edge) const;

    // Innermost region strictly containing the point; nullopt when outside every loop or on a
    // boundary.
    std::optional<std::size_t> regionAt(const Point2& point) const;

    // Regions of even depth: outer contours and islands (the usual "material" of a profile).
    std::vector<std::size_t> materialRegions() const;

    // Face (one region) or compound of faces (several). Errors: empty selection, index out of
    // range, duplicate index, point outside every region.
    Result<TopoDS_Shape> makeFaces(const std::vector<std::size_t>& regionIndices) const;
    Result<TopoDS_Shape> makeFacesAt(const std::vector<Point2>& points) const;
    Result<TopoDS_Shape> makeMaterialFaces() const;

private:
    SketchProfile(const SketchPlacement& placement) : placement_(placement) {}

    SketchPlacement placement_;
    std::vector<ProfileLoop> loops_;
    std::vector<ProfileRegion> regions_;
    std::vector<ProfileEdge> edges_;
};

// ---------------------------------------------------------------------------------------------
// Path: one chain of curves (sweep trajectory, rib contour)
// ---------------------------------------------------------------------------------------------

// Profile curves of a sketch joined end to end into one chain, open or closed
// (docs/modeling/SKETCH_PROFILE.md, "Траектория").
struct SketchPath {
    TopoDS_Wire wire;                      // edges in traversal order (from a free end, if any)
    std::vector<TopoDS_Edge> edges;        // the same edges, oriented along the traversal
    std::vector<SketchEntityId> entities;  // entity of each edge
    bool closed = false;
};

// Errors (InvalidInput): no profile curves, any profile issue other than an open end
// (branching, crossing, overlap, degenerate curve), several separate chains.
Result<SketchPath> buildSketchPath(const SketchPlacement& placement, const std::vector<SketchCurve>& curves,
                                   const ProfileOptions& options = {});

// ---------------------------------------------------------------------------------------------
// Binding to the PlaneGCS sketch (src/sketch2d.cpp)
// ---------------------------------------------------------------------------------------------

// Entity layer over the PlaneGCS sketch (docs/modeling/SKETCH.md): profile entity ids 1, 2, 3, ...
// in creation order (never reused; removed entities keep their id), each mapped to a
// sketch2d geometry. Arcs are native PlaneGCS arcs, ellipses/elliptical arcs native PlaneGCS
// conics, B-splines have sketch points as poles. The construction flag lives in sketch2d.
class BoundSketch {
public:
    enum class EntityKind { Line, Circle, Arc, Ellipse, EllipticalArc, BSpline };

    struct Entity {
        SketchEntityId id = 0;
        EntityKind kind = EntityKind::Line;
        bool construction = false;
        bool removed = false;
        sketch2d::Geo geo;         // the sketch2d geometry
        sketch2d::Id line = -1;    // Line: sketch2d line id
        sketch2d::Id circle = -1;  // Circle: sketch2d circle id
        sketch2d::Id arc = -1;     // Arc: sketch2d arc id
        sketch2d::Id start = -1;   // Line, Arc, EllipticalArc: start point
        sketch2d::Id end = -1;     // Line, Arc, EllipticalArc: end point
        sketch2d::Id center = -1;  // Circle, Arc, Ellipse, EllipticalArc: centre point
        sketch2d::Id focus = -1;   // Ellipse, EllipticalArc: focus point (major axis direction)
        bool counterClockwise = true;
    };

    sketch2d::Sketch& sketch() noexcept { return sketch_; }
    const sketch2d::Sketch& sketch() const noexcept { return sketch_; }

    SketchEntityId addLine(sketch2d::Id start, sketch2d::Id end, bool construction = false);
    SketchEntityId addCircle(sketch2d::Id center, double radius, bool construction = false);
    // Native PlaneGCS arc (radius = current |start - center|).
    SketchEntityId addArc(sketch2d::Id center, sketch2d::Id start, sketch2d::Id end,
                          bool counterClockwise, bool construction = false);
    SketchEntityId addEllipse(sketch2d::Id center, sketch2d::Id focus, double minorRadius,
                              bool construction = false);
    SketchEntityId addEllipticalArc(sketch2d::Id center, sketch2d::Id focus, double minorRadius,
                                    sketch2d::Id start, sketch2d::Id end, bool counterClockwise,
                                    bool construction = false);
    SketchEntityId addBSpline(const sketch2d::BSplineData& data, bool construction = false);
    // Throws std::out_of_range for an unknown id (programming error, like sketch2d itself).
    void setConstruction(SketchEntityId id, bool construction);
    // Removes the entity and the constraints that reference its geometry (not its points).
    void removeEntity(SketchEntityId id);
    // Replace the end points of a line or an arc (editing tools).
    void setEndPoints(SketchEntityId id, sketch2d::Id start, sketch2d::Id end);

    const std::vector<Entity>& entities() const noexcept { return entities_; }
    const Entity& entity(SketchEntityId id) const;
    std::optional<SketchEntityId> entityOf(const sketch2d::Geo& geo) const;

    sketch2d::Report solve() { return sketch_.solve(); }

    // Curve of one entity from the current point coordinates and radii. Error when an arc end point
    // is off its circle/ellipse by more than `tolerance` (sketch not solved), or the entity is removed.
    Result<SketchCurve> curve(SketchEntityId id, double tolerance = 1e-6) const;
    // Curves of all live entities. Degenerate curves are reported later by checkSketchCurves /
    // SketchProfile::build.
    Result<std::vector<SketchCurve>> curves(double tolerance = 1e-6) const;

private:
    SketchEntityId push(Entity e);
    sketch2d::Sketch sketch_;
    std::vector<Entity> entities_;
};

// ---------------------------------------------------------------------------------------------
// Projection of model geometry onto the sketch plane
// ---------------------------------------------------------------------------------------------

struct ProjectedEdge {
    std::optional<std::uint32_t> edgeIndex;  // TopologyIndex edge, when projected by index
    // An edge parallel to the sketch normal projects to a point; then `curve` is unset.
    std::optional<Curve2d> curve;
    std::optional<Point2> point;
};

// Orthogonal projection along the sketch normal. Lines -> LineSegment2d; circles -> Circle2d /
// CircularArc2d on a parallel plane and Ellipse2d / EllipticalArc2d otherwise; ellipses ->
// ellipses (or circles); other curves -> BSpline2d. Errors: degenerated or curveless edge.
Result<ProjectedEdge> projectEdge(const SketchPlacement& placement, const TopoDS_Edge& edge,
                                  double tolerance = 1e-6);
Result<std::vector<ProjectedEdge>> projectEdges(const SketchPlacement& placement,
                                                const kernel::TopologyIndex& topology,
                                                const std::vector<std::uint32_t>& edgeIndices,
                                                double tolerance = 1e-6);
Result<Point2> projectVertex(const SketchPlacement& placement,
                             const kernel::TopologyIndex& topology, std::uint32_t vertexIndex);

}  // namespace bendcore::modeling
