// Body features of the part document on top of src/modeling/features/
// (docs/modeling/PART_DOCUMENT.md, section "Операции").

#include "part_features_internal.hpp"

#include "../features/feature_common.hpp"

#include <bendcore/modeling/features.hpp>
#include <bendcore/modeling/reference_geometry.hpp>

#include <BRepAdaptor_Surface.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepGProp.hxx>
#include <BRep_Builder.hxx>
#include <GProp_GProps.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Compound.hxx>
#include <gp_Circ.hxx>

#include <cmath>
#include <stdexcept>

namespace bendcore::modeling {

namespace features {

namespace {

using detail::fail;
using detail::num;
namespace fc = ::bendcore::modeling::detail;

constexpr double kDeg = M_PI / 180.0;

std::vector<OptionSpec> withMode(std::vector<OptionSpec> specs) {
    specs.insert(specs.begin(), OptionSpec{"mode", {"boss", "cut", "newBody"}});
    return specs;
}

FeatureMode modeOf(const FeatureContext& ctx) {
    return featureModeFromString(ctx.option("mode")).value_or(FeatureMode::Boss);
}

TopoDS_Shape compound(const std::vector<TopoDS_Shape>& shapes) {
    if (shapes.size() == 1) return shapes.front();
    TopoDS_Compound c;
    BRep_Builder b;
    b.MakeCompound(c);
    for (const auto& s : shapes) b.Add(c, s);
    return c;
}

// Faces of the sketch regions in the order of the named face shape.
Result<std::vector<TopoDS_Face>> profileFaces(const LinkedFeature* sketch) {
    using R = Result<std::vector<TopoDS_Face>>;
    if (!sketch || !sketch->sketch) return R::failure(ErrorCode::InvalidInput, "link 'sketch' must name a sketch");
    if (!sketch->sketch->faces) {
        return R::failure(ErrorCode::InvalidInput, "sketch '" + sketch->id + "' has no closed profile");
    }
    std::vector<TopoDS_Face> out;
    for (std::size_t i = 0; i < sketch->sketch->faces->count(ElementKind::Face); ++i) {
        out.push_back(TopoDS::Face(sketch->sketch->faces->element(ElementKind::Face, i)));
    }
    return out;
}

// Applies an operation to every profile face in turn, composing the histories.
template <class Op>
Result<FeatureOutput> sweepRegions(const FeatureContext& ctx, const char* what, FeatureMode mode, Op op) {
    const LinkedFeature* sk = ctx.link("sketch");
    auto faces = profileFaces(sk);
    if (!faces) return faces.error();
    TopoDS_Shape body = ctx.input().shape();
    fc::History history = fc::newHistory();
    std::vector<TopoDS_Shape> tools;
    FeatureOutput out;
    for (std::size_t i = 0; i < faces.value().size(); ++i) {
        Profile profile{faces.value()[i], sk->sketch->placement.frame()};
        Result<FeatureResult> step = op(profile, body);
        if (!step) {
            std::string where = faces.value().size() > 1 ? " (region " + std::to_string(i + 1) + ")" : "";
            return fail(step.error().code, std::string(what) + where + ": " + step.error().message);
        }
        fc::mergeInto(history, step.value().history);
        body = step.value().shape;
        tools.push_back(step.value().tool);
        out.warnings.insert(out.warnings.end(), step.value().warnings.begin(), step.value().warnings.end());
    }
    out.shape = body;
    out.history = history;
    out.tool = compound(tools);
    out.toolMode = mode;
    out.namingInputs.push_back(sk->sketch->faces);
    return out;
}

// ---------------------------------------------------------------- extrude

class ExtrudeFeature final : public Feature {
public:
    std::string type() const override { return "Extrude"; }
    std::vector<ParameterSpec> parameterSpecs() const override {
        return {{"distance", "10"}, {"distance2", "0"}, {"draftAngle", "0"}, {"thickness", "1"}};
    }
    std::vector<ReferenceSlot> referenceSlots() const override { return {{"limit", ElementKind::Face, false, true}}; }
    std::vector<OptionSpec> optionSpecs() const override {
        return withMode({{"extent", {"blind", "symmetric", "twoSided", "throughAll", "throughAllBoth", "upToFace", "upToSurface"}},
                         {"reversed", {"false", "true"}},
                         {"draftDirection", {"inward", "outward"}},
                         {"thin", {"none", "inside", "outside"}}});
    }
    std::vector<LinkSlot> linkSlots() const override { return {{"sketch", {"Sketch"}, false}}; }

