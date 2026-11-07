// Basic OCCT features with history (docs/modeling/PART_DOCUMENT.md).

#include <bendcore/modeling/basic_features.hpp>

#include <BRepAdaptor_Surface.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepBndLib.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepFilletAPI_MakeChamfer.hxx>
#include <BRepFilletAPI_MakeFillet.hxx>
#include <BRepGProp.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <Bnd_Box.hxx>
#include <GProp_GProps.hxx>
#include <Message_Report.hxx>
#include <NCollection_List.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <gp_Ax2.hxx>
#include <gp_Circ.hxx>
#include <gp_Pln.hxx>

#include <cmath>
#include <sstream>

namespace bendcore::modeling::basic {

namespace {

Result<FeatureOutput> fail(ErrorCode code, std::string message) {
    return Result<FeatureOutput>::failure(code, std::move(message));
}

std::string num(double v) {
    std::ostringstream os;
    os << v;
    return os.str();
}

double volumeOf(const TopoDS_Shape& shape) {
    GProp_GProps props;
    BRepGProp::VolumeProperties(shape, props);
    return props.Mass();
}

std::string algoErrors(const BRepAlgoAPI_BooleanOperation& op) {
    std::ostringstream os;
    op.DumpErrors(os);
    std::string s = os.str();
    return s.empty() ? std::string("no diagnostics") : s;
}

// Fuses a freshly built primitive with the input (if any). Roles stay on the primitive's
// faces; the fuse history carries them to the result.
Result<FeatureOutput> placePrimitive(const FeatureContext& ctx, const TopoDS_Shape& primitive,
                                     std::vector<ElementRole> roles) {
    FeatureOutput out;
    out.roles = std::move(roles);
    if (ctx.input().isNull()) {
        out.shape = primitive;
        return out;
    }
    BRepAlgoAPI_Fuse fuse;
    NCollection_List<TopoDS_Shape> args, tools;
    args.Append(ctx.input().shape());
    tools.Append(primitive);
    fuse.SetArguments(args);
    fuse.SetTools(tools);
    fuse.SetToFillHistory(true);
    fuse.Build();
    if (!fuse.IsDone() || fuse.HasErrors()) {
        return fail(ErrorCode::OperationFailed, "fuse with the existing body failed: " + algoErrors(fuse));
    }
    out.shape = fuse.Shape();
    out.history = fuse.History();
    return out;
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
            return fail(ErrorCode::InvalidInput,
                        "box dimensions must be positive: " + num(l) + " x " + num(w) + " x " + num(h));
        }
        BRepPrimAPI_MakeBox mk(gp_Pnt(ctx.parameter("x"), ctx.parameter("y"), ctx.parameter("z")), l, w, h);
        mk.Build();
        if (!mk.IsDone()) return fail(ErrorCode::OperationFailed, "BRepPrimAPI_MakeBox failed");
        // Roles follow the usual CAD views (the front view looks along +Y), not the OCCT
        // accessor names: OCCT calls the XMax face "Front" and the YMin face "Left".
        std::vector<ElementRole> roles{{mk.TopFace(), "Top"},    {mk.BottomFace(), "Bottom"},
                                       {mk.LeftFace(), "Front"}, {mk.RightFace(), "Back"},
                                       {mk.BackFace(), "Left"},  {mk.FrontFace(), "Right"}};
        return placePrimitive(ctx, mk.Shape(), std::move(roles));
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
            return fail(ErrorCode::InvalidInput, "cylinder radius and height must be positive: r=" + num(r) +
                                                     ", h=" + num(h));
        }
        gp_Ax2 axis(gp_Pnt(ctx.parameter("x"), ctx.parameter("y"), ctx.parameter("z")), gp::DZ());
        BRepPrimAPI_MakeCylinder mk(axis, r, h);
        mk.Build();
        if (!mk.IsDone()) return fail(ErrorCode::OperationFailed, "BRepPrimAPI_MakeCylinder failed");
        std::vector<ElementRole> roles;
        for (TopExp_Explorer ex(mk.Shape(), TopAbs_FACE); ex.More(); ex.Next()) {
            const TopoDS_Face& f = TopoDS::Face(ex.Current());
            BRepAdaptor_Surface s(f, false);
            if (s.GetType() == GeomAbs_Cylinder) {
                roles.push_back({f, "Side"});
            } else {
                gp_Dir n = s.Plane().Axis().Direction();
                if (!s.Plane().Position().Direct()) n.Reverse();
                if (f.Orientation() == TopAbs_REVERSED) n.Reverse();
                roles.push_back({f, n.Z() > 0 ? "Top" : "Bottom"});
            }
        }
        return placePrimitive(ctx, mk.Shape(), std::move(roles));
    }
};

