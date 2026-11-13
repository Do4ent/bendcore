#include "sketch_internal.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <numeric>
#include <set>
#include <sstream>

#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepClass_FaceClassifier.hxx>
#include <BRepGProp.hxx>
#include <BRep_Builder.hxx>
#include <BndLib_Add2dCurve.hxx>
#include <Bnd_Box2d.hxx>
#include <GProp_GProps.hxx>
#include <Geom2dAPI_InterCurveCurve.hxx>
#include <Geom2d_BSplineCurve.hxx>
#include <Geom2d_Line.hxx>
#include <Geom2d_TrimmedCurve.hxx>
#include <GeomAPI.hxx>
#include <Geom_Curve.hxx>
#include <Geom_Plane.hxx>
#include <Precision.hxx>
#include <TopAbs_State.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Vertex.hxx>
#include <gp_Vec2d.hxx>

namespace bendcore::modeling {

const char* toString(ProfileIssueKind kind) noexcept {
    switch (kind) {
        case ProfileIssueKind::DuplicateId: return "duplicate id";
        case ProfileIssueKind::Degenerate: return "degenerate curve";
        case ProfileIssueKind::OpenContour: return "open contour";
        case ProfileIssueKind::Branching: return "branching";
        case ProfileIssueKind::Intersection: return "intersection";
        case ProfileIssueKind::Overlap: return "overlap";
        case ProfileIssueKind::SelfIntersection: return "self-intersection";
        case ProfileIssueKind::Empty: return "empty profile";
    }
    return "unknown";
}

namespace {

using detail::formatPoint;
using detail::Geom2dSegment;
using detail::toPoint2;

struct Incidence {
    std::size_t curve = 0;
    int end = 0;  // 0 = start, 1 = end
};

struct Node {
    gp_Pnt2d point;
    double spread = 0.0;  // largest distance of a joined end point from `point`
    std::vector<Incidence> incidences;
};

struct Prepared {
    SketchEntityId id = 0;
    Geom2dSegment segment;
    occ::handle<Geom2d_Curve> trimmed;  // basis on [first, last]
    Bnd_Box2d box;
    int node[2] = {-1, -1};
};

struct Analysis {
    std::vector<ProfileIssue> issues;
    std::vector<Prepared> curves;
    std::vector<Node> nodes;
};

void addIssue(std::vector<ProfileIssue>& issues, ProfileIssueKind kind,
              std::vector<SketchEntityId> entities, const Point2& at, std::string detail) {
    std::sort(entities.begin(), entities.end());
    entities.erase(std::unique(entities.begin(), entities.end()), entities.end());
    issues.push_back(ProfileIssue{kind, std::move(entities), at, std::move(detail)});
}

bool sharedNodeNear(const Prepared& a, const Prepared& b, const std::vector<Node>& nodes,
                    const gp_Pnt2d& p, double radius) {
    for (int ea = 0; ea < 2; ++ea) {
        if (a.node[ea] < 0) continue;
        for (int eb = 0; eb < 2; ++eb) {
            if (a.node[ea] != b.node[eb]) continue;
            const Node& node = nodes[static_cast<std::size_t>(a.node[ea])];
            if (node.point.Distance(p) <= radius + node.spread) return true;
        }
    }
    return false;
}

Analysis analyse(const std::vector<SketchCurve>& input, const ProfileOptions& options) {
    Analysis result;
    const double tol = options.tolerance;
    // Contacts at a shared end point are located only to the accuracy of the intersector,
    // which is much worse than tol for tangent junctions (line tangent to an arc).
    const double junctionRadius = std::max(1000.0 * tol, 1e-4);

    std::map<SketchEntityId, int> seen;
    for (const SketchCurve& c : input) ++seen[c.id];
    for (const auto& [id, count] : seen) {
        if (count > 1) {
            addIssue(result.issues, ProfileIssueKind::DuplicateId, {id}, Point2{},
                     std::to_string(count) + " curves share id " + std::to_string(id));
        }
    }

    bool anyProfileCurve = false;
    for (const SketchCurve& c : input) {
        if (c.construction) continue;
        anyProfileCurve = true;
        auto segment = detail::toGeom2d(c.geometry, tol);
        if (!segment.ok()) {
            addIssue(result.issues, ProfileIssueKind::Degenerate, {c.id}, Point2{},
                     segment.error().message);
            continue;
        }
        Prepared p;
        p.id = c.id;
        p.segment = segment.value();
        p.trimmed = new Geom2d_TrimmedCurve(p.segment.basis, p.segment.first, p.segment.last);
        BndLib_Add2dCurve::Add(p.trimmed, tol, p.box);
        result.curves.push_back(std::move(p));
    }
    if (!anyProfileCurve) {
        addIssue(result.issues, ProfileIssueKind::Empty, {}, Point2{},
                 "the sketch has no profile (non-construction) curves");
        return result;
    }

    // Join end points of open curves into nodes.
    for (std::size_t i = 0; i < result.curves.size(); ++i) {
        Prepared& p = result.curves[i];
        if (p.segment.closed) continue;
        for (int end = 0; end < 2; ++end) {
            const gp_Pnt2d point = p.segment.basis->Value(end == 0 ? p.segment.first : p.segment.last);
            int found = -1;
            for (std::size_t n = 0; n < result.nodes.size(); ++n) {
                if (result.nodes[n].point.Distance(point) <= tol) {
                    found = static_cast<int>(n);
                    break;
                }
            }
            if (found < 0) {
                result.nodes.push_back(Node{point, 0.0, {}});
                found = static_cast<int>(result.nodes.size() - 1);
            }
            Node& node = result.nodes[static_cast<std::size_t>(found)];
            node.spread = std::max(node.spread, node.point.Distance(point));
            node.incidences.push_back(Incidence{i, end});
            p.node[end] = found;
        }
    }

    // Pairwise intersections: allowed only at a node both curves end in.
    for (std::size_t i = 0; i < result.curves.size(); ++i) {
        for (std::size_t j = i + 1; j < result.curves.size(); ++j) {
            const Prepared& a = result.curves[i];
            const Prepared& b = result.curves[j];
            if (a.box.IsOut(b.box)) continue;
            const Geom2dAPI_InterCurveCurve inter(a.trimmed, b.trimmed, tol);
            for (int k = 1; k <= inter.NbSegments(); ++k) {
                occ::handle<Geom2d_Curve> s1;
                occ::handle<Geom2d_Curve> s2;
                inter.Segment(k, s1, s2);
                const gp_Pnt2d from = s1->Value(s1->FirstParameter());
                const gp_Pnt2d to = s1->Value(s1->LastParameter());
                if (from.Distance(to) <= junctionRadius &&
                    sharedNodeNear(a, b, result.nodes, from, junctionRadius)) {
                    continue;  // tangent contact at the shared end point
                }
                addIssue(result.issues, ProfileIssueKind::Overlap, {a.id, b.id}, toPoint2(from),
                         "curves " + std::to_string(a.id) + " and " + std::to_string(b.id) +
                             " overlap from " + formatPoint(toPoint2(from)) + " to " +
                             formatPoint(toPoint2(to)));
            }
            for (int k = 1; k <= inter.NbPoints(); ++k) {
                const gp_Pnt2d point = inter.Point(k);
                if (sharedNodeNear(a, b, result.nodes, point, junctionRadius)) continue;
                addIssue(result.issues, ProfileIssueKind::Intersection, {a.id, b.id},
                         toPoint2(point),
                         "curves " + std::to_string(a.id) + " and " + std::to_string(b.id) +
                             " meet at " + formatPoint(toPoint2(point)) +
                             " which is not a shared end point");
            }
        }
    }

    // Self-intersections (only free-form curves can cross themselves).
    for (const Prepared& p : result.curves) {
        if (p.segment.basis->DynamicType() != STANDARD_TYPE(Geom2d_BSplineCurve)) continue;
        const Geom2dAPI_InterCurveCurve inter(p.trimmed, tol);
        const gp_Pnt2d start = p.segment.basis->Value(p.segment.first);
        for (int k = 1; k <= inter.NbPoints(); ++k) {
            const gp_Pnt2d point = inter.Point(k);
            if (p.segment.closed && point.Distance(start) <= junctionRadius) continue;
            addIssue(result.issues, ProfileIssueKind::SelfIntersection, {p.id}, toPoint2(point),
                     "curve " + std::to_string(p.id) + " crosses itself at " +
                         formatPoint(toPoint2(point)));
        }
        for (int k = 1; k <= inter.NbSegments(); ++k) {
            addIssue(result.issues, ProfileIssueKind::SelfIntersection, {p.id}, toPoint2(start),
                     "curve " + std::to_string(p.id) + " overlaps itself");
        }
    }

    // Node degrees: 1 = free end of an open chain, > 2 = branching.
    std::vector<std::size_t> component(result.curves.size());
    std::iota(component.begin(), component.end(), 0);
    auto find = [&](std::size_t x) {
        while (component[x] != x) x = component[x] = component[component[x]];
        return x;
    };
    for (const Node& node : result.nodes) {
        for (std::size_t k = 1; k < node.incidences.size(); ++k) {
            component[find(node.incidences[k].curve)] = find(node.incidences[0].curve);
        }
    }
    std::set<std::size_t> openComponents;
    for (const Node& node : result.nodes) {
        if (node.incidences.size() == 1) {
            const std::size_t root = find(node.incidences[0].curve);
            if (!openComponents.insert(root).second) continue;
            std::vector<SketchEntityId> chain;
            for (std::size_t i = 0; i < result.curves.size(); ++i) {
                if (find(i) == root) chain.push_back(result.curves[i].id);
            }
            addIssue(result.issues, ProfileIssueKind::OpenContour, chain, toPoint2(node.point),
                     "contour is not closed: free end at " + formatPoint(toPoint2(node.point)));
        } else if (node.incidences.size() > 2) {
            std::vector<SketchEntityId> ids;
            for (const Incidence& inc : node.incidences) ids.push_back(result.curves[inc.curve].id);
            addIssue(result.issues, ProfileIssueKind::Branching, ids, toPoint2(node.point),
                     std::to_string(node.incidences.size()) + " curve ends meet at " +
                         formatPoint(toPoint2(node.point)));
        }
    }

    std::stable_sort(result.issues.begin(), result.issues.end(),
                     [](const ProfileIssue& x, const ProfileIssue& y) {
                         if (x.kind != y.kind) return x.kind < y.kind;
                         return x.entities < y.entities;
                     });
    return result;
}

// Signed area contribution  integral x dy  of a curve piece (Gauss-Legendre, 5 points per
// sub-interval); exact for lines, ~1e-15 relative for arcs with 64 sub-intervals.
double greenIntegral(const occ::handle<Geom2d_Curve>& c, double first, double last,
                     int intervals) {
    static const double nodes[5] = {-0.9061798459386640, -0.5384693101056831, 0.0,
                                    0.5384693101056831, 0.9061798459386640};
    static const double weights[5] = {0.2369268850561891, 0.4786286704993665,
                                      0.5688888888888889, 0.4786286704993665,
                                      0.2369268850561891};
    double sum = 0.0;
    const double h = (last - first) / intervals;
    for (int k = 0; k < intervals; ++k) {
        const double mid = first + (k + 0.5) * h;
        for (int q = 0; q < 5; ++q) {
            gp_Pnt2d p;
            gp_Vec2d d;
            c->D1(mid + 0.5 * h * nodes[q], p, d);
            sum += weights[q] * p.X() * d.Y() * 0.5 * h;
        }
    }
    return sum;
}

int quadratureIntervals(const Geom2dSegment& s) {
    if (s.basis->DynamicType() == STANDARD_TYPE(Geom2d_Line)) return 1;
    if (auto spline = occ::down_cast<Geom2d_BSplineCurve>(s.basis)) {
        return std::max(64, 8 * spline->NbKnots() * (spline->Degree() + 1));
    }
    return 64;
}

std::string issuesText(const std::vector<ProfileIssue>& issues) {
    std::ostringstream out;
    out << "sketch profile is invalid:";
    for (const ProfileIssue& issue : issues) {
        out << "\n  " << toString(issue.kind);
        if (!issue.entities.empty()) {
            out << " [entities";
            for (std::size_t i = 0; i < issue.entities.size(); ++i) {
                out << (i == 0 ? " " : ", ") << issue.entities[i];
            }
            out << "]";
        }
        out << ": " << issue.detail;
    }
    return out.str();
}

Result<TopoDS_Shape> faceFailure(const std::string& text) {
    return Result<TopoDS_Shape>::failure(ErrorCode::InvalidInput, text);
}

}  // namespace

std::vector<ProfileIssue> checkSketchCurves(const std::vector<SketchCurve>& curves,
                                            const ProfileOptions& options) {
    try {
        return analyse(curves, options).issues;
    } catch (const Standard_Failure& failure) {
        return {ProfileIssue{ProfileIssueKind::Degenerate, {}, Point2{},
                             "curve analysis failed: " + detail::failureText(failure)}};
    }
}

Result<SketchProfile> SketchProfile::build(const SketchPlacement& placement,
                                           const std::vector<SketchCurve>& curves,
                                           const ProfileOptions& options) {
    using R = Result<SketchProfile>;
    if (!(options.tolerance > 0.0)) {
        return R::failure(ErrorCode::InvalidInput, "profile tolerance must be positive");
    }
    try {
        Analysis analysis = analyse(curves, options);
        if (!analysis.issues.empty()) {
            return R::failure(ErrorCode::InvalidInput, issuesText(analysis.issues));
        }
        SketchProfile profile(placement);
        const gp_Pln plane = placement.plane();
        occ::handle<Geom_Plane> surface = new Geom_Plane(placement.frame());
        BRep_Builder builder;

        // One vertex per node, tolerance covering the joined end points.
        std::vector<TopoDS_Vertex> vertices(analysis.nodes.size());
        for (std::size_t n = 0; n < analysis.nodes.size(); ++n) {
            const Node& node = analysis.nodes[n];
            const double vertexTol = std::max(Precision::Confusion(), 1.01 * node.spread);
            builder.MakeVertex(vertices[n],
                               placement.toWorld(toPoint2(node.point)), vertexTol);
        }

        // One edge per curve.
        std::vector<TopoDS_Edge> edges(analysis.curves.size());
        for (std::size_t i = 0; i < analysis.curves.size(); ++i) {
            const Prepared& p = analysis.curves[i];
            const occ::handle<Geom_Curve> c3d = GeomAPI::To3d(p.segment.basis, plane);
            BRepBuilderAPI_MakeEdge make =
                p.segment.closed
                    ? BRepBuilderAPI_MakeEdge(c3d, p.segment.first, p.segment.last)
                    : BRepBuilderAPI_MakeEdge(c3d, vertices[static_cast<std::size_t>(p.node[0])],
                                              vertices[static_cast<std::size_t>(p.node[1])],
                                              p.segment.first, p.segment.last);
            if (!make.IsDone()) {
                return R::failure(ErrorCode::OperationFailed,
                                  "cannot make an edge for sketch entity " + std::to_string(p.id) +
                                      " (BRepBuilderAPI_EdgeError " +
                                      std::to_string(static_cast<int>(make.Error())) + ")");
            }
            edges[i] = make.Edge();
            profile.edges_.push_back(ProfileEdge{p.id, edges[i]});
        }
        std::sort(profile.edges_.begin(), profile.edges_.end(),
                  [](const ProfileEdge& x, const ProfileEdge& y) { return x.entity < y.entity; });

        // Trace loops. Every node has exactly two incidences here.
        struct Step {
            std::size_t curve;
            bool forward;
        };
        std::vector<std::vector<Step>> loopSteps;
        std::vector<bool> used(analysis.curves.size(), false);
        std::vector<std::size_t> order(analysis.curves.size());
        std::iota(order.begin(), order.end(), 0);
        std::sort(order.begin(), order.end(), [&](std::size_t x, std::size_t y) {
            return analysis.curves[x].id < analysis.curves[y].id;
        });
        for (std::size_t startCurve : order) {
            if (used[startCurve]) continue;
            std::vector<Step> steps;
            std::size_t current = startCurve;
            bool forward = true;
            while (true) {
                used[current] = true;
                steps.push_back(Step{current, forward});
                const Prepared& p = analysis.curves[current];
                if (p.segment.closed) break;
                const int exitNode = p.node[forward ? 1 : 0];
                const Node& node = analysis.nodes[static_cast<std::size_t>(exitNode)];
                const Incidence& next =
                    (node.incidences[0].curve == current && node.incidences[0].end == (forward ? 1 : 0))
                        ? node.incidences[1]
                        : node.incidences[0];
                if (next.curve == startCurve) break;
                current = next.curve;
                forward = next.end == 0;
            }
            loopSteps.push_back(std::move(steps));
        }

        for (const auto& steps : loopSteps) {
            ProfileLoop loop;
            BRepBuilderAPI_MakeWire makeWire;
            double signedArea = 0.0;
            for (const Step& step : steps) {
                const Prepared& p = analysis.curves[step.curve];
                loop.entities.push_back(p.id);
                makeWire.Add(step.forward ? edges[step.curve]
                                          : TopoDS::Edge(edges[step.curve].Reversed()));
                const double integral = greenIntegral(p.segment.basis, p.segment.first,
                                                      p.segment.last, quadratureIntervals(p.segment));
                signedArea += step.forward ? integral : -integral;
            }
            if (!makeWire.IsDone()) {
                std::string ids;
                for (SketchEntityId id : loop.entities) ids += " " + std::to_string(id);
                return R::failure(ErrorCode::OperationFailed,
                                  "cannot make a wire from sketch entities" + ids);
            }
            loop.wire = makeWire.Wire();
            if (signedArea < 0.0) loop.wire.Reverse();
            loop.area = std::abs(signedArea);
            profile.loops_.push_back(std::move(loop));
        }

        // Nesting: classify one interior point of every loop against every other loop's face.
        const std::size_t loopCount = profile.loops_.size();
        std::vector<TopoDS_Face> loopFaces(loopCount);
        for (std::size_t i = 0; i < loopCount; ++i) {
            BRepBuilderAPI_MakeFace makeFace(surface, profile.loops_[i].wire, true);
            if (!makeFace.IsDone()) {
                return R::failure(ErrorCode::OperationFailed,
                                  "cannot make a face from the loop starting at entity " +
                                      std::to_string(profile.loops_[i].entities.front()));
            }
            loopFaces[i] = makeFace.Face();
        }
        for (std::size_t i = 0; i < loopCount; ++i) {
            const Step& first = loopSteps[i].front();
            const Geom2dSegment& s = analysis.curves[first.curve].segment;
            const gp_Pnt2d probe = s.basis->Value(0.5 * (s.first + s.last));
            double parentArea = 0.0;
            for (std::size_t j = 0; j < loopCount; ++j) {
                if (j == i) continue;
                const BRepClass_FaceClassifier classifier(loopFaces[j], probe, options.tolerance);
                if (classifier.State() != TopAbs_IN) continue;
                ++profile.loops_[i].depth;
                if (profile.loops_[i].parent < 0 || profile.loops_[j].area < parentArea) {
                    profile.loops_[i].parent = static_cast<int>(j);
                    parentArea = profile.loops_[j].area;
                }
            }
        }
        for (std::size_t i = 0; i < loopCount; ++i) {
            if (profile.loops_[i].parent >= 0) {
                profile.loops_[static_cast<std::size_t>(profile.loops_[i].parent)].children.push_back(i);
            }
        }

        // Regions.
        for (std::size_t i = 0; i < loopCount; ++i) {
            const ProfileLoop& loop = profile.loops_[i];
            BRepBuilderAPI_MakeFace makeFace(surface, loop.wire, true);
            for (std::size_t child : loop.children) {
                makeFace.Add(TopoDS::Wire(profile.loops_[child].wire.Reversed()));
            }
            if (!makeFace.IsDone()) {
                return R::failure(ErrorCode::OperationFailed,
                                  "cannot make the face of region " + std::to_string(i));
            }
            ProfileRegion region;
            region.outerLoop = i;
            region.holeLoops = loop.children;
            region.depth = loop.depth;
            region.face = makeFace.Face();
            const BRepCheck_Analyzer check(region.face);
            if (!check.IsValid()) {
                return R::failure(ErrorCode::OperationFailed,
                                  "face of region " + std::to_string(i) + " is invalid");
            }
            // Exact boundary integrals (more accurate than BRepGProp's default surface
            // integration for B-spline boundaries); cross-checked against the face below.
            region.area = loop.area;
            for (std::size_t child : loop.children) region.area -= profile.loops_[child].area;
            GProp_GProps props;
            BRepGProp::SurfaceProperties(region.face, props);
            if (!(props.Mass() > 0.0) ||
                std::abs(props.Mass() - region.area) > 1e-3 * std::max(1.0, region.area)) {
                return R::failure(ErrorCode::OperationFailed,
                                  "face of region " + std::to_string(i) +
                                      " is inconsistent with its boundary (area " +
                                      std::to_string(props.Mass()) + " vs " +
                                      std::to_string(region.area) + ")");
            }
            profile.regions_.push_back(std::move(region));
        }
        return profile;
    } catch (const Standard_Failure& failure) {
        return R::failure(ErrorCode::OperationFailed,
                          "profile construction failed: " + detail::failureText(failure));
    }
}

std::optional<SketchEntityId> SketchProfile::entityOf(const TopoDS_Shape& edge) const {
    for (const ProfileEdge& e : edges_) {
        if (e.edge.IsSame(edge)) return e.entity;
    }
    return std::nullopt;
}

std::optional<std::size_t> SketchProfile::regionAt(const Point2& point) const {
    const gp_Pnt2d probe(point.x, point.y);
    for (std::size_t i = 0; i < regions_.size(); ++i) {
        const BRepClass_FaceClassifier classifier(regions_[i].face, probe, Precision::Confusion());
        if (classifier.State() == TopAbs_IN) return i;
    }
    return std::nullopt;
}

std::vector<std::size_t> SketchProfile::materialRegions() const {
    std::vector<std::size_t> out;
    for (std::size_t i = 0; i < regions_.size(); ++i) {
        if (regions_[i].depth % 2 == 0) out.push_back(i);
    }
    return out;
}

Result<TopoDS_Shape> SketchProfile::makeFaces(const std::vector<std::size_t>& regionIndices) const {
    if (regionIndices.empty()) return faceFailure("no regions selected");
    std::set<std::size_t> unique;
    for (std::size_t index : regionIndices) {
        if (index >= regions_.size()) {
            return faceFailure("region index " + std::to_string(index) + " out of range (" +
                               std::to_string(regions_.size()) + " regions)");
        }
        if (!unique.insert(index).second) {
            return faceFailure("region " + std::to_string(index) + " selected twice");
        }
    }
    if (regionIndices.size() == 1) return TopoDS_Shape(regions_[regionIndices.front()].face);
    BRep_Builder builder;
    TopoDS_Compound compound;
    builder.MakeCompound(compound);
    for (std::size_t index : regionIndices) builder.Add(compound, regions_[index].face);
    return TopoDS_Shape(compound);
}

Result<TopoDS_Shape> SketchProfile::makeFacesAt(const std::vector<Point2>& points) const {
    std::vector<std::size_t> indices;
    for (const Point2& p : points) {
        const auto region = regionAt(p);
        if (!region) {
            return faceFailure("point " + formatPoint(p) +
                               " is not inside any region (outside or on a boundary)");
        }
        if (std::find(indices.begin(), indices.end(), *region) == indices.end()) {
            indices.push_back(*region);
        }
    }
    return makeFaces(indices);
}

Result<TopoDS_Shape> SketchProfile::makeMaterialFaces() const {
    return makeFaces(materialRegions());
}

Result<SketchPath> buildSketchPath(const SketchPlacement& placement, const std::vector<SketchCurve>& curves,
                                   const ProfileOptions& options) {
    using R = Result<SketchPath>;
    if (!(options.tolerance > 0.0)) return R::failure(ErrorCode::InvalidInput, "profile tolerance must be positive");
    try {
        Analysis analysis = analyse(curves, options);
        std::vector<ProfileIssue> blocking;
        for (const auto& issue : analysis.issues)
            if (issue.kind != ProfileIssueKind::OpenContour) blocking.push_back(issue);
        if (!blocking.empty()) return R::failure(ErrorCode::InvalidInput, issuesText(blocking));
        if (analysis.curves.empty()) return R::failure(ErrorCode::InvalidInput, "the sketch has no path curves");
        const gp_Pln plane = placement.plane();
        BRep_Builder builder;
        std::vector<TopoDS_Vertex> vertices(analysis.nodes.size());
        for (std::size_t n = 0; n < analysis.nodes.size(); ++n) {
            const double vertexTol = std::max(Precision::Confusion(), 1.01 * analysis.nodes[n].spread);
            builder.MakeVertex(vertices[n], placement.toWorld(toPoint2(analysis.nodes[n].point)), vertexTol);
        }
        std::vector<TopoDS_Edge> edges(analysis.curves.size());
        for (std::size_t i = 0; i < analysis.curves.size(); ++i) {
            const Prepared& p = analysis.curves[i];
            const occ::handle<Geom_Curve> c3d = GeomAPI::To3d(p.segment.basis, plane);
            BRepBuilderAPI_MakeEdge make =
                p.segment.closed
                    ? BRepBuilderAPI_MakeEdge(c3d, p.segment.first, p.segment.last)
                    : BRepBuilderAPI_MakeEdge(c3d, vertices[static_cast<std::size_t>(p.node[0])],
                                              vertices[static_cast<std::size_t>(p.node[1])], p.segment.first,
                                              p.segment.last);
            if (!make.IsDone()) {
                return R::failure(ErrorCode::OperationFailed,
                                  "cannot make an edge for sketch entity " + std::to_string(p.id));
            }
            edges[i] = make.Edge();
        }
        // Start: a free end (node with one incidence) of the smallest entity, else the smallest entity.
        std::size_t start = 0;
        int startEnd = 0;
        bool foundFree = false;
        for (std::size_t i = 0; i < analysis.curves.size(); ++i) {
            const Prepared& p = analysis.curves[i];
            if (p.segment.closed) continue;
            for (int e = 0; e < 2; ++e) {
                const Node& node = analysis.nodes[static_cast<std::size_t>(p.node[e])];
                if (node.incidences.size() != 1) continue;
                if (!foundFree || p.id < analysis.curves[start].id) {
                    start = i;
                    startEnd = e;
                    foundFree = true;
                }
            }
        }
        if (!foundFree) {
            for (std::size_t i = 1; i < analysis.curves.size(); ++i)
                if (analysis.curves[i].id < analysis.curves[start].id) start = i;
        }
        SketchPath path;
        BRepBuilderAPI_MakeWire wire;
        std::vector<bool> used(analysis.curves.size(), false);
        std::size_t current = start;
        bool forward = startEnd == 0;
        while (true) {
            used[current] = true;
            const Prepared& p = analysis.curves[current];
            const TopoDS_Edge e = forward ? edges[current] : TopoDS::Edge(edges[current].Reversed());
            wire.Add(e);
            path.edges.push_back(e);
            path.entities.push_back(p.id);
            if (p.segment.closed) break;
            const Node& node = analysis.nodes[static_cast<std::size_t>(p.node[forward ? 1 : 0])];
            std::optional<Incidence> next;
            for (const Incidence& inc : node.incidences)
                if (!(inc.curve == current && inc.end == (forward ? 1 : 0))) next = inc;
            if (!next || used[next->curve]) break;
            current = next->curve;
            forward = next->end == 0;
        }
        if (std::find(used.begin(), used.end(), false) != used.end())
            return R::failure(ErrorCode::InvalidInput, "the sketch path consists of several separate chains");
        if (!wire.IsDone()) return R::failure(ErrorCode::OperationFailed, "cannot join the sketch path into a wire");
        path.wire = wire.Wire();
        path.closed = !foundFree;
        return path;
    } catch (const Standard_Failure& failure) {
        return R::failure(ErrorCode::OperationFailed, "sketch path: " + detail::failureText(failure));
    }
}

}  // namespace bendcore::modeling
