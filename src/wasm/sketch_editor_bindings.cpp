// Sketch editor entry points of bendcore_modeling_wasm (docs/ui/SKETCH_EDITOR.md).
//
// The sketch node of the part document fails as a whole when its profile is open, when the
// constraints conflict or while a curve is being drawn, and then gives no geometry. The editor
// needs the solved geometry and the diagnostics in exactly these states, so it solves the
// sketch data itself, in the context of the document (variables for expressions, placement and
// projected edges of the node):
//
//   docSketchSolve(handle, featureId, sketch, options)
//     builds the PlaneGCS system of the sketch data exactly as the sketch node does
//     (BoundSketch, same operands, same reference-dimension rules), optionally drags a point
//     (options.drag = {point, x, y}), solves and returns: solved points and curves (polylines
//     sampled from OCCT curves), intersections of curves, dof, definition, conflicting /
//     redundant constraints (numbers 1-based in the sketch data), definition of every point and
//     entity, dimension values (reference ones measured), profile issues, regions of the
//     profile (outer-loop entities, area, depth, selected flag, triangulated face) and the sketch
//     data with free points and radii moved to the solution.
//   docSketchEdit(handle, featureId, sketch, tool)
//     applies an editing tool of bendcore::modeling (sketch_tools.hpp) to the solved sketch and
//     converts the result back into sketch data (removed points and entities are dropped and the
//     rest renumbered; existing constraints keep their expressions).
//
// Point index k of the sketch data is sketch2d point k and entity id e is BoundSketch entity e,
// because both are created in data order.

#include "modeling_bindings_internal.hpp"

#include "../modeling/sketch/sketch_internal.hpp"

#include <bendcore/formula.hpp>
#include <bendcore/kernel/shape.hpp>
#include <bendcore/kernel/tessellation.hpp>
#include <bendcore/modeling/reference_geometry.hpp>
#include <bendcore/modeling/sketch_tools.hpp>

#include <Geom2dAPI_InterCurveCurve.hxx>
#include <Geom2d_TrimmedCurve.hxx>
#include <gp_Ax3.hxx>
#include <gp_Pln.hxx>

#include <algorithm>
#include <cmath>
#include <optional>
#include <set>
#include <variant>