class PrismCutFeature final : public Feature {
public:
    std::string type() const override { return "PrismCut"; }
    std::vector<ParameterSpec> parameterSpecs() const override {
        return {{"centerX", "0"}, {"centerY", "0"}, {"centerZ", "0"}, {"sizeU", "5"},
                {"sizeV", "5"},   {"radius", "2"},  {"depth", "5"}};
    }
    std::vector<ReferenceSlot> referenceSlots() const override { return {{"face", ElementKind::Face, false}}; }
    std::vector<OptionSpec> optionSpecs() const override {
        return {{"profile", {"rectangle", "circle"}}, {"extent", {"blind", "through"}}};
    }

    Result<FeatureOutput> execute(const FeatureContext& ctx) const override {
        if (ctx.input().isNull()) return fail(ErrorCode::InvalidInput, "cut needs an existing body");
        const auto& faces = ctx.references("face");
        if (faces.size() != 1) {
            return fail(ErrorCode::InvalidInput,
                        "sketch face reference resolved to " + std::to_string(faces.size()) + " faces, expected 1");
        }
        const TopoDS_Face& face = TopoDS::Face(faces.front());
        BRepAdaptor_Surface surface(face, false);
        if (surface.GetType() != GeomAbs_Plane) return fail(ErrorCode::InvalidInput, "sketch face is not planar");
        const gp_Pln plane = surface.Plane();
        gp_Dir n = plane.Axis().Direction();
        if (!plane.Position().Direct()) n.Reverse();  // left-handed plane: Du x Dv = -axis
        if (face.Orientation() == TopAbs_REVERSED) n.Reverse();

        // Profile frame: the given point projected onto the face plane; u = global X
        // projected onto the plane (global Y when X is normal to it), v = n x u.
        gp_XYZ p(ctx.parameter("centerX"), ctx.parameter("centerY"), ctx.parameter("centerZ"));
        p -= n.XYZ() * (p - plane.Location().XYZ()).Dot(n.XYZ());
        gp_XYZ ux = gp::DX().XYZ() - n.XYZ() * gp::DX().XYZ().Dot(n.XYZ());
        if (ux.Modulus() < 1e-6) ux = gp::DY().XYZ() - n.XYZ() * gp::DY().XYZ().Dot(n.XYZ());
        const gp_Dir u(ux);
        const gp_Dir v = n.Crossed(u);

        double depth = ctx.parameter("depth");
        if (ctx.option("extent") == "through") {
            Bnd_Box box;
            BRepBndLib::Add(ctx.input().shape(), box);
            depth = 2.0 * std::sqrt(box.SquareExtent()) + 1.0;
        } else if (!(depth > 0)) {
            return fail(ErrorCode::InvalidInput, "cut depth must be positive: " + num(depth));
        }

        TopoDS_Face profile;
        std::vector<std::pair<TopoDS_Shape, std::string>> profileEdges;
        if (ctx.option("profile") == "rectangle") {
            const double a = ctx.parameter("sizeU"), b = ctx.parameter("sizeV");
            if (!(a > 0) || !(b > 0)) {
                return fail(ErrorCode::InvalidInput, "rectangle size must be positive: " + num(a) + " x " + num(b));
            }
            auto corner = [&](double su, double sv) {
                return gp_Pnt(p + u.XYZ() * (su * a / 2) + v.XYZ() * (sv * b / 2));
            };
            // Sides in order -V, +U, +V, -U.
            const gp_Pnt c[4] = {corner(-1, -1), corner(1, -1), corner(1, 1), corner(-1, 1)};
            const char* roles[4] = {"Side-V", "Side+U", "Side+V", "Side-U"};
            BRepBuilderAPI_MakeWire wire;
            for (int i = 0; i < 4; ++i) {
                BRepBuilderAPI_MakeEdge e(c[i], c[(i + 1) % 4]);
                wire.Add(e.Edge());
                profileEdges.emplace_back(wire.Edge(), roles[i]);
            }
            BRepBuilderAPI_MakeFace mf(gp_Pln(gp_Ax3(gp_Pnt(p), n, u)), wire.Wire(), true);
            if (!mf.IsDone()) return fail(ErrorCode::OperationFailed, "rectangle profile face failed");
            profile = mf.Face();
        } else {
            const double r = ctx.parameter("radius");
            if (!(r > 0)) return fail(ErrorCode::InvalidInput, "hole radius must be positive: " + num(r));
            BRepBuilderAPI_MakeEdge e(gp_Circ(gp_Ax2(gp_Pnt(p), n, u), r));
            BRepBuilderAPI_MakeWire wire(e.Edge());
            profileEdges.emplace_back(wire.Edge(), "Side");
            BRepBuilderAPI_MakeFace mf(gp_Pln(gp_Ax3(gp_Pnt(p), n, u)), wire.Wire(), true);
            if (!mf.IsDone()) return fail(ErrorCode::OperationFailed, "circle profile face failed");
            profile = mf.Face();
        }

        // Profile edges inside the face may be copies of the wire edges; match by geometry.
        BRepPrimAPI_MakePrism prism(profile, gp_Vec(n) * -depth);
        prism.Build();
        if (!prism.IsDone()) return fail(ErrorCode::OperationFailed, "prism tool failed");
        FeatureOutput out;
        for (TopExp_Explorer ex(profile, TopAbs_EDGE); ex.More(); ex.Next()) {
            const TopoDS_Shape& edge = ex.Current();
            std::string role;
            for (const auto& [pe, name] : profileEdges) {
                if (pe.IsSame(edge)) role = name;
            }
            if (role.empty()) return fail(ErrorCode::OperationFailed, "profile edge lost while building the face");
            for (NCollection_List<TopoDS_Shape>::Iterator it(prism.Generated(edge)); it.More(); it.Next()) {
                if (it.Value().ShapeType() == TopAbs_FACE) out.roles.push_back({it.Value(), role});
            }
        }
        out.roles.push_back({prism.FirstShape(), "Start"});
        out.roles.push_back({prism.LastShape(), "Floor"});

        BRepAlgoAPI_Cut cut;
        NCollection_List<TopoDS_Shape> args, tools;
        args.Append(ctx.input().shape());
        tools.Append(prism.Shape());
        cut.SetArguments(args);
        cut.SetTools(tools);
        cut.SetToFillHistory(true);
        cut.Build();
        if (!cut.IsDone() || cut.HasErrors()) {
            return fail(ErrorCode::OperationFailed, "boolean cut failed: " + algoErrors(cut));
        }
        out.shape = cut.Shape();
        out.history = cut.History();
        const double before = volumeOf(ctx.input().shape()), after = volumeOf(out.shape);
        if (std::abs(before - after) <= 1e-9 * std::abs(before)) {
            out.warnings.push_back("the cut removes no material (tool does not intersect the body)");
        }
        return out;
    }
};

