#include "test_drawing_document_support.hpp"

#include "../modeling/modeling_document_test_util.hpp"

#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRep_Tool.hxx>
#include <TopoDS.hxx>
#include <gp_Circ.hxx>
#include <gp_Cylinder.hxx>
#include <gp_Lin.hxx>

#include <gtest/gtest.h>

#include <cmath>
#include <cstdlib>

namespace ddtest {

using namespace bendcore::modeling;
using namespace bendcore::modeling::testutil;

namespace {

FeatureSpec sketchSpec(const std::string& id, const std::string& plane, SketchData data) {
    FeatureSpec s;
    s.type = "Sketch";
    s.id = id;
    s.options = {{"plane", plane}};
    s.sketch = std::move(data);
    return s;
}

FeatureSpec faceSketch(const std::string& id, const TopoReference& face, std::string ox, std::string oy, std::string oz,
                       SketchData data) {
    FeatureSpec s = sketchSpec(id, "face", std::move(data));
    s.references["face"] = {face};
    s.parameters = {{"originX", std::move(ox)}, {"originY", std::move(oy)}, {"originZ", std::move(oz)}};
    return s;
}

FeatureSpec extrudeSpec(const std::string& id, const std::string& sketch, std::map<std::string, std::string> options,
                        std::map<std::string, std::string> params) {
    FeatureSpec s;
    s.type = "Extrude";
    s.id = id;
    s.options = std::move(options);
    s.parameters = std::move(params);
    s.links = {{"sketch", sketch}};
    return s;
}

NamedShape built(PartDocument& doc) {
    RebuildReport rep = doc.rebuild();
    EXPECT_TRUE(rep.failed.empty()) << (rep.failed.empty() ? "" : rep.failed.front());
    return doc.finalShape();
}

TopoDS_Shape circleEdgeOf(const TopoDS_Shape& shape, gp_Pnt center, double radius, int axis) {
    TopoDS_Shape found;
    for (TopExp_Explorer ex(shape, TopAbs_EDGE); ex.More(); ex.Next()) {
        BRepAdaptor_Curve c(TopoDS::Edge(ex.Current()));
        if (c.GetType() != GeomAbs_Circle) continue;
        const gp_Circ ci = c.Circle();
        const gp_Dir d = ci.Axis().Direction();
        const double comp[3] = {d.X(), d.Y(), d.Z()};
        if (std::abs(std::abs(comp[axis]) - 1) > 1e-9 || std::abs(ci.Radius() - radius) > 1e-6) continue;
        if (ci.Location().Distance(center) > 1e-6) continue;
        found = ex.Current();
    }
    return found;
}

void setVars(PartDocument& doc, const std::map<std::string, double>& v) {
    for (const auto& [k, x] : v) setVar(doc, k, x);
}

bool near(const gp_Pnt& p, const Vec3& q, double tol) { return p.Distance(gp_Pnt(q.x, q.y, q.z)) <= tol; }

}  // namespace

void setVariable(PartDocument& doc, const std::string& name, double value) { setVar(doc, name, value); }

std::unique_ptr<PartDocument> makeBracket() {
    auto doc = std::make_unique<PartDocument>(partRegistry());
    setVars(*doc, {{"A", 60}, {"B", 50}, {"T", 8}, {"Wd", 30}, {"r", 4}, {"hx", 40}, {"d", 8}});
    SketchBuilder b;
    b.fixedPolygon({{"0", "0"}, {"A", "0"}, {"A", "T"}, {"T", "T"}, {"T", "B"}, {"0", "B"}});
    add(*doc, sketchSpec("Profile", "XZ", b.build()));
    add(*doc, extrudeSpec("Body", "Profile", {{"extent", "symmetric"}}, {{"distance", "Wd"}}));
    NamedShape s = built(*doc);
    add(*doc, {"Fillet", "InnerRound", "", {{"radius", "r"}}, {{"edges", {refTo(s, singleLine(s.shape(), 1, 8, 8))}}}, {}, false});
    s = built(*doc);
    const TopoReference top = refTo(s, planarFace(s.shape(), gp::DZ(), 8));
    SketchBuilder hb;
    hb.point("hx", "0", true);
    add(*doc, faceSketch("HoleSketch", top, "0", "0", "T", hb.build()));
    FeatureSpec hole{"Hole", "MountHole", "", {{"diameter", "d"}}, {{"face", {top}}}, {{"end", "through"}}, false};
    hole.links = {{"sketch", "HoleSketch"}};
    add(*doc, hole);
    built(*doc);
    return doc;
}

std::unique_ptr<PartDocument> makeShaft() {
    auto doc = std::make_unique<PartDocument>(partRegistry());
    setVars(*doc, {{"L", 100}, {"L1", 60}, {"R1", 15}, {"R2", 10}, {"lk", 30}, {"b", 6}, {"t", 4}, {"kx", 25}, {"c", 1.5}});
    SketchBuilder b;
    b.fixedPolygon({{"0", "0"}, {"L", "0"}, {"L", "R2"}, {"L1", "R2"}, {"L1", "R1"}, {"0", "R1"}});
    add(*doc, sketchSpec("Profile", "XZ", b.build()));
    add(*doc, {"Revolve", "Body", "", {}, {}, {{"axis", "X"}}, false, {{"sketch", "Profile"}}});
    add(*doc, {"DatumPlane", "KeyPlane", "", {{"offset", "R1"}}, {}, {{"base", "XY"}}, false});
    SketchBuilder k;
    k.fixedPolygon({{"kx - lk/2", "-b/2"}, {"kx + lk/2", "-b/2"}, {"kx + lk/2", "b/2"}, {"kx - lk/2", "b/2"}});
    FeatureSpec ks = sketchSpec("KeySketch", "datum", k.build());
    ks.links = {{"plane", "KeyPlane"}};
    add(*doc, ks);
    add(*doc, extrudeSpec("Keyway", "KeySketch", {{"mode", "cut"}, {"reversed", "true"}}, {{"distance", "t"}}));
    NamedShape s = built(*doc);
    add(*doc, {"Chamfer", "EndChamfers", "", {{"distance", "c"}},
               {{"edges", {refTo(s, circleEdgeOf(s.shape(), gp_Pnt(0, 0, 0), 15, 0)),
                           refTo(s, circleEdgeOf(s.shape(), gp_Pnt(100, 0, 0), 10, 0))}}},
               {}, false});
    built(*doc);
    return doc;
}

std::unique_ptr<PartDocument> makeFlange() {
    auto doc = std::make_unique<PartDocument>(partRegistry());
    setVars(*doc, {{"D", 120}, {"d0", 40}, {"T", 12}, {"Dh", 70}, {"h2", 15}, {"BC", 95}, {"dh", 10}, {"N", 6}, {"c", 1.5}});
    SketchBuilder b;
    b.fixedCircle("0", "0", "D/2");
    b.fixedCircle("0", "0", "d0/2");
    add(*doc, sketchSpec("Disc", "XY", b.build()));
    add(*doc, extrudeSpec("Plate", "Disc", {}, {{"distance", "T"}}));
    NamedShape s = built(*doc);
    SketchBuilder h;
    h.fixedCircle("0", "0", "Dh/2");
    h.fixedCircle("0", "0", "d0/2");
    add(*doc, faceSketch("HubSketch", refTo(s, planarFace(s.shape(), gp::DZ(), 12)), "0", "0", "T", h.build()));
    add(*doc, extrudeSpec("Hub", "HubSketch", {}, {{"distance", "h2"}}));
    s = built(*doc);
    const TopoReference top = refTo(s, planarFace(s.shape(), gp::DZ(), 12));
    SketchBuilder hb;
    hb.point("BC/2", "0", true);
    add(*doc, faceSketch("BoltSketch", top, "0", "0", "T", hb.build()));
    FeatureSpec hole{"Hole", "Bolt", "", {{"diameter", "dh"}}, {{"face", {top}}}, {{"end", "through"}}, false};
    hole.links = {{"sketch", "BoltSketch"}};
    add(*doc, hole);
    add(*doc, {"CircularPattern", "BoltCircle", "", {{"count", "N"}}, {}, {{"axis", "Z"}}, false, {{"feature", "Bolt"}}});
    s = built(*doc);
    add(*doc, {"Chamfer", "RimChamfer", "", {{"distance", "c"}},
               {{"edges", {refTo(s, circleEdgeOf(s.shape(), gp_Pnt(0, 0, 0), 60, 2))}}}, {}, false});
    built(*doc);
    return doc;
}

std::unique_ptr<PartDocument> makeRod(double R, double L) {
    auto doc = std::make_unique<PartDocument>(partRegistry());
    setVars(*doc, {{"R", R}, {"L", L}});
    SketchBuilder b;
    b.fixedCircle("0", "0", "R");
    add(*doc, sketchSpec("Section", "XY", b.build()));
    add(*doc, extrudeSpec("Rod", "Section", {}, {{"distance", "L"}}));
    built(*doc);
    return doc;
}

std::unique_ptr<PartDocument> makeBushing() {
    auto doc = std::make_unique<PartDocument>(partRegistry());
    setVars(*doc, {{"D", 90}, {"d0", 30}, {"T", 12}, {"Dh", 50}, {"h2", 20}, {"BC", 70}, {"dh", 11}, {"N", 4}});
    SketchBuilder b;
    b.fixedCircle("0", "0", "D/2");
    b.fixedCircle("0", "0", "d0/2");
    add(*doc, sketchSpec("Disc", "XY", b.build()));
    add(*doc, extrudeSpec("Flange", "Disc", {}, {{"distance", "T"}}));
    NamedShape s = built(*doc);
    SketchBuilder h;
    h.fixedCircle("0", "0", "Dh/2");
    h.fixedCircle("0", "0", "d0/2");
    add(*doc, faceSketch("HubSketch", refTo(s, planarFace(s.shape(), gp::DZ(), 12)), "0", "0", "T", h.build()));
    add(*doc, extrudeSpec("Hub", "HubSketch", {}, {{"distance", "h2"}}));
    s = built(*doc);
    const TopoReference top = refTo(s, planarFace(s.shape(), gp::DZ(), 12));
    SketchBuilder hb;
    hb.point("BC/2", "0", true);
    add(*doc, faceSketch("BoltSketch", top, "0", "0", "T", hb.build()));
    FeatureSpec hole{"Hole", "Bolt", "", {{"diameter", "dh"}}, {{"face", {top}}}, {{"end", "through"}}, false};
    hole.links = {{"sketch", "BoltSketch"}};
    add(*doc, hole);
    add(*doc, {"CircularPattern", "BoltCircle", "", {{"count", "N"}}, {}, {{"axis", "Z"}}, false, {{"feature", "Bolt"}}});
    built(*doc);
    return doc;
}

std::optional<std::uint32_t> vertexAt(const ModelSnapshot& m, Vec3 p, double tol) {
    std::optional<std::uint32_t> out;
    const std::size_t n = m.named->count(ElementKind::Vertex);
    for (std::size_t i = 0; i < n; ++i) {
        if (!near(BRep_Tool::Pnt(TopoDS::Vertex(m.named->element(ElementKind::Vertex, i))), p, tol)) continue;
        if (out) return std::nullopt;
        out = std::uint32_t(i);
    }
    return out;
}

std::optional<std::uint32_t> lineEdge(const ModelSnapshot& m, Vec3 a, Vec3 b, double tol) {
    std::optional<std::uint32_t> out;
    const std::size_t n = m.named->count(ElementKind::Edge);
    for (std::size_t i = 0; i < n; ++i) {
        const TopoDS_Edge e = TopoDS::Edge(m.named->element(ElementKind::Edge, i));
        BRepAdaptor_Curve c(e);
        if (c.GetType() != GeomAbs_Line) continue;
        const gp_Pnt p0 = c.Value(c.FirstParameter()), p1 = c.Value(c.LastParameter());
        if (!((near(p0, a, tol) && near(p1, b, tol)) || (near(p0, b, tol) && near(p1, a, tol)))) continue;
        if (out) return std::nullopt;
        out = std::uint32_t(i);
    }
    return out;
}

std::optional<std::uint32_t> circleEdge(const ModelSnapshot& m, Vec3 c, double r, double tol) {
    std::optional<std::uint32_t> out;
    const std::size_t n = m.named->count(ElementKind::Edge);
    for (std::size_t i = 0; i < n; ++i) {
        BRepAdaptor_Curve cv(TopoDS::Edge(m.named->element(ElementKind::Edge, i)));
        if (cv.GetType() != GeomAbs_Circle) continue;
        const gp_Circ ci = cv.Circle();
        if (std::abs(ci.Radius() - r) > tol || !near(ci.Location(), c, tol)) continue;
        if (out) return std::nullopt;
        out = std::uint32_t(i);
    }
    return out;
}

std::optional<std::uint32_t> cylinderFace(const ModelSnapshot& m, double r, Vec3 axisPoint, Vec3 axisDir, double tol) {
    std::optional<std::uint32_t> out;
    const gp_Lin axis(gp_Pnt(axisPoint.x, axisPoint.y, axisPoint.z), gp_Dir(axisDir.x, axisDir.y, axisDir.z));
    const std::size_t n = m.named->count(ElementKind::Face);
    for (std::size_t i = 0; i < n; ++i) {
        BRepAdaptor_Surface s(TopoDS::Face(m.named->element(ElementKind::Face, i)));
        if (s.GetType() != GeomAbs_Cylinder) continue;
        const gp_Cylinder cy = s.Cylinder();
        if (std::abs(cy.Radius() - r) > tol) continue;
        if (!cy.Axis().Direction().IsParallel(axis.Direction(), 1e-9) || axis.Distance(cy.Location()) > tol) continue;
        if (out) return std::nullopt;
        out = std::uint32_t(i);
    }
    return out;
}

annotations::Anchor anchorOn(const ModelSnapshot& m, ElementKind kind, std::optional<std::uint32_t> index,
                             annotations::AnchorKind ak) {
    EXPECT_TRUE(index.has_value()) << "model element not found";
    annotations::Anchor a;
    a.kind = ak;
    a.ref.kind = kind == ElementKind::Face ? annotations::RefKind::Face
                                           : (kind == ElementKind::Edge ? annotations::RefKind::Edge : annotations::RefKind::Vertex);
    if (!index) return a;
    a.ref.index = *index;
    auto r = elementReference(m, kind, *index);
    EXPECT_TRUE(r.ok()) << (r.ok() ? "" : r.error().message);
    if (r.ok()) a.ref.persistent = r.value();
    return a;
}

DimensionItem dimension(const std::string& id, const std::string& view, annotations::DimensionType type,
                        std::vector<annotations::Anchor> anchors, double offset) {
    DimensionItem d;
    d.id = id;
    d.view = view;
    d.def.type = type;
    d.def.anchors = std::move(anchors);
    d.def.offset = offset;
    return d;
}

DxfContent readDxf(const std::string& path) {
    DxfContent out;
    const auto file = dv2test::parseDxf(dv2test::readFile(path));
    if (const auto* hdr = file.section("HEADER")) {
        // Разборщик тестов отдаёт переменные HEADER объектами с типом "$ИМЯ".
        for (const auto& o : *hdr) {
            if (o.type == "$LIMMAX") {
                out.limMaxX = o.real(10);
                out.limMaxY = o.real(20);
            }
        }
    }
    const auto* ents = file.section("ENTITIES");
    if (!ents) return out;
    out.entities = ents->size();
    for (const auto& e : *ents) {
        if (e.type == "TEXT") out.texts.push_back(dv2test::decodeDxfUnicode(e.first(1).value_or("")));
        if (e.type == "LINE")
            out.lines.push_back({e.real(10), e.real(20), e.real(11), e.real(21), e.first(8).value_or("")});
    }
    return out;
}

bool hasText(const DxfContent& d, const std::string& text) {
    for (const auto& t : d.texts)
        if (t == text) return true;
    return false;
}

bool hasDimLine(const DxfContent& d, double length, bool horizontal, double tol) {
    for (const auto& l : d.lines) {
        if (l.layer != "DIM") continue;
        const double dx = std::abs(l.x1 - l.x0), dy = std::abs(l.y1 - l.y0);
        if (horizontal ? (dy < 1e-9 && std::abs(dx - length) <= tol) : (dx < 1e-9 && std::abs(dy - length) <= tol)) return true;
    }
    return false;
}

const annotations::DimensionResult* dimensionResult(const DrawingDocument& doc, const std::string& sheet,
                                                    const std::string& id) {
    for (std::size_t si = 0; si < doc.sheets().size(); ++si) {
        if (doc.sheets()[si].id != sheet) continue;
        for (std::size_t i = 0; i < doc.sheets()[si].dimensions.size(); ++i)
            if (doc.sheets()[si].dimensions[i].id == id) return &doc.computedSheets()[si].dimensions[i];
    }
    return nullptr;
}

std::string ezdxfCheck(const std::string& path, const std::string& expectText) {
    const char* py = std::getenv("BENDCORE_PYTHON_EZDXF");
    if (!py || !*py) return {};
    const std::string script =
        "import sys, ezdxf\n"
        "doc = ezdxf.readfile(sys.argv[1])\n"
        "a = doc.audit()\n"
        "msp = doc.modelspace()\n"
        "t = [ezdxf.decode_dxf_unicode(e.dxf.text) for e in msp.query('TEXT')]\n"
        "print(doc.dxfversion, len(a.errors), doc.header.get('$INSUNITS'), int(sys.argv[2] in t))\n";
    const std::string sp = dv2test::outPath("drawing_document_ezdxf.py");
    if (!saveToFile(sp, script).ok()) return "cannot write the ezdxf script";
    int code = 0;
    const std::string out = dv2test::runCommand(std::string(py) + " " + sp + " '" + path + "' '" + expectText + "'", &code);
    if (code != 0) return "ezdxf failed: " + out;
    if (out != "AC1015 0 4 1\n") return "ezdxf: unexpected '" + out + "'";
    return {};
}

}  // namespace ddtest