namespace bendcore::wasm_modeling {

namespace {

namespace s2 = bendcore::sketch2d;

constexpr double kPi = 3.14159265358979323846;

std::string num(double v) { return val::global("String")(val(v)).as<std::string>(); }

// ------------------------------------------------------------------ evaluation in the document

double evaluate(const m::PartDocument& doc, const std::string& text, const std::string& what) {
    formula::EvaluationContext ctx;
    for (const auto& [name, expr] : doc.variables()) {
        auto v = doc.variableValue(name);
        if (!v) continue;
        ctx.variables[name] = v.value();
        ctx.variables["$" + name] = v.value();
    }
    double v = 0.0;
    try {
        auto ast = formula::FormulaParser::parse(text);
        if (!ast) throw InputError(what + ": empty expression");
        v = formula::FormulaEvaluator::evaluate(ast, ctx);
    } catch (const InputError&) {
        throw;
    } catch (const std::exception& e) {
        throw InputError(what + " '" + text + "': " + e.what());
    }
    if (!std::isfinite(v)) throw InputError(what + " '" + text + "' is not finite");
    return v;
}

// Sketch frame of the node: from its last result; before a first successful build, the base
// planes are placed directly (face and datum placements need the built node).
m::SketchPlacement placementOf(const m::FeatureInfo& f) {
    if (f.sketchResult) return f.sketchResult->placement;
    auto opt = f.options.find("plane");
    const std::string choice = opt == f.options.end() ? std::string("XY") : opt->second;
    gp_Pln base;
    if (choice == "XY") {
        base = gp_Pln(gp_Ax3(gp::Origin(), gp::DZ(), gp::DX()));
    } else if (choice == "XZ") {
        base = gp_Pln(gp_Ax3(gp::Origin(), -gp::DY(), gp::DX()));
    } else if (choice == "YZ") {
        base = gp_Pln(gp_Ax3(gp::Origin(), gp::DX(), gp::DY()));
    } else {
        std::string why;
        for (const auto& msg : f.messages) why += " " + msg;
        throw InputError("sketch '" + f.id + "' on a " + choice + " plane has no placement until the node builds:" + why);
    }
    auto off = f.values.find("offset");
    auto moved = m::planeOffset(base, off == f.values.end() ? 0.0 : off->second);
    if (!moved) throw InputError(moved.error().message);
    return m::SketchPlacement::fromPlane(moved.value());
}

m::FeatureInfo sketchFeature(const m::PartDocument& doc, const val& idValue) {
    const std::string id = requireString(idValue, "feature id");
    auto info = doc.feature(id);
    if (!info) throw InputError(info.error().message);
    if (info.value().type != "Sketch") throw InputError("feature '" + id + "' is not a sketch");
    return std::move(info).value();
}

// ------------------------------------------------------------------ solver model of sketch data

struct Built {
    m::BoundSketch bs;
    std::vector<int> tags;     // tag of data constraint k
    std::set<int> internal;    // tags added by the build (radius of projected circles)
    std::vector<bool> pinned;  // point k is fixed (fixed point or point of a projected curve)
    double scale = 1.0;
};

// Mirrors the sketch node (src/modeling/document/sketch_features.cpp, SketchFeature::execute):
// the same curves, operands, positivity checks and reference-dimension rule. Projected curves
// keep the positions of the node's last result (the model edges are not re-projected here).
Built build(const m::PartDocument& doc, const m::FeatureInfo& f, const m::SketchData& data) {
    Built out;
    const std::size_t np = data.points.size(), ng = data.geometry.size();
    std::vector<m::Point2> pos(np);
    out.pinned.assign(np, false);
    for (std::size_t k = 0; k < np; ++k) {
        const std::string what = "sketch point " + std::to_string(k);
        pos[k] = {evaluate(doc, data.points[k].x, what + " x"), evaluate(doc, data.points[k].y, what + " y")};
        out.pinned[k] = data.points[k].fixed;
    }
    std::vector<double> radii(ng);
    std::vector<std::optional<double>> projectedRadius(ng);
    const m::SketchResult* last = f.sketchResult.get();
    for (std::size_t g = 0; g < ng; ++g) {
        const m::SketchGeometryDef& def = data.geometry[g];
        const std::string where = "sketch entity " + std::to_string(g + 1);
        radii[g] = evaluate(doc, def.radius, where + " radius");
        if (def.projection < 0) continue;
        for (int p : {def.start, def.end, def.center}) {
            if (p < 0) continue;
            if (static_cast<std::size_t>(p) >= np) throw InputError(where + ": point " + std::to_string(p) + " does not exist");
            out.pinned[static_cast<std::size_t>(p)] = true;
            if (last && static_cast<std::size_t>(p) < last->points.size()) pos[static_cast<std::size_t>(p)] = last->points[static_cast<std::size_t>(p)];
        }
        if (def.type == m::SketchGeometryType::Circle && last) {
            for (const m::SketchCurve& c : last->curves) {
                if (c.id == g + 1 && std::holds_alternative<m::Circle2d>(c.geometry)) {
                    radii[g] = std::get<m::Circle2d>(c.geometry).radius;
                    projectedRadius[g] = radii[g];
                }
            }
        }
    }
    for (std::size_t k = 0; k < np; ++k) out.scale = std::max({out.scale, std::abs(pos[k].x), std::abs(pos[k].y)});

    m::BoundSketch& bs = out.bs;
    for (std::size_t k = 0; k < np; ++k) {
        bs.sketch().add_point(pos[k].x, pos[k].y);
        if (out.pinned[k]) bs.sketch().fix_point(static_cast<s2::Id>(k));
    }
    for (std::size_t g = 0; g < ng; ++g) {
        const m::SketchGeometryDef& def = data.geometry[g];
        const std::string where = "sketch entity " + std::to_string(g + 1) + " (" + m::toString(def.type) + ")";
        try {
            switch (def.type) {
                case m::SketchGeometryType::Line: bs.addLine(def.start, def.end, def.construction); break;
                case m::SketchGeometryType::Circle:
                    if (!(radii[g] > 0)) throw std::invalid_argument("radius must be positive, is " + num(radii[g]));
                    bs.addCircle(def.center, radii[g], def.construction);
                    break;
                case m::SketchGeometryType::Arc:
                    bs.addArc(def.center, def.start, def.end, def.projection >= 0 || def.counterClockwise, def.construction);
                    break;
                case m::SketchGeometryType::Ellipse:
                case m::SketchGeometryType::EllipticalArc:
                    if (!(radii[g] > 0)) throw std::invalid_argument("minor radius must be positive, is " + num(radii[g]));
                    if (def.type == m::SketchGeometryType::Ellipse) {
                        bs.addEllipse(def.center, def.focus, radii[g], def.construction);
                    } else {
                        bs.addEllipticalArc(def.center, def.focus, radii[g], def.start, def.end, def.counterClockwise,
                                            def.construction);
                    }
                    break;
                case m::SketchGeometryType::BSpline: {
                    s2::BSplineData sd;
                    sd.poles = def.poles;
                    sd.degree = def.degree;
                    sd.knots = def.knots;
                    sd.multiplicities = def.multiplicities;
                    sd.weights = def.weights;
                    sd.periodic = def.periodic;
                    bs.addBSpline(sd, def.construction);
                    break;
                }
            }
        } catch (const std::exception& e) {
            throw InputError(where + ": " + e.what());
        }
    }
    for (std::size_t g = 0; g < ng; ++g) {
        if (projectedRadius[g]) {
            out.internal.insert(bs.sketch().constrain_radius(bs.entity(static_cast<m::SketchEntityId>(g + 1)).circle,
                                                             *projectedRadius[g]));
        }
    }
    using SC = m::SketchConstraintType;
    using K = m::BoundSketch::EntityKind;
    for (std::size_t k = 0; k < data.constraints.size(); ++k) {
        const m::SketchConstraintDef& c = data.constraints[k];
        const std::string where = "sketch constraint " + std::to_string(k + 1) + " (" + m::toString(c.type) + ")";
        const double v = evaluate(doc, c.value, where + " value");
        auto ent = [&](int e) -> const m::BoundSketch::Entity& {
            if (e < 1 || static_cast<std::size_t>(e) > ng) throw std::out_of_range("no sketch entity " + std::to_string(e));
            return bs.entity(static_cast<m::SketchEntityId>(e));
        };
        auto geo = [&](int e) { return ent(e).geo; };
        auto lineOf = [&](int e) {
            if (ent(e).kind != K::Line) throw std::invalid_argument("entity " + std::to_string(e) + " is not a line");
            return ent(e).line;
        };
        auto circular = [&](int e) {
            if (ent(e).kind != K::Circle && ent(e).kind != K::Arc) {
                throw std::invalid_argument("entity " + std::to_string(e) + " is not a circle or an arc");
            }
            return ent(e).geo;
        };
        auto positive = [&](const char* what, bool allowZero = false) {
            if (c.driven) return;
            if (!(v > 0) && !(allowZero && v == 0)) throw std::invalid_argument(std::string(what) + " must be positive, is " + num(v));
        };
        s2::Sketch& s = bs.sketch();
        int tag = 0;
        try {
            switch (c.type) {
                case SC::Coincident: tag = s.constrain_coincident(c.a, c.b); break;
                case SC::Horizontal: tag = s.constrain_horizontal(c.a, c.b); break;
                case SC::Vertical: tag = s.constrain_vertical(c.a, c.b); break;
                case SC::Distance:
                    positive("distance");
                    tag = s.constrain_distance(c.a, c.b, v);
                    break;
                case SC::DistanceX: tag = s.constrain_distance_x(c.a, c.b, v); break;
                case SC::DistanceY: tag = s.constrain_distance_y(c.a, c.b, v); break;
                case SC::PointCurveDistance:
                    positive("distance", true);
                    tag = s.constrain_point_curve_distance(c.a, geo(c.b), v);
                    break;
                case SC::Radius:
                    positive("radius");
                    tag = s.constrain_radius_of(circular(c.a), v);
                    break;
                case SC::Diameter:
                    positive("diameter");
                    tag = s.constrain_diameter(circular(c.a), v);
                    break;
                case SC::ArcLength:
                    positive("arc length");
                    if (ent(c.a).kind != K::Arc) throw std::invalid_argument("entity " + std::to_string(c.a) + " is not an arc");
                    tag = s.constrain_arc_length(ent(c.a).arc, v);
                    break;
                case SC::MajorRadius:
                    positive("major radius");
                    tag = s.constrain_major_radius(geo(c.a), v);
                    break;
                case SC::MinorRadius:
                    positive("minor radius");
                    tag = s.constrain_minor_radius(geo(c.a), v);
                    break;
                case SC::Parallel: tag = s.constrain_parallel(lineOf(c.a), lineOf(c.b)); break;
                case SC::Perpendicular: tag = s.constrain_perpendicular(lineOf(c.a), lineOf(c.b)); break;
                case SC::EqualLength: tag = s.constrain_equal_length(lineOf(c.a), lineOf(c.b)); break;
                case SC::Angle: tag = s.constrain_angle(lineOf(c.a), lineOf(c.b), v); break;
                case SC::PointOnLine: tag = s.constrain_point_on_line(c.a, lineOf(c.b)); break;
                case SC::PointOnCircle: tag = s.constrain_point_on(c.a, circular(c.b)); break;
                case SC::EqualRadius: tag = s.constrain_equal(circular(c.a), circular(c.b)); break;
                case SC::Symmetric: tag = s.constrain_symmetric(c.a, c.b, lineOf(c.c)); break;
                case SC::Tangent: tag = s.constrain_tangent(geo(c.a), geo(c.b)); break;
                case SC::Equal: tag = s.constrain_equal(geo(c.a), geo(c.b)); break;
                case SC::Concentric: tag = s.constrain_concentric(geo(c.a), geo(c.b)); break;
                case SC::Midpoint: tag = s.constrain_midpoint(c.a, geo(c.b)); break;
                case SC::PointOn: tag = s.constrain_point_on(c.a, geo(c.b)); break;
                case SC::Fix: tag = s.constrain_fix(c.a); break;
                case SC::SymmetricPoint: tag = s.constrain_symmetric_point(c.a, c.b, c.c); break;
                case SC::Rotation: tag = s.constrain_rotation(c.a, c.b, c.c, v); break;
            }
            if (c.driven) {
                if (!s2::is_dimension(s.constraint(tag).kind)) {
                    throw std::invalid_argument("only a dimension can be a reference (driven) constraint");
                }
                s.set_driving(tag, false);
            }
        } catch (const std::exception& e) {
            throw InputError(where + ": " + e.what());
        }
        out.tags.push_back(tag);
    }
    return out;
}

int indexOfTag(const Built& b, int tag) {
    for (std::size_t k = 0; k < b.tags.size(); ++k) {
        if (b.tags[k] == tag) return static_cast<int>(k) + 1;
    }
    return 0;
}

val dataNumbers(const Built& b, const std::vector<s2::Id>& tags) {
    val out = val::array();
    for (int t : tags) {
        if (int k = indexOfTag(b, t)) out.call<void>("push", k);
    }
    return out;
}

// Solve; with a drag the point is pulled to (x, y) by temporary constraints to a temporary
// fixed point at (x, y): both coordinates, else only x, else only y (a point that can move
// along one direction only follows the cursor in that direction). An attempt counts when it
// converges without conflicts; otherwise the positions are restored and the next one is tried.
// Diagnostics always come from a final solve without the temporary geometry.
s2::Report solve(Built& b, std::optional<std::pair<int, m::Point2>> drag) {
    s2::Sketch& s = b.bs.sketch();
    if (drag) {
        const int p = drag->first;
        const m::Point2 at = drag->second;
        std::vector<s2::Point2> start;
        for (std::size_t k = 0; k < b.pinned.size(); ++k) start.push_back(s.point(static_cast<s2::Id>(k)));
        const s2::Id target = s.add_point(at.x, at.y);
        s.fix_point(target);
        enum class Pull { Both, X, Y };
        for (Pull pull : {Pull::Both, Pull::X, Pull::Y}) {
            std::vector<s2::Id> temp;
            if (pull != Pull::Y) temp.push_back(s.constrain_distance_x(target, p, 0.0));
            if (pull != Pull::X) temp.push_back(s.constrain_distance_y(target, p, 0.0));
            const s2::Report pulled = s.solve();
            for (s2::Id t : temp) s.remove_constraint(t);
            if (pulled.converged && pulled.conflicting.empty()) break;
            for (std::size_t k = 0; k < start.size(); ++k) {
                if (!b.pinned[k]) s.set_point(static_cast<s2::Id>(k), start[k].x, start[k].y);
            }
        }
        s.remove(s2::point_geo(target));
    }
    return s.solve();
}

// ------------------------------------------------------------------ curves for display

struct Sampled {
    std::vector<double> xy;
    std::optional<m::Point2> mid;
    occ::handle<Geom2d_Curve> curve;  // trimmed to the curve piece (null when degenerate)
};

Sampled sample(const m::SketchCurve& c, double tolerance) {
    Sampled out;
    auto seg = m::detail::toGeom2d(c.geometry, tolerance);
    if (!seg) return out;
    const auto& g = seg.value();
    int n = 1;
    if (std::holds_alternative<m::BSpline2d>(c.geometry)) {
        n = std::max<int>(16, 24 * static_cast<int>(std::get<m::BSpline2d>(c.geometry).poles.size()));
    } else if (!std::holds_alternative<m::LineSegment2d>(c.geometry)) {
        n = std::max(8, static_cast<int>(std::ceil(std::abs(g.last - g.first) / (2 * kPi / 96))));
    }
    for (int i = 0; i <= n; ++i) {
        const gp_Pnt2d p = g.basis->Value(g.first + (g.last - g.first) * i / n);
        out.xy.push_back(p.X());
        out.xy.push_back(p.Y());
    }
    if (!g.closed) {
        const gp_Pnt2d p = g.basis->Value(0.5 * (g.first + g.last));
        out.mid = m::Point2{p.X(), p.Y()};
    }
    out.curve = g.closed ? g.basis : occ::handle<Geom2d_Curve>(new Geom2d_TrimmedCurve(g.basis, g.first, g.last));
    return out;
}

val xyArray(double x, double y) {
    val a = val::array();
    a.call<void>("push", x);
    a.call<void>("push", y);
    return a;
}

const char* kindName(m::BoundSketch::EntityKind k) {
    using K = m::BoundSketch::EntityKind;
    switch (k) {
        case K::Line: return "line";
        case K::Circle: return "circle";
        case K::Arc: return "arc";
        case K::Ellipse: return "ellipse";
        case K::EllipticalArc: return "elliptical-arc";
        case K::BSpline: return "bspline";
    }
    return "unknown";
}

// ------------------------------------------------------------------ regions

std::optional<std::size_t> regionOfEntity(const m::SketchProfile& profile, m::SketchEntityId e) {
    std::optional<std::size_t> best;
    for (std::size_t r = 0; r < profile.regions().size(); ++r) {
        const m::ProfileRegion& reg = profile.regions()[r];
        const auto& ents = profile.loops()[reg.outerLoop].entities;
        if (std::find(ents.begin(), ents.end(), e) == ents.end()) continue;
        if (!best || reg.area < profile.regions()[*best].area) best = r;
    }
    return best;
}

val profileJs(const m::SketchData& data, const std::vector<m::SketchCurve>& curves, double tol) {
    val out = val::object();
    val issues = val::array();
    for (const m::ProfileIssue& i : m::checkSketchCurves(curves, m::ProfileOptions{tol})) {
        val o = val::object();
        o.set("kind", std::string(m::toString(i.kind)));
        val ents = val::array();
        for (auto e : i.entities) ents.call<void>("push", static_cast<double>(e));
        o.set("entities", ents);
        o.set("location", xyArray(i.location.x, i.location.y));
        o.set("detail", i.detail);
        issues.call<void>("push", o);
    }
    out.set("issues", issues);
    out.set("regions", val::array());
    out.set("error", val::null());
    const bool hasProfile =
        std::any_of(curves.begin(), curves.end(), [](const m::SketchCurve& c) { return !c.construction; });
    out.set("hasProfile", hasProfile);
    if (!hasProfile || issues["length"].as<int>() > 0) return out;
    // Sketch-local frame: region faces come out in sketch coordinates (z = 0).
    auto profile = m::SketchProfile::build(m::SketchPlacement::fromPlane(gp_Pln()), curves, m::ProfileOptions{tol});
    if (!profile) {
        out.set("error", profile.error().message);
        return out;
    }
    const m::SketchProfile& pr = profile.value();
    std::vector<bool> selected(pr.regions().size(), false);
    if (data.regions.empty()) {
        for (std::size_t r : pr.materialRegions()) selected[r] = true;
    } else {
        for (m::SketchEntityId e : data.regions) {
            auto r = regionOfEntity(pr, e);
            if (!r) {
                out.set("error", "sketch entity " + std::to_string(e) + " is not on the outer contour of any region");
                continue;
            }
            selected[*r] = true;
        }
    }
    k::TessellationParams tp;
    tp.linearDeflection = std::max(1e-3, tol * 1e3);
    tp.angularDeflection = 0.2;
    tp.inParallel = false;
    val regions = val::array();
    for (std::size_t r = 0; r < pr.regions().size(); ++r) {
        const m::ProfileRegion& reg = pr.regions()[r];
        val o = val::object();
        o.set("index", static_cast<double>(r));
        val ents = val::array();
        for (auto e : pr.loops()[reg.outerLoop].entities) ents.call<void>("push", static_cast<double>(e));
        o.set("entities", ents);
        val holes = val::array();
        for (std::size_t h : reg.holeLoops) {
            val loop = val::array();
            for (auto e : pr.loops()[h].entities) loop.call<void>("push", static_cast<double>(e));
            holes.call<void>("push", loop);
        }
        o.set("holes", holes);
        o.set("area", reg.area);
        o.set("depth", reg.depth);
        o.set("material", reg.depth % 2 == 0);
        o.set("selected", static_cast<bool>(selected[r]));
        // Entity that selects this region in SketchData::regions (smallest-area rule), if any.
        val selector = val::null();
        for (auto e : pr.loops()[reg.outerLoop].entities) {
            if (regionOfEntity(pr, e) == r) {
                selector = val(static_cast<double>(e));
                break;
            }
        }
        o.set("selector", selector);
        auto mesh = k::tessellate(k::Shape(reg.face), tp);
        if (mesh) {
            o.set("positions", typedArray("Float32Array", mesh.value().positions));
            o.set("indices", typedArray("Uint32Array", mesh.value().indices));
        } else {
            o.set("positions", typedArray("Float32Array", std::vector<float>{}));
            o.set("indices", typedArray("Uint32Array", std::vector<std::uint32_t>{}));
        }
        regions.call<void>("push", o);
    }
    out.set("regions", regions);
    return out;
}

// ------------------------------------------------------------------ write-back

// Sketch data at the solution: free points and initial radii take the solved values; fixed
// points, projected curves and every expression of the constraints are kept.
m::SketchData solvedData(const Built& b, const m::SketchData& data) {
    m::SketchData out = data;
    const s2::Sketch& s = b.bs.sketch();
    for (std::size_t k = 0; k < out.points.size(); ++k) {
        if (b.pinned[k]) continue;
        const auto p = s.point(static_cast<s2::Id>(k));
        out.points[k].x = num(p.x);
        out.points[k].y = num(p.y);
    }
    for (std::size_t g = 0; g < out.geometry.size(); ++g) {
        m::SketchGeometryDef& def = out.geometry[g];
        if (def.projection >= 0) continue;
        const auto& e = b.bs.entity(static_cast<m::SketchEntityId>(g + 1));
        if (def.type == m::SketchGeometryType::Circle) def.radius = num(s.circle(e.geo.id).radius);
        if (def.type == m::SketchGeometryType::Ellipse) def.radius = num(s.ellipse(e.geo.id).minor_radius);
        if (def.type == m::SketchGeometryType::EllipticalArc) def.radius = num(s.ellipse_arc(e.geo.id).minor_radius);
    }
    return out;
}

std::optional<std::pair<int, m::Point2>> dragFromJs(const val& options, const m::SketchData& data, const Built& b) {
    if (!present(options) || !present(options["drag"])) return std::nullopt;
    const val d = options["drag"];
    const int p = requireInt(d["point"], "options.drag.point");
    if (p < 0 || static_cast<std::size_t>(p) >= data.points.size()) {
        throw InputError("options.drag.point " + std::to_string(p) + " does not exist");
    }
    if (b.pinned[static_cast<std::size_t>(p)]) {
        throw InputError("sketch point " + std::to_string(p) + " is fixed and cannot be dragged");
    }
    if (!d["x"].isNumber() || !d["y"].isNumber()) throw InputError("options.drag.x and .y must be numbers");
    return std::make_pair(p, m::Point2{d["x"].as<double>(), d["y"].as<double>()});
}

val docSketchSolve(std::uint32_t handle, val featureId, val sketch, val options) {
    return withDocument(handle, [&](m::PartDocument& doc) {
        const m::FeatureInfo f = sketchFeature(doc, featureId);
        const m::SketchData data = sketchFromJs(sketch);
        Built b = build(doc, f, data);
        const auto drag = dragFromJs(options, data, b);
        s2::Report report;
        try {
            report = solve(b, drag);
        } catch (const std::exception& e) {
            throw InputError(std::string("sketch cannot be solved: ") + e.what());
        }
        const s2::Sketch& s = b.bs.sketch();
        const double tol = 1e-6 * b.scale;
        val o = val::object();
        o.set("converged", report.converged);
        o.set("status", report.status);
        o.set("dof", report.dof);
        o.set("definition", std::string(s2::to_string(report.definition)));
        o.set("conflicting", dataNumbers(b, report.conflicting));
        o.set("redundant", dataNumbers(b, report.redundant));
        o.set("partiallyRedundant", dataNumbers(b, report.partially_redundant));
        o.set("placement", placementJs(placementOf(f)));

        val points = val::array();
        for (std::size_t k = 0; k < data.points.size(); ++k) {
            const auto p = s.point(static_cast<s2::Id>(k));
            val q = val::object();
            q.set("x", p.x);
            q.set("y", p.y);
            q.set("fixed", static_cast<bool>(b.pinned[k]));
            q.set("defined", report.fully_defined(s2::point_geo(static_cast<s2::Id>(k))));
            points.call<void>("push", q);
        }
        o.set("points", points);

        val values = val::array(), residuals = val::array();
        for (std::size_t k = 0; k < b.tags.size(); ++k) {
            const auto& info = s.constraint(b.tags[k]);
            values.call<void>("push", s2::is_dimension(info.kind) ? s.constraint_value(b.tags[k])
                                                                 : evaluate(doc, data.constraints[k].value, "value"));
            residuals.call<void>("push", info.driving ? s.residual(b.tags[k]) : 0.0);
        }
        o.set("constraintValues", values);
        o.set("residuals", residuals);

        // Curves of every entity; with an unsolved (conflicting) sketch the arcs may be off
        // their circles, so the display curves accept any end-point deviation.
        std::vector<m::SketchCurve> profileCurves;
        bool exact = report.converged && report.conflicting.empty();
        val curves = val::array();
        std::vector<std::pair<m::SketchEntityId, occ::handle<Geom2d_Curve>>> pieces;
        for (const auto& e : b.bs.entities()) {
            auto c = b.bs.curve(e.id, tol);
            if (!c) {
                exact = false;
                c = b.bs.curve(e.id, 1e300);
            }
            val co = val::object();
            co.set("entity", static_cast<double>(e.id));
            co.set("type", std::string(kindName(e.kind)));
            co.set("construction", e.construction);
            co.set("projection", data.geometry[e.id - 1].projection);
            co.set("defined", report.fully_defined(e.geo));
            if (!c) {
                co.set("polyline", typedArray("Float64Array", std::vector<double>{}));
                co.set("midpoint", val::null());
                curves.call<void>("push", co);
                continue;
            }
            const m::SketchCurve& curve = c.value();
            profileCurves.push_back(curve);
            Sampled smp = sample(curve, tol);
            co.set("polyline", typedArray("Float64Array", smp.xy));
            co.set("midpoint", smp.mid ? xyArray(smp.mid->x, smp.mid->y) : val::null());
            if (std::holds_alternative<m::Circle2d>(curve.geometry)) {
                const auto& ci = std::get<m::Circle2d>(curve.geometry);
                co.set("center", xyArray(ci.center.x, ci.center.y));
                co.set("radius", ci.radius);
            } else if (std::holds_alternative<m::CircularArc2d>(curve.geometry)) {
                const auto& a = std::get<m::CircularArc2d>(curve.geometry);
                co.set("center", xyArray(a.center.x, a.center.y));
                co.set("radius", std::hypot(a.start.x - a.center.x, a.start.y - a.center.y));
            }
            if (!smp.curve.IsNull()) pieces.emplace_back(e.id, smp.curve);
            curves.call<void>("push", co);
        }
        o.set("curves", curves);

        // Pairwise intersections of the curves (snap targets of the editor).
        // options.lite (previews while drawing or dragging): no intersections, no profile.
        const bool lite = present(options) && optionalBool(options, "lite", false, "options");
        val inters = val::array();
        for (std::size_t i = 0; !lite && i < pieces.size(); ++i) {
            for (std::size_t j = i + 1; j < pieces.size(); ++j) {
                Geom2dAPI_InterCurveCurve ic(pieces[i].second, pieces[j].second, tol);
                for (int n = 1; n <= ic.NbPoints(); ++n) {
                    const gp_Pnt2d p = ic.Point(n);
                    val q = val::object();
                    q.set("x", p.X());
                    q.set("y", p.Y());
                    q.set("a", static_cast<double>(pieces[i].first));
                    q.set("b", static_cast<double>(pieces[j].first));
                    inters.call<void>("push", q);
                }
            }
        }
        o.set("intersections", inters);

        if (exact) {
            o.set("profile", lite ? val::null() : profileJs(data, profileCurves, tol));
            o.set("sketch", toJs(solvedData(b, data)));
        } else {
            o.set("profile", val::null());
            o.set("sketch", toJs(data));
        }
        return success(o);
    });
}

// ------------------------------------------------------------------ editing tools

int pointOf(const std::vector<int>& map, const s2::Geo& g) {
    if (g.kind != s2::GeoKind::Point || g.id < 0 || static_cast<std::size_t>(g.id) >= map.size() || map[g.id] < 0) {
        throw std::logic_error("constraint operand is not a live point");
    }
    return map[static_cast<std::size_t>(g.id)];
}

m::SketchConstraintType typeOf(s2::ConstraintKind k) {
    using CK = s2::ConstraintKind;
    using SC = m::SketchConstraintType;
    switch (k) {
        case CK::Coincident: return SC::Coincident;
        case CK::Horizontal: return SC::Horizontal;
        case CK::Vertical: return SC::Vertical;
        case CK::Distance: return SC::Distance;
        case CK::DistanceX: return SC::DistanceX;
        case CK::DistanceY: return SC::DistanceY;
        case CK::PointCurveDistance: return SC::PointCurveDistance;
        case CK::Radius: return SC::Radius;
        case CK::Diameter: return SC::Diameter;
        case CK::Angle: return SC::Angle;
        case CK::ArcLength: return SC::ArcLength;
        case CK::MajorRadius: return SC::MajorRadius;
        case CK::MinorRadius: return SC::MinorRadius;
        case CK::Parallel: return SC::Parallel;
        case CK::Perpendicular: return SC::Perpendicular;
        case CK::Tangent: return SC::Tangent;
        case CK::Equal: return SC::Equal;
        case CK::Symmetric: return SC::Symmetric;
        case CK::SymmetricPoint: return SC::SymmetricPoint;
        case CK::Midpoint: return SC::Midpoint;
        case CK::PointOn: return SC::PointOn;
        case CK::Concentric: return SC::Concentric;
        case CK::Fix: return SC::Fix;
        case CK::Rotation: return SC::Rotation;
    }
    throw std::logic_error("unknown constraint kind");
}

// The sketch2d kind a data constraint type is stored as (legacy types share kinds).
s2::ConstraintKind storedKind(m::SketchConstraintType t) {
    using SC = m::SketchConstraintType;
    switch (t) {
        case SC::EqualLength:
        case SC::EqualRadius: return s2::ConstraintKind::Equal;
        case SC::PointOnLine:
        case SC::PointOnCircle: return s2::ConstraintKind::PointOn;
        default: break;
    }
    for (auto k : {s2::ConstraintKind::Coincident, s2::ConstraintKind::Horizontal, s2::ConstraintKind::Vertical,
                   s2::ConstraintKind::Distance, s2::ConstraintKind::DistanceX, s2::ConstraintKind::DistanceY,
                   s2::ConstraintKind::PointCurveDistance, s2::ConstraintKind::Radius, s2::ConstraintKind::Diameter,
                   s2::ConstraintKind::Angle, s2::ConstraintKind::ArcLength, s2::ConstraintKind::MajorRadius,
                   s2::ConstraintKind::MinorRadius, s2::ConstraintKind::Parallel, s2::ConstraintKind::Perpendicular,
                   s2::ConstraintKind::Tangent, s2::ConstraintKind::Equal, s2::ConstraintKind::Symmetric,
                   s2::ConstraintKind::SymmetricPoint, s2::ConstraintKind::Midpoint, s2::ConstraintKind::PointOn,
                   s2::ConstraintKind::Concentric, s2::ConstraintKind::Fix, s2::ConstraintKind::Rotation}) {
        if (typeOf(k) == t) return k;
    }
    throw std::logic_error("unknown constraint type");
}

struct Converted {
    m::SketchData data;
    std::vector<int> pointMap;   // sketch2d point id -> data point index (-1 removed)
    std::vector<int> entityMap;  // BoundSketch id -> data entity id (0 removed), index = id
    std::map<int, int> constraintMap;  // tag -> data constraint number (1-based)
};

// Sketch data of an edited BoundSketch built from `original`.
Converted toData(const Built& b, const m::SketchData& original) {
    Converted out;
    const s2::Sketch& s = b.bs.sketch();
    const std::size_t np = original.points.size(), ng = original.geometry.size();
    out.pointMap.assign(s.point_count(), -1);
    for (std::size_t id = 0; id < s.point_count(); ++id) {
        if (s.is_removed(s2::point_geo(static_cast<s2::Id>(id)))) continue;
        out.pointMap[id] = static_cast<int>(out.data.points.size());
        const auto p = s.point(static_cast<s2::Id>(id));
        m::SketchPointDef def;
        if (id < np) def = original.points[id];
        if (id >= np || !b.pinned[id]) {
            def.x = num(p.x);
            def.y = num(p.y);
        }
        out.data.points.push_back(def);
    }
    auto pt = [&](s2::Id id) { return pointOf(out.pointMap, s2::point_geo(id)); };
    out.entityMap.assign(b.bs.entities().size() + 1, 0);
    using K = m::BoundSketch::EntityKind;
    for (const auto& e : b.bs.entities()) {
        if (e.removed) continue;
        m::SketchGeometryDef def;
        if (e.id <= ng) def = original.geometry[e.id - 1];
        def.construction = s.is_construction(e.geo);
        switch (e.kind) {
            case K::Line:
                def.type = m::SketchGeometryType::Line;
                def.start = pt(e.start);
                def.end = pt(e.end);
                break;
            case K::Circle:
                def.type = m::SketchGeometryType::Circle;
                def.center = pt(e.center);
                if (def.projection < 0) def.radius = num(s.circle(e.geo.id).radius);
                break;
            case K::Arc: {
                def.type = m::SketchGeometryType::Arc;
                def.center = pt(e.center);
                def.start = pt(e.start);
                def.end = pt(e.end);
                def.counterClockwise = e.counterClockwise;
                break;
            }
            case K::Ellipse:
                def.type = m::SketchGeometryType::Ellipse;
                def.center = pt(e.center);
                def.focus = pt(e.focus);
                def.radius = num(s.ellipse(e.geo.id).minor_radius);
                break;
            case K::EllipticalArc:
                def.type = m::SketchGeometryType::EllipticalArc;
                def.center = pt(e.center);
                def.focus = pt(e.focus);
                def.start = pt(e.start);
                def.end = pt(e.end);
                def.counterClockwise = e.counterClockwise;
                def.radius = num(s.ellipse_arc(e.geo.id).minor_radius);
                break;
            case K::BSpline: {
                const s2::BSplineData& sd = s.bspline(e.geo.id);
                def.type = m::SketchGeometryType::BSpline;
                def.poles.clear();
                for (s2::Id q : sd.poles) def.poles.push_back(pt(q));
                def.degree = sd.degree;
                def.knots = sd.knots;
                def.multiplicities = sd.multiplicities;
                def.weights = sd.weights;
                def.periodic = sd.periodic;
                break;
            }
        }
        out.data.geometry.push_back(def);
        out.entityMap[e.id] = static_cast<int>(out.data.geometry.size());
    }
    auto ent = [&](const s2::Geo& g) {
        auto id = b.bs.entityOf(g);
        if (!id || out.entityMap[*id] == 0) throw std::logic_error("constraint operand is not a live entity");
        return out.entityMap[*id];
    };
    using CK = s2::ConstraintKind;
    for (int tag = 1; tag <= static_cast<int>(s.constraint_count()); ++tag) {
        const s2::ConstraintInfo& info = s.constraint(tag);
        if (info.removed || b.internal.count(tag)) continue;
        m::SketchConstraintDef def;
        const int k = indexOfTag(b, tag);
        if (k > 0 && storedKind(original.constraints[k - 1].type) == info.kind) {
            def = original.constraints[k - 1];
        } else {
            def.type = typeOf(info.kind);
            def.value = num(info.value);
        }
        def.driven = !info.driving;
        def.a = def.b = def.c = -1;
        switch (info.kind) {
            case CK::Coincident:
            case CK::Horizontal:
            case CK::Vertical:
            case CK::Distance:
            case CK::DistanceX:
            case CK::DistanceY:
                def.a = pt(info.a.id);
                def.b = pt(info.b.id);
                break;
            case CK::PointCurveDistance:
            case CK::Midpoint:
            case CK::PointOn:
                def.a = pt(info.a.id);
                def.b = ent(info.b);
                break;
            case CK::Radius:
            case CK::Diameter:
            case CK::ArcLength:
            case CK::MajorRadius:
            case CK::MinorRadius: def.a = ent(info.a); break;
            case CK::Angle:
            case CK::Parallel:
            case CK::Perpendicular:
            case CK::Tangent:
            case CK::Equal:
            case CK::Concentric:
                def.a = ent(info.a);
                def.b = ent(info.b);
                break;
            case CK::Symmetric:
                def.a = pt(info.a.id);
                def.b = pt(info.b.id);
                def.c = ent(info.c);
                break;
            case CK::SymmetricPoint:
            case CK::Rotation:
                def.a = pt(info.a.id);
                def.b = pt(info.b.id);
                def.c = pt(info.c.id);
                break;
            case CK::Fix: def.a = pt(info.a.id); break;
        }
        out.data.constraints.push_back(def);
        out.constraintMap[tag] = static_cast<int>(out.data.constraints.size());
    }
    for (m::SketchEntityId r : original.regions) {
        if (r < out.entityMap.size() && out.entityMap[r] > 0) out.data.regions.push_back(static_cast<m::SketchEntityId>(out.entityMap[r]));
    }
    return out;
}

double numberArg(const val& tool, const char* field) {
    const val v = tool[field];
    if (!v.isNumber() || !std::isfinite(v.as<double>())) throw InputError(std::string("tool.") + field + " must be a finite number");
    return v.as<double>();
}

m::SketchEntityId entityArg(const val& v, std::size_t count, const std::string& what) {
    const int id = requireInt(v, what);
    if (id < 1 || static_cast<std::size_t>(id) > count) {
        throw InputError(what + ": no sketch entity " + std::to_string(id) + " (1.." + std::to_string(count) + ")");
    }
    return static_cast<m::SketchEntityId>(id);
}

std::vector<m::SketchEntityId> entityList(const val& v, std::size_t count, const std::string& what) {
    std::vector<m::SketchEntityId> out;
    for (const val& e : arrayItems(v, what)) out.push_back(entityArg(e, count, what + "[]"));
    if (out.empty()) throw InputError(what + " must not be empty");
    return out;
}

m::Point2 xyArg(const val& v, const std::string& what) {
    const std::vector<val> xy = arrayItems(v, what);
    if (xy.size() != 2 || !xy[0].isNumber() || !xy[1].isNumber()) throw InputError(what + " must be [x, y]");
    return {xy[0].as<double>(), xy[1].as<double>()};
}

val docSketchEdit(std::uint32_t handle, val featureId, val sketch, val tool) {
    return withDocument(handle, [&](m::PartDocument& doc) {
        const m::FeatureInfo f = sketchFeature(doc, featureId);
        const m::SketchData data = sketchFromJs(sketch);
        if (!isObject(tool)) throw InputError("tool must be an object {name, ...}");
        const std::string name = requireString(tool["name"], "tool.name");
        Built b = build(doc, f, data);
        const s2::Report report = solve(b, std::nullopt);
        if (!report.converged || !report.conflicting.empty()) {
            return failure("InvalidInput", "the sketch must be solved without conflicts before '" + name + "' (status " +
                                               report.status + ")");
        }
        const std::size_t ng = data.geometry.size();
        const bool constrain = !present(tool["constrain"]) || optionalBool(tool, "constrain", true, "tool");
        m::BoundSketch& bs = b.bs;
        k::Result<m::SketchEditResult> r = k::Result<m::SketchEditResult>::failure(k::ErrorCode::InvalidInput, "");
        if (name == "trim" || name == "extend") {
            const auto e = entityArg(tool["entity"], ng, "tool.entity");
            const m::Point2 pick = xyArg(tool["pick"], "tool.pick");
            r = name == "trim" ? m::trimEntity(bs, e, pick) : m::extendEntity(bs, e, pick);
        } else if (name == "offset") {
            r = m::offsetChain(bs, entityList(tool["chain"], ng, "tool.chain"), numberArg(tool, "distance"), constrain);
        } else if (name == "mirror") {
            r = m::mirrorEntities(bs, entityList(tool["entities"], ng, "tool.entities"),
                                  entityArg(tool["axis"], ng, "tool.axis"), constrain);
        } else if (name == "linear-pattern") {
            r = m::linearPattern(bs, entityList(tool["entities"], ng, "tool.entities"), xyArg(tool["step"], "tool.step"),
                                 requireInt(tool["count"], "tool.count"), constrain);
        } else if (name == "circular-pattern") {
            const int c = requireInt(tool["center"], "tool.center");
            if (c < 0 || static_cast<std::size_t>(c) >= data.points.size()) throw InputError("tool.center: no sketch point " + std::to_string(c));
            r = m::circularPattern(bs, entityList(tool["entities"], ng, "tool.entities"), c,
                                   requireInt(tool["count"], "tool.count"), numberArg(tool, "angle"), constrain);
        } else if (name == "fillet") {
            r = m::filletCorner(bs, entityArg(tool["a"], ng, "tool.a"), entityArg(tool["b"], ng, "tool.b"),
                                numberArg(tool, "radius"));
        } else if (name == "chamfer") {
            r = m::chamferCorner(bs, entityArg(tool["a"], ng, "tool.a"), entityArg(tool["b"], ng, "tool.b"),
                                 numberArg(tool, "d1"), numberArg(tool, "d2"));
        } else {
            throw InputError("unknown tool '" + name +
                             "' (trim, extend, offset, mirror, linear-pattern, circular-pattern, fillet, chamfer)");
        }
        if (!r) return failure(r.error());
        const Converted c = toData(b, data);
        const m::SketchEditResult& res = r.value();
        auto ids = [&](const std::vector<m::SketchEntityId>& list, bool mapped) {
            val a = val::array();
            for (auto e : list) {
                if (!mapped) {
                    a.call<void>("push", static_cast<double>(e));
                } else if (e < c.entityMap.size() && c.entityMap[e] > 0) {
                    a.call<void>("push", c.entityMap[e]);
                }
            }
            return a;
        };
        val o = val::object();
        o.set("sketch", toJs(c.data));
        o.set("created", ids(res.created, true));
        o.set("modified", ids(res.modified, true));
        o.set("removed", ids(res.removed, false));
        val pts = val::array();
        for (auto p : res.points) {
            if (static_cast<std::size_t>(p) < c.pointMap.size() && c.pointMap[p] >= 0) pts.call<void>("push", c.pointMap[p]);
        }
        o.set("points", pts);
        val cons = val::array();
        for (auto t : res.constraints) {
            auto it = c.constraintMap.find(t);
            if (it != c.constraintMap.end()) cons.call<void>("push", it->second);
        }
        o.set("constraints", cons);
        return success(o);
    });
}

}  // namespace

}  // namespace bendcore::wasm_modeling

EMSCRIPTEN_BINDINGS(bendcore_sketch_editor) {
    emscripten::function("docSketchSolve", &bendcore::wasm_modeling::docSketchSolve);
    emscripten::function("docSketchEdit", &bendcore::wasm_modeling::docSketchEdit);
}