template <class Maker>
Result<FeatureOutput> edgeTreatment(const FeatureContext& ctx, const char* what, double size, Maker& maker) {
    if (ctx.input().isNull()) return fail(ErrorCode::InvalidInput, std::string(what) + " needs an existing body");
    const auto& edges = ctx.references("edges");
    if (edges.empty()) return fail(ErrorCode::InvalidInput, std::string(what) + " has no edges");
    for (const auto& e : edges) maker.Add(size, TopoDS::Edge(e));
    maker.Build();
    if (!maker.IsDone()) {
        return fail(ErrorCode::OperationFailed, std::string(what) + " of size " + num(size) + " on " +
                                                    std::to_string(edges.size()) +
                                                    " edge(s) failed (size too large for the adjacent faces?)");
    }
    FeatureOutput out;
    out.shape = maker.Shape();
    NCollection_List<TopoDS_Shape> args;
    args.Append(ctx.input().shape());
    out.history = new BRepTools_History(args, maker);
    return out;
}

class FilletFeature final : public Feature {
public:
    std::string type() const override { return "Fillet"; }
    std::vector<ParameterSpec> parameterSpecs() const override { return {{"radius", "1"}}; }
    std::vector<ReferenceSlot> referenceSlots() const override { return {{"edges", ElementKind::Edge, true}}; }
    std::vector<OptionSpec> optionSpecs() const override { return {}; }
    Result<FeatureOutput> execute(const FeatureContext& ctx) const override {
        const double r = ctx.parameter("radius");
        if (!(r > 0)) return fail(ErrorCode::InvalidInput, "fillet radius must be positive: " + num(r));
        if (ctx.input().isNull()) return fail(ErrorCode::InvalidInput, "fillet needs an existing body");
        BRepFilletAPI_MakeFillet maker(ctx.input().shape());
        return edgeTreatment(ctx, "fillet", r, maker);
    }
};

