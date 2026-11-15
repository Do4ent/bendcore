#include "gearbox_internal.hpp"

#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepClass_FaceClassifier.hxx>
#include <BRepGProp.hxx>
#include <BRep_Tool.hxx>
#include <GProp_GProps.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <gp_Circ.hxx>
#include <gp_Cylinder.hxx>
#include <gp_Lin.hxx>
#include <gp_Pln.hxx>

#include <cmath>
#include <cstdio>
#include <sstream>

namespace bendcore::examples::gearbox::detail {

using namespace bendcore::modeling;

void fail(const std::string& message) { throw BuildError(message); }

std::string num(double v) {
    char buf[40];
    std::snprintf(buf, sizeof(buf), "%.17g", v);
    return buf;
}

namespace {

std::string pnt(const gp_Pnt& p) {
    std::ostringstream s;
    s << "(" << p.X() << ", " << p.Y() << ", " << p.Z() << ")";
    return s.str();
}

gp_Dir outwardNormal(const TopoDS_Face& f) {
    BRepAdaptor_Surface s(f, false);
    gp_Dir n = s.Plane().Axis().Direction();
    if (!s.Plane().Position().Direct()) n.Reverse();
    if (f.Orientation() == TopAbs_REVERSED) n.Reverse();
    return n;
}

bool containsPoint(const TopoDS_Face& f, const gp_Pnt& p, double tol = 1e-6) {
    BRepClass_FaceClassifier c(f, p, tol);
    return c.State() == TopAbs_IN || c.State() == TopAbs_ON;
}

template <class Pred>
std::vector<TopoDS_Shape> collect(const TopoDS_Shape& shape, TopAbs_ShapeEnum kind, Pred pred) {
    std::vector<TopoDS_Shape> out;
    for (TopExp_Explorer ex(shape, kind); ex.More(); ex.Next()) {
        bool dup = false;
        for (const auto& s : out) dup = dup || s.IsSame(ex.Current());
        if (!dup && pred(ex.Current())) out.push_back(ex.Current());
    }
    return out;
}

TopoDS_Shape single(std::vector<TopoDS_Shape> v, const std::string& what) {
    if (v.size() != 1) fail(what + ": found " + std::to_string(v.size()) + " elements, expected 1");
    return v.front();
}

}  // namespace

// ---------------------------------------------------------------- PartBuilder

PartBuilder::PartBuilder(std::string part) : part_(std::move(part)), doc_(std::make_unique<PartDocument>(partRegistry())) {}

void PartBuilder::var(const std::string& name, double value) { var(name, num(value)); }

void PartBuilder::var(const std::string& name, const std::string& expression) {
    auto st = doc_->setVariable(name, expression);
    if (!st.ok()) fail(part_ + ": variable " + name + ": " + st.error().message);
}

double PartBuilder::value(const std::string& name) const {
    auto v = doc_->variableValue(name);
    if (!v.ok()) fail(part_ + ": variable " + name + ": " + v.error().message);
    return v.value();
}

const NamedShape& PartBuilder::add(FeatureSpec spec) {
    const std::string id = spec.id;
    auto r = doc_->addFeature(std::move(spec));
    if (!r.ok()) fail(part_ + ": add " + id + ": " + r.error().message);
    RebuildReport rep = doc_->rebuild();
    auto info = doc_->feature(r.value());
    if (!info.ok()) fail(part_ + ": " + id + ": " + info.error().message);
    if (info.value().state != FeatureState::Ok && info.value().state != FeatureState::Warning) {
        std::string m;
        for (const auto& s : info.value().messages) m += " " + s;
        fail(part_ + ": feature " + id + " is " + toString(info.value().state) + ":" + m);
    }
    if (!rep.failed.empty()) fail(part_ + ": rebuild failed at " + rep.failed.front());
    shape_ = doc_->finalShape();
    return shape_;
}

void PartBuilder::sketch(const std::string& id, const std::string& plane, const std::string& offset, SketchData data,
                         bool open) {
    FeatureSpec s;
    s.type = "Sketch";
    s.id = id;
    s.options = {{"plane", plane}};
    if (open) s.options["contour"] = "open";
    s.parameters = {{"offset", offset}};
    s.sketch = std::move(data);
    add(std::move(s));
}

void PartBuilder::faceSketch(const std::string& id, const TopoDS_Shape& face, const std::string& ox,
                             const std::string& oy, const std::string& oz, SketchData data) {
    FeatureSpec s;
    s.type = "Sketch";
    s.id = id;
    s.options = {{"plane", "face"}};
    s.references["face"] = {ref(face)};
    s.parameters = {{"originX", ox}, {"originY", oy}, {"originZ", oz}};
    s.sketch = std::move(data);
    add(std::move(s));
}

void PartBuilder::extrude(const std::string& id, const std::string& sketch, std::map<std::string, std::string> options,
                          std::map<std::string, std::string> params) {
    FeatureSpec s;
    s.type = "Extrude";
    s.id = id;
    s.options = std::move(options);
    s.parameters = std::move(params);
    s.links = {{"sketch", sketch}};
    add(std::move(s));
}

void PartBuilder::revolve(const std::string& id, const std::string& sketch, const std::string& axis,
                          const std::string& mode) {
    FeatureSpec s;
    s.type = "Revolve";
    s.id = id;
    s.options = {{"axis", axis}, {"mode", mode}};
    s.links = {{"sketch", sketch}};
    add(std::move(s));
}

void PartBuilder::chamfer(const std::string& id, const std::vector<TopoDS_Shape>& edges, const std::string& distance) {
    FeatureSpec s;
    s.type = "Chamfer";
    s.id = id;
    s.parameters = {{"distance", distance}};
    for (const auto& e : edges) s.references["edges"].push_back(ref(e));
    add(std::move(s));
}

void PartBuilder::fillet(const std::string& id, const std::vector<TopoDS_Shape>& edges, const std::string& radius) {
    FeatureSpec s;
    s.type = "Fillet";
    s.id = id;
    s.parameters = {{"radius", radius}};
    for (const auto& e : edges) s.references["edges"].push_back(ref(e));
    add(std::move(s));
}

TopoReference PartBuilder::ref(const TopoDS_Shape& element) const {
    auto r = makeReference(shape_, element);
    if (!r.ok()) fail(part_ + ": reference: " + r.error().message);
    return r.value();
}

// ---------------------------------------------------------------- зонды

TopoDS_Shape planarFace(const TopoDS_Shape& shape, const gp_Dir& n, double d) {
    return single(collect(shape, TopAbs_FACE,
                          [&](const TopoDS_Shape& s) {
                              const TopoDS_Face& f = TopoDS::Face(s);
                              BRepAdaptor_Surface a(f, false);
                              if (a.GetType() != GeomAbs_Plane) return false;
                              if (outwardNormal(f).Dot(n) < 1 - 1e-9) return false;
                              return std::abs(a.Plane().Location().XYZ().Dot(n.XYZ()) - d) < 1e-6;
                          }),
                  "planar face n = (" + num(n.X()) + ", " + num(n.Y()) + ", " + num(n.Z()) + "), d = " + num(d));
}

TopoDS_Shape planarFaceAt(const TopoDS_Shape& shape, const gp_Dir& n, const gp_Pnt& inside) {
    return single(collect(shape, TopAbs_FACE,
                          [&](const TopoDS_Shape& s) {
                              const TopoDS_Face& f = TopoDS::Face(s);
                              BRepAdaptor_Surface a(f, false);
                              if (a.GetType() != GeomAbs_Plane) return false;
                              if (outwardNormal(f).Dot(n) < 1 - 1e-9) return false;
                              if (a.Plane().Distance(inside) > 1e-6) return false;
                              return containsPoint(f, inside);
                          }),
                  "planar face with normal (" + num(n.X()) + ", " + num(n.Y()) + ", " + num(n.Z()) + ") at " + pnt(inside));
}

std::vector<TopoDS_Shape> circleEdges(const TopoDS_Shape& shape, const gp_Pnt& c, double r) {
    return collect(shape, TopAbs_EDGE, [&](const TopoDS_Shape& s) {
        BRepAdaptor_Curve a(TopoDS::Edge(s));
        if (a.GetType() != GeomAbs_Circle) return false;
        return std::abs(a.Circle().Radius() - r) < 1e-6 && a.Circle().Location().Distance(c) < 1e-6;
    });
}

TopoDS_Shape circleEdge(const TopoDS_Shape& shape, const gp_Pnt& c, double r) {
    return single(circleEdges(shape, c, r), "circle edge r = " + num(r) + " at " + pnt(c));
}

TopoDS_Shape lineEdge(const TopoDS_Shape& shape, const gp_Pnt& a, const gp_Pnt& b) {
    return single(collect(shape, TopAbs_EDGE,
                          [&](const TopoDS_Shape& s) {
                              BRepAdaptor_Curve c(TopoDS::Edge(s));
                              if (c.GetType() != GeomAbs_Line) return false;
                              const gp_Pnt p0 = c.Value(c.FirstParameter()), p1 = c.Value(c.LastParameter());
                              return (p0.Distance(a) < 1e-6 && p1.Distance(b) < 1e-6) ||
                                     (p0.Distance(b) < 1e-6 && p1.Distance(a) < 1e-6);
                          }),
                  "line edge " + pnt(a) + " – " + pnt(b));
}

std::vector<TopoDS_Shape> linesThrough(const TopoDS_Shape& shape, const gp_Pnt& p, const gp_Dir& dir) {
    return collect(shape, TopAbs_EDGE, [&](const TopoDS_Shape& s) {
        BRepAdaptor_Curve c(TopoDS::Edge(s));
        if (c.GetType() != GeomAbs_Line) return false;
        if (!c.Line().Direction().IsParallel(dir, 1e-9)) return false;
        return c.Line().Distance(p) < 1e-6;
    });
}

namespace {
std::vector<TopoDS_Shape> cylinders(const TopoDS_Shape& shape, double r, const gp_Pnt& p, const gp_Dir& dir) {
    const gp_Lin axis(p, dir);
    return collect(shape, TopAbs_FACE, [&](const TopoDS_Shape& s) {
        BRepAdaptor_Surface a(TopoDS::Face(s), false);
        if (a.GetType() != GeomAbs_Cylinder) return false;
        const gp_Cylinder cy = a.Cylinder();
        return std::abs(cy.Radius() - r) < 1e-6 && cy.Axis().Direction().IsParallel(dir, 1e-9) &&
               axis.Distance(cy.Location()) < 1e-6;
    });
}
}  // namespace

TopoDS_Shape cylinderFace(const TopoDS_Shape& shape, double r, const gp_Pnt& p, const gp_Dir& dir) {
    return single(cylinders(shape, r, p, dir), "cylinder r = " + num(r) + " on axis through " + pnt(p));
}

TopoDS_Shape cylinderFaceAt(const TopoDS_Shape& shape, double r, const gp_Pnt& p, const gp_Dir& dir,
                            const gp_Pnt& inside) {
    std::vector<TopoDS_Shape> out;
    for (const auto& f : cylinders(shape, r, p, dir))
        if (containsPoint(TopoDS::Face(f), inside, 1e-5)) out.push_back(f);
    return single(out, "cylinder r = " + num(r) + " containing " + pnt(inside));
}

// ---------------------------------------------------------------- эскизы

SketchData polygon(const std::vector<std::pair<std::string, std::string>>& corners) {
    SketchBuilder b;
    b.fixedPolygon(corners);
    return b.build();
}

SketchData circles(const std::vector<std::tuple<std::string, std::string, std::string>>& c) {
    SketchBuilder b;
    for (const auto& [x, y, r] : c) b.fixedCircle(x, y, r);
    return b.build();
}

SketchData points(const std::vector<std::pair<std::string, std::string>>& p) {
    SketchBuilder b;
    for (const auto& [x, y] : p) b.point(x, y, true);
    return b.build();
}

SketchData slot(const std::string& xc, const std::string& yc, const std::string& l, const std::string& b) {
    SketchBuilder s;
    const std::string h = "((" + l + ") - (" + b + ")) / 2", r = "(" + b + ") / 2";
    const std::string x1 = "(" + xc + ") - " + h, x2 = "(" + xc + ") + " + h;
    const std::string ylo = "(" + yc + ") - " + r, yhi = "(" + yc + ") + " + r;
    const int c1 = s.point(x1, yc, true), c2 = s.point(x2, yc, true);
    const int p1 = s.point(x1, ylo, true), p2 = s.point(x2, ylo, true), p3 = s.point(x2, yhi, true),
              p4 = s.point(x1, yhi, true);
    s.line(p1, p2);
    s.arc(c2, p2, p3, true);
    s.line(p3, p4);
    s.arc(c1, p4, p1, true);
    return s.build();
}

SketchData rect(const std::string& cx, const std::string& cy, const std::string& a, const std::string& b) {
    const std::string u = "(" + a + ") / 2", v = "(" + b + ") / 2";
    const std::string X = "(" + cx + ")", Y = "(" + cy + ")";
    return polygon({{X + " - " + u, Y + " - " + v}, {X + " + " + u, Y + " - " + v}, {X + " + " + u, Y + " + " + v},
                    {X + " - " + u, Y + " + " + v}});
}

SketchData frame(const std::string& x0, const std::string& y0, const std::string& x1, const std::string& y1,
                 const std::string& u0, const std::string& v0, const std::string& u1, const std::string& v1) {
    SketchBuilder b;
    b.fixedPolygon({{x0, y0}, {x1, y0}, {x1, y1}, {x0, y1}});
    b.fixedPolygon({{u0, v0}, {u1, v0}, {u1, v1}, {u0, v1}});
    return b.build();
}

double volumeOf(const TopoDS_Shape& shape) {
    GProp_GProps p;
    BRepGProp::VolumeProperties(shape, p);
    return p.Mass();
}

}  // namespace bendcore::examples::gearbox::detail
