// Operations of the part document, phase 2 (docs/modeling/PART_DOCUMENT.md, section "Операции"):
// Sweep, Loft, Rib, ThreadedHole and the multibody nodes BodyBoolean, DeleteBody,
// BodyLinearPattern, BodyCircularPattern, BodyMirror.

#include "part_features_internal.hpp"

#include "../features/feature_common.hpp"

#include <bendcore/modeling/features.hpp>
#include <bendcore/modeling/reference_geometry.hpp>

#include <BRepAdaptor_Surface.hxx>
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>

#include <cmath>
#include <set>

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

FeatureMode modeOf(const FeatureContext& ctx) { return featureModeFromString(ctx.option("mode")).value_or(FeatureMode::Boss); }

const SketchResult* sketchOf(const FeatureContext& ctx, const char* slot, std::string& error) {
    const LinkedFeature* sk = ctx.link(slot);
    if (!sk || !sk->sketch) {
        error = std::string("link '") + slot + "' must name a sketch";
        return nullptr;
    }
    return sk->sketch.get();
}

// Faces of the selected regions of a closed-contour sketch.
Result<std::vector<TopoDS_Face>> regionFaces(const FeatureContext& ctx, const char* slot) {
    using R = Result<std::vector<TopoDS_Face>>;
    std::string error;
    const SketchResult* sk = sketchOf(ctx, slot, error);
    if (!sk) return R::failure(ErrorCode::InvalidInput, error);
    if (!sk->faces) return R::failure(ErrorCode::InvalidInput, "sketch '" + ctx.link(slot)->id + "' has no closed profile");
    std::vector<TopoDS_Face> out;
    for (std::size_t i = 0; i < sk->faces->count(ElementKind::Face); ++i)
        out.push_back(TopoDS::Face(sk->faces->element(ElementKind::Face, i)));
    return out;
}

// Path of an open- or closed-contour sketch, with the element names of its edges
// ("<sketch>:E<id>") for the naming of the elements generated from them.
struct NamedPath {
    SketchPath path;
    std::shared_ptr<const NamedShape> named;
};

Result<NamedPath> pathOf(const FeatureContext& ctx, const char* slot) {
    using R = Result<NamedPath>;
    std::string error;
    const SketchResult* sk = sketchOf(ctx, slot, error);
    if (!sk) return R::failure(ErrorCode::InvalidInput, error);
    double scale = 1.0;
    for (const auto& p : sk->points) scale = std::max({scale, std::abs(p.x), std::abs(p.y)});
    auto path = buildSketchPath(sk->placement, sk->curves, ProfileOptions{1e-6 * scale});
    if (!path) return R::failure(path.error().code, "sketch '" + ctx.link(slot)->id + "': " + path.error().message);
    NamingRequest req;
    req.featureId = ctx.link(slot)->id;
    req.result = path.value().wire;
    for (std::size_t k = 0; k < path.value().edges.size(); ++k)
        req.roles.push_back({path.value().edges[k], "E" + std::to_string(path.value().entities[k])});
    auto named = buildNamedShape(req);
    if (!named) return named.error();
    return NamedPath{std::move(path).value(), std::make_shared<const NamedShape>(std::move(named).value())};
}

// Path: a sketch (link) or a chain of model edges (references).
Result<TopoDS_Wire> pathFromChoice(const FeatureContext& ctx, const std::string& choice, const char* link,
                                   const char* edgesSlot, std::vector<std::shared_ptr<const NamedShape>>& names) {
    if (choice == "edges") {
        const auto& edges = ctx.references(edgesSlot);
        if (edges.empty()) return Error{ErrorCode::InvalidInput, std::string("reference slot '") + edgesSlot + "' is empty"};
        return chainEdges(edges);
    }
    auto p = pathOf(ctx, link);
    if (!p) return p.error();
    names.push_back(p.value().named);
    return p.value().path.wire;
}