class ChamferFeature final : public Feature {
public:
    std::string type() const override { return "Chamfer"; }
    std::vector<ParameterSpec> parameterSpecs() const override { return {{"distance", "1"}}; }
    std::vector<ReferenceSlot> referenceSlots() const override { return {{"edges", ElementKind::Edge, true}}; }
    std::vector<OptionSpec> optionSpecs() const override { return {}; }
    Result<FeatureOutput> execute(const FeatureContext& ctx) const override {
        const double d = ctx.parameter("distance");
        if (!(d > 0)) return fail(ErrorCode::InvalidInput, "chamfer distance must be positive: " + num(d));
        if (ctx.input().isNull()) return fail(ErrorCode::InvalidInput, "chamfer needs an existing body");
        BRepFilletAPI_MakeChamfer maker(ctx.input().shape());
        return edgeTreatment(ctx, "chamfer", d, maker);
    }
};

}  // namespace

Status registerBasicFeatures(FeatureRegistry& registry) {
    const std::vector<std::pair<std::string, FeatureRegistry::Factory>> all{
        {"Box", [] { return std::make_unique<BoxFeature>(); }},
        {"Cylinder", [] { return std::make_unique<CylinderFeature>(); }},
        {"PrismCut", [] { return std::make_unique<PrismCutFeature>(); }},
        {"Fillet", [] { return std::make_unique<FilletFeature>(); }},
        {"Chamfer", [] { return std::make_unique<ChamferFeature>(); }},
    };
    for (const auto& [type, factory] : all) {
        auto st = registry.add(type, factory);
        if (!st) return st;
    }
    return std::monostate{};
}

std::shared_ptr<const FeatureRegistry> basicRegistry() {
    auto registry = std::make_shared<FeatureRegistry>();
    auto st = registerBasicFeatures(*registry);
    if (!st) throw std::logic_error("basic feature registration failed: " + st.error().message);
    return registry;
}

}  // namespace bendcore::modeling::basic