    Result<FeatureOutput> execute(const FeatureContext& ctx) const override {
        ExtrudeParams p;
        p.mode = modeOf(ctx);
        const std::string& e = ctx.option("extent");
        p.extent = e == "symmetric"        ? ExtentType::Symmetric
                   : e == "twoSided"       ? ExtentType::TwoSided
                   : e == "throughAll"     ? ExtentType::ThroughAll
                   : e == "throughAllBoth" ? ExtentType::ThroughAllBoth
                   : e == "upToFace"       ? ExtentType::UpToFace
                   : e == "upToSurface"    ? ExtentType::UpToSurface
                                           : ExtentType::Blind;
        p.distance = ctx.parameter("distance");
        p.distance2 = ctx.parameter("distance2");
        p.reversed = ctx.option("reversed") == "true";
        p.draftAngle = ctx.parameter("draftAngle") * kDeg;
        p.draftDirection = ctx.option("draftDirection") == "outward" ? DraftDirection::Outward : DraftDirection::Inward;
        if (ctx.option("thin") != "none") {
            p.thin = ThinWall{ctx.parameter("thickness"), ctx.option("thin") == "outside" ? ThinSide::Outside : ThinSide::Inside};
        }
        if (p.extent == ExtentType::UpToFace || p.extent == ExtentType::UpToSurface) {
            const auto& limit = ctx.references("limit");
            if (limit.size() != 1) return fail(ErrorCode::InvalidInput, "up-to extent needs one 'limit' face reference");
            p.limit = TopoDS::Face(limit.front());
        }
        return sweepRegions(ctx, "extrude", p.mode,
                            [&](const Profile& profile, const TopoDS_Shape& body) { return extrude(profile, body, p); });
    }
};

// ---------------------------------------------------------------- revolve

class RevolveFeature final : public Feature {
public:
    std::string type() const override { return "Revolve"; }
    std::vector<ParameterSpec> parameterSpecs() const override {
        return {{"angle", "360"}, {"angle2", "0"}, {"axisEntity", "0"}};
    }
    std::vector<ReferenceSlot> referenceSlots() const override { return {{"axisEdge", ElementKind::Edge, false, true}}; }
    std::vector<OptionSpec> optionSpecs() const override {
        return withMode({{"extent", {"full", "angle", "symmetric", "twoSided"}},
                         {"axis", {"entity", "X", "Y", "Z", "edge", "datum"}},
                         {"reversed", {"false", "true"}}});
    }
    std::vector<LinkSlot> linkSlots() const override {
        return {{"sketch", {"Sketch"}, false}, {"axis", {"DatumAxis"}, true}};
    }

    Result<FeatureOutput> execute(const FeatureContext& ctx) const override {
        RevolveParams p;
        p.mode = modeOf(ctx);
        const std::string& e = ctx.option("extent");
        p.extent = e == "angle"       ? RevolveExtent::Angle
                   : e == "symmetric" ? RevolveExtent::Symmetric
                   : e == "twoSided"  ? RevolveExtent::TwoSided
                                      : RevolveExtent::Full;
        p.angle = ctx.parameter("angle") * kDeg;
        p.angle2 = ctx.parameter("angle2") * kDeg;
        p.reversed = ctx.option("reversed") == "true";
        const std::string& axisChoice = ctx.option("axis");
        if (axisChoice == "entity") {
            const LinkedFeature* sk = ctx.link("sketch");
            if (!sk || !sk->sketch) return fail(ErrorCode::InvalidInput, "link 'sketch' must name a sketch");
            const double idv = ctx.parameter("axisEntity");
            const auto id = static_cast<SketchEntityId>(std::llround(idv));
            const SketchCurve* axisCurve = nullptr;
            for (const auto& c : sk->sketch->curves) {
                if (c.id == id) axisCurve = &c;
            }
            if (!axisCurve || !std::holds_alternative<LineSegment2d>(axisCurve->geometry)) {
                return fail(ErrorCode::InvalidInput, "axisEntity " + num(idv) + " is not a line of sketch '" + sk->id + "'");
            }
            const auto& l = std::get<LineSegment2d>(axisCurve->geometry);
            auto ax = axisThroughPoints(sk->sketch->placement.toWorld(l.start), sk->sketch->placement.toWorld(l.end));
            if (!ax) return ax.error();
            p.axis = ax.value();
        } else {
            auto ax = axisFromOption(ctx, axisChoice, "", "axisEdge", "axis");
            if (!ax) return ax.error();
            p.axis = ax.value();
        }
        return sweepRegions(ctx, "revolve", p.mode,
                            [&](const Profile& profile, const TopoDS_Shape& body) { return revolve(profile, body, p); });
    }
};

// ---------------------------------------------------------------- hole

class HoleFeature final : public Feature {
public:
    std::string type() const override { return "Hole"; }
    std::vector<ParameterSpec> parameterSpecs() const override {
        return {{"diameter", "5"},  {"depth", "10"},     {"cbDiameter", "10"}, {"cbDepth", "3"}, {"csDiameter", "10"},
                {"csAngle", "90"}, {"drillAngle", "118"}, {"x", "0"},         {"y", "0"},       {"z", "0"}};
    }
    std::vector<ReferenceSlot> referenceSlots() const override { return {{"face", ElementKind::Face, false, false}}; }
    std::vector<OptionSpec> optionSpecs() const override {
        return {{"type", {"simple", "counterbore", "countersink"}},
                {"end", {"blind", "through"}},
                {"drillPoint", {"true", "false"}}};
    }
    std::vector<LinkSlot> linkSlots() const override { return {{"sketch", {"Sketch"}, true}}; }

