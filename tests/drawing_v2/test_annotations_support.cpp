#include "test_annotations_support.hpp"

#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRep_Tool.hxx>
#include <TopExp.hxx>
#include <gp_Circ.hxx>

#include <cmath>
#include <stdexcept>

namespace annot_test {

namespace {

bool near(const gp_Pnt& p, Vec3 q, double tol = 1e-6) {
    return std::abs(p.X() - q.x) < tol && std::abs(p.Y() - q.y) < tol && std::abs(p.Z() - q.z) < tol;
}

void check(const bendcore::modeling::Status& s, const char* what) {
    if (!s) throw std::runtime_error(std::string(what) + ": " + s.error().message);
}

}  // namespace

BlockPart::BlockPart(double L, double W, double H, double hx, double hy, double r) {
    using namespace bendcore::modeling;
    check(doc.setVariable("L", std::to_string(L)), "L");
    check(doc.setVariable("W", std::to_string(W)), "W");
    check(doc.setVariable("H", std::to_string(H)), "H");
    check(doc.setVariable("hx", std::to_string(hx)), "hx");
    check(doc.setVariable("hy", std::to_string(hy)), "hy");
    check(doc.setVariable("r", std::to_string(r)), "r");
    FeatureSpec box{"Box", "Body", "", {{"length", "L"}, {"width", "W"}, {"height", "H"}}, {}, {}, false};
    if (!doc.addFeature(box)) throw std::runtime_error("add Box");
    doc.rebuild();
    const NamedShape s = doc.finalShape();
    // Верхняя грань: роль Body:Top.
    const auto top = s.find(ElementKind::Face, "Body:Top");
    if (!top) throw std::runtime_error("no Body:Top");
    auto ref = makeReference(s, s.element(ElementKind::Face, *top));
    if (!ref) throw std::runtime_error("reference to top face");
    FeatureSpec hole{"PrismCut", "Hole", "", {{"centerX", "hx"}, {"centerY", "hy"}, {"centerZ", "H"}, {"radius", "r"}},
                     {{"face", {ref.value()}}}, {{"profile", "circle"}, {"extent", "through"}}, false};
    if (!doc.addFeature(hole)) throw std::runtime_error("add Hole");
    doc.rebuild();
}

void BlockPart::set(const std::string& var, double value) {
    check(doc.setVariable(var, std::to_string(value)), var.c_str());
    doc.rebuild();
}

ModelBinding BlockPart::binding() const {
    auto b = ModelBinding::fromNamedShape(doc.finalShape());
    if (!b) throw std::runtime_error(b.error().message);
    return b.value();
}

std::optional<std::uint32_t> lineEdge(const ModelBinding& m, Vec3 a, Vec3 b) {
    const auto& edges = m.topology().edges();
    for (std::uint32_t i = 0; i < edges.size(); ++i) {
        if (edges[i].curve != bendcore::kernel::CurveType::Line) continue;
        const gp_Pnt p = BRep_Tool::Pnt(TopExp::FirstVertex(edges[i].edge)),
                     q = BRep_Tool::Pnt(TopExp::LastVertex(edges[i].edge));
        if ((near(p, a) && near(q, b)) || (near(p, b) && near(q, a))) return i;
    }
    return std::nullopt;
}

std::optional<std::uint32_t> circleEdge(const ModelBinding& m, Vec3 c, double r) {
    const auto& edges = m.topology().edges();
    for (std::uint32_t i = 0; i < edges.size(); ++i) {
        BRepAdaptor_Curve cu(edges[i].edge);
        if (cu.GetType() != GeomAbs_Circle) continue;
        if (near(cu.Circle().Location(), c) && std::abs(cu.Circle().Radius() - r) < 1e-6) return i;
    }
    return std::nullopt;
}

std::optional<std::uint32_t> cylinderFace(const ModelBinding& m, double r) {
    const auto& faces = m.topology().faces();
    for (std::uint32_t i = 0; i < faces.size(); ++i) {
        BRepAdaptor_Surface s(faces[i].face, false);
        if (s.GetType() == GeomAbs_Cylinder && std::abs(s.Cylinder().Radius() - r) < 1e-6) return i;
    }
    return std::nullopt;
}

std::optional<std::uint32_t> vertexAt(const ModelBinding& m, Vec3 p) {
    const auto& vs = m.topology().vertices();
    for (std::uint32_t i = 0; i < vs.size(); ++i) {
        if (near(BRep_Tool::Pnt(vs[i].vertex), p)) return i;
    }
    return std::nullopt;
}

Anchor anchor(const ModelBinding& m, RefKind kind, std::uint32_t index, AnchorKind ak) {
    auto r = m.ref(kind, index);
    if (!r) throw std::runtime_error(r.error().message);
    return Anchor{r.value(), ak};
}

}  // namespace annot_test
