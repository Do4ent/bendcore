// Нагрузка: деталь из 300 операций в документе детали (ROADMAP_GENERAL_CAD.md, фаза 5, п. 1).
//
// Дерево: эскиз прямоугольника L × W на плоскости XY и выдавливание на H (2 операции), затем
// 149 пар «эскиз окружности радиуса r на XY + вырез насквозь» — сетка 15 × 10 без последнего
// узла. Всего 2 + 2·149 = 300 операций. Изменение первого эскиза (переменная L, от которой
// зависит прямоугольник) заставляет перестроить все 300 операций.
//
// Эталон — аналитика: объём L·W·H − 149·π·r²·H, число цилиндрических граней 149, габариты
// [0, L] × [0, W] × [0, H]; ни одна операция не в ошибке, перестроены все.

#include "stress_util.hpp"

#include <bendcore/modeling/part_document.hpp>
#include <bendcore/modeling/part_features.hpp>
#include <bendcore/modeling/part_sketch.hpp>

#include <BRepAdaptor_Surface.hxx>
#include <BRepBndLib.hxx>
#include <BRepGProp.hxx>
#include <Bnd_Box.hxx>
#include <GProp_GProps.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>

#include <cmath>
#include <cstdio>
#include <string>

using namespace bendcore::modeling;
using bendcore::qa::stress::timed;

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr int kCols = 15, kRows = 10, kHoles = kCols * kRows - 1;
constexpr double kPitch = 8.0, kStart = 6.0, kR = 1.5, kH = 10.0, kW = 90.0;

std::string num(double v) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.17g", v);
    return buf;
}

FeatureSpec sketchOnXY(const std::string& id, SketchData data) {
    FeatureSpec s;
    s.type = "Sketch";
    s.id = id;
    s.options = {{"plane", "XY"}};
    s.sketch = std::move(data);
    return s;
}

FeatureSpec extrude(const std::string& id, const std::string& sketch, const std::string& mode,
                    const std::string& extent, const std::string& distance) {
    FeatureSpec e;
    e.type = "Extrude";
    e.id = id;
    e.options = {{"mode", mode}, {"extent", extent}};
    if (!distance.empty()) e.parameters = {{"distance", distance}};
    e.links = {{"sketch", sketch}};
    return e;
}

void addOrFail(PartDocument& doc, FeatureSpec spec) {
    auto r = doc.addFeature(std::move(spec));
    ASSERT_TRUE(r.ok()) << r.error().message;
}

struct Measured {
    double volume = 0;
    int cylinders = 0;
    double bbox[6] = {};
};

Measured measure(const TopoDS_Shape& s) {
    Measured m;
    GProp_GProps p;
    BRepGProp::VolumeProperties(s, p);
    m.volume = p.Mass();
    for (TopExp_Explorer ex(s, TopAbs_FACE); ex.More(); ex.Next())
        m.cylinders += BRepAdaptor_Surface(TopoDS::Face(ex.Current())).GetType() == GeomAbs_Cylinder;
    Bnd_Box b;
    BRepBndLib::AddOptimal(s, b, false, false);
    b.Get(m.bbox[0], m.bbox[1], m.bbox[2], m.bbox[3], m.bbox[4], m.bbox[5]);
    return m;
}

void expectPlate(const PartDocument& doc, double length) {
    const Measured m = measure(doc.finalShape().shape());
    const double expected = length * kW * kH - kHoles * kPi * kR * kR * kH;
    EXPECT_NEAR(m.volume, expected, 1e-6 * expected) << "L = " << length;
    EXPECT_EQ(m.cylinders, kHoles);
    const double want[6] = {0, 0, 0, length, kW, kH};
    for (int i = 0; i < 6; ++i) EXPECT_NEAR(m.bbox[i], want[i], 1e-4) << "bbox[" << i << "], L = " << length;
}

}  // namespace

TEST(StressPart, ThreeHundredFeaturesRebuildAfterFirstSketchEdit) {
    PartDocument doc(partRegistry());
    ASSERT_TRUE(doc.setVariable("L", "130").ok());
    ASSERT_TRUE(doc.setVariable("W", num(kW)).ok());

    SketchBuilder base;
    base.fixedPolygon({{"0", "0"}, {"L", "0"}, {"L", "W"}, {"0", "W"}});
    addOrFail(doc, sketchOnXY("BaseSketch", base.build()));
    addOrFail(doc, extrude("Base", "BaseSketch", "boss", "blind", num(kH)));
    int n = 0;
    for (int j = 0; j < kRows; ++j) {
        for (int i = 0; i < kCols && n < kHoles; ++i, ++n) {
            SketchBuilder c;
            c.fixedCircle(num(kStart + kPitch * i), num(kStart + kPitch * j), num(kR));
            const std::string id = "Hole" + std::to_string(n);
            addOrFail(doc, sketchOnXY(id + "Sketch", c.build()));
            addOrFail(doc, extrude(id, id + "Sketch", "cut", "throughAllBoth", ""));
        }
    }
    ASSERT_EQ(doc.size(), 300u);

    RebuildReport rep;
    timed("part_300", "initial_rebuild", 120, [&] { rep = doc.rebuild(); });
    EXPECT_TRUE(rep.failed.empty()) << rep.failed.size() << " failed, first " << rep.failed.front();
    EXPECT_EQ(rep.recomputed.size(), 300u);
    expectPlate(doc, 130);

    // Изменение первого эскиза: прямоугольник длиннее на 10 мм — перестраивается всё дерево.
    ASSERT_TRUE(doc.setVariable("L", "140").ok());
    timed("part_300", "rebuild_after_sketch_edit", 120, [&] { rep = doc.rebuild(); });
    EXPECT_TRUE(rep.failed.empty());
    EXPECT_EQ(rep.recomputed.size(), 300u) << "every feature depends on the first sketch";
    expectPlate(doc, 140);

    // Перестроение без изменений — только повторное использование результатов.
    timed("part_300", "idle_rebuild", 5, [&] { rep = doc.rebuild(); });
    EXPECT_TRUE(rep.recomputed.empty());
    EXPECT_EQ(rep.reused.size(), 300u);

    // Сохранение и загрузка с кэшем BRep: загрузка не пересчитывает операции.
    std::string json;
    timed("part_300", "save_with_cache", 60, [&] {
        SaveOptions o;
        o.includeCache = true;
        auto t = doc.toJson(o);
        ASSERT_TRUE(t.ok()) << t.error().message;
        json = std::move(t.value());
    });
    RebuildReport loadRep;
    timed("part_300", "load_with_cache", 120, [&] {
        auto loaded = PartDocument::fromJson(json, partRegistry(), &loadRep);
        ASSERT_TRUE(loaded.ok()) << loaded.error().message;
        expectPlate(loaded.value(), 140);
    }, "\"json_mib\":" + num(double(json.size()) / (1024.0 * 1024.0)));
    EXPECT_TRUE(loadRep.failed.empty());
    EXPECT_EQ(loadRep.fromCache.size(), 300u);
}
