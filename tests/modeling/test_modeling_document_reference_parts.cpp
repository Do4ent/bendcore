// Reference parts of phases 1 and 2 (docs/ROADMAP_GENERAL_CAD.md, docs/modeling/REFERENCE_PARTS.md):
// every part is built through the part document from sketches and feature operations (phase 1
// parts here, phase 2 parts in reference_parts_phase2.cpp). For each part:
//   * the volume is checked against an analytic formula, the mass (kernel mass properties) against
//     the formula volume times the density, the number of bodies against the expected one;
//   * scenarios change sketch dimensions and feature parameters (document variables used by
//     sketch constraints and feature parameters); after each rebuild every reference of the
//     fillets, chamfers, holes, patterns, mirrors, shells, drafts, face sketches, phase 2
//     operations and body operations must still resolve (exactly or through history) with the
//     feature built, and the volume must match; the share of surviving references over all
//     parts and scenarios is printed (>= 95 %);
//   * save -> load gives identical volumes, states and reference names (with and without the
//     BRep cache);
//   * undo / redo of a dimension change and of a structural edit return the exact volumes;
//   * the final shape goes through STEP (write -> read) with the same volume and bodies; with
//     BENDCORE_WRITE_REFERENCE_STEP=1 the files and manifest.json are written to
//     tests/corpus/reference_parts/, and the committed files are checked by a second test.

#include "reference_parts.hpp"

#include <bendcore/kernel/mass_properties.hpp>
#include <bendcore/kernel/step_io.hpp>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <functional>
#include <map>
#include <set>

using namespace bendcore::modeling;
using namespace bendcore::modeling::testutil;
using namespace bendcore::modeling::refparts;