FeatureOutput outputOf(FeatureResult r, std::optional<FeatureMode> mode) {
    FeatureOutput out;
    out.shape = std::move(r.shape);
    out.history = std::move(r.history);
    out.warnings = std::move(r.warnings);
    out.tool = std::move(r.tool);
    out.toolMode = mode;
    for (auto& [element, role] : r.roles) out.roles.push_back({element, role, {}});
    return out;
}

// ---------------------------------------------------------------- sweep

class SweepFeature final : public Feature {
public:
    std::string type() const override { return "Sweep"; }
    std::vector<ParameterSpec> parameterSpecs() const override { return {{"thickness", "1"}}; }
    std::vector<ReferenceSlot> referenceSlots() const override {
        return {{"pathEdges", ElementKind::Edge, true, true}, {"guideEdges", ElementKind::Edge, true, true}};
    }
    std::vector<OptionSpec> optionSpecs() const override {
        return withMode({{"path", {"sketch", "edges"}},
                         {"orientation", {"correctedFrenet", "frenet", "fixed", "binormal", "guide"}},
                         {"binormal", {"Z", "X", "Y"}},
                         {"guide", {"sketch", "edges"}},
                         {"thin", {"none", "inside", "outside"}}});
    }
    std::vector<LinkSlot> linkSlots() const override {
        return {{"sketch", {"Sketch"}, false}, {"pathSketch", {"Sketch"}, true}, {"guideSketch", {"Sketch"}, true}};
    }
    Result<FeatureOutput> execute(const FeatureContext& ctx) const override {
        SweepParams p;
        p.mode = modeOf(ctx);
        const std::string& o = ctx.option("orientation");
        p.orientation = o == "frenet"     ? SweepOrientation::Frenet
                        : o == "fixed"    ? SweepOrientation::Fixed
                        : o == "binormal" ? SweepOrientation::Binormal
                        : o == "guide"    ? SweepOrientation::Guide
                                          : SweepOrientation::CorrectedFrenet;
        const std::string& b = ctx.option("binormal");
        p.binormal = b == "X" ? gp::DX() : b == "Y" ? gp::DY() : gp::DZ();
        if (ctx.option("thin") != "none")
            p.thin = ThinWall{ctx.parameter("thickness"), ctx.option("thin") == "outside" ? ThinSide::Outside : ThinSide::Inside};
        std::vector<std::shared_ptr<const NamedShape>> extraNames;
        auto path = pathFromChoice(ctx, ctx.option("path"), "pathSketch", "pathEdges", extraNames);
        if (!path) return fail(path.error().code, "sweep path: " + path.error().message);
        if (p.orientation == SweepOrientation::Guide) {
            std::vector<std::shared_ptr<const NamedShape>> unused;
            auto guide = pathFromChoice(ctx, ctx.option("guide"), "guideSketch", "guideEdges", unused);
            if (!guide) return fail(guide.error().code, "sweep guide: " + guide.error().message);
            p.guide = guide.value();
        }
        auto faces = regionFaces(ctx, "sketch");
        if (!faces) return faces.error();
        const LinkedFeature* sk = ctx.link("sketch");
        TopoDS_Shape body = ctx.input().shape();
        fc::History history = fc::newHistory();
        std::vector<TopoDS_Shape> tools;
        FeatureOutput out;
        for (std::size_t i = 0; i < faces.value().size(); ++i) {
            auto step = sweep(Profile{faces.value()[i], sk->sketch->placement.frame()}, path.value(), body, p);
            if (!step) {
                std::string where = faces.value().size() > 1 ? " (region " + std::to_string(i + 1) + ")" : "";
                return fail(step.error().code, "sweep" + where + ": " + step.error().message);
            }
            fc::mergeInto(history, step.value().history);
            body = step.value().shape;
            tools.push_back(step.value().tool);
            out.warnings.insert(out.warnings.end(), step.value().warnings.begin(), step.value().warnings.end());
        }
        out.shape = body;
        out.history = history;
        out.tool = makeCompound(tools);
        out.toolMode = p.mode;
        out.namingInputs.push_back(sk->sketch->faces);
        return out;
    }
};

