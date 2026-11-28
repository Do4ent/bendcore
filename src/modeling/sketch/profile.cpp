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
#include <Geom2dAPI_ProjectPointOnCurve.hxx>
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

// Point where two curves cross or touch away from a shared end point (split mode).
struct Crossing {
    std::size_t a = 0, b = 0;
    gp_Pnt2d point;
    double reach = 0.0;  // touching curves: half-length of the piece where they are within tol
};

// Part of a curve between split points (the whole curve when nothing crosses it).
struct Piece {
    std::size_t curve = 0;
    std::uint32_t part = 0;
    double first = 0.0, last = 0.0;
    int node[2] = {-1, -1};  // -1, -1: a closed curve without split points
};

struct PieceEnd {
    std::size_t piece = 0;
    int end = 0;
};

struct Analysis {
    std::vector<ProfileIssue> issues;
    std::vector<Prepared> curves;
    std::vector<Node> nodes;  // incidences: curve ends
    std::vector<Crossing> crossings;
    // Planar graph of the parts: nodes are `nodes` (end points and split points).
    std::vector<Piece> pieces;
    std::vector<std::uint32_t> partCount;  // per curve
    std::vector<std::vector<PieceEnd>> nodeEnds;
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

// Point of the piece `c` (its own parameter range) closest to curve `other`: golden-section
// search of the distance, which is unimodal on the short piece where two curves touch.
gp_Pnt2d closestApproach(const occ::handle<Geom2d_Curve>& c, const occ::handle<Geom2d_Curve>& other) {
    auto dist = [&](double t) {
        const gp_Pnt2d p = c->Value(t);
        Geom2dAPI_ProjectPointOnCurve proj(p, other);
        return proj.NbPoints() > 0 ? proj.LowerDistance() : 1e300;
    };
    double lo = c->FirstParameter(), hi = c->LastParameter();
    const double g = 0.5 * (std::sqrt(5.0) - 1.0);
    double x1 = hi - g * (hi - lo), x2 = lo + g * (hi - lo);
    double f1 = dist(x1), f2 = dist(x2);
    for (int k = 0; k < 80 && hi - lo > 1e-15 * std::max(1.0, std::abs(hi)); ++k) {
        if (f1 <= f2) {
            hi = x2; x2 = x1; f2 = f1; x1 = hi - g * (hi - lo); f1 = dist(x1);
        } else {
            lo = x1; x1 = x2; f1 = f2; x2 = lo + g * (hi - lo); f2 = dist(x2);
        }
    }
    return c->Value(0.5 * (lo + hi));
}

void sortIssues(std::vector<ProfileIssue>& issues) {
    std::stable_sort(issues.begin(), issues.end(), [](const ProfileIssue& x, const ProfileIssue& y) {
        if (x.kind != y.kind) return x.kind < y.kind;
        return x.entities < y.entities;
    });
}

double parameterOn(const Prepared& c, const gp_Pnt2d& p) {
    Geom2dAPI_ProjectPointOnCurve proj(p, c.segment.basis, c.segment.first, c.segment.last);
    if (proj.NbPoints() == 0) {
        // Only an end point can be missed by the projection (the seam of a closed curve).
        const gp_Pnt2d a = c.segment.basis->Value(c.segment.first);
        const gp_Pnt2d b = c.segment.basis->Value(c.segment.last);
        return p.Distance(a) <= p.Distance(b) ? c.segment.first : c.segment.last;
    }
    return proj.LowerDistanceParameter();
}

// Split mode: every crossing becomes a node (joined with an end-point node or another crossing
// within `radius`), each curve is cut at the nodes on it into parts, and free ends of the parts
// graph are reported as open contours.
void arrange(Analysis& r, double radius) {
    auto nodeAt = [&](const gp_Pnt2d& p) {
        for (std::size_t n = 0; n < r.nodes.size(); ++n)
            if (r.nodes[n].point.Distance(p) <= radius) return static_cast<int>(n);
        r.nodes.push_back(Node{p, 0.0, {}});
        return static_cast<int>(r.nodes.size() - 1);
    };
    std::vector<std::vector<std::pair<double, int>>> cuts(r.curves.size());
    for (const Crossing& c : r.crossings) {
        const int n = nodeAt(c.point);
        // The vertex covers the piece where touching curves are closer than tol (else the face
        // boundary would be self-intersecting there).
        Node& node = r.nodes[static_cast<std::size_t>(n)];
        node.spread = std::max(node.spread, c.reach);
        for (std::size_t k : {c.a, c.b})
            cuts[k].push_back({parameterOn(r.curves[k], r.nodes[static_cast<std::size_t>(n)].point), n});
    }
    r.partCount.assign(r.curves.size(), 1);
    for (std::size_t i = 0; i < r.curves.size(); ++i) {
        const Prepared& c = r.curves[i];
        const occ::handle<Geom2d_Curve>& g = c.segment.basis;
        int ends[2] = {c.node[0], c.node[1]};
        const bool periodic = c.segment.closed && g->IsPeriodic();
        if (c.segment.closed && !periodic && !cuts[i].empty()) {
            // Closed but not periodic: the start point becomes a node shared by both ends.
            ends[0] = ends[1] = nodeAt(g->Value(c.segment.first));
        }
        auto& sp = cuts[i];
        std::sort(sp.begin(), sp.end());
        std::vector<std::pair<double, int>> cut;
        for (const auto& [t, n] : sp) {
            if (n == ends[0] || n == ends[1]) continue;  // the curve's own end point
            if (std::any_of(cut.begin(), cut.end(), [&](const auto& x) { return x.second == n; })) continue;
            cut.push_back({t, n});
        }
        auto addPiece = [&](std::uint32_t part, double t0, double t1, int n0, int n1) {
            r.pieces.push_back(Piece{i, part, t0, t1, {n0, n1}});
            for (int e = 0; e < 2; ++e) {
                const int n = e == 0 ? n0 : n1;
                if (n < 0) continue;
                Node& node = r.nodes[static_cast<std::size_t>(n)];
                node.spread = std::max(node.spread, node.point.Distance(g->Value(e == 0 ? t0 : t1)));
            }
        };
        if (cut.empty()) {
            addPiece(0, c.segment.first, c.segment.last, ends[0], ends[1]);
            continue;
        }
        if (periodic) {
            r.partCount[i] = static_cast<std::uint32_t>(cut.size());
            const double period = g->Period();
            for (std::size_t k = 0; k < cut.size(); ++k) {
                const bool wrap = k + 1 == cut.size();
                addPiece(static_cast<std::uint32_t>(k), cut[k].first,
                         wrap ? cut.front().first + period : cut[k + 1].first, cut[k].second,
                         wrap ? cut.front().second : cut[k + 1].second);
            }
        } else {
            r.partCount[i] = static_cast<std::uint32_t>(cut.size() + 1);
            double t = c.segment.first;
            int n = ends[0];
            for (std::size_t k = 0; k < cut.size(); ++k) {
                addPiece(static_cast<std::uint32_t>(k), t, cut[k].first, n, cut[k].second);
                t = cut[k].first;
                n = cut[k].second;
            }
            addPiece(static_cast<std::uint32_t>(cut.size()), t, c.segment.last, n, ends[1]);
        }
    }
    r.nodeEnds.assign(r.nodes.size(), {});
    for (std::size_t k = 0; k < r.pieces.size(); ++k)
        for (int e = 0; e < 2; ++e)
            if (r.pieces[k].node[e] >= 0)
                r.nodeEnds[static_cast<std::size_t>(r.pieces[k].node[e])].push_back(PieceEnd{k, e});

    // Free ends of the parts graph.
    std::vector<std::size_t> component(r.pieces.size());
    std::iota(component.begin(), component.end(), 0);
    auto find = [&](std::size_t x) {
        while (component[x] != x) x = component[x] = component[component[x]];
        return x;
    };
    for (const auto& ends : r.nodeEnds)
        for (std::size_t k = 1; k < ends.size(); ++k) component[find(ends[k].piece)] = find(ends[0].piece);
    std::set<std::size_t> reported;
    for (std::size_t n = 0; n < r.nodes.size(); ++n) {
        if (r.nodeEnds[n].size() != 1) continue;
        const std::size_t root = find(r.nodeEnds[n].front().piece);
        if (!reported.insert(root).second) continue;
        std::vector<SketchEntityId> chain;
        for (std::size_t k = 0; k < r.pieces.size(); ++k)
            if (find(k) == root) chain.push_back(r.curves[r.pieces[k].curve].id);
        addIssue(r.issues, ProfileIssueKind::OpenContour, chain, toPoint2(r.nodes[n].point),
                 "contour is not closed: free end at " + formatPoint(toPoint2(r.nodes[n].point)));
    }
}

Analysis analyse(const std::vector<SketchCurve>& input, const ProfileOptions& options) {
    Analysis result;
    const double tol = options.tolerance;
    // Contacts at a shared end point are located only to the accuracy of the intersector,
    // which is much worse than tol for tangent junctions (line tangent to an arc).
    const double junctionRadius = std::max(1000.0 * tol, 1e-4);
    // Longest piece along which two touching curves are reported as overlapping (split mode);
    // a real overlap of curves is always longer.
    const double contactLength = std::max(junctionRadius, 100.0 * std::sqrt(tol));

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
                if (options.splitAtIntersections && from.Distance(to) <= contactLength) {
                    // Curves touching (tangent) are within tol of each other along a short piece
                    // of length ~ 2 sqrt(2 tol / curvature difference): one touching point, the
                    // point of closest approach on that piece.
                    const gp_Pnt2d touch = closestApproach(s1, b.trimmed);
                    if (sharedNodeNear(a, b, result.nodes, touch, junctionRadius)) continue;
                    result.crossings.push_back(Crossing{i, j, touch, std::max(touch.Distance(from), touch.Distance(to))});
                    continue;
                }
                addIssue(result.issues, ProfileIssueKind::Overlap, {a.id, b.id}, toPoint2(from),
                         "curves " + std::to_string(a.id) + " and " + std::to_string(b.id) +
                             " overlap from " + formatPoint(toPoint2(from)) + " to " +
                             formatPoint(toPoint2(to)));
            }
            for (int k = 1; k <= inter.NbPoints(); ++k) {
                const gp_Pnt2d point = inter.Point(k);
                if (sharedNodeNear(a, b, result.nodes, point, junctionRadius)) continue;
                if (options.splitAtIntersections) {
                    result.crossings.push_back(Crossing{i, j, point, 0.0});
                    continue;
                }
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

    if (options.splitAtIntersections) {
        arrange(result, junctionRadius);
        sortIssues(result.issues);
        return result;
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

    // Strict mode: every curve is one part.
    result.partCount.assign(result.curves.size(), 1);
    result.nodeEnds.assign(result.nodes.size(), {});
    for (std::size_t i = 0; i < result.curves.size(); ++i) {
        const Prepared& c = result.curves[i];
        result.pieces.push_back(Piece{i, 0, c.segment.first, c.segment.last, {c.node[0], c.node[1]}});
        for (int e = 0; e < 2; ++e)
            if (c.node[e] >= 0) result.nodeEnds[static_cast<std::size_t>(c.node[e])].push_back(PieceEnd{i, e});
    }
    sortIssues(result.issues);
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

namespace {

// Length of a part (polyline through 32 points; used only to scale the departure probe).
double approximateLength(const Geom2dSegment& s, const Piece& p) {
    double len = 0.0;
    gp_Pnt2d prev = s.basis->Value(p.first);
    for (int i = 1; i <= 32; ++i) {
        const gp_Pnt2d q = s.basis->Value(p.first + (p.last - p.first) * i / 32.0);
        len += prev.Distance(q);
        prev = q;
    }
    return len;
}

// Direction in which a part leaves a node: the angle of the chord to the point at arc length
// `arc` along it. A chord (not the tangent) also orders curves that touch at the node: their
// tangents coincide, the chords differ by arc * (curvature difference) / 2.
double departure(const Geom2dSegment& s, const Piece& p, bool forward, double arc) {
    gp_Pnt2d o;
    gp_Vec2d d;
    s.basis->D1(forward ? p.first : p.last, o, d);
    const double speed = std::max(d.Magnitude(), 1e-300);
    const double dt = std::min(arc / speed, 0.5 * (p.last - p.first));
    const gp_Pnt2d q = s.basis->Value(forward ? p.first + dt : p.last - dt);
    return std::atan2(q.Y() - o.Y(), q.X() - o.X());
}

}  // namespace

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
        const std::vector<Piece>& pieces = analysis.pieces;
        const std::size_t partTotal = pieces.size();
        auto curveOf = [&](std::size_t k) -> const Prepared& { return analysis.curves[pieces[k].curve]; };
        // Order of parts: (entity id, part).
        auto pieceLess = [&](std::size_t x, std::size_t y) {
            const SketchEntityId ix = curveOf(x).id, iy = curveOf(y).id;
            return ix != iy ? ix < iy : pieces[x].part < pieces[y].part;
        };

        // One vertex per node, tolerance covering the joined end points.
        std::vector<TopoDS_Vertex> vertices(analysis.nodes.size());
        for (std::size_t n = 0; n < analysis.nodes.size(); ++n) {
            const Node& node = analysis.nodes[n];
            if (analysis.nodeEnds[n].empty()) continue;
            const double vertexTol = std::max(Precision::Confusion(), 1.01 * node.spread);
            builder.MakeVertex(vertices[n], placement.toWorld(toPoint2(node.point)), vertexTol);
        }

        // One edge per part.
        profile.partEdges_.resize(partTotal);
        profile.partArea_.resize(partTotal);
        for (std::size_t k = 0; k < partTotal; ++k) {
            const Piece& p = pieces[k];
            const Prepared& c = curveOf(k);
            const occ::handle<Geom_Curve> c3d = GeomAPI::To3d(c.segment.basis, plane);
            BRepBuilderAPI_MakeEdge make =
                p.node[0] < 0
                    ? BRepBuilderAPI_MakeEdge(c3d, p.first, p.last)
                    : BRepBuilderAPI_MakeEdge(c3d, vertices[static_cast<std::size_t>(p.node[0])],
                                              vertices[static_cast<std::size_t>(p.node[1])], p.first, p.last);
            if (!make.IsDone()) {
                return R::failure(ErrorCode::OperationFailed,
                                  "cannot make an edge for sketch entity " + std::to_string(c.id) +
                                      (analysis.partCount[p.curve] > 1 ? " part " + std::to_string(p.part) : "") +
                                      " (BRepBuilderAPI_EdgeError " +
                                      std::to_string(static_cast<int>(make.Error())) + ")");
            }
            profile.partEdges_[k] = make.Edge();
            profile.partArea_[k] = greenIntegral(c.segment.basis, p.first, p.last, quadratureIntervals(c.segment));
            profile.edges_.push_back(ProfileEdge{c.id, profile.partEdges_[k], p.part, analysis.partCount[p.curve]});
        }
        std::sort(profile.edges_.begin(), profile.edges_.end(), [](const ProfileEdge& x, const ProfileEdge& y) {
            return x.entity != y.entity ? x.entity < y.entity : x.part < y.part;
        });

        // Half-edges: 2k = part k forward (node[0] -> node[1]), 2k + 1 backward. At every node the
        // outgoing half-edges are sorted counter-clockwise; the successor of h along its face (face
        // on the left) is the outgoing half-edge just clockwise of h's reverse.
        const std::size_t H = 2 * partTotal;
        std::vector<int> next(H, -1);
        auto origin = [&](std::size_t h) { return pieces[h / 2].node[h % 2]; };
        auto target = [&](std::size_t h) { return pieces[h / 2].node[1 - h % 2]; };
        for (std::size_t n = 0; n < analysis.nodes.size(); ++n) {
            double shortest = 0.0;
            for (const PieceEnd& e : analysis.nodeEnds[n]) {
                const double len = approximateLength(curveOf(e.piece).segment, pieces[e.piece]);
                shortest = shortest == 0.0 ? len : std::min(shortest, len);
            }
            std::vector<std::pair<double, std::size_t>> out;
            for (const PieceEnd& e : analysis.nodeEnds[n]) {
                const std::size_t h = 2 * e.piece + static_cast<std::size_t>(e.end);
                out.push_back({departure(curveOf(e.piece).segment, pieces[e.piece], e.end == 0, 0.05 * shortest), h});
            }
            std::sort(out.begin(), out.end());
            const std::size_t m = out.size();
            for (std::size_t i = 0; i < m; ++i) {
                const std::size_t incoming = out[i].second ^ 1u;  // arrives at n, its reverse leaves n
                next[incoming] = static_cast<int>(out[(i + m - 1) % m].second);
            }
        }

        // Cycles: positive area = a region (face of the arrangement), negative = the outline of a
        // connected group of parts. A closed part without nodes is a group of its own.
        struct Cycle {
            std::vector<std::size_t> halves;
            double area = 0.0;
            std::size_t group = 0;
            std::size_t key = 0;  // smallest part
        };
        std::vector<Cycle> cycles;
        std::vector<int> cycleOf(H, -1);
        std::vector<std::size_t> group(partTotal);
        std::iota(group.begin(), group.end(), 0);
        auto find = [&](std::size_t x) {
            while (group[x] != x) x = group[x] = group[group[x]];
            return x;
        };
        for (const auto& ends : analysis.nodeEnds)
            for (std::size_t k = 1; k < ends.size(); ++k) group[find(ends[k].piece)] = find(ends[0].piece);
        for (std::size_t h0 = 0; h0 < H; ++h0) {
            if (cycleOf[h0] >= 0) continue;
            Cycle c;
            std::size_t h = h0;
            while (true) {
                cycleOf[h] = static_cast<int>(cycles.size());
                c.halves.push_back(h);
                c.area += h % 2 == 0 ? profile.partArea_[h / 2] : -profile.partArea_[h / 2];
                const std::size_t k = h / 2;
                if (pieces[k].node[0] < 0) break;  // closed part: each direction is a cycle
                if (next[h] < 0) return R::failure(ErrorCode::OperationFailed, "profile graph is inconsistent");
                h = static_cast<std::size_t>(next[h]);
                if (h == h0) break;
                if (cycleOf[h] >= 0) return R::failure(ErrorCode::OperationFailed, "profile graph is inconsistent");
            }
            c.group = find(c.halves.front() / 2);
            c.key = c.halves.front() / 2;
            for (std::size_t x : c.halves) {
                if (pieceLess(x / 2, c.key)) c.key = x / 2;
                if (cycleOf[x ^ 1u] == cycleOf[x] && pieces[x / 2].node[0] >= 0) {
                    const Prepared& p = curveOf(x / 2);
                    return R::failure(ErrorCode::InvalidInput,
                                      "sketch profile is invalid:\n  open contour [entities " + std::to_string(p.id) +
                                          "]: curve " + std::to_string(p.id) +
                                          " bounds the same region on both sides (a bridge or a dangling chain)");
                }
            }
            cycles.push_back(std::move(c));
        }

        // Regions in the order of their smallest part, then the outlines of groups with several
        // regions.
        std::vector<std::size_t> faceCycles, outlineCycles;
        std::map<std::size_t, std::vector<std::size_t>> groupFaces, groupOutlines;
        for (std::size_t i = 0; i < cycles.size(); ++i) {
            if (cycles[i].area > 0.0) {
                faceCycles.push_back(i);
                groupFaces[cycles[i].group].push_back(i);
            } else {
                groupOutlines[cycles[i].group].push_back(i);
            }
        }
        for (const auto& [g, outs] : groupOutlines) {
            if (outs.size() != 1 || groupFaces[g].empty()) {
                return R::failure(ErrorCode::OperationFailed, "profile group starting at sketch entity " +
                                                                  std::to_string(curveOf(cycles[outs.front()].key).id) +
                                                                  " has no single outline");
            }
            if (groupFaces[g].size() > 1) outlineCycles.push_back(outs.front());
        }
        auto cycleLess = [&](std::size_t x, std::size_t y) { return pieceLess(cycles[x].key, cycles[y].key); };
        std::sort(faceCycles.begin(), faceCycles.end(), cycleLess);
        std::sort(outlineCycles.begin(), outlineCycles.end(), cycleLess);
        std::vector<int> loopOfCycle(cycles.size(), -1);
        std::vector<std::size_t> loopCycle;
        for (std::size_t c : faceCycles) {
            loopOfCycle[c] = static_cast<int>(loopCycle.size());
            loopCycle.push_back(c);
        }
        for (std::size_t c : outlineCycles) {
            loopOfCycle[c] = static_cast<int>(loopCycle.size());
            loopCycle.push_back(c);
        }
        const std::size_t regionCount = faceCycles.size();

        for (std::size_t c : loopCycle) {
            const Cycle& cy = cycles[c];
            // Counter-clockwise half-edges: the region cycle itself, the reverse of an outline.
            std::vector<std::size_t> ccw = cy.halves;
            if (cy.area < 0.0) {
                std::reverse(ccw.begin(), ccw.end());
                for (std::size_t& h : ccw) h ^= 1u;
            }
            ProfileLoop loop;
            BRepBuilderAPI_MakeWire makeWire;
            const std::size_t start = static_cast<std::size_t>(
                std::find_if(ccw.begin(), ccw.end(), [&](std::size_t h) { return h / 2 == cy.key; }) - ccw.begin());
            for (std::size_t i = 0; i < ccw.size(); ++i) {
                const std::size_t h = ccw[(start + i) % ccw.size()];
                makeWire.Add(h % 2 == 0 ? profile.partEdges_[h / 2] : TopoDS::Edge(profile.partEdges_[h / 2].Reversed()));
            }
            // Entities in traversal order from the smallest part, in that part's own direction.
            std::vector<std::size_t> seq;
            for (std::size_t i = 0; i < ccw.size(); ++i) seq.push_back(ccw[(start + i) % ccw.size()]);
            if (seq.front() % 2 == 1) std::reverse(seq.begin() + 1, seq.end());
            for (std::size_t h : seq) {
                loop.entities.push_back(curveOf(h / 2).id);
                loop.parts.push_back(pieces[h / 2].part);
            }
            if (!makeWire.IsDone()) {
                std::string ids;
                for (SketchEntityId id : loop.entities) ids += " " + std::to_string(id);
                return R::failure(ErrorCode::OperationFailed, "cannot make a wire from sketch entities" + ids);
            }
            loop.wire = makeWire.Wire();
            loop.area = std::abs(cy.area);
            profile.loops_.push_back(std::move(loop));
        }

        // Nesting of groups: a point of each group classified against the regions of the others.
        std::vector<TopoDS_Face> regionOuter(regionCount);
        for (std::size_t i = 0; i < regionCount; ++i) {
            BRepBuilderAPI_MakeFace makeFace(surface, profile.loops_[i].wire, true);
            if (!makeFace.IsDone()) {
                return R::failure(ErrorCode::OperationFailed, "cannot make a face from the loop starting at entity " +
                                                                  std::to_string(profile.loops_[i].entities.front()));
            }
            regionOuter[i] = makeFace.Face();
        }
        std::map<std::size_t, int> groupParent, groupDepth;
        std::map<std::size_t, std::size_t> groupOutlineLoop;
        for (const auto& [g, outs] : groupOutlines) {
            const std::size_t c = outs.front();
            groupOutlineLoop[g] = loopOfCycle[c] >= 0 ? static_cast<std::size_t>(loopOfCycle[c])
                                                      : static_cast<std::size_t>(loopOfCycle[groupFaces[g].front()]);
            const Piece& p = pieces[cycles[c].key];
            const gp_Pnt2d probe = curveOf(cycles[c].key).segment.basis->Value(0.5 * (p.first + p.last));
            int parent = -1, depth = 0;
            double parentArea = 0.0;
            for (std::size_t j = 0; j < regionCount; ++j) {
                if (cycles[loopCycle[j]].group == g) continue;
                const BRepClass_FaceClassifier classifier(regionOuter[j], probe, options.tolerance);
                if (classifier.State() != TopAbs_IN) continue;
                ++depth;
                if (parent < 0 || profile.loops_[j].area < parentArea) {
                    parent = static_cast<int>(j);
                    parentArea = profile.loops_[j].area;
                }
            }
            groupParent[g] = parent;
            groupDepth[g] = depth;
        }
        for (std::size_t l = 0; l < profile.loops_.size(); ++l) {
            const std::size_t g = cycles[loopCycle[l]].group;
            profile.loops_[l].parent = groupParent[g];
            profile.loops_[l].depth = groupDepth[g];
        }
        for (std::size_t l = 0; l < profile.loops_.size(); ++l) {
            if (profile.loops_[l].parent >= 0)
                profile.loops_[static_cast<std::size_t>(profile.loops_[l].parent)].children.push_back(l);
        }

        // A boundary that passes a node twice (curves touching inside a region, groups touching
        // at a point inside a region) would make a face touching itself: not supported.
        auto pinch = [&](std::size_t loopIndex) -> std::optional<gp_Pnt2d> {
            std::set<int> seen;
            for (std::size_t h : cycles[loopCycle[loopIndex]].halves) {
                const int n = origin(h);
                if (n >= 0 && !seen.insert(n).second) return analysis.nodes[static_cast<std::size_t>(n)].point;
            }
            return std::nullopt;
        };

        // Regions: a face of the arrangement minus the outlines of the groups directly inside it.
        for (std::size_t i = 0; i < regionCount; ++i) {
            const ProfileLoop& loop = profile.loops_[i];
            ProfileRegion region;
            region.outerLoop = i;
            region.depth = loop.depth;
            for (const auto& [g, parent] : groupParent)
                if (parent == static_cast<int>(i)) region.holeLoops.push_back(groupOutlineLoop[g]);
            std::sort(region.holeLoops.begin(), region.holeLoops.end());
            std::vector<std::size_t> boundary{i};
            boundary.insert(boundary.end(), region.holeLoops.begin(), region.holeLoops.end());
            for (std::size_t l : boundary) {
                if (auto at = pinch(l)) {
                    return R::failure(ErrorCode::Unsupported,
                                      "sketch profile: the boundary of region " + std::to_string(i) +
                                          " touches itself at " + formatPoint(toPoint2(*at)) +
                                          " (curves tangent inside a region or contours touching at a point)");
                }
            }
            BRepBuilderAPI_MakeFace makeFace(surface, loop.wire, true);
            for (std::size_t hole : region.holeLoops) makeFace.Add(TopoDS::Wire(profile.loops_[hole].wire.Reversed()));
            if (!makeFace.IsDone()) {
                return R::failure(ErrorCode::OperationFailed, "cannot make the face of region " + std::to_string(i));
            }
            region.face = makeFace.Face();
            const BRepCheck_Analyzer check(region.face);
            if (!check.IsValid()) {
                return R::failure(ErrorCode::OperationFailed, "face of region " + std::to_string(i) + " is invalid");
            }
            // Exact boundary integrals (more accurate than BRepGProp's default surface
            // integration for B-spline boundaries); cross-checked against the face below.
            region.area = loop.area;
            for (std::size_t hole : region.holeLoops) region.area -= profile.loops_[hole].area;
            GProp_GProps props;
            BRepGProp::SurfaceProperties(region.face, props);
            if (!(props.Mass() > 0.0) || std::abs(props.Mass() - region.area) > 1e-3 * std::max(1.0, region.area)) {
                return R::failure(ErrorCode::OperationFailed,
                                  "face of region " + std::to_string(i) + " is inconsistent with its boundary (area " +
                                      std::to_string(props.Mass()) + " vs " + std::to_string(region.area) + ")");
            }
            profile.regions_.push_back(std::move(region));
        }

        // Faces of the half-edges for merging selected regions.
        profile.halfFace_.assign(H, -1);
        profile.halfNext_.assign(H, -1);
        for (std::size_t h = 0; h < H; ++h) {
            const int l = loopOfCycle[static_cast<std::size_t>(cycleOf[h])];
            const bool isRegion = cycles[static_cast<std::size_t>(cycleOf[h])].area > 0.0;
            profile.halfFace_[h] = isRegion ? l : -1;
            profile.halfNext_[h] = pieces[h / 2].node[0] < 0 ? static_cast<int>(h) : next[h];
        }
        return profile;
    } catch (const Standard_Failure& failure) {
        return R::failure(ErrorCode::OperationFailed, "profile construction failed: " + detail::failureText(failure));
    }
}

std::optional<SketchEntityId> SketchProfile::entityOf(const TopoDS_Shape& edge) const {
    for (const ProfileEdge& e : edges_) {
        if (e.edge.IsSame(edge)) return e.entity;
    }
    return std::nullopt;
}

std::optional<ProfileEdge> SketchProfile::edgeOf(const TopoDS_Shape& edge) const {
    for (const ProfileEdge& e : edges_) {
        if (e.edge.IsSame(edge)) return e;
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

Result<std::vector<ProfileFace>> SketchProfile::makeFaceGroups(const std::vector<std::size_t>& regionIndices) const {
    using R = Result<std::vector<ProfileFace>>;
    if (regionIndices.empty()) return R::failure(ErrorCode::InvalidInput, "no regions selected");
    std::set<std::size_t> unique;
    for (std::size_t index : regionIndices) {
        if (index >= regions_.size()) {
            return R::failure(ErrorCode::InvalidInput, "region index " + std::to_string(index) + " out of range (" +
                                                           std::to_string(regions_.size()) + " regions)");
        }
        if (!unique.insert(index).second) {
            return R::failure(ErrorCode::InvalidInput, "region " + std::to_string(index) + " selected twice");
        }
    }
    const std::size_t H = halfFace_.size();
    // Selected regions that share a part are one group.
    std::vector<std::size_t> group(regions_.size());
    std::iota(group.begin(), group.end(), 0);
    auto find = [&](std::size_t x) {
        while (group[x] != x) x = group[x] = group[group[x]];
        return x;
    };
    for (std::size_t h = 0; h < H; h += 2) {
        const int a = halfFace_[h], b = halfFace_[h + 1];
        if (a >= 0 && b >= 0 && a != b && unique.count(static_cast<std::size_t>(a)) && unique.count(static_cast<std::size_t>(b)))
            group[find(static_cast<std::size_t>(a))] = find(static_cast<std::size_t>(b));
    }
    std::vector<ProfileFace> out;
    std::map<std::size_t, std::size_t> faceOfGroup;
    for (std::size_t index : regionIndices) {
        const std::size_t g = find(index);
        auto [it, inserted] = faceOfGroup.try_emplace(g, out.size());
        if (inserted) out.push_back(ProfileFace{});
        out[it->second].regions.push_back(index);
    }
    try {
        occ::handle<Geom_Plane> surface = new Geom_Plane(placement_.frame());
        for (ProfileFace& f : out) {
            std::sort(f.regions.begin(), f.regions.end());
            if (f.regions.size() == 1) {
                f.face = regions_[f.regions.front()].face;
                continue;
            }
            const std::set<std::size_t> members(f.regions.begin(), f.regions.end());
            auto inside = [&](int face) { return face >= 0 && members.count(static_cast<std::size_t>(face)) > 0; };
            auto boundary = [&](std::size_t h) { return inside(halfFace_[h]) && !inside(halfFace_[h ^ 1u]); };
            // Boundary of the union: half-edges of member regions whose other side is not a member;
            // the successor turns around the end node through member regions until it leaves them.
            std::vector<bool> done(H, false);
            std::vector<std::pair<TopoDS_Wire, double>> cycles;
            for (std::size_t h0 = 0; h0 < H; ++h0) {
                if (done[h0] || !boundary(h0)) continue;
                BRepBuilderAPI_MakeWire wire;
                double area = 0.0;
                std::size_t h = h0;
                for (std::size_t guard = 0; guard <= H; ++guard) {
                    done[h] = true;
                    wire.Add(h % 2 == 0 ? partEdges_[h / 2] : TopoDS::Edge(partEdges_[h / 2].Reversed()));
                    area += h % 2 == 0 ? partArea_[h / 2] : -partArea_[h / 2];
                    std::size_t c = static_cast<std::size_t>(halfNext_[h]);
                    for (std::size_t turn = 0; !boundary(c) && turn <= H; ++turn)
                        c = static_cast<std::size_t>(halfNext_[c ^ 1u]);
                    h = c;
                    if (h == h0) break;
                }
                if (h != h0 || !wire.IsDone())
                    return R::failure(ErrorCode::OperationFailed, "cannot trace the boundary of merged regions");
                cycles.push_back({wire.Wire(), area});
            }
            std::vector<std::size_t> outer;
            for (std::size_t i = 0; i < cycles.size(); ++i)
                if (cycles[i].second > 0.0) outer.push_back(i);
            if (outer.size() != 1) {
                return R::failure(ErrorCode::InvalidInput, "selected regions touch only at a point: they cannot form one face");
            }
            BRepBuilderAPI_MakeFace makeFace(surface, cycles[outer.front()].first, true);
            double expected = 0.0;
            for (std::size_t i = 0; i < cycles.size(); ++i)
                if (i != outer.front()) makeFace.Add(cycles[i].first);  // clockwise: a hole
            for (std::size_t r : f.regions) {
                expected += regions_[r].area;
                for (std::size_t hole : regions_[r].holeLoops) makeFace.Add(TopoDS::Wire(loops_[hole].wire.Reversed()));
            }
            if (!makeFace.IsDone()) return R::failure(ErrorCode::OperationFailed, "cannot make the face of merged regions");
            f.face = makeFace.Face();
            GProp_GProps props;
            BRepGProp::SurfaceProperties(f.face, props);
            if (!BRepCheck_Analyzer(f.face).IsValid() || std::abs(props.Mass() - expected) > 1e-3 * std::max(1.0, expected)) {
                return R::failure(ErrorCode::OperationFailed, "face of merged regions is invalid (area " +
                                                                  std::to_string(props.Mass()) + " vs " + std::to_string(expected) + ")");
            }
        }
    } catch (const Standard_Failure& failure) {
        return R::failure(ErrorCode::OperationFailed, "merging regions failed: " + detail::failureText(failure));
    }
    return out;
}

Result<TopoDS_Shape> SketchProfile::makeFaces(const std::vector<std::size_t>& regionIndices) const {
    auto groups = makeFaceGroups(regionIndices);
    if (!groups) return groups.error();
    if (groups.value().size() == 1) return TopoDS_Shape(groups.value().front().face);
    BRep_Builder builder;
    TopoDS_Compound compound;
    builder.MakeCompound(compound);
    for (const ProfileFace& f : groups.value()) builder.Add(compound, f.face);
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
        // A path is one chain: crossings and branchings are errors, not split points.
        ProfileOptions strict = options;
        strict.splitAtIntersections = false;
        Analysis analysis = analyse(curves, strict);
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