namespace {

// ------------------------------------------------------------------ parts

ReferencePart bracket() {
    ReferencePart p;
    p.name = "bracket";
    p.base = {{"A", 60}, {"B", 50}, {"T", 8}, {"Wd", 30}, {"r", 4}, {"hx", 40}, {"d", 8}};
    p.volume = [](const Values& v) {
        const double A = v.at("A"), B = v.at("B"), T = v.at("T"), Wd = v.at("Wd"), r = v.at("r"), d = v.at("d");
        return (A * T + (B - T) * T) * Wd + filletLoss(r, Wd) - kPi * d * d / 4 * T;
    };
    p.build = [](PartDocument& doc) {
        SketchBuilder b;
        b.fixedPolygon({{"0", "0"}, {"A", "0"}, {"A", "T"}, {"T", "T"}, {"T", "B"}, {"0", "B"}});
        add(doc, sketchSpec("Profile", "XZ", b.build()));
        add(doc, extrudeSpec("Body", "Profile", {{"extent", "symmetric"}}, {{"distance", "Wd"}}));
        NamedShape s = built(doc);
        add(doc, {"Fillet", "InnerRound", "", {{"radius", "r"}}, {{"edges", {refTo(s, singleLine(s.shape(), 1, 8, 8))}}}, {}, false});
        s = built(doc);
        const TopoReference top = refTo(s, planarFace(s.shape(), gp::DZ(), 8));
        SketchBuilder hb;
        hb.point("hx", "0", true);
        add(doc, faceSketch("HoleSketch", top, "0", "0", "T", hb.build()));
        FeatureSpec hole{"Hole", "MountHole", "", {{"diameter", "d"}}, {{"face", {top}}}, {{"end", "through"}}, false};
        hole.links = {{"sketch", "HoleSketch"}};
        add(doc, hole);
    };
    p.scenarios = {{"longer leg", {{"A", 80}}},       {"shorter upright", {{"B", 40}}}, {"thicker", {{"T", 10}}},
                   {"wider", {{"Wd", 40}}},           {"bigger fillet", {{"r", 6}}},     {"hole moved", {{"hx", 30}}},
                   {"bigger hole", {{"d", 10}}},      {"all legs", {{"A", 70}, {"B", 60}, {"T", 6}}}};
    return p;
}

ReferencePart shaft() {
    ReferencePart p;
    p.name = "shaft with keyway";
    p.base = {{"L", 100}, {"L1", 60}, {"R1", 15}, {"R2", 10}, {"lk", 30}, {"b", 6}, {"t", 4}, {"kx", 25}, {"c", 1.5}};
    p.volume = [](const Values& v) {
        const double R = v.at("R1"), b = v.at("b"), t = v.at("t");
        const double section = (b / 2) * std::sqrt(R * R - b * b / 4) + R * R * std::asin(b / (2 * R)) - b * (R - t);
        return kPi * R * R * v.at("L1") + kPi * v.at("R2") * v.at("R2") * (v.at("L") - v.at("L1")) - v.at("lk") * section -
               ringChamfer(v.at("c"), R, 1) - ringChamfer(v.at("c"), v.at("R2"), 1);
    };
    p.build = [](PartDocument& doc) {
        SketchBuilder b;
        b.fixedPolygon({{"0", "0"}, {"L", "0"}, {"L", "R2"}, {"L1", "R2"}, {"L1", "R1"}, {"0", "R1"}});
        add(doc, sketchSpec("Profile", "XZ", b.build()));
        add(doc, {"Revolve", "Body", "", {}, {}, {{"axis", "X"}}, false, {{"sketch", "Profile"}}});
        add(doc, {"DatumPlane", "KeyPlane", "", {{"offset", "R1"}}, {}, {{"base", "XY"}}, false});
        SketchBuilder k;
        k.fixedPolygon({{"kx - lk/2", "-b/2"}, {"kx + lk/2", "-b/2"}, {"kx + lk/2", "b/2"}, {"kx - lk/2", "b/2"}});
        FeatureSpec ks = sketchSpec("KeySketch", "datum", k.build());
        ks.links = {{"plane", "KeyPlane"}};
        add(doc, ks);
        add(doc, extrudeSpec("Keyway", "KeySketch", {{"mode", "cut"}, {"reversed", "true"}}, {{"distance", "t"}}));
        NamedShape s = built(doc);
        add(doc, {"Chamfer", "EndChamfers", "", {{"distance", "c"}},
                  {{"edges", {refTo(s, circleEdge(s.shape(), gp_Pnt(0, 0, 0), 15, 0)),
                              refTo(s, circleEdge(s.shape(), gp_Pnt(100, 0, 0), 10, 0))}}},
                  {}, false});
    };
    p.scenarios = {{"thicker journal", {{"R1", 18}}}, {"thicker end", {{"R2", 12}}}, {"longer", {{"L", 120}}},
                   {"longer journal", {{"L1", 70}}}, {"shorter key", {{"lk", 20}}},   {"wider key", {{"b", 8}}},
                   {"deeper key", {{"t", 5}}},       {"bigger chamfers", {{"c", 2}}}, {"key moved", {{"kx", 30}}}};
    return p;
}

ReferencePart flange() {
    ReferencePart p;
    p.name = "flange with bolt circle";
    p.base = {{"D", 120}, {"d0", 40}, {"T", 12}, {"Dh", 70}, {"h2", 15}, {"BC", 95}, {"dh", 10}, {"N", 6}, {"c", 1.5}};
    p.volume = [](const Values& v) {
        const double D = v.at("D"), d0 = v.at("d0"), T = v.at("T"), Dh = v.at("Dh"), dh = v.at("dh");
        return kPi * (D * D - d0 * d0) / 4 * T + kPi * (Dh * Dh - d0 * d0) / 4 * v.at("h2") -
               v.at("N") * kPi * dh * dh / 4 * T - ringChamfer(v.at("c"), D / 2, 1);
    };
    p.build = [](PartDocument& doc) {
        SketchBuilder b;
        b.fixedCircle("0", "0", "D/2");
        b.fixedCircle("0", "0", "d0/2");
        add(doc, sketchSpec("Disc", "XY", b.build()));
        add(doc, extrudeSpec("Plate", "Disc", {}, {{"distance", "T"}}));
        NamedShape s = built(doc);
        SketchBuilder h;
        h.fixedCircle("0", "0", "Dh/2");
        h.fixedCircle("0", "0", "d0/2");
        add(doc, faceSketch("HubSketch", refTo(s, planarFace(s.shape(), gp::DZ(), 12)), "0", "0", "T", h.build()));
        add(doc, extrudeSpec("Hub", "HubSketch", {}, {{"distance", "h2"}}));
        s = built(doc);
        const TopoReference top = refTo(s, planarFace(s.shape(), gp::DZ(), 12));
        SketchBuilder hb;
        hb.point("BC/2", "0", true);
        add(doc, faceSketch("BoltSketch", top, "0", "0", "T", hb.build()));
        FeatureSpec hole{"Hole", "Bolt", "", {{"diameter", "dh"}}, {{"face", {top}}}, {{"end", "through"}}, false};
        hole.links = {{"sketch", "BoltSketch"}};
        add(doc, hole);
        add(doc, {"CircularPattern", "BoltCircle", "", {{"count", "N"}}, {}, {{"axis", "Z"}}, false, {{"feature", "Bolt"}}});
        s = built(doc);
        add(doc, {"Chamfer", "RimChamfer", "", {{"distance", "c"}},
                  {{"edges", {refTo(s, circleEdge(s.shape(), gp_Pnt(0, 0, 0), 60, 2))}}}, {}, false});
    };
    p.scenarios = {{"bigger disc", {{"D", 140}}}, {"thicker", {{"T", 15}}},   {"wider bolt circle", {{"BC", 100}}},
                   {"8 bolts", {{"N", 8}}},       {"bigger bolts", {{"dh", 12}}}, {"slimmer hub", {{"Dh", 60}}},
                   {"taller hub", {{"h2", 20}}},  {"bigger chamfer", {{"c", 2}}}, {"smaller bore", {{"d0", 30}}}};
    return p;
}

ReferencePart cover() {
    ReferencePart p;
    p.name = "cover with rounded corners";
    p.base = {{"L", 100}, {"W", 60}, {"H", 12}, {"r", 8}, {"e", 10}, {"dh", 6}};
    p.volume = [](const Values& v) {
        const double H = v.at("H"), r = v.at("r"), dh = v.at("dh");
        return v.at("L") * v.at("W") * H - 4 * filletLoss(r, H) - 4 * kPi * dh * dh / 4 * H;
    };
    p.build = [](PartDocument& doc) {
        SketchBuilder b;
        b.rectangle("0", "0", "L", "W");
        add(doc, sketchSpec("Outline", "XY", b.build()));
        add(doc, extrudeSpec("Body", "Outline", {}, {{"distance", "H"}}));
        NamedShape s = built(doc);
        std::vector<TopoReference> corners;
        for (auto [x, y] : {std::pair{0.0, 0.0}, {100.0, 0.0}, {100.0, 60.0}, {0.0, 60.0}}) {
            corners.push_back(refTo(s, singleLine(s.shape(), 2, x, y)));
        }
        add(doc, {"Fillet", "Corners", "", {{"radius", "r"}}, {{"edges", corners}}, {}, false});
        s = built(doc);
        const TopoReference top = refTo(s, planarFace(s.shape(), gp::DZ(), 12));
        SketchBuilder hb;
        hb.point("e", "e", true);
        add(doc, faceSketch("HoleSketch", top, "0", "0", "H", hb.build()));
        FeatureSpec hole{"Hole", "Screw", "", {{"diameter", "dh"}}, {{"face", {top}}}, {{"end", "through"}}, false};
        hole.links = {{"sketch", "HoleSketch"}};
        add(doc, hole);
        add(doc, {"LinearPattern", "ScrewGrid", "",
                  {{"count1", "2"}, {"spacing1", "L - 2*e"}, {"count2", "2"}, {"spacing2", "W - 2*e"}}, {},
                  {{"direction1", "X"}, {"direction2", "Y"}}, false, {{"feature", "Screw"}}});
    };
    p.scenarios = {{"longer", {{"L", 120}}},  {"wider", {{"W", 70}}},       {"thicker", {{"H", 16}}},
                   {"smaller radius", {{"r", 6}}}, {"holes inward", {{"e", 12}}}, {"smaller holes", {{"dh", 5}}},
                   {"smaller cover", {{"L", 90}, {"W", 50}}}};
    return p;
}

ReferencePart housing() {
    ReferencePart p;
    p.name = "housing with shell";
    p.base = {{"L", 80}, {"W", 50}, {"H", 40}, {"t", 3}, {"d", 10}};
    p.volume = [](const Values& v) {
        const double L = v.at("L"), W = v.at("W"), H = v.at("H"), t = v.at("t"), d = v.at("d");
        return L * W * H - (L - 2 * t) * (W - 2 * t) * (H - t) - kPi * d * d / 4 * t;
    };
    p.build = [](PartDocument& doc) {
        SketchBuilder b;
        b.rectangle("0", "0", "L", "W");
        add(doc, sketchSpec("Outline", "XY", b.build()));
        add(doc, extrudeSpec("Block", "Outline", {}, {{"distance", "H"}}));
        NamedShape s = built(doc);
        add(doc, {"Shell", "Hollow", "", {{"thickness", "t"}}, {{"faces", {refTo(s, planarFace(s.shape(), gp::DZ(), 40))}}}, {}, false});
        s = built(doc);
        const TopoReference front = refTo(s, planarFace(s.shape(), gp::DY().Reversed(), 0));
        SketchBuilder hb;
        hb.point("0", "0", true);
        add(doc, faceSketch("PortSketch", front, "L/2", "0", "H/2", hb.build()));
        FeatureSpec hole{"Hole", "Port", "", {{"diameter", "d"}, {"depth", "t + 2"}}, {{"face", {front}}},
                         {{"drillPoint", "false"}}, false};
        hole.links = {{"sketch", "PortSketch"}};
        add(doc, hole);
    };
    p.scenarios = {{"longer", {{"L", 100}}}, {"wider", {{"W", 60}}},  {"taller", {{"H", 50}}},
                   {"thicker wall", {{"t", 4}}}, {"bigger port", {{"d", 12}}}, {"lower", {{"H", 30}}}};
    return p;
}

ReferencePart counterboredPlate() {
    ReferencePart p;
    p.name = "plate with counterbores";
    p.base = {{"L", 120}, {"W", 80}, {"T", 20}, {"d", 9}, {"D", 15}, {"k", 9}, {"p1", 30}, {"p2", 90}, {"c", 2}};
    p.volume = [](const Values& v) {
        const double T = v.at("T"), d = v.at("d"), D = v.at("D"), k = v.at("k");
        return v.at("L") * v.at("W") * T - 2 * (kPi * D * D / 4 * k + kPi * d * d / 4 * (T - k)) -
               chamferLoss(v.at("c"), v.at("L"));
    };
    p.build = [](PartDocument& doc) {
        SketchBuilder b;
        b.rectangle("0", "0", "L", "W");
        add(doc, sketchSpec("Outline", "XY", b.build()));
        add(doc, extrudeSpec("Plate", "Outline", {}, {{"distance", "T"}}));
        NamedShape s = built(doc);
        const TopoReference top = refTo(s, planarFace(s.shape(), gp::DZ(), 20));
        SketchBuilder hb;
        hb.point("p1", "W/2", true);
        hb.point("p2", "W/2", true);
        add(doc, faceSketch("HoleSketch", top, "0", "0", "T", hb.build()));
        FeatureSpec hole{"Hole", "Counterbores", "", {{"diameter", "d"}, {"cbDiameter", "D"}, {"cbDepth", "k"}},
                         {{"face", {top}}}, {{"type", "counterbore"}, {"end", "through"}}, false};
        hole.links = {{"sketch", "HoleSketch"}};
        add(doc, hole);
        s = built(doc);
        add(doc, {"Chamfer", "EdgeBreak", "", {{"distance", "c"}}, {{"edges", {refTo(s, singleLine(s.shape(), 0, 0, 20))}}}, {}, false});
    };
    p.scenarios = {{"longer", {{"L", 140}}}, {"wider", {{"W", 90}}},     {"thicker", {{"T", 25}}},
                   {"bigger hole", {{"d", 11}}}, {"bigger counterbore", {{"D", 18}}}, {"deeper counterbore", {{"k", 11}}},
                   {"hole moved", {{"p1", 25}}}, {"bigger chamfer", {{"c", 3}}}};
    return p;
}

ReferencePart pulley() {
    ReferencePart p;
    p.name = "pulley with groove";
    p.base = {{"R", 50}, {"ri", 15}, {"w", 30}, {"g", 8}, {"gw", 10}, {"rf", 1.5}};
    p.volume = [](const Values& v) {
        const double R = v.at("R"), g = v.at("g");
        return kPi * (R * R - v.at("ri") * v.at("ri")) * v.at("w") - kPi * (R * R - (R - g) * (R - g)) * v.at("gw") +
               2 * ringFillet(v.at("rf"), R - g);
    };
    p.build = [](PartDocument& doc) {
        SketchBuilder b;
        b.fixedPolygon({{"ri", "0"}, {"R", "0"}, {"R", "w"}, {"ri", "w"}});
        add(doc, sketchSpec("Section", "XZ", b.build()));
        add(doc, {"Revolve", "Wheel", "", {}, {}, {{"axis", "Z"}}, false, {{"sketch", "Section"}}});
        SketchBuilder g;
        g.fixedPolygon({{"R - g", "w/2 - gw/2"}, {"R + 2", "w/2 - gw/2"}, {"R + 2", "w/2 + gw/2"}, {"R - g", "w/2 + gw/2"}});
        add(doc, sketchSpec("GrooveSection", "XZ", g.build()));
        add(doc, {"Revolve", "Groove", "", {}, {}, {{"axis", "Z"}, {"mode", "cut"}}, false, {{"sketch", "GrooveSection"}}});
        NamedShape s = built(doc);
        add(doc, {"Fillet", "GrooveRounds", "", {{"radius", "rf"}},
                  {{"edges", {refTo(s, circleEdge(s.shape(), gp_Pnt(0, 0, 10), 42, 2)),
                              refTo(s, circleEdge(s.shape(), gp_Pnt(0, 0, 20), 42, 2))}}},
                  {}, false});
    };
    p.scenarios = {{"bigger", {{"R", 60}}},    {"bigger bore", {{"ri", 20}}}, {"wider", {{"w", 36}}},
                   {"deeper groove", {{"g", 10}}}, {"wider groove", {{"gw", 12}}}, {"bigger rounds", {{"rf", 2}}}};
    return p;
}

ReferencePart bushing() {
    ReferencePart p;
    p.name = "bushing with chamfers";
    p.base = {{"R", 25}, {"r", 15}, {"h", 40}, {"c1", 2}, {"c2", 1.5}};
    p.volume = [](const Values& v) {
        const double R = v.at("R"), r = v.at("r");
        return kPi * (R * R - r * r) * v.at("h") - ringChamfer(v.at("c1"), R, 1) - ringChamfer(v.at("c2"), r, -1);
    };
    p.build = [](PartDocument& doc) {
        SketchBuilder b;
        b.fixedCircle("0", "0", "R");
        b.fixedCircle("0", "0", "r");
        add(doc, sketchSpec("Rings", "XY", b.build()));
        add(doc, extrudeSpec("Tube", "Rings", {}, {{"distance", "h"}}));
        NamedShape s = built(doc);
        add(doc, {"Chamfer", "OuterChamfer", "", {{"distance", "c1"}},
                  {{"edges", {refTo(s, circleEdge(s.shape(), gp_Pnt(0, 0, 40), 25, 2))}}}, {}, false});
        add(doc, {"Chamfer", "InnerChamfer", "", {{"distance", "c2"}},
                  {{"edges", {refTo(s, circleEdge(s.shape(), gp_Pnt(0, 0, 40), 15, 2))}}}, {}, false});
    };
    p.scenarios = {{"bigger", {{"R", 30}}},          {"smaller bore", {{"r", 12}}}, {"longer", {{"h", 50}}},
                   {"bigger outer chamfer", {{"c1", 3}}}, {"bigger inner chamfer", {{"c2", 2}}}};
    return p;
}

ReferencePart support() {
    ReferencePart p;
    p.name = "support with mirrored boss";
    p.base = {{"L", 120}, {"W", 40}, {"T", 10}, {"bx", 35}, {"rb", 8}, {"hb", 25}, {"rf", 2}};
    p.volume = [](const Values& v) {
        const double rb = v.at("rb");
        return v.at("L") * v.at("W") * v.at("T") + 2 * kPi * rb * rb * v.at("hb") + ringFillet(v.at("rf"), rb);
    };
    p.build = [](PartDocument& doc) {
        SketchBuilder b;
        b.fixedPolygon({{"-L/2", "0"}, {"L/2", "0"}, {"L/2", "W"}, {"-L/2", "W"}});
        add(doc, sketchSpec("Base", "XY", b.build()));
        add(doc, extrudeSpec("Plate", "Base", {}, {{"distance", "T"}}));
        NamedShape s = built(doc);
        SketchBuilder c;
        c.fixedCircle("bx", "W/2", "rb");
        add(doc, faceSketch("BossSketch", refTo(s, planarFace(s.shape(), gp::DZ(), 10)), "0", "0", "T", c.build()));
        add(doc, extrudeSpec("Boss", "BossSketch", {}, {{"distance", "hb"}}));
        add(doc, {"Mirror", "BossMirror", "", {}, {}, {{"what", "feature"}, {"plane", "YZ"}}, false, {{"feature", "Boss"}}});
        s = built(doc);
        add(doc, {"Fillet", "MirroredRoot", "", {{"radius", "rf"}},
                  {{"edges", {refTo(s, circleEdge(s.shape(), gp_Pnt(-35, 20, 10), 8, 2))}}}, {}, false});
    };
    p.scenarios = {{"longer", {{"L", 140}}},  {"wider", {{"W", 50}}},     {"bosses apart", {{"bx", 45}}},
                   {"thicker bosses", {{"rb", 10}}}, {"taller bosses", {{"hb", 30}}}, {"bigger root", {{"rf", 3}}},
                   {"thicker plate", {{"T", 12}}}};
    return p;
}

ReferencePart gridPlate() {
    ReferencePart p;
    p.name = "grid plate with slot pattern";
    p.base = {{"L", 150}, {"W", 100}, {"T", 8}, {"a", 20}, {"b", 10}, {"x0", 20}, {"y0", 20},
              {"s1", 30}, {"s2", 25}, {"n1", 4}, {"n2", 3}, {"c", 1}};
    p.volume = [](const Values& v) {
        const double T = v.at("T");
        return v.at("L") * v.at("W") * T - v.at("n1") * v.at("n2") * v.at("a") * v.at("b") * T - chamferLoss(v.at("c"), v.at("L"));
    };
    p.build = [](PartDocument& doc) {
        SketchBuilder b;
        b.rectangle("0", "0", "L", "W");
        add(doc, sketchSpec("Outline", "XY", b.build()));
        add(doc, extrudeSpec("Plate", "Outline", {}, {{"distance", "T"}}));
        NamedShape s = built(doc);
        SketchBuilder sl;
        sl.fixedPolygon({{"x0", "y0"}, {"x0 + a", "y0"}, {"x0 + a", "y0 + b"}, {"x0", "y0 + b"}});
        add(doc, faceSketch("SlotSketch", refTo(s, planarFace(s.shape(), gp::DZ(), 8)), "0", "0", "T", sl.build()));
        add(doc, extrudeSpec("Slot", "SlotSketch", {{"mode", "cut"}, {"reversed", "true"}, {"extent", "throughAll"}}, {}));
        add(doc, {"LinearPattern", "Slots", "", {{"count1", "n1"}, {"spacing1", "s1"}, {"count2", "n2"}, {"spacing2", "s2"}},
                  {}, {{"direction1", "X"}, {"direction2", "Y"}}, false, {{"feature", "Slot"}}});
        s = built(doc);
        add(doc, {"Chamfer", "EdgeBreak", "", {{"distance", "c"}}, {{"edges", {refTo(s, singleLine(s.shape(), 0, 0, 0))}}}, {}, false});
    };
    p.scenarios = {{"longer", {{"L", 170}}},      {"five columns", {{"L", 180}, {"n1", 5}}}, {"rows apart", {{"s2", 30}}},
                   {"narrower slots", {{"a", 15}}}, {"thicker", {{"T", 10}}},               {"bigger chamfer", {{"c", 2}}},
                   {"wider", {{"W", 110}}}};
    return p;
}

ReferencePart draftedBoss() {
    ReferencePart p;
    p.name = "cast boss with draft";
    p.base = {{"a", 40}, {"h", 20}, {"al", 5}};
    p.volume = [](const Values& v) {
        const double a = v.at("a"), h = v.at("h"), a2 = a - 2 * h * std::tan(v.at("al") * kPi / 180);
        return h / 3 * (a * a + a2 * a2 + a * a2);
    };
    p.build = [](PartDocument& doc) {
        SketchBuilder b;
        b.rectangle("0", "0", "a", "a");
        add(doc, sketchSpec("Outline", "XY", b.build()));
        add(doc, extrudeSpec("Block", "Outline", {}, {{"distance", "h"}}));
        NamedShape s = built(doc);
        std::vector<TopoReference> sides{refTo(s, planarFace(s.shape(), gp::DX().Reversed(), 0)),
                                         refTo(s, planarFace(s.shape(), gp::DX(), 40)),
                                         refTo(s, planarFace(s.shape(), gp::DY().Reversed(), 0)),
                                         refTo(s, planarFace(s.shape(), gp::DY(), 40))};
        add(doc, {"Draft", "Taper", "", {{"angle", "al"}},
                  {{"faces", sides}, {"neutralFace", {refTo(s, planarFace(s.shape(), gp::DZ().Reversed(), 0))}}},
                  {{"neutral", "face"}}, false});
    };
    p.scenarios = {{"bigger", {{"a", 50}}}, {"taller", {{"h", 25}}}, {"less draft", {{"al", 3}}}, {"more draft", {{"al", 8}}}};
    return p;
}

// Phase 2. Plate L × W × T with a swept grip: circle r on the top face at (gx, W/2), path in
// the plane y = W/2 — up by Hg, then a quarter arc Rb that ends horizontally; tapped hole M8
// through the plate at (hx, W/2).
ReferencePart sweptGrip() {
    ReferencePart p;
    p.name = "plate with swept grip";
    p.base = {{"L", 80}, {"W", 40}, {"T", 10}, {"r", 4}, {"gx", 20}, {"Hg", 25}, {"Rb", 20}, {"hx", 60}};
    p.volume = [](const Values& v) {
        const double drill = 6.75;  // M8: D − P
        return v.at("L") * v.at("W") * v.at("T") + kPi * v.at("r") * v.at("r") * (v.at("Hg") + kPi / 2 * v.at("Rb")) -
               kPi * drill * drill / 4 * v.at("T");
    };
    p.build = [](PartDocument& doc) {
        add(doc, {"Box", "Plate", "", {{"length", "L"}, {"width", "W"}, {"height", "T"}}, {}, {}, false});
        SketchBuilder c;
        c.fixedCircle("gx", "W/2", "r");
        FeatureSpec prof = sketchSpec("GripProfile", "XY", c.build());
        prof.parameters = {{"offset", "T"}};
        add(doc, prof);
        SketchBuilder path;  // XZ: (u, v) -> (u, -offset, v); offset -W/2 puts it at y = W/2
        const int a = path.point("gx", "T", true), b = path.point("gx", "T + Hg", true),
                  c0 = path.point("gx + Rb", "T + Hg", true), e = path.point("gx + Rb", "T + Hg + Rb", true);
        path.line(a, b);
        path.arc(c0, b, e, false);
        FeatureSpec ps = sketchSpec("GripPath", "XZ", path.build());
        ps.options["contour"] = "open";
        ps.parameters = {{"offset", "-W/2"}};
        add(doc, ps);
        add(doc, {"Sweep", "Grip", "", {}, {}, {}, false, {{"sketch", "GripProfile"}, {"pathSketch", "GripPath"}}});
        NamedShape s = built(doc);
        add(doc, {"ThreadedHole", "Tap", "", {{"nominal", "8"}, {"x", "hx"}, {"y", "W/2"}, {"z", "T"}},
                  {{"face", {refTo(s, planarFace(s.shape(), gp::DZ(), 10))}}}, {{"end", "through"}}, false});
    };
    p.scenarios = {{"thicker profile", {{"r", 5}}},     {"taller grip", {{"Hg", 35}}}, {"wider bend", {{"Rb", 28}}},
                   {"grip moved", {{"gx", 15}}},        {"hole moved", {{"hx", 55}}}, {"thicker plate", {{"T", 14}}},
                   {"longer plate", {{"L", 100}}},      {"wider plate", {{"W", 50}}}};
    return p;
}

// Phase 2. Angle bracket (base 40 × 20 × T, wall T × 20 × 40) with a rib in the middle plane
// from the wall (z = Hr) to the base (x = Lr), a lofted boss a × a → b × b of height hb on the
// base at x = bx, and a tapped hole M6 (length 8, drill depth 10) in the top of the boss.
ReferencePart ribbedBracket() {
    ReferencePart p;
    p.name = "ribbed bracket with lofted boss";
    p.base = {{"T", 5}, {"Lr", 22}, {"Hr", 25}, {"t", 2}, {"a", 10}, {"b", 7}, {"hb", 8}, {"bx", 32}};
    p.volume = [](const Values& v) {
        const double T = v.at("T"), a = v.at("a"), b = v.at("b"), hb = v.at("hb");
        return 40 * 20 * T + T * 20 * (40 - T) + (v.at("Lr") - T) * (v.at("Hr") - T) / 2 * v.at("t") +
               hb / 3 * (a * a + b * b + a * b) - kPi * 2.5 * 2.5 * 10;
    };
    p.build = [](PartDocument& doc) {
        add(doc, {"Box", "Base", "", {{"length", "40"}, {"width", "20"}, {"height", "T"}}, {}, {}, false});
        add(doc, {"Box", "Wall", "", {{"length", "T"}, {"width", "20"}, {"height", "40"}}, {}, {}, false});
        SketchBuilder line;
        line.line(line.point("T", "Hr", true), line.point("Lr", "T", true));
        FeatureSpec rs = sketchSpec("RibLine", "XZ", line.build());
        rs.options["contour"] = "open";
        rs.parameters = {{"offset", "-10"}};
        add(doc, rs);
        add(doc, {"Rib", "Rib", "", {{"thickness", "t"}}, {}, {}, false, {{"sketch", "RibLine"}}});
        SketchBuilder s1, s2;
        s1.fixedPolygon({{"bx - a/2", "10 - a/2"}, {"bx + a/2", "10 - a/2"}, {"bx + a/2", "10 + a/2"}, {"bx - a/2", "10 + a/2"}});
        s2.fixedPolygon({{"bx - b/2", "10 - b/2"}, {"bx + b/2", "10 - b/2"}, {"bx + b/2", "10 + b/2"}, {"bx - b/2", "10 + b/2"}});
        FeatureSpec f1 = sketchSpec("FootBottom", "XY", s1.build());
        f1.parameters = {{"offset", "T"}};
        FeatureSpec f2 = sketchSpec("FootTop", "XY", s2.build());
        f2.parameters = {{"offset", "T + hb"}};
        add(doc, f1);
        add(doc, f2);
        add(doc, {"Loft", "Boss", "", {}, {}, {{"surface", "ruled"}}, false, {{"section1", "FootBottom"}, {"section2", "FootTop"}}});
        NamedShape s = built(doc);
        add(doc, {"ThreadedHole", "Tap", "", {{"nominal", "6"}, {"threadLength", "8"}, {"drillDepth", "10"},
                                              {"x", "bx"}, {"y", "10"}, {"z", "T + hb"}},
                  {{"face", {refTo(s, planarFace(s.shape(), gp::DZ(), 13))}}}, {{"drillPoint", "false"}}, false});
    };
    p.scenarios = {{"longer rib", {{"Lr", 26}}},  {"taller rib", {{"Hr", 30}}},  {"thicker rib", {{"t", 3}}},
                   {"wider boss", {{"a", 12}}},   {"taller boss", {{"hb", 12}}}, {"boss moved", {{"bx", 30}}},
                   {"thicker walls", {{"T", 6}}}, {"all rib", {{"Lr", 20}, {"Hr", 22}, {"t", 2.5}}}};
    return p;
}

std::vector<ReferencePart> phase1Parts() {
    return {bracket(), shaft(), flange(), cover(), housing(), counterboredPlate(),
            pulley(), bushing(), support(), gridPlate(), draftedBoss(), sweptGrip(), ribbedBracket()};
}

std::vector<ReferencePart> allParts() {
    std::vector<ReferencePart> parts = phase1Parts();
    for (auto& part : phase2Parts()) parts.push_back(std::move(part));
    return parts;
}

// ------------------------------------------------------------------ checks

const std::set<std::string> kWatched{"Fillet", "Chamfer", "Hole", "LinearPattern", "CircularPattern",
                                     "Mirror", "Shell", "Draft", "Sketch", "DatumPlane",
                                     "ThreadedHole", "Sweep", "Loft", "Rib", "BodyBoolean", "BodyMirror",
                                     "BodyLinearPattern", "BodyCircularPattern", "DeleteBody"};

struct Tally {
    int total = 0;
    int survived = 0;
};

void setValues(PartDocument& doc, const Values& v) {
    for (const auto& [k, x] : v) setVar(doc, k, x);
}

// References of the watched features in the current state; a watched feature without
// references (a pattern, a mirror) counts once by its state.
void tallyReferences(const PartDocument& doc, const std::string& where, bool volumeOk, Tally& tally) {
    for (const auto& id : doc.featureIds()) {
        FeatureInfo fi = info(doc, id);
        if (!kWatched.count(fi.type)) continue;
        if (fi.type == "Sketch" && fi.references.at("face").empty()) continue;
        if (fi.type == "DatumPlane" && fi.referenceReports.empty()) continue;
        const bool built = fi.state == FeatureState::Ok;
        const std::size_t n = std::max<std::size_t>(1, fi.referenceReports.size());
        for (std::size_t k = 0; k < n; ++k) {
            bool ok = built && volumeOk;
            if (k < fi.referenceReports.size()) {
                const ResolveMethod m = fi.referenceReports[k].resolution.method;
                ok = ok && (m == ResolveMethod::Exact || m == ResolveMethod::History);
            }
            ++tally.total;
            if (ok) {
                ++tally.survived;
            } else {
                ADD_FAILURE() << where << ": reference " << k << " of " << id << " lost (state " << toString(fi.state)
                              << ")\n" << messages(fi);
            }
        }
    }
}

bool volumeMatches(const PartDocument& doc, double expected, const std::string& where) {
    const double v = volume(doc.finalShape().shape());
    EXPECT_NEAR(v, expected, 1e-6 * expected) << where;
    return std::abs(v - expected) <= 1e-6 * expected;
}

void expectSameState(const PartDocument& a, const PartDocument& b, const std::string& where) {
    ASSERT_EQ(a.featureIds(), b.featureIds()) << where;
    for (const auto& id : a.featureIds()) {
        FeatureInfo fa = info(a, id), fb = info(b, id);
        EXPECT_EQ(fa.state, fb.state) << where << " " << id;
        for (const auto& [slot, refs] : fa.references) {
            ASSERT_EQ(refs.size(), fb.references.at(slot).size()) << where << " " << id;
            for (std::size_t k = 0; k < refs.size(); ++k) {
                EXPECT_EQ(refs[k].name, fb.references.at(slot)[k].name) << where << " " << id;
            }
        }
        ASSERT_EQ(static_cast<bool>(fa.output), static_cast<bool>(fb.output)) << where << " " << id;
        if (!fa.output) continue;
        const double va = volume(fa.output->shape()), vb = volume(fb.output->shape());
        EXPECT_NEAR(va, vb, 1e-9 * va) << where << " " << id;
        for (ElementKind k : {ElementKind::Face, ElementKind::Edge}) {
            ASSERT_EQ(fa.output->count(k), fb.output->count(k)) << where << " " << id;
            for (std::size_t i = 0; i < fa.output->count(k); ++i) {
                EXPECT_EQ(fa.output->record(k, i).name, fb.output->record(k, i).name) << where << " " << id;
            }
        }
    }
}

// Mass of the part (kernel mass properties of the final shape) against the analytic volume
// times the density: kg = mm³ · 1e-9 · kg/m³.
void expectMass(const ReferencePart& part, const TopoDS_Shape& shape) {
    auto mp = bendcore::kernel::computeMassProperties(bendcore::kernel::Shape(shape), part.density);
    ASSERT_TRUE(mp.ok()) << mp.error().message;
    const double expected = part.volume(part.base) * 1e-9 * part.density;
    EXPECT_NEAR(mp.value().mass, expected, 1e-6 * expected) << part.name;
    EXPECT_EQ(mp.value().bodyCount, part.bodies) << part.name;
}

std::size_t solidCount(const TopoDS_Shape& shape) {
    std::size_t n = 0;
    for (TopExp_Explorer ex(shape, TopAbs_SOLID); ex.More(); ex.Next()) ++n;
    return n;
}

// STEP of the final shape: written, read back, volume and number of solids against the formula.
// With BENDCORE_WRITE_REFERENCE_STEP set, the file is also written to
// tests/corpus/reference_parts/ (docs/modeling/REFERENCE_PARTS.md, "STEP для сверки").
std::string stepRoundTrip(const ReferencePart& part, const TopoDS_Shape& shape) {
    auto text = bendcore::kernel::writeStep(bendcore::kernel::Shape(shape));
    EXPECT_TRUE(text.ok()) << (text.ok() ? "" : text.error().message);
    if (!text.ok()) return {};
    auto back = bendcore::kernel::readStep(text.value());
    EXPECT_TRUE(back.ok()) << (back.ok() ? "" : back.error().message);
    if (!back.ok()) return {};
    const double expected = part.volume(part.base);
    EXPECT_NEAR(volume(back.value().occt()), expected, 1e-6 * expected) << part.name << " (STEP)";
    EXPECT_EQ(solidCount(back.value().occt()), part.bodies) << part.name << " (STEP)";
    return text.value();
}

std::string readFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream out;
    out << in.rdbuf();
    return out.str();
}

}  // namespace