    Result<FeatureOutput> execute(const FeatureContext& ctx) const override {
        if (ctx.input().isNull()) return fail(ErrorCode::InvalidInput, "hole needs an existing body");
        HoleParams p;
        const std::string& t = ctx.option("type");
        p.type = t == "counterbore" ? HoleType::Counterbore : t == "countersink" ? HoleType::Countersink : HoleType::Simple;
        p.end = ctx.option("end") == "through" ? HoleEnd::Through : HoleEnd::Blind;
        p.diameter = ctx.parameter("diameter");
        p.depth = ctx.parameter("depth");
        p.drillPoint = ctx.option("drillPoint") == "true";
        p.drillPointAngle = ctx.parameter("drillAngle") * kDeg;
        p.counterboreDiameter = ctx.parameter("cbDiameter");
        p.counterboreDepth = ctx.parameter("cbDepth");
        p.countersinkDiameter = ctx.parameter("csDiameter");
        p.countersinkAngle = ctx.parameter("csAngle") * kDeg;

        TopoDS_Face face = TopoDS::Face(ctx.references("face").front());
        auto pln = planeOfFace(face);
        if (!pln) return pln.error();
        std::vector<gp_Pnt> centers;
        if (const LinkedFeature* sk = ctx.link("sketch")) {
            if (!sk->sketch) return fail(ErrorCode::InvalidInput, "link 'sketch' must name a sketch");
            for (std::size_t k : sk->sketch->standalonePoints) {
                centers.push_back(sk->sketch->placement.toWorld(sk->sketch->points[k]));
            }
            if (centers.empty()) {
                return fail(ErrorCode::InvalidInput, "sketch '" + sk->id + "' has no standalone points for hole centres");
            }
        } else {
            centers.push_back(pointProjectedOnPlane(gp_Pnt(ctx.parameter("x"), ctx.parameter("y"), ctx.parameter("z")),
                                                    pln.value()));
        }

        TopoDS_Shape body = ctx.input().shape();
        fc::History history = fc::newHistory();
        std::vector<TopoDS_Shape> tools;
        FeatureOutput out;
        for (std::size_t k = 0; k < centers.size(); ++k) {
            auto step = hole(body, face, centers[k], p);
            if (!step) {
                std::string where = centers.size() > 1 ? " #" + std::to_string(k + 1) : "";
                return fail(step.error().code, "hole" + where + ": " + step.error().message);
            }
            addRoles(step.value().tool, centers[k], p, static_cast<int>(k + 1), centers.size() > 1, out.roles);
            // The placement face of the next hole: the image of this face in the result.
            const auto& mods = step.value().history->Modified(face);
            if (!mods.IsEmpty()) {
                if (mods.Extent() != 1) return fail(ErrorCode::OperationFailed, "hole face was split by hole #" + std::to_string(k + 1));
                face = TopoDS::Face(mods.First());
            }
            fc::mergeInto(history, step.value().history);
            body = step.value().shape;
            tools.push_back(step.value().tool);
            out.warnings.insert(out.warnings.end(), step.value().warnings.begin(), step.value().warnings.end());
        }
        out.shape = body;
        out.history = history;
        out.tool = compound(tools);
        out.toolMode = FeatureMode::Cut;
        return out;
    }

private:
    // Roles of the tool faces: Wall, CounterboreWall, CounterboreFloor, Countersink, Tip,
    // Bottom, Top (prefixed with H<k>. when there are several holes).
    static void addRoles(const TopoDS_Shape& tool, const gp_Pnt& center, const HoleParams& p, int index, bool many,
                         std::vector<ElementRole>& roles) {
        const std::string prefix = many ? "H" + std::to_string(index) + "." : "";
        // The tool axis: the drilling direction is the axis of its cylindrical wall.
        gp_Dir axis;
        bool haveAxis = false;
        for (TopExp_Explorer ex(tool, TopAbs_FACE); ex.More() && !haveAxis; ex.Next()) {
            BRepAdaptor_Surface s(TopoDS::Face(ex.Current()), false);
            if (s.GetType() == GeomAbs_Cylinder) {
                axis = s.Cylinder().Axis().Direction();
                haveAxis = true;
            }
        }
        for (TopExp_Explorer ex(tool, TopAbs_FACE); ex.More(); ex.Next()) {
            const TopoDS_Face& f = TopoDS::Face(ex.Current());
            BRepAdaptor_Surface s(f, false);
            GProp_GProps g;
            BRepGProp::SurfaceProperties(f, g);
            const double along = haveAxis ? std::abs(gp_Vec(center, g.CentreOfMass()).Dot(gp_Vec(axis))) : 0.0;
            std::string role;
            switch (s.GetType()) {
                case GeomAbs_Cylinder:
                    role = std::abs(s.Cylinder().Radius() - p.diameter / 2) < 1e-7 ? "Wall" : "CounterboreWall";
                    break;
                case GeomAbs_Cone: role = along < 1e-7 + 0.5 * p.countersinkDiameter && p.type == HoleType::Countersink &&
                                                  along < p.depth / 2
                                              ? "Countersink"
                                              : "Tip";
                    break;
                case GeomAbs_Plane:
                    role = along < 1e-7 ? "Top" : (p.type == HoleType::Counterbore && std::abs(along - p.counterboreDepth) < 1e-7)
                                                      ? "CounterboreFloor"
                                                      : "Bottom";
                    break;
                default: role = "Face"; break;
            }
            roles.push_back({f, prefix + role});
        }
    }
};

// ---------------------------------------------------------------- fillet / chamfer

class FilletFeature final : public Feature {
public:
    std::string type() const override { return "Fillet"; }
    std::vector<ParameterSpec> parameterSpecs() const override { return {{"radius", "1"}}; }
    std::vector<ReferenceSlot> referenceSlots() const override { return {{"edges", ElementKind::Edge, true}}; }
    std::vector<OptionSpec> optionSpecs() const override { return {}; }
    Result<FeatureOutput> execute(const FeatureContext& ctx) const override {
        if (ctx.input().isNull()) return fail(ErrorCode::InvalidInput, "fillet needs an existing body");
        const double r = ctx.parameter("radius");
        if (!(r > 0)) return fail(ErrorCode::InvalidInput, "fillet radius must be positive: " + num(r));
        auto res = fillet(ctx.input().shape(), ctx.references("edges"), r);
        if (!res) return res.error();
        FeatureOutput out;
        out.shape = res.value().shape;
        out.history = res.value().history;
        out.warnings = res.value().warnings;
        return out;
    }
};

class ChamferFeature final : public Feature {
public:
    std::string type() const override { return "Chamfer"; }
    std::vector<ParameterSpec> parameterSpecs() const override {
        return {{"distance", "1"}, {"distance2", "1"}, {"angle", "45"}};
    }
    std::vector<ReferenceSlot> referenceSlots() const override {
        return {{"edges", ElementKind::Edge, true}, {"face", ElementKind::Face, false, true}};
    }
    std::vector<OptionSpec> optionSpecs() const override { return {{"type", {"equal", "twoDistances", "distanceAngle"}}}; }
    Result<FeatureOutput> execute(const FeatureContext& ctx) const override {
        if (ctx.input().isNull()) return fail(ErrorCode::InvalidInput, "chamfer needs an existing body");
        ChamferParams p;
        const std::string& t = ctx.option("type");
        p.type = t == "twoDistances" ? ChamferType::TwoDistances
                 : t == "distanceAngle" ? ChamferType::DistanceAngle
                                        : ChamferType::EqualDistance;
        p.distance = ctx.parameter("distance");
        p.distance2 = ctx.parameter("distance2");
        p.angle = ctx.parameter("angle") * kDeg;
        if (!(p.distance > 0)) return fail(ErrorCode::InvalidInput, "chamfer distance must be positive: " + num(p.distance));
        if (!ctx.references("face").empty()) p.referenceFace = ctx.references("face").front();
        auto res = chamfer(ctx.input().shape(), ctx.references("edges"), p);
        if (!res) return res.error();
        FeatureOutput out;
        out.shape = res.value().shape;
        out.history = res.value().history;
        out.warnings = res.value().warnings;
        return out;
    }
};

// ---------------------------------------------------------------- patterns and mirror

// Source feature of a pattern or a feature mirror.
Result<const LinkedFeature*> sourceTool(const FeatureContext& ctx) {
    using R = Result<const LinkedFeature*>;
    const LinkedFeature* src = ctx.link("feature");
    if (!src) return R::failure(ErrorCode::InvalidInput, "link 'feature' is not set");
    if (src->tool.IsNull() || !src->toolMode) {
        return R::failure(ErrorCode::InvalidInput, "feature '" + src->id + "' (" + src->type + ") has no tool body to repeat");
    }
    return src;
}

// Threads of the copies (threadCopies): the threaded walls named by the source feature (src)
// or, without a source (mirror of the body), every threaded wall of the input body.
std::vector<std::pair<TopoDS_Shape, ElementAttributes>> copiedThreads(const FeatureResult& r, const LinkedFeature* src,
                                                                      const NamedShape& input) {
    if (r.copies.empty()) return {};
    const NamedShape* named = src ? src->output.get() : &input;
    if (!named) return {};
    std::vector<ThreadSpec> seeds;
    for (const ThreadInfo& t : threadsOf(*named)) {
        if (src && named->record(ElementKind::Face, t.face).feature != src->id) continue;
        seeds.push_back(t.thread);
    }
    return threadCopies(seeds, r.copies, r.shape);
}

FeatureOutput fromResult(FeatureResult r, const LinkedFeature* src, std::optional<FeatureMode> mode,
                         const NamedShape& input) {
    FeatureOutput out;
    out.attributes = copiedThreads(r, src, input);
    out.shape = std::move(r.shape);
    out.history = std::move(r.history);
    out.warnings = std::move(r.warnings);
    out.tool = std::move(r.tool);
    out.toolMode = mode;
    if (src && src->toolNamed) out.namingInputs.push_back(src->toolNamed);
    return out;
}

Result<gp_Dir> patternDirection(const FeatureContext& ctx, const std::string& choice, const char* edgeSlot,
                                const char* axisLink, bool reversed) {
    auto ax = axisFromOption(ctx, choice, "", edgeSlot, axisLink);
    if (!ax) return ax.error();
    gp_Dir d = ax.value().Direction();
    if (reversed) d.Reverse();
    return d;
}

int countOf(const FeatureContext& ctx, const char* name) { return static_cast<int>(std::llround(ctx.parameter(name))); }

class LinearPatternFeature final : public Feature {
public:
    std::string type() const override { return "LinearPattern"; }
    std::vector<ParameterSpec> parameterSpecs() const override {
        return {{"count1", "2"}, {"spacing1", "10"}, {"count2", "1"}, {"spacing2", "10"}};
    }
    std::vector<ReferenceSlot> referenceSlots() const override {
        return {{"edge1", ElementKind::Edge, false, true}, {"edge2", ElementKind::Edge, false, true}};
    }
    std::vector<OptionSpec> optionSpecs() const override {
        return {{"direction1", {"X", "Y", "Z", "edge", "datum"}},
                {"reversed1", {"false", "true"}},
                {"direction2", {"Y", "X", "Z", "edge", "datum"}},
                {"reversed2", {"false", "true"}}};
    }
    std::vector<LinkSlot> linkSlots() const override {
        return {{"feature", {}, false}, {"axis1", {"DatumAxis"}, true}, {"axis2", {"DatumAxis"}, true}};
    }
    Result<FeatureOutput> execute(const FeatureContext& ctx) const override {
        if (ctx.input().isNull()) return fail(ErrorCode::InvalidInput, "pattern needs an existing body");
        auto src = sourceTool(ctx);
        if (!src) return src.error();
        LinearPatternParams p;
        auto d1 = patternDirection(ctx, ctx.option("direction1"), "edge1", "axis1", ctx.option("reversed1") == "true");
        if (!d1) return d1.error();
        p.direction1 = d1.value();
        p.spacing1 = ctx.parameter("spacing1");
        p.count1 = countOf(ctx, "count1");
        p.count2 = countOf(ctx, "count2");
        p.spacing2 = ctx.parameter("spacing2");
        if (p.count2 > 1) {
            auto d2 = patternDirection(ctx, ctx.option("direction2"), "edge2", "axis2", ctx.option("reversed2") == "true");
            if (!d2) return d2.error();
            p.direction2 = d2.value();
        }
        auto res = linearPattern(ctx.input().shape(), src.value()->tool, *src.value()->toolMode, p);
        if (!res) return res.error();
        return fromResult(std::move(res).value(), src.value(), src.value()->toolMode, ctx.input());
    }
};

class CircularPatternFeature final : public Feature {
public:
    std::string type() const override { return "CircularPattern"; }
    std::vector<ParameterSpec> parameterSpecs() const override { return {{"count", "4"}, {"angle", "360"}}; }
    std::vector<ReferenceSlot> referenceSlots() const override {
        return {{"axisFace", ElementKind::Face, false, true}, {"axisEdge", ElementKind::Edge, false, true}};
    }
    std::vector<OptionSpec> optionSpecs() const override {
        return {{"axis", {"Z", "X", "Y", "face", "edge", "datum"}}, {"spacing", {"equal", "step"}}};
    }
    std::vector<LinkSlot> linkSlots() const override { return {{"feature", {}, false}, {"axis", {"DatumAxis"}, true}}; }
    Result<FeatureOutput> execute(const FeatureContext& ctx) const override {
        if (ctx.input().isNull()) return fail(ErrorCode::InvalidInput, "pattern needs an existing body");
        auto src = sourceTool(ctx);
        if (!src) return src.error();
        auto ax = axisFromOption(ctx, ctx.option("axis"), "axisFace", "axisEdge", "axis");
        if (!ax) return ax.error();
        CircularPatternParams p;
        p.axis = ax.value();
        p.count = countOf(ctx, "count");
        p.angle = ctx.parameter("angle") * kDeg;
        p.equalSpacing = ctx.option("spacing") == "equal";
        auto res = circularPattern(ctx.input().shape(), src.value()->tool, *src.value()->toolMode, p);
        if (!res) return res.error();
        return fromResult(std::move(res).value(), src.value(), src.value()->toolMode, ctx.input());
    }
};

class MirrorFeature final : public Feature {
public:
    std::string type() const override { return "Mirror"; }
    std::vector<ParameterSpec> parameterSpecs() const override { return {}; }
    std::vector<ReferenceSlot> referenceSlots() const override { return {{"face", ElementKind::Face, false, true}}; }
    std::vector<OptionSpec> optionSpecs() const override {
        return {{"what", {"feature", "body"}}, {"plane", {"YZ", "XZ", "XY", "face", "datum"}}, {"merge", {"true", "false"}}};
    }
    std::vector<LinkSlot> linkSlots() const override { return {{"feature", {}, true}, {"plane", {"DatumPlane"}, true}}; }
    Result<FeatureOutput> execute(const FeatureContext& ctx) const override {
        if (ctx.input().isNull()) return fail(ErrorCode::InvalidInput, "mirror needs an existing body");
        auto pln = planeFromOption(ctx, ctx.option("plane"), "face", "plane");
        if (!pln) return pln.error();
        const gp_Ax2 plane(pln.value().Location(), pln.value().Axis().Direction());
        if (ctx.option("what") == "body") {
            auto res = mirrorBody(ctx.input().shape(), plane, ctx.option("merge") == "true");
            if (!res) return res.error();
            return fromResult(std::move(res).value(), nullptr, std::nullopt, ctx.input());
        }
        auto src = sourceTool(ctx);
        if (!src) return src.error();
        auto res = mirrorFeature(ctx.input().shape(), src.value()->tool, *src.value()->toolMode, plane);
        if (!res) return res.error();
        return fromResult(std::move(res).value(), src.value(), src.value()->toolMode, ctx.input());
    }
};

// ---------------------------------------------------------------- shell / draft

class ShellFeature final : public Feature {
public:
    std::string type() const override { return "Shell"; }
    std::vector<ParameterSpec> parameterSpecs() const override { return {{"thickness", "1"}}; }
    std::vector<ReferenceSlot> referenceSlots() const override { return {{"faces", ElementKind::Face, true, true}}; }
    std::vector<OptionSpec> optionSpecs() const override { return {{"direction", {"inward", "outward"}}}; }
    Result<FeatureOutput> execute(const FeatureContext& ctx) const override {
        if (ctx.input().isNull()) return fail(ErrorCode::InvalidInput, "shell needs an existing body");
        auto res = shell(ctx.input().shape(), ctx.references("faces"), ctx.parameter("thickness"),
                         ctx.option("direction") == "outward" ? ShellDirection::Outward : ShellDirection::Inward);
        if (!res) return res.error();
        return fromResult(std::move(res).value(), nullptr, std::nullopt, ctx.input());
    }
};

class DraftFeature final : public Feature {
public:
    std::string type() const override { return "Draft"; }
    std::vector<ParameterSpec> parameterSpecs() const override { return {{"angle", "3"}}; }
    std::vector<ReferenceSlot> referenceSlots() const override {
        return {{"faces", ElementKind::Face, true}, {"neutralFace", ElementKind::Face, false, true}};
    }
    std::vector<OptionSpec> optionSpecs() const override {
        return {{"neutral", {"face", "datum", "XY", "XZ", "YZ"}}, {"reversed", {"false", "true"}}};
    }
    std::vector<LinkSlot> linkSlots() const override { return {{"plane", {"DatumPlane"}, true}}; }
    Result<FeatureOutput> execute(const FeatureContext& ctx) const override {
        if (ctx.input().isNull()) return fail(ErrorCode::InvalidInput, "draft needs an existing body");
        auto pln = planeFromOption(ctx, ctx.option("neutral"), "neutralFace", "plane");
        if (!pln) return pln.error();
        // Pull direction: the normal of the neutral plane (outward normal for a face): faces
        // are drafted so that the body narrows away from the neutral plane.
        gp_Dir pull = pln.value().Axis().Direction();
        if (ctx.option("neutral") == "face") pull.Reverse();
        if (ctx.option("reversed") == "true") pull.Reverse();
        auto res = draftFaces(ctx.input().shape(), ctx.references("faces"), pull, ctx.parameter("angle") * kDeg,
                              pln.value().Position());
        if (!res) return res.error();
        return fromResult(std::move(res).value(), nullptr, std::nullopt, ctx.input());
    }
};

// ---------------------------------------------------------------- primitives

// Extrudes a planar face with features::extrude (no target), names the tool faces with
// `roleOf`, then fuses with the input body through the common tool application.
template <class RoleOf>
Result<FeatureOutput> primitive(const FeatureContext& ctx, const TopoDS_Face& face, const gp_Ax3& frame, double height,
                                RoleOf roleOf) {
    ExtrudeParams p;
    p.distance = height;
    auto tool = extrude(Profile{face, frame}, TopoDS_Shape(), p);
    if (!tool) return tool.error();
    FeatureOutput out;
    for (TopExp_Explorer ex(tool.value().shape, TopAbs_FACE); ex.More(); ex.Next()) {
        out.roles.push_back({ex.Current(), roleOf(TopoDS::Face(ex.Current()))});
    }
    auto applied = fc::applyTool(FeatureMode::Boss, ctx.input().shape(), tool.value().shape, fc::History(), {});
    if (!applied) return applied.error();
    out.shape = applied.value().shape;
    out.history = applied.value().history;
    out.warnings = applied.value().warnings;
    out.tool = tool.value().shape;
    out.toolMode = FeatureMode::Boss;
    return out;
}

gp_Dir outwardNormal(const TopoDS_Face& f) {
    BRepAdaptor_Surface s(f, false);
    gp_Dir n = s.Plane().Axis().Direction();
    if (!s.Plane().Position().Direct()) n.Reverse();  // left-handed plane: Du x Dv = -axis
    if (f.Orientation() == TopAbs_REVERSED) n.Reverse();
    return n;
}

class BoxFeature final : public Feature {
public:
    std::string type() const override { return "Box"; }
    std::vector<ParameterSpec> parameterSpecs() const override {
        return {{"length", "10"}, {"width", "10"}, {"height", "10"}, {"x", "0"}, {"y", "0"}, {"z", "0"}};
    }
    std::vector<ReferenceSlot> referenceSlots() const override { return {}; }
    std::vector<OptionSpec> optionSpecs() const override { return {}; }
    Result<FeatureOutput> execute(const FeatureContext& ctx) const override {
        const double l = ctx.parameter("length"), w = ctx.parameter("width"), h = ctx.parameter("height");
        if (!(l > 0) || !(w > 0) || !(h > 0)) {
            return fail(ErrorCode::InvalidInput, "box dimensions must be positive: " + num(l) + " x " + num(w) + " x " + num(h));
        }
        const gp_Pnt o(ctx.parameter("x"), ctx.parameter("y"), ctx.parameter("z"));
        BRepBuilderAPI_MakePolygon poly(o, o.Translated(gp_Vec(l, 0, 0)), o.Translated(gp_Vec(l, w, 0)),
                                        o.Translated(gp_Vec(0, w, 0)), true);
        BRepBuilderAPI_MakeFace mf(gp_Pln(gp_Ax3(o, gp::DZ(), gp::DX())), poly.Wire(), true);
        if (!mf.IsDone()) return fail(ErrorCode::OperationFailed, "box base face failed");
        // Roles follow the usual CAD views (the front view looks along +Y).
        return primitive(ctx, mf.Face(), gp_Ax3(o, gp::DZ(), gp::DX()), h, [](const TopoDS_Face& f) {
            const gp_Dir n = outwardNormal(f);
            if (n.Z() > 0.5) return "Top";
            if (n.Z() < -0.5) return "Bottom";
            if (n.Y() < -0.5) return "Front";
            if (n.Y() > 0.5) return "Back";
            return n.X() < 0 ? "Left" : "Right";
        });
    }
};

class CylinderFeature final : public Feature {
public:
    std::string type() const override { return "Cylinder"; }
    std::vector<ParameterSpec> parameterSpecs() const override {
        return {{"radius", "5"}, {"height", "10"}, {"x", "0"}, {"y", "0"}, {"z", "0"}};
    }
    std::vector<ReferenceSlot> referenceSlots() const override { return {}; }
    std::vector<OptionSpec> optionSpecs() const override { return {}; }
    Result<FeatureOutput> execute(const FeatureContext& ctx) const override {
        const double r = ctx.parameter("radius"), h = ctx.parameter("height");
        if (!(r > 0) || !(h > 0)) {
            return fail(ErrorCode::InvalidInput, "cylinder radius and height must be positive: r=" + num(r) + ", h=" + num(h));
        }
        const gp_Pnt o(ctx.parameter("x"), ctx.parameter("y"), ctx.parameter("z"));
        BRepBuilderAPI_MakeEdge edge(gp_Circ(gp_Ax2(o, gp::DZ(), gp::DX()), r));
        BRepBuilderAPI_MakeWire wire(edge.Edge());
        BRepBuilderAPI_MakeFace mf(gp_Pln(gp_Ax3(o, gp::DZ(), gp::DX())), wire.Wire(), true);
        if (!mf.IsDone()) return fail(ErrorCode::OperationFailed, "cylinder base face failed");
        return primitive(ctx, mf.Face(), gp_Ax3(o, gp::DZ(), gp::DX()), h, [](const TopoDS_Face& f) {
            BRepAdaptor_Surface s(f, false);
            if (s.GetType() != GeomAbs_Plane) return "Side";
            return outwardNormal(f).Z() > 0 ? "Top" : "Bottom";
        });
    }
};

}  // namespace

void addBodyFeatures(std::vector<std::pair<std::string, FeatureRegistry::Factory>>& all) {
    all.emplace_back("Extrude", [] { return std::make_unique<ExtrudeFeature>(); });
    all.emplace_back("Revolve", [] { return std::make_unique<RevolveFeature>(); });
    all.emplace_back("Hole", [] { return std::make_unique<HoleFeature>(); });
    all.emplace_back("Fillet", [] { return std::make_unique<FilletFeature>(); });
    all.emplace_back("Chamfer", [] { return std::make_unique<ChamferFeature>(); });
    all.emplace_back("LinearPattern", [] { return std::make_unique<LinearPatternFeature>(); });
    all.emplace_back("CircularPattern", [] { return std::make_unique<CircularPatternFeature>(); });
    all.emplace_back("Mirror", [] { return std::make_unique<MirrorFeature>(); });
    all.emplace_back("Shell", [] { return std::make_unique<ShellFeature>(); });
    all.emplace_back("Draft", [] { return std::make_unique<DraftFeature>(); });
    all.emplace_back("Box", [] { return std::make_unique<BoxFeature>(); });
    all.emplace_back("Cylinder", [] { return std::make_unique<CylinderFeature>(); });
}

}  // namespace features

Status registerPartFeatures(FeatureRegistry& registry) {
    std::vector<std::pair<std::string, FeatureRegistry::Factory>> all;
    features::addSketchFeatures(all);
    features::addBodyFeatures(all);
    features::addAdvancedFeatures(all);
    for (const auto& [type, factory] : all) {
        auto st = registry.add(type, factory);
        if (!st) return st;
    }
    return std::monostate{};
}

std::shared_ptr<const FeatureRegistry> partRegistry() {
    auto registry = std::make_shared<FeatureRegistry>();
    auto st = registerPartFeatures(*registry);
    if (!st) throw std::logic_error("part feature registration failed: " + st.error().message);
    return registry;
}

}  // namespace bendcore::modeling