// ---------------------------------------------------------------- loft

constexpr int kMaxSections = 8;

class LoftFeature final : public Feature {
public:
    std::string type() const override { return "Loft"; }
    std::vector<ParameterSpec> parameterSpecs() const override { return {}; }
    std::vector<ReferenceSlot> referenceSlots() const override { return {}; }
    std::vector<OptionSpec> optionSpecs() const override {
        return withMode({{"surface", {"smooth", "ruled"}}, {"closed", {"false", "true"}}});
    }
    std::vector<LinkSlot> linkSlots() const override {
        std::vector<LinkSlot> slots;
        for (int i = 1; i <= kMaxSections; ++i) slots.push_back({"section" + std::to_string(i), {"Sketch"}, i > 2});
        return slots;
    }
    Result<FeatureOutput> execute(const FeatureContext& ctx) const override {
        LoftParams p;
        p.mode = modeOf(ctx);
        p.ruled = ctx.option("surface") == "ruled";
        p.closed = ctx.option("closed") == "true";
        std::vector<Profile> sections;
        FeatureOutput out;
        bool gap = false;
        for (int i = 1; i <= kMaxSections; ++i) {
            const std::string slot = "section" + std::to_string(i);
            const LinkedFeature* sk = ctx.link(slot);
            if (!sk) {
                gap = true;
                continue;
            }
            if (gap) return fail(ErrorCode::InvalidInput, "loft sections must be consecutive: '" + slot + "' follows an empty slot");
            auto faces = regionFaces(ctx, slot.c_str());
            if (!faces) return faces.error();
            if (faces.value().size() != 1)
                return fail(ErrorCode::InvalidInput, "loft section '" + sk->id + "' must have exactly one region, has " +
                                                         std::to_string(faces.value().size()));
            sections.push_back(Profile{faces.value().front(), sk->sketch->placement.frame()});
            out.namingInputs.push_back(sk->sketch->faces);
        }
        auto res = loft(sections, ctx.input().shape(), p);
        if (!res) return fail(res.error().code, "loft: " + res.error().message);
        FeatureOutput r = outputOf(std::move(res).value(), p.mode);
        r.namingInputs = std::move(out.namingInputs);
        return r;
    }
};

// ---------------------------------------------------------------- rib

class RibFeature final : public Feature {
public:
    std::string type() const override { return "Rib"; }
    std::vector<ParameterSpec> parameterSpecs() const override { return {{"thickness", "2"}}; }
    std::vector<ReferenceSlot> referenceSlots() const override { return {}; }
    std::vector<OptionSpec> optionSpecs() const override {
        return {{"direction", {"parallel", "perpendicular"}},
                {"side", {"symmetric", "first", "second"}},
                {"flip", {"auto", "false", "true"}}};
    }
    std::vector<LinkSlot> linkSlots() const override { return {{"sketch", {"Sketch"}, false}}; }
    Result<FeatureOutput> execute(const FeatureContext& ctx) const override {
        if (ctx.input().isNull()) return fail(ErrorCode::InvalidInput, "rib needs an existing body");
        auto path = pathOf(ctx, "sketch");
        if (!path) return path.error();
        if (path.value().path.closed) return fail(ErrorCode::InvalidInput, "rib contour must be open");
        RibParams p;
        p.thickness = ctx.parameter("thickness");
        p.direction = ctx.option("direction") == "perpendicular" ? RibDirection::PerpendicularToSketch
                                                                 : RibDirection::ParallelToSketch;
        const std::string& side = ctx.option("side");
        p.side = side == "first" ? RibThickness::FirstSide : side == "second" ? RibThickness::SecondSide : RibThickness::Symmetric;
        if (ctx.option("flip") != "auto") p.flip = ctx.option("flip") == "true";
        const SketchResult* sk = ctx.link("sketch")->sketch.get();
        auto res = rib(path.value().path.wire, sk->placement.frame(), ctx.input().shape(), p);
        if (!res) return fail(res.error().code, "rib: " + res.error().message);
        // Wall.E<k> (k — edge of the contour in traversal order) -> Wall.E<entity id>.
        for (auto& [element, role] : res.value().roles) {
            if (role.rfind("Wall.E", 0) != 0) continue;
            const std::size_t dot = role.find('.', 6);
            const std::size_t k = static_cast<std::size_t>(std::stoul(role.substr(6, dot - 6)));
            if (k >= 1 && k <= path.value().path.entities.size())
                role = "Wall.E" + std::to_string(path.value().path.entities[k - 1]) + role.substr(dot);
        }
        FeatureOutput out = outputOf(std::move(res).value(), FeatureMode::Boss);
        out.namingInputs.push_back(path.value().named);
        return out;
    }
};