TEST(ModelingReferenceParts, BuildChangeSaveLoadUndo) {
    Tally tally;
    int scenarioCount = 0;
    const auto parts = allParts();
    ASSERT_GE(parts.size(), 25u);
    const bool writeStep = std::getenv("BENDCORE_WRITE_REFERENCE_STEP") != nullptr;
    nlohmann::json manifest = nlohmann::json::array();
    for (const ReferencePart& part : parts) {
        SCOPED_TRACE(part.name);
        PartDocument doc(partRegistry());
        setValues(doc, part.base);
        part.build(doc);
        RebuildReport rep = doc.rebuild();
        ASSERT_TRUE(rep.failed.empty()) << part.name << ": " << rep.failed.front() << ": "
                                        << messages(info(doc, rep.failed.front()));
        for (const auto& id : doc.featureIds()) {
            EXPECT_EQ(info(doc, id).state, FeatureState::Ok) << part.name << " " << id << ": " << messages(info(doc, id));
        }
        const double v0 = volume(doc.finalShape().shape());
        tallyReferences(doc, part.name + " / base", volumeMatches(doc, part.volume(part.base), part.name), tally);
        ++scenarioCount;
        EXPECT_EQ(doc.bodies().size(), part.bodies) << part.name;
        expectMass(part, doc.finalShape().shape());
        const std::string step = stepRoundTrip(part, doc.finalShape().shape());
        if (writeStep && !step.empty()) {
            const std::string file = stepFileName(part.name);
            std::ofstream(std::string(BENDCORE_REFERENCE_PARTS_DIR) + "/" + file, std::ios::binary) << step;
            manifest.push_back({{"name", part.name},
                                {"file", file},
                                {"material", part.material},
                                {"density_kg_m3", part.density},
                                {"volume_mm3", part.volume(part.base)},
                                {"mass_kg", part.volume(part.base) * 1e-9 * part.density},
                                {"bodies", part.bodies},
                                {"parameters", part.base}});
        }

        // Dimension and parameter changes.
        for (const auto& [name, changes] : part.scenarios) {
            Values v = part.base;
            for (const auto& [k, x] : changes) v[k] = x;
            setValues(doc, v);
            doc.rebuild();
            const std::string where = part.name + " / " + name;
            tallyReferences(doc, where, volumeMatches(doc, part.volume(v), where), tally);
            ++scenarioCount;
        }
        setValues(doc, part.base);
        doc.rebuild();
        tallyReferences(doc, part.name + " / back to base", volumeMatches(doc, part.volume(part.base), part.name + " back"), tally);
        ++scenarioCount;
        EXPECT_EQ(volume(doc.finalShape().shape()), v0) << "the pool returns the base results exactly";

        // Save -> load, without and with the BRep cache.
        for (bool cache : {false, true}) {
            auto text = doc.toJson({.includeCache = cache});
            ASSERT_TRUE(text.ok()) << text.error().message;
            RebuildReport lrep;
            auto loaded = PartDocument::fromJson(text.value(), partRegistry(), &lrep);
            ASSERT_TRUE(loaded.ok()) << loaded.error().message;
            EXPECT_TRUE(lrep.failed.empty());
            if (cache) EXPECT_FALSE(lrep.fromCache.empty());
            expectSameState(doc, loaded.value(), part.name + (cache ? " (cache)" : " (no cache)"));
            // The loaded document stays parametric.
            const auto& [sname, schanges] = part.scenarios.front();
            Values v = part.base;
            for (const auto& [k, x] : schanges) v[k] = x;
            setValues(loaded.value(), v);
            loaded.value().rebuild();
            volumeMatches(loaded.value(), part.volume(v), part.name + " loaded / " + sname);
        }

        // Undo / redo: one transaction with a dimension change, then a structural edit.
        doc.clearHistory();
        const auto& [sname, schanges] = part.scenarios.front();
        ASSERT_TRUE(doc.beginTransaction(sname).ok());
        Values v = part.base;
        for (const auto& [k, x] : schanges) v[k] = x;
        setValues(doc, v);
        ASSERT_TRUE(doc.commitTransaction().ok());
        doc.rebuild();
        const double v1 = volume(doc.finalShape().shape());
        ASSERT_EQ(doc.undoHistory().size(), 1u);
        auto u = doc.undo();
        ASSERT_TRUE(u.ok());
        EXPECT_EQ(volume(doc.finalShape().shape()), v0) << "undo restores the exact volume";
        EXPECT_TRUE(u.value().recomputed.empty()) << "results come from the pool";
        auto r = doc.redo();
        ASSERT_TRUE(r.ok());
        EXPECT_EQ(volume(doc.finalShape().shape()), v1) << "redo restores the exact volume";
        const std::string last = doc.featureIds().back();
        ASSERT_TRUE(doc.removeFeature(last).ok());
        doc.rebuild();
        EXPECT_NE(volume(doc.finalShape().shape()), v1);
        ASSERT_TRUE(doc.undo().ok());
        EXPECT_EQ(doc.featureIds().back(), last);
        EXPECT_EQ(volume(doc.finalShape().shape()), v1) << "undo of the removal";
        ASSERT_TRUE(doc.undo().ok());
        EXPECT_EQ(volume(doc.finalShape().shape()), v0);
        EXPECT_FALSE(doc.canUndo());
        std::printf("[reference part] %-30s volume %.6f mm3, %zu features, %zu scenarios\n", part.name.c_str(), v0,
                    doc.size(), part.scenarios.size() + 2);
    }
    const double rate = static_cast<double>(tally.survived) / tally.total;
    std::printf("[reference parts] parts: %zu, scenarios: %d, references checked: %d, survived: %d, rate: %.1f %%\n",
                parts.size(), scenarioCount, tally.total, tally.survived, 100.0 * rate);
    RecordProperty("survival_rate_percent", std::to_string(100.0 * rate));
    EXPECT_GE(rate, 0.95);
    if (writeStep) {
        std::ofstream(std::string(BENDCORE_REFERENCE_PARTS_DIR) + "/manifest.json") << manifest.dump(2) << "\n";
    }
}

