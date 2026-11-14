// Reference parts of phase 2 (docs/ROADMAP_GENERAL_CAD.md, phase 2: "the reference set grows to
// 25 parts, including a cast housing with drafts and a shell"; docs/modeling/REFERENCE_PARTS.md,
// parts 14–26). Each part is built through the part document with the operations of phase 2:
// draft and shell of a cast body, ribs, sweeps (boss and cut), lofts (boss and cut, three
// sections), threaded holes, thin-walled and drafted extrusions, a shelled revolved body, new
// bodies with booleans, mirror and circular pattern of bodies. The volume of every part is an
// analytic formula written out below from the dimensions (never measured by OCCT); the checks
// (scenarios, survival of references, save/load, undo/redo, mass, STEP) are in
// test_modeling_document_reference_parts.cpp.

#include "reference_parts.hpp"

#include <cmath>

namespace bendcore::modeling::refparts {

namespace {

double deg(double a) { return a * kPi / 180; }

TopoReference bodyRef(const NamedShape& s, const std::string& name) {
    auto r = makeBodyReference(s, name);
    EXPECT_TRUE(r.ok()) << name << ": " << (r.ok() ? "" : r.error().message);
    return r.ok() ? r.value() : TopoReference{};
}

// Names of the bodies of `s` other than `name`.
std::vector<std::string> otherBodies(const NamedShape& s, const std::string& name) {
    std::vector<std::string> out;
    for (std::size_t i = 0; i < s.bodyCount(); ++i)
        if (s.bodyName(i) != name) out.push_back(s.bodyName(i));
    return out;
}

FeatureSpec offsetSketch(const std::string& id, const std::string& plane, const std::string& offset, SketchData data,
                         bool open = false) {
    FeatureSpec s = sketchSpec(id, plane, std::move(data));
    s.parameters = {{"offset", offset}};
    if (open) s.options["contour"] = "open";
    return s;
}

SketchData rectangleAt(const std::string& cx, const std::string& cy, const std::string& a, const std::string& b) {
    SketchBuilder sb;
    const std::string u = "(" + a + ") / 2", v = "(" + b + ") / 2";
    sb.fixedPolygon({{cx + " - " + u, cy + " - " + v}, {cx + " + " + u, cy + " - " + v}, {cx + " + " + u, cy + " + " + v},
                     {cx + " - " + u, cy + " + " + v}});
    return sb.build();
}

SketchData circleAt(const std::string& cx, const std::string& cy, const std::string& r) {
    SketchBuilder sb;
    sb.fixedCircle(cx, cy, r);
    return sb.build();
}

}  // namespace

// 14. Cast housing: block L × W × H, draft α of the four sides from the bottom face (neutral),
// shell t inward without the top face, through hole d in the bottom wall at (hx, W/2).
// Outer section at height z: (L − 2kz) × (W − 2kz), k = tg α. The side planes of the cavity are
// the outer ones offset by t along their normals: x = kz + t / cos α, so the cavity section is
// (L − 2t/cos α − 2kz) × (W − 2t/cos α − 2kz) for t ≤ z ≤ H (sharp edges: inward offset of a
// convex solid with intersection joins).
ReferencePart castHousing() {
    ReferencePart p;
    p.name = "cast housing with drafts and shell";
    p.material = "aluminium";
    p.density = 2700;
    p.base = {{"L", 80}, {"W", 60}, {"H", 35}, {"al", 3}, {"t", 4}, {"d", 8}, {"hx", 30}};
    p.volume = [](const Values& v) {
        const double L = v.at("L"), W = v.at("W"), H = v.at("H"), t = v.at("t"), d = v.at("d");
        const double k = std::tan(deg(v.at("al"))), w = 2 * t / std::cos(deg(v.at("al")));
        return taperedBlock(L, W, k, 0, H) - taperedBlock(L - w, W - w, k, t, H) - kPi * d * d / 4 * t;
    };
    p.build = [](PartDocument& doc) {
        SketchBuilder b;
        b.rectangle("0", "0", "L", "W");
        add(doc, sketchSpec("Outline", "XY", b.build()));
        add(doc, extrudeSpec("Block", "Outline", {}, {{"distance", "H"}}));
        NamedShape s = built(doc);
        std::vector<TopoReference> sides{refTo(s, planarFace(s.shape(), gp::DX().Reversed(), 0)),
                                         refTo(s, planarFace(s.shape(), gp::DX(), 80)),
                                         refTo(s, planarFace(s.shape(), gp::DY().Reversed(), 0)),
                                         refTo(s, planarFace(s.shape(), gp::DY(), 60))};
        add(doc, {"Draft", "Drafts", "", {{"angle", "al"}},
                  {{"faces", sides}, {"neutralFace", {refTo(s, planarFace(s.shape(), gp::DZ().Reversed(), 0))}}},
                  {{"neutral", "face"}}, false});
        s = built(doc);
        add(doc, {"Shell", "Walls", "", {{"thickness", "t"}}, {{"faces", {refTo(s, planarFace(s.shape(), gp::DZ(), 35))}}},
                  {{"direction", "inward"}}, false});
        s = built(doc);
        add(doc, {"Hole", "Drain", "", {{"diameter", "d"}, {"x", "hx"}, {"y", "W / 2"}, {"z", "0"}},
                  {{"face", {refTo(s, planarFace(s.shape(), gp::DZ().Reversed(), 0))}}}, {{"end", "through"}}, false});
    };
    p.scenarios = {{"longer", {{"L", 100}}},      {"wider", {{"W", 70}}},       {"taller", {{"H", 45}}},
                   {"less draft", {{"al", 1.5}}}, {"more draft", {{"al", 5}}},  {"thicker walls", {{"t", 5}}},
                   {"bigger drain", {{"d", 10}}}, {"drain moved", {{"hx", 45}}}, {"all", {{"L", 90}, {"H", 30}, {"t", 3}}}};
    return p;
}

// 15. Angle bracket (base A × W × T, wall T × W × B) with two ribs t in the planes y = e and
// y = W − e, each a triangle with legs Lr − T and Hr − T, and a through hole d in the base.
ReferencePart stiffenedBracket() {
    ReferencePart p;
    p.name = "bracket with stiffening ribs";
    p.base = {{"A", 60}, {"B", 50}, {"W", 40}, {"T", 6}, {"Lr", 35}, {"Hr", 30}, {"t", 4}, {"e", 6}, {"d", 9}, {"hx", 45}};
    p.volume = [](const Values& v) {
        const double A = v.at("A"), B = v.at("B"), W = v.at("W"), T = v.at("T"), d = v.at("d");
        return A * W * T + T * W * (B - T) + 2 * (v.at("Lr") - T) * (v.at("Hr") - T) / 2 * v.at("t") - kPi * d * d / 4 * T;
    };
    p.build = [](PartDocument& doc) {
        add(doc, {"Box", "Base", "", {{"length", "A"}, {"width", "W"}, {"height", "T"}}, {}, {}, false});
        add(doc, {"Box", "Wall", "", {{"length", "T"}, {"width", "W"}, {"height", "B"}}, {}, {}, false});
        for (const auto& [id, y] : std::vector<std::pair<std::string, std::string>>{{"Rib1", "e"}, {"Rib2", "W - e"}}) {
            SketchBuilder line;
            line.line(line.point("T", "Hr", true), line.point("Lr", "T", true));
            add(doc, offsetSketch(id + "Line", "XZ", "-(" + y + ")", line.build(), true));
            add(doc, {"Rib", id, "", {{"thickness", "t"}}, {}, {}, false, {{"sketch", id + "Line"}}});
        }
        NamedShape s = built(doc);
        add(doc, {"Hole", "Mount", "", {{"diameter", "d"}, {"x", "hx"}, {"y", "W / 2"}, {"z", "T"}},
                  {{"face", {refTo(s, planarFace(s.shape(), gp::DZ(), 6))}}}, {{"end", "through"}}, false});
    };
    p.scenarios = {{"longer ribs", {{"Lr", 40}}}, {"taller ribs", {{"Hr", 40}}}, {"thicker ribs", {{"t", 5}}},
                   {"ribs apart", {{"e", 4}}},    {"longer base", {{"A", 70}}},  {"taller wall", {{"B", 60}}},
                   {"thicker", {{"T", 8}}},       {"wider", {{"W", 50}}},        {"bigger hole", {{"d", 11}}},
                   {"hole moved", {{"hx", 50}}}};
    return p;
}

// 16. Bent tube on a flange (sweep): ring R/r swept along a path in the plane y = 0 — up L1,
// a quarter arc Rb, then L2 horizontally; flange disc Rf × hf under it with a blind bore r of
// depth hf continuing the tube. Pappus: the ring area times the length of the path through its
// centroid.
ReferencePart bentTube() {
    ReferencePart p;
    p.name = "bent tube on flange";
    p.material = "copper";
    p.density = 8900;
    p.base = {{"R", 6}, {"r", 4}, {"L1", 30}, {"Rb", 25}, {"L2", 40}, {"Rf", 15}, {"hf", 5}};
    p.volume = [](const Values& v) {
        const double R = v.at("R"), r = v.at("r"), Rf = v.at("Rf"), hf = v.at("hf");
        return kPi * Rf * Rf * hf + kPi * (R * R - r * r) * (v.at("L1") + kPi / 2 * v.at("Rb") + v.at("L2")) -
               kPi * r * r * hf;
    };
    p.build = [](PartDocument& doc) {
        add(doc, {"Cylinder", "Flange", "", {{"radius", "Rf"}, {"height", "hf"}, {"z", "-hf"}}, {}, {}, false});
        SketchBuilder ring;
        ring.fixedCircle("0", "0", "R");
        ring.fixedCircle("0", "0", "r");
        add(doc, sketchSpec("TubeSection", "XY", ring.build()));
        SketchBuilder path;  // XZ: (u, v) -> (u, 0, v)
        const int a = path.point("0", "0", true), b = path.point("0", "L1", true), c = path.point("Rb", "L1", true),
                  e = path.point("Rb", "L1 + Rb", true), f = path.point("Rb + L2", "L1 + Rb", true);
        path.line(a, b);
        path.arc(c, b, e, false);
        path.line(e, f);
        add(doc, offsetSketch("TubePath", "XZ", "0", path.build(), true));
        add(doc, {"Sweep", "Tube", "", {}, {}, {}, false, {{"sketch", "TubeSection"}, {"pathSketch", "TubePath"}}});
        NamedShape s = built(doc);
        add(doc, {"Hole", "Bore", "", {{"diameter", "2 * r"}, {"depth", "hf"}, {"x", "0"}, {"y", "0"}, {"z", "-hf"}},
                  {{"face", {refTo(s, planarFace(s.shape(), gp::DZ().Reversed(), 5))}}},
                  {{"end", "blind"}, {"drillPoint", "false"}}, false});
    };
    p.scenarios = {{"thicker wall", {{"R", 7}}}, {"bigger bore", {{"r", 4.5}}}, {"longer riser", {{"L1", 40}}},
                   {"wider bend", {{"Rb", 30}}}, {"longer run", {{"L2", 55}}},  {"bigger flange", {{"Rf", 18}}},
                   {"thicker flange", {{"hf", 7}}}};
    return p;
}

// 17. Reducer (loft through three sections): flange disc Rf × hf, ruled loft through circles
// R1 (z = hf), R2 (z = hf + h1), R3 (z = hf + h1 + h2) — two cone frustums — and a through
// bore d along the axis.
ReferencePart loftedReducer() {
    ReferencePart p;
    p.name = "lofted reducer";
    p.base = {{"Rf", 30}, {"hf", 6}, {"R1", 20}, {"R2", 14}, {"R3", 10}, {"h1", 25}, {"h2", 15}, {"d", 10}};
    p.volume = [](const Values& v) {
        const double hf = v.at("hf"), h1 = v.at("h1"), h2 = v.at("h2"), d = v.at("d");
        return kPi * v.at("Rf") * v.at("Rf") * hf + frustum(v.at("R1"), v.at("R2"), h1) + frustum(v.at("R2"), v.at("R3"), h2) -
               kPi * d * d / 4 * (hf + h1 + h2);
    };
    p.build = [](PartDocument& doc) {
        add(doc, sketchSpec("FlangeSketch", "XY", circleAt("0", "0", "Rf")));
        add(doc, extrudeSpec("Flange", "FlangeSketch", {}, {{"distance", "hf"}}));
        add(doc, offsetSketch("Section1", "XY", "hf", circleAt("0", "0", "R1")));
        add(doc, offsetSketch("Section2", "XY", "hf + h1", circleAt("0", "0", "R2")));
        add(doc, offsetSketch("Section3", "XY", "hf + h1 + h2", circleAt("0", "0", "R3")));
        add(doc, {"Loft", "Cone", "", {}, {}, {{"surface", "ruled"}}, false,
                  {{"section1", "Section1"}, {"section2", "Section2"}, {"section3", "Section3"}}});
        NamedShape s = built(doc);
        add(doc, {"Hole", "Bore", "", {{"diameter", "d"}, {"x", "0"}, {"y", "0"}, {"z", "hf + h1 + h2"}},
                  {{"face", {refTo(s, planarFace(s.shape(), gp::DZ(), 46))}}}, {{"end", "through"}}, false});
    };
    p.scenarios = {{"bigger inlet", {{"R1", 22}}}, {"thinner waist", {{"R2", 12}}}, {"bigger outlet", {{"R3", 12}}},
                   {"longer cone", {{"h1", 35}}},  {"longer neck", {{"h2", 20}}},   {"bigger flange", {{"Rf", 34}}},
                   {"thicker flange", {{"hf", 8}}}, {"bigger bore", {{"d", 12}}}};
    return p;
}

// 18. Housing with tapped holes: block L × W × H shelled inward t without the bottom face; four
// M5 tapped holes through the top wall at (e, e), (L − e, e), (L − e, W − e), (e, W − e) — the
// centres are the points of a sketch. Drill M5: 5 − 0.8 = 4.2.
ReferencePart tappedHousing() {
    ReferencePart p;
    p.name = "housing with tapped holes";
    p.material = "aluminium";
    p.density = 2700;
    p.base = {{"L", 90}, {"W", 60}, {"H", 30}, {"t", 5}, {"e", 10}};
    p.volume = [](const Values& v) {
        const double L = v.at("L"), W = v.at("W"), H = v.at("H"), t = v.at("t"), drill = coarseDrill(5);
        return L * W * H - (L - 2 * t) * (W - 2 * t) * (H - t) - 4 * kPi * drill * drill / 4 * t;
    };
    p.build = [](PartDocument& doc) {
        add(doc, {"Box", "Block", "", {{"length", "L"}, {"width", "W"}, {"height", "H"}}, {}, {}, false});
        NamedShape s = built(doc);
        add(doc, {"Shell", "Walls", "", {{"thickness", "t"}}, {{"faces", {refTo(s, planarFace(s.shape(), gp::DZ().Reversed(), 0))}}},
                  {{"direction", "inward"}}, false});
        SketchBuilder pts;
        for (const auto& [x, y] : std::vector<std::pair<std::string, std::string>>{
                 {"e", "e"}, {"L - e", "e"}, {"L - e", "W - e"}, {"e", "W - e"}})
            pts.point(x, y, true);
        add(doc, offsetSketch("TapCentres", "XY", "H", pts.build()));
        s = built(doc);
        FeatureSpec tap{"ThreadedHole", "Taps", "", {{"nominal", "5"}, {"threadLength", "0"}},
                        {{"face", {refTo(s, planarFace(s.shape(), gp::DZ(), 30))}}}, {{"end", "through"}}, false};
        tap.links = {{"sketch", "TapCentres"}};
        add(doc, tap);
    };
    p.scenarios = {{"longer", {{"L", 110}}}, {"wider", {{"W", 70}}},       {"taller", {{"H", 40}}},
                   {"thinner", {{"t", 4}}},  {"holes inward", {{"e", 12}}}, {"all", {{"L", 80}, {"W", 50}, {"H", 25}}}};
    return p;
}

// 19. Multibody part with booleans between bodies: plate L × W × T (body "Plate"), post r of
// height T + h at (px, W/2) as a new body, block (c + 5)² × (T + 4) at the corner x = L,
// y = 0 as a new body; union plate + post; plate minus block keeping the block — two bodies:
// the notched plate with the post and the block.
ReferencePart multibodyPlate() {
    ReferencePart p;
    p.name = "multibody plate with body booleans";
    p.bodies = 2;
    p.base = {{"L", 80}, {"W", 50}, {"T", 8}, {"r", 6}, {"h", 20}, {"px", 25}, {"c", 15}};
    p.volume = [](const Values& v) {
        const double L = v.at("L"), W = v.at("W"), T = v.at("T"), r = v.at("r"), c = v.at("c");
        return L * W * T + kPi * r * r * v.at("h") - c * c * T + (c + 5) * (c + 5) * (T + 4);
    };
    p.build = [](PartDocument& doc) {
        SketchBuilder b;
        b.rectangle("0", "0", "L", "W");
        add(doc, sketchSpec("PlateSketch", "XY", b.build()));
        add(doc, extrudeSpec("Plate", "PlateSketch", {}, {{"distance", "T"}}));
        add(doc, sketchSpec("PostSketch", "XY", circleAt("px", "W / 2", "r")));
        add(doc, extrudeSpec("Post", "PostSketch", {{"mode", "newBody"}}, {{"distance", "T + h"}}));
        SketchBuilder k;
        k.rectangle("L - c", "-5", "c + 5", "c + 5");
        add(doc, offsetSketch("BlockSketch", "XY", "-2", k.build()));
        add(doc, extrudeSpec("Block", "BlockSketch", {{"mode", "newBody"}}, {{"distance", "T + 4"}}));
        NamedShape s = built(doc);
        add(doc, {"BodyBoolean", "Join", "", {}, {{"target", {bodyRef(s, "Plate")}}, {"tools", {bodyRef(s, "Post")}}},
                  {{"operation", "union"}}, false});
        s = built(doc);
        add(doc, {"BodyBoolean", "Notch", "", {}, {{"target", {bodyRef(s, "Plate")}}, {"tools", {bodyRef(s, "Block")}}},
                  {{"operation", "subtract"}, {"keepTools", "true"}}, false});
    };
    p.scenarios = {{"longer", {{"L", 100}}},    {"wider", {{"W", 60}}},       {"thicker", {{"T", 10}}},
                   {"thicker post", {{"r", 8}}}, {"taller post", {{"h", 30}}}, {"post moved", {{"px", 35}}},
                   {"bigger notch", {{"c", 20}}}};
    return p;
}

// 20. Plate with a cooling channel (sweep cut): plate L × W × T, circle r swept from the face
// x = 0 at y = y0, z = T/2 along +X for L1, a quarter arc Rc, then along +Y out of the face
// y = W. The path starts and ends e outside the plate; inside it is L1 + π/2·Rc + (W − y0 − Rc).
ReferencePart cooledPlate() {
    ReferencePart p;
    p.name = "plate with swept cooling channel";
    p.material = "aluminium";
    p.density = 2700;
    p.base = {{"L", 100}, {"W", 70}, {"T", 16}, {"r", 4}, {"y0", 20}, {"L1", 50}, {"Rc", 20}};
    p.volume = [](const Values& v) {
        const double r = v.at("r");
        return v.at("L") * v.at("W") * v.at("T") -
               kPi * r * r * (v.at("L1") + kPi / 2 * v.at("Rc") + v.at("W") - v.at("y0") - v.at("Rc"));
    };
    p.build = [](PartDocument& doc) {
        add(doc, {"Box", "Plate", "", {{"length", "L"}, {"width", "W"}, {"height", "T"}}, {}, {}, false});
        // YZ: (u, v) -> (offset, u, v).
        add(doc, offsetSketch("ChannelSection", "YZ", "-5", circleAt("y0", "T / 2", "r")));
        SketchBuilder path;  // XY at z = T/2: (u, v) -> (u, v, T/2)
        const int a = path.point("-5", "y0", true), b = path.point("L1", "y0", true), c = path.point("L1", "y0 + Rc", true),
                  e = path.point("L1 + Rc", "y0 + Rc", true), f = path.point("L1 + Rc", "W + 5", true);
        path.line(a, b);
        path.arc(c, b, e, true);
        path.line(e, f);
        add(doc, offsetSketch("ChannelPath", "XY", "T / 2", path.build(), true));
        add(doc, {"Sweep", "Channel", "", {}, {}, {{"mode", "cut"}}, false,
                  {{"sketch", "ChannelSection"}, {"pathSketch", "ChannelPath"}}});
    };
    p.scenarios = {{"bigger channel", {{"r", 5}}}, {"channel moved", {{"y0", 25}}}, {"longer run", {{"L1", 60}}},
                   {"wider bend", {{"Rc", 25}}},   {"thicker plate", {{"T", 20}}},  {"longer plate", {{"L", 120}}},
                   {"wider plate", {{"W", 80}}}};
    return p;
}

// 21. Thin-walled tray: base L × W × T and a thin-walled extrusion (inside, wall t, height h)
// of the outline L × W on top of it; the four outer vertical edges (T + h long after the
// merge of the coplanar side faces) rounded r.
ReferencePart thinTray() {
    ReferencePart p;
    p.name = "thin-walled tray";
    p.base = {{"L", 90}, {"W", 60}, {"T", 4}, {"t", 2.5}, {"h", 20}, {"r", 5}};
    p.volume = [](const Values& v) {
        const double L = v.at("L"), W = v.at("W"), T = v.at("T"), t = v.at("t"), h = v.at("h");
        return L * W * T + (L * W - (L - 2 * t) * (W - 2 * t)) * h - 4 * filletLoss(v.at("r"), T + h);
    };
    p.build = [](PartDocument& doc) {
        add(doc, {"Box", "Base", "", {{"length", "L"}, {"width", "W"}, {"height", "T"}}, {}, {}, false});
        SketchBuilder b;
        b.rectangle("0", "0", "L", "W");
        add(doc, offsetSketch("WallSketch", "XY", "T", b.build()));
        add(doc, extrudeSpec("Wall", "WallSketch", {{"thin", "inside"}}, {{"distance", "h"}, {"thickness", "t"}}));
        NamedShape s = built(doc);
        add(doc, {"Fillet", "Corners", "", {{"radius", "r"}},
                  {{"edges", {refTo(s, singleLine(s.shape(), 2, 0, 0)), refTo(s, singleLine(s.shape(), 2, 90, 0)),
                              refTo(s, singleLine(s.shape(), 2, 90, 60)), refTo(s, singleLine(s.shape(), 2, 0, 60))}}},
                  {}, false});
    };
    p.scenarios = {{"longer", {{"L", 110}}}, {"wider", {{"W", 70}}},          {"thicker base", {{"T", 6}}},
                   {"thicker wall", {{"t", 3}}}, {"taller wall", {{"h", 30}}}, {"bigger corners", {{"r", 7}}}};
    return p;
}

// 22. Cast post: plate L × W × T, a drafted extrusion of the circle R at (L/2, W/2) on top of
// it (height h, draft α inward — a cone frustum R → R − h·tg α), a through bore d from the top
// of the post, a chamfer c on the front top edge of the plate.
ReferencePart draftedPost() {
    ReferencePart p;
    p.name = "plate with drafted post";
    p.material = "cast iron";
    p.density = 7200;
    p.base = {{"L", 80}, {"W", 60}, {"T", 10}, {"R", 15}, {"h", 30}, {"al", 4}, {"d", 8}, {"c", 2}};
    p.volume = [](const Values& v) {
        const double L = v.at("L"), W = v.at("W"), T = v.at("T"), R = v.at("R"), h = v.at("h"), d = v.at("d");
        return L * W * T + frustum(R, R - h * std::tan(deg(v.at("al"))), h) - kPi * d * d / 4 * (T + h) -
               chamferLoss(v.at("c"), L);
    };
    p.build = [](PartDocument& doc) {
        add(doc, {"Box", "Plate", "", {{"length", "L"}, {"width", "W"}, {"height", "T"}}, {}, {}, false});
        add(doc, offsetSketch("PostSketch", "XY", "T", circleAt("L / 2", "W / 2", "R")));
        add(doc, extrudeSpec("Post", "PostSketch", {{"draftDirection", "inward"}}, {{"distance", "h"}, {"draftAngle", "al"}}));
        NamedShape s = built(doc);
        add(doc, {"Hole", "Bore", "", {{"diameter", "d"}, {"x", "L / 2"}, {"y", "W / 2"}, {"z", "T + h"}},
                  {{"face", {refTo(s, planarFace(s.shape(), gp::DZ(), 40))}}}, {{"end", "through"}}, false});
        s = built(doc);
        add(doc, {"Chamfer", "Edge", "", {{"distance", "c"}}, {{"edges", {refTo(s, singleLine(s.shape(), 0, 0, 10))}}}, {}, false});
    };
    p.scenarios = {{"bigger post", {{"R", 18}}}, {"taller post", {{"h", 40}}}, {"less draft", {{"al", 2}}},
                   {"more draft", {{"al", 7}}},  {"bigger bore", {{"d", 10}}}, {"longer plate", {{"L", 100}}},
                   {"thicker plate", {{"T", 12}}}, {"bigger chamfer", {{"c", 3}}}};
    return p;
}

// 23. Revolved cap: profile in XZ — skirt R × Hs and a quarter circle R on top — revolved about
// Z, shelled inward t without the bottom face (cylinder and hemisphere of inner radius R − t),
// and a flange ring from R − t to R + wf, height hfl, extruded over the skirt.
ReferencePart revolvedCap() {
    ReferencePart p;
    p.name = "revolved cap with shell";
    p.base = {{"R", 25}, {"Hs", 20}, {"t", 3}, {"wf", 8}, {"hfl", 4}};
    p.volume = [](const Values& v) {
        const double R = v.at("R"), Ri = R - v.at("t"), Rf = R + v.at("wf");
        return kPi * (R * R - Ri * Ri) * v.at("Hs") + 2 * kPi / 3 * (R * R * R - Ri * Ri * Ri) +
               kPi * (Rf * Rf - R * R) * v.at("hfl");
    };
    p.build = [](PartDocument& doc) {
        SketchBuilder b;  // XZ: (u, v) -> (u, 0, v)
        const int o = b.point("0", "0", true), a = b.point("R", "0", true), c = b.point("R", "Hs", true),
                  m = b.point("0", "Hs", true), top = b.point("0", "Hs + R", true);
        b.line(o, a);
        b.line(a, c);
        b.arc(m, c, top, true);
        b.line(top, o);
        add(doc, sketchSpec("CapProfile", "XZ", b.build()));
        add(doc, {"Revolve", "Cap", "", {}, {}, {{"axis", "Z"}}, false, {{"sketch", "CapProfile"}}});
        NamedShape s = built(doc);
        add(doc, {"Shell", "Walls", "", {{"thickness", "t"}}, {{"faces", {refTo(s, planarFace(s.shape(), gp::DZ().Reversed(), 0))}}},
                  {{"direction", "inward"}}, false});
        SketchBuilder ring;
        ring.fixedCircle("0", "0", "R + wf");
        ring.fixedCircle("0", "0", "R - t");
        add(doc, sketchSpec("FlangeSketch", "XY", ring.build()));
        add(doc, extrudeSpec("Flange", "FlangeSketch", {}, {{"distance", "hfl"}}));
    };
    p.scenarios = {{"bigger", {{"R", 30}}},           {"taller skirt", {{"Hs", 30}}}, {"thicker", {{"t", 4}}},
                   {"wider flange", {{"wf", 12}}},     {"thicker flange", {{"hfl", 6}}}};
    return p;
}

// 24. Fork (mirror of a body, union of bodies): plate L × W × T centred on x = 0, prong
// pw × (W − 10) × hp on top of it at x = g/2, y = 5 as a new body, its mirror about YZ as a new body, union of
// the plate with both prongs, through hole d at (0, W/2).
ReferencePart fork() {
    ReferencePart p;
    p.name = "fork with mirrored prong";
    p.base = {{"L", 70}, {"W", 30}, {"T", 8}, {"g", 30}, {"pw", 10}, {"hp", 35}, {"d", 10}};
    p.volume = [](const Values& v) {
        const double W = v.at("W"), T = v.at("T"), d = v.at("d");
        return v.at("L") * W * T + 2 * v.at("pw") * (W - 10) * v.at("hp") - kPi * d * d / 4 * T;
    };
    p.build = [](PartDocument& doc) {
        add(doc, {"Box", "Plate", "", {{"length", "L"}, {"width", "W"}, {"height", "T"}, {"x", "-L / 2"}}, {}, {}, false});
        SketchBuilder b;
        b.rectangle("g / 2", "5", "pw", "W - 10");
        add(doc, offsetSketch("ProngSketch", "XY", "T", b.build()));
        add(doc, extrudeSpec("Prong", "ProngSketch", {{"mode", "newBody"}}, {{"distance", "hp"}}));
        NamedShape s = built(doc);
        add(doc, {"BodyMirror", "Twin", "", {}, {{"bodies", {bodyRef(s, "Prong")}}}, {{"plane", "YZ"}, {"merge", "false"}}, false});
        s = built(doc);
        std::vector<TopoReference> tools;
        for (const auto& name : otherBodies(s, "Plate")) tools.push_back(bodyRef(s, name));
        EXPECT_EQ(tools.size(), 2u) << "prong and its mirror";
        add(doc, {"BodyBoolean", "Join", "", {}, {{"target", {bodyRef(s, "Plate")}}, {"tools", tools}}, {{"operation", "union"}}, false});
        s = built(doc);
        add(doc, {"Hole", "Pivot", "", {{"diameter", "d"}, {"x", "0"}, {"y", "W / 2"}, {"z", "T"}},
                  {{"face", {refTo(s, planarFace(s.shape(), gp::DZ(), 8))}}}, {{"end", "through"}}, false});
    };
    p.scenarios = {{"longer plate", {{"L", 80}}}, {"wider", {{"W", 36}}},       {"thicker", {{"T", 10}}},
                   {"wider gap", {{"g", 36}}},    {"thicker prongs", {{"pw", 12}}}, {"longer prongs", {{"hp", 45}}},
                   {"bigger hole", {{"d", 12}}}};
    return p;
}

// 25. Hopper block (loft cut): block L × W × H with a ruled lofted pocket from the rectangle
// a1 × b1 on the top face to a2 × b2 at depth dp (both centred), and a through hole d from the
// pocket floor. The pocket section varies linearly, so its volume is the prismoid formula
// dp/6·(A1 + 4·Am + A2) with Am = (a1 + a2)/2 · (b1 + b2)/2.
ReferencePart hopperBlock() {
    ReferencePart p;
    p.name = "hopper block with lofted pocket";
    p.base = {{"L", 80}, {"W", 60}, {"H", 40}, {"a1", 50}, {"b1", 36}, {"a2", 24}, {"b2", 16}, {"dp", 25}, {"d", 8}};
    p.volume = [](const Values& v) {
        const double a1 = v.at("a1"), b1 = v.at("b1"), a2 = v.at("a2"), b2 = v.at("b2"), dp = v.at("dp"), d = v.at("d");
        const double pocket = dp / 6 * (a1 * b1 + (a1 + a2) * (b1 + b2) + a2 * b2);
        return v.at("L") * v.at("W") * v.at("H") - pocket - kPi * d * d / 4 * (v.at("H") - dp);
    };
    p.build = [](PartDocument& doc) {
        add(doc, {"Box", "Block", "", {{"length", "L"}, {"width", "W"}, {"height", "H"}}, {}, {}, false});
        add(doc, offsetSketch("Mouth", "XY", "H", rectangleAt("L / 2", "W / 2", "a1", "b1")));
        add(doc, offsetSketch("Throat", "XY", "H - dp", rectangleAt("L / 2", "W / 2", "a2", "b2")));
        add(doc, {"Loft", "Pocket", "", {}, {}, {{"mode", "cut"}, {"surface", "ruled"}}, false,
                  {{"section1", "Mouth"}, {"section2", "Throat"}}});
        NamedShape s = built(doc);
        add(doc, {"Hole", "Outlet", "", {{"diameter", "d"}, {"x", "L / 2"}, {"y", "W / 2"}, {"z", "H - dp"}},
                  {{"face", {refTo(s, planarFace(s.shape(), gp::DZ(), 15))}}}, {{"end", "through"}}, false});
    };
    p.scenarios = {{"wider mouth", {{"a1", 56}}}, {"deeper mouth", {{"b1", 40}}}, {"wider throat", {{"a2", 28}}},
                   {"deeper pocket", {{"dp", 30}}}, {"taller block", {{"H", 50}}}, {"longer block", {{"L", 90}}},
                   {"bigger outlet", {{"d", 10}}}};
    return p;
}

// 26. Hub with pins (circular pattern of a body, union): disc R0 × h, a pin rp of height hp on
// its top at radius Rp as a new body, six copies about Z (circular pattern of the body), union
// of the disc with all six pins, through bore d in the centre.
ReferencePart pinnedHub() {
    ReferencePart p;
    p.name = "hub with patterned pins";
    p.base = {{"R0", 40}, {"h", 10}, {"Rp", 28}, {"rp", 4}, {"hp", 15}, {"d", 16}};
    p.volume = [](const Values& v) {
        const double R0 = v.at("R0"), h = v.at("h"), rp = v.at("rp"), d = v.at("d");
        return kPi * R0 * R0 * h + 6 * kPi * rp * rp * v.at("hp") - kPi * d * d / 4 * h;
    };
    p.build = [](PartDocument& doc) {
        add(doc, {"Cylinder", "Disc", "", {{"radius", "R0"}, {"height", "h"}}, {}, {}, false});
        add(doc, offsetSketch("PinSketch", "XY", "h", circleAt("Rp", "0", "rp")));
        add(doc, extrudeSpec("Pin", "PinSketch", {{"mode", "newBody"}}, {{"distance", "hp"}}));
        NamedShape s = built(doc);
        add(doc, {"BodyCircularPattern", "Pins", "", {{"count", "6"}, {"angle", "360"}}, {{"bodies", {bodyRef(s, "Pin")}}},
                  {{"axis", "Z"}, {"merge", "false"}}, false});
        s = built(doc);
        std::vector<TopoReference> tools;
        for (const auto& name : otherBodies(s, "Disc")) tools.push_back(bodyRef(s, name));
        EXPECT_EQ(tools.size(), 6u) << "the pin and five copies";
        add(doc, {"BodyBoolean", "Join", "", {}, {{"target", {bodyRef(s, "Disc")}}, {"tools", tools}}, {{"operation", "union"}}, false});
        s = built(doc);
        add(doc, {"Hole", "Bore", "", {{"diameter", "d"}, {"x", "0"}, {"y", "0"}, {"z", "h"}},
                  {{"face", {refTo(s, planarFace(s.shape(), gp::DZ(), 10))}}}, {{"end", "through"}}, false});
    };
    p.scenarios = {{"bigger disc", {{"R0", 45}}}, {"thicker disc", {{"h", 12}}}, {"pins outward", {{"Rp", 32}}},
                   {"thicker pins", {{"rp", 5}}}, {"taller pins", {{"hp", 20}}},  {"bigger bore", {{"d", 20}}}};
    return p;
}

std::vector<ReferencePart> phase2Parts() {
    return {castHousing(), stiffenedBracket(), bentTube(), loftedReducer(), tappedHousing(), multibodyPlate(), cooledPlate(),
            thinTray(), draftedPost(), revolvedCap(), fork(), hopperBlock(), pinnedHub()};
}

}  // namespace bendcore::modeling::refparts