// ---------------------------------------------------------------- threaded hole

class ThreadedHoleFeature final : public Feature {
public:
    std::string type() const override { return "ThreadedHole"; }
    std::vector<ParameterSpec> parameterSpecs() const override {
        return {{"nominal", "10"},   {"pitch", "0"}, {"threadLength", "10"}, {"drillDepth", "0"},
                {"drillDiameter", "0"}, {"x", "0"},  {"y", "0"},             {"z", "0"}};
    }
    std::vector<ReferenceSlot> referenceSlots() const override { return {{"face", ElementKind::Face, false, false}}; }
    std::vector<OptionSpec> optionSpecs() const override {
        return {{"end", {"blind", "through"}}, {"hand", {"right", "left"}}, {"drillPoint", {"true", "false"}}};
    }
    std::vector<LinkSlot> linkSlots() const override { return {{"sketch", {"Sketch"}, true}}; }
    Result<FeatureOutput> execute(const FeatureContext& ctx) const override {
        if (ctx.input().isNull()) return fail(ErrorCode::InvalidInput, "threaded hole needs an existing body");
        ThreadedHoleParams p;
        const double pitch = ctx.parameter("pitch");
        auto thread = pitch == 0.0 ? metricCoarse(ctx.parameter("nominal")) : metricThread(ctx.parameter("nominal"), pitch);
        if (!thread) return thread.error();
        p.thread = thread.value();
        p.hand = ctx.option("hand") == "left" ? ThreadHand::Left : ThreadHand::Right;
        p.end = ctx.option("end") == "through" ? ThreadEnd::Through : ThreadEnd::Blind;
        p.threadLength = ctx.parameter("threadLength");
        p.drillDepth = ctx.parameter("drillDepth");
        if (ctx.parameter("drillDiameter") != 0.0) p.drillDiameter = ctx.parameter("drillDiameter");
        p.drillPoint = ctx.option("drillPoint") == "true";

        TopoDS_Face face = TopoDS::Face(ctx.references("face").front());
        auto pln = planeOfFace(face);
        if (!pln) return pln.error();
        std::vector<gp_Pnt> centers;
        if (const LinkedFeature* sk = ctx.link("sketch")) {
            if (!sk->sketch) return fail(ErrorCode::InvalidInput, "link 'sketch' must name a sketch");
            for (std::size_t k : sk->sketch->standalonePoints) centers.push_back(sk->sketch->placement.toWorld(sk->sketch->points[k]));
            if (centers.empty())
                return fail(ErrorCode::InvalidInput, "sketch '" + sk->id + "' has no standalone points for hole centres");
        } else {
            centers.push_back(pointProjectedOnPlane(gp_Pnt(ctx.parameter("x"), ctx.parameter("y"), ctx.parameter("z")), pln.value()));
        }
        TopoDS_Shape body = ctx.input().shape();
        fc::History history = fc::newHistory();
        std::vector<TopoDS_Shape> tools;
        FeatureOutput out;
        for (std::size_t k = 0; k < centers.size(); ++k) {
            auto step = threadedHole(body, face, centers[k], p);
            if (!step) {
                std::string where = centers.size() > 1 ? " #" + std::to_string(k + 1) : "";
                return fail(step.error().code, "threaded hole" + where + ": " + step.error().message);
            }
            const std::string prefix = centers.size() > 1 ? "H" + std::to_string(k + 1) + "." : "";
            const ThreadSpec& spec = step.value().threads.front().thread;
            // Roles on the drill tool faces; the history carries them to the result.
            for (TopExp_Explorer ex(step.value().tool, TopAbs_FACE); ex.More(); ex.Next()) {
                BRepAdaptor_Surface s(TopoDS::Face(ex.Current()), false);
                GProp_GProps g;
                BRepGProp::SurfaceProperties(ex.Current(), g);
                const double along = gp_Vec(centers[k], g.CentreOfMass()).Dot(gp_Vec(spec.direction));
                if (s.GetType() == GeomAbs_Cylinder) {
                    out.roles.push_back({ex.Current(), prefix + "Wall", threadAttributes(spec)});
                } else if (s.GetType() == GeomAbs_Cone) {
                    out.roles.push_back({ex.Current(), prefix + "Tip", {}});
                } else {
                    out.roles.push_back({ex.Current(), prefix + (std::abs(along) < 1e-7 ? "Top" : "Bottom"), {}});
                }
            }
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
        out.tool = makeCompound(tools);
        out.toolMode = FeatureMode::Cut;
        return out;
    }
};

// ---------------------------------------------------------------- bodies

// Bodies designated by face references (any face of the body).
Result<std::vector<TopoDS_Shape>> bodiesOf(const FeatureContext& ctx, const char* slot) {
    using R = Result<std::vector<TopoDS_Shape>>;
    std::vector<TopoDS_Shape> out;
    const NamedShape& in = ctx.input();
    for (const auto& f : ctx.references(slot)) {
        auto b = in.bodyOf(f);
        if (!b) return R::failure(ErrorCode::InvalidInput, std::string("a face of slot '") + slot + "' belongs to no body");
        const TopoDS_Shape& s = in.body(*b);
        if (std::none_of(out.begin(), out.end(), [&](const TopoDS_Shape& x) { return x.IsSame(s); })) out.push_back(s);
    }
    return out;
}

class BodyBooleanFeature final : public Feature {
public:
    std::string type() const override { return "BodyBoolean"; }
    std::vector<ParameterSpec> parameterSpecs() const override { return {}; }
    std::vector<ReferenceSlot> referenceSlots() const override {
        return {{"target", ElementKind::Face, false, false}, {"tools", ElementKind::Face, true, false}};
    }
    std::vector<OptionSpec> optionSpecs() const override {
        return {{"operation", {"union", "subtract", "intersect"}}, {"keepTools", {"false", "true"}}};
    }
    Result<FeatureOutput> execute(const FeatureContext& ctx) const override {
        if (ctx.input().bodyCount() < 2) return fail(ErrorCode::InvalidInput, "body boolean needs a part with at least two bodies");
        auto target = bodiesOf(ctx, "target");
        if (!target) return target.error();
        auto tools = bodiesOf(ctx, "tools");
        if (!tools) return tools.error();
        const std::string& op = ctx.option("operation");
        const BodyBooleanType type = op == "subtract" ? BodyBooleanType::Subtract
                                     : op == "intersect" ? BodyBooleanType::Intersect
                                                         : BodyBooleanType::Union;
        auto res = bodyBoolean(ctx.input().shape(), target.value().front(), tools.value(), type, ctx.option("keepTools") == "true");
        if (!res) return res.error();
        return outputOf(std::move(res).value(), std::nullopt);
    }
};

class DeleteBodyFeature final : public Feature {
public:
    std::string type() const override { return "DeleteBody"; }
    std::vector<ParameterSpec> parameterSpecs() const override { return {}; }
    std::vector<ReferenceSlot> referenceSlots() const override { return {{"bodies", ElementKind::Face, true, false}}; }
    std::vector<OptionSpec> optionSpecs() const override { return {}; }
    Result<FeatureOutput> execute(const FeatureContext& ctx) const override {
        auto bodies = bodiesOf(ctx, "bodies");
        if (!bodies) return bodies.error();
        auto res = deleteBodies(ctx.input().shape(), bodies.value());
        if (!res) return res.error();
        return outputOf(std::move(res).value(), std::nullopt);
    }
};

int countOf(const FeatureContext& ctx, const char* name) { return static_cast<int>(std::llround(ctx.parameter(name))); }

Result<FeatureOutput> copies(const FeatureContext& ctx, const std::vector<gp_Trsf>& transforms) {
    auto bodies = bodiesOf(ctx, "bodies");
    if (!bodies) return bodies.error();
    auto res = copyBodies(ctx.input().shape(), bodies.value(), transforms, ctx.option("merge") == "true");
    if (!res) return res.error();
    return outputOf(std::move(res).value(), std::nullopt);
}

class BodyLinearPatternFeature final : public Feature {
public:
    std::string type() const override { return "BodyLinearPattern"; }
    std::vector<ParameterSpec> parameterSpecs() const override {
        return {{"count1", "2"}, {"spacing1", "10"}, {"count2", "1"}, {"spacing2", "10"}};
    }
    std::vector<ReferenceSlot> referenceSlots() const override {
        return {{"bodies", ElementKind::Face, true, false},
                {"edge1", ElementKind::Edge, false, true},
                {"edge2", ElementKind::Edge, false, true}};
    }
    std::vector<OptionSpec> optionSpecs() const override {
        return {{"direction1", {"X", "Y", "Z", "edge", "datum"}}, {"reversed1", {"false", "true"}},
                {"direction2", {"Y", "X", "Z", "edge", "datum"}}, {"reversed2", {"false", "true"}},
                {"merge", {"false", "true"}}};
    }
    std::vector<LinkSlot> linkSlots() const override {
        return {{"axis1", {"DatumAxis"}, true}, {"axis2", {"DatumAxis"}, true}};
    }
    Result<FeatureOutput> execute(const FeatureContext& ctx) const override {
        const int n1 = countOf(ctx, "count1"), n2 = countOf(ctx, "count2");
        if (n1 < 1 || n2 < 1) return fail(ErrorCode::InvalidInput, "pattern counts must be >= 1");
        auto a1 = axisFromOption(ctx, ctx.option("direction1"), "", "edge1", "axis1");
        if (!a1) return a1.error();
        gp_Dir d1 = a1.value().Direction();
        if (ctx.option("reversed1") == "true") d1.Reverse();
        gp_Dir d2 = d1;
        if (n2 > 1) {
            auto a2 = axisFromOption(ctx, ctx.option("direction2"), "", "edge2", "axis2");
            if (!a2) return a2.error();
            d2 = a2.value().Direction();
            if (ctx.option("reversed2") == "true") d2.Reverse();
            if (d1.IsParallel(d2, 1e-9)) return fail(ErrorCode::InvalidInput, "pattern directions must not be parallel");
        }
        std::vector<gp_Trsf> transforms;
        for (int j = 0; j < n2; ++j)
            for (int i = 0; i < n1; ++i) {
                if (i == 0 && j == 0) continue;
                gp_Trsf t;
                t.SetTranslation(gp_Vec(d1) * (ctx.parameter("spacing1") * i) + gp_Vec(d2) * (ctx.parameter("spacing2") * j));
                transforms.push_back(t);
            }
        if (transforms.empty()) return fail(ErrorCode::InvalidInput, "pattern has no instances besides the original");
        return copies(ctx, transforms);
    }
};

class BodyCircularPatternFeature final : public Feature {
public:
    std::string type() const override { return "BodyCircularPattern"; }
    std::vector<ParameterSpec> parameterSpecs() const override { return {{"count", "4"}, {"angle", "360"}}; }
    std::vector<ReferenceSlot> referenceSlots() const override {
        return {{"bodies", ElementKind::Face, true, false},
                {"axisFace", ElementKind::Face, false, true},
                {"axisEdge", ElementKind::Edge, false, true}};
    }
    std::vector<OptionSpec> optionSpecs() const override {
        return {{"axis", {"Z", "X", "Y", "face", "edge", "datum"}}, {"spacing", {"equal", "step"}}, {"merge", {"false", "true"}}};
    }
    std::vector<LinkSlot> linkSlots() const override { return {{"axis", {"DatumAxis"}, true}}; }
    Result<FeatureOutput> execute(const FeatureContext& ctx) const override {
        const int n = countOf(ctx, "count");
        if (n < 2) return fail(ErrorCode::InvalidInput, "circular pattern count must be >= 2");
        auto ax = axisFromOption(ctx, ctx.option("axis"), "axisFace", "axisEdge", "axis");
        if (!ax) return ax.error();
        const double angle = ctx.parameter("angle") * kDeg;
        if (!(std::abs(angle) > 0)) return fail(ErrorCode::InvalidInput, "circular pattern angle must be non-zero");
        double step = angle;
        if (ctx.option("spacing") == "equal") {
            if (std::abs(angle) > 2 * M_PI + 1e-12) return fail(ErrorCode::InvalidInput, "total pattern angle must not exceed 360");
            step = std::abs(std::abs(angle) - 2 * M_PI) < 1e-12 ? angle / n : angle / (n - 1);
        }
        std::vector<gp_Trsf> transforms;
        for (int i = 1; i < n; ++i) {
            gp_Trsf t;
            t.SetRotation(ax.value(), step * i);
            transforms.push_back(t);
        }
        return copies(ctx, transforms);
    }
};

class BodyMirrorFeature final : public Feature {
public:
    std::string type() const override { return "BodyMirror"; }
    std::vector<ParameterSpec> parameterSpecs() const override { return {}; }
    std::vector<ReferenceSlot> referenceSlots() const override {
        return {{"bodies", ElementKind::Face, true, false}, {"face", ElementKind::Face, false, true}};
    }
    std::vector<OptionSpec> optionSpecs() const override {
        return {{"plane", {"YZ", "XZ", "XY", "face", "datum"}}, {"merge", {"false", "true"}}};
    }
    std::vector<LinkSlot> linkSlots() const override { return {{"plane", {"DatumPlane"}, true}}; }
    Result<FeatureOutput> execute(const FeatureContext& ctx) const override {
        auto pln = planeFromOption(ctx, ctx.option("plane"), "face", "plane");
        if (!pln) return pln.error();
        gp_Trsf t;
        t.SetMirror(gp_Ax2(pln.value().Location(), pln.value().Axis().Direction()));
        return copies(ctx, {t});
    }
};

}  // namespace

void addAdvancedFeatures(std::vector<std::pair<std::string, FeatureRegistry::Factory>>& all) {
    all.emplace_back("Sweep", [] { return std::make_unique<SweepFeature>(); });
    all.emplace_back("Loft", [] { return std::make_unique<LoftFeature>(); });
    all.emplace_back("Rib", [] { return std::make_unique<RibFeature>(); });
    all.emplace_back("ThreadedHole", [] { return std::make_unique<ThreadedHoleFeature>(); });
    all.emplace_back("BodyBoolean", [] { return std::make_unique<BodyBooleanFeature>(); });
    all.emplace_back("DeleteBody", [] { return std::make_unique<DeleteBodyFeature>(); });
    all.emplace_back("BodyLinearPattern", [] { return std::make_unique<BodyLinearPatternFeature>(); });
    all.emplace_back("BodyCircularPattern", [] { return std::make_unique<BodyCircularPatternFeature>(); });
    all.emplace_back("BodyMirror", [] { return std::make_unique<BodyMirrorFeature>(); });
}

}  // namespace features

}  // namespace bendcore::modeling