// The committed STEP files of tests/corpus/reference_parts/ (for the mass comparison in another
// CAD system): one per part, read back with the volume, the mass and the number of bodies of
// the analytic formula; the manifest lists the same values.
TEST(ModelingReferenceParts, CommittedStepFilesMatchFormulas) {
    const std::string dir = BENDCORE_REFERENCE_PARTS_DIR;
    const std::string text = readFile(dir + "/manifest.json");
    ASSERT_FALSE(text.empty()) << dir << "/manifest.json";
    const nlohmann::json manifest = nlohmann::json::parse(text);
    const auto parts = allParts();
    ASSERT_EQ(manifest.size(), parts.size());
    for (const ReferencePart& part : parts) {
        SCOPED_TRACE(part.name);
        const auto entry = std::find_if(manifest.begin(), manifest.end(), [&](const nlohmann::json& e) {
            return e.at("name").get<std::string>() == part.name;
        });
        ASSERT_NE(entry, manifest.end());
        const double expected = part.volume(part.base);
        EXPECT_EQ((*entry).at("file").get<std::string>(), stepFileName(part.name));
        EXPECT_NEAR((*entry).at("volume_mm3").get<double>(), expected, 1e-9 * expected);
        EXPECT_NEAR((*entry).at("mass_kg").get<double>(), expected * 1e-9 * part.density, 1e-9 * expected * 1e-9 * part.density);
        auto shape = bendcore::kernel::readStep(readFile(dir + "/" + stepFileName(part.name)));
        ASSERT_TRUE(shape.ok()) << shape.error().message;
        EXPECT_NEAR(volume(shape.value().occt()), expected, 1e-6 * expected);
        EXPECT_EQ(solidCount(shape.value().occt()), part.bodies);
        auto mp = bendcore::kernel::computeMassProperties(shape.value(), part.density);
        ASSERT_TRUE(mp.ok()) << mp.error().message;
        EXPECT_NEAR(mp.value().mass, expected * 1e-9 * part.density, 1e-6 * expected * 1e-9 * part.density);
    }
}
