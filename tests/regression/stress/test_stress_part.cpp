// Нагрузка: деталь из 300 операций в документе детали (ROADMAP_GENERAL_CAD.md, фаза 5, п. 1).
//
// Дерево: эскиз прямоугольника L × W на плоскости XY и выдавливание на H (2 операции), затем
// 149 пар «эскиз окружности радиуса r на XY + вырез насквозь» — сетка 15 × 10 без последнего
// узла. Всего 2 + 2·149 = 300 операций. Изменение первого эскиза (переменная L, от которой
// зависит прямоугольник) заставляет перестроить эскиз, выдавливание и все 149 вырезов; эскизы
// окружностей тела не видят (нет ссылок) и переиспользуются. Изменение последней операции
// (радиус последнего отверстия, переменная RL) перестраивает только её эскиз и вырез;
// перестроение без изменений ничего не пересчитывает.
//
// Эталон — аналитика: объём L·W·H − 148·π·r²·H − π·RL²·H, число цилиндрических граней 149,
// габариты [0, L] × [0, W] × [0, H]; ни одна операция не в ошибке.

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
#include <vector>

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

void expectPlate(const PartDocument& doc, double length, double lastR = kR) {
    const Measured m = measure(doc.finalShape().shape());
    const double expected = length * kW * kH - (kHoles - 1) * kPi * kR * kR * kH - kPi * lastR * lastR * kH;
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
    ASSERT_TRUE(doc.setVariable("RL", num(kR)).ok());

    SketchBuilder base;
    base.fixedPolygon({{"0", "0"}, {"L", "0"}, {"L", "W"}, {"0", "W"}});
    addOrFail(doc, sketchOnXY("BaseSketch", base.build()));
    addOrFail(doc, extrude("Base", "BaseSketch", "boss", "blind", num(kH)));
    int n = 0;
    for (int j = 0; j < kRows; ++j) {
        for (int i = 0; i < kCols && n < kHoles; ++i, ++n) {
            SketchBuilder c;
            c.fixedCircle(num(kStart + kPitch * i), num(kStart + kPitch * j), n == kHoles - 1 ? "RL" : num(kR));
            const std::string id = "Hole" + std::to_string(n);
            addOrFail(doc, sketchOnXY(id + "Sketch", c.build()));
            addOrFail(doc, extrude(id, id + "Sketch", "cut", "throughAllBoth", ""));
        }
    }
    ASSERT_EQ(doc.size(), 300u);

    RebuildReport rep;
    timed("part_300", "initial_rebuild", 60, [&] { rep = doc.rebuild(); });
    EXPECT_TRUE(rep.failed.empty()) << rep.failed.size() << " failed, first " << rep.failed.front();
    EXPECT_EQ(rep.recomputed.size(), 300u);
    expectPlate(doc, 130);

    // Изменение первого эскиза: прямоугольник длиннее на 10 мм — перестраиваются выдавливание и
    // все вырезы (тело изменилось), эскизы окружностей переиспользуются.
    ASSERT_TRUE(doc.setVariable("L", "140").ok());
    timed("part_300", "rebuild_after_sketch_edit", 60, [&] { rep = doc.rebuild(); });
    EXPECT_TRUE(rep.failed.empty());
    EXPECT_EQ(rep.recomputed.size(), std::size_t(2 + kHoles)) << "base sketch, base and every cut";
    EXPECT_EQ(rep.reused.size(), std::size_t(kHoles)) << "hole sketches do not depend on the body";
    for (const auto& id : rep.reused) EXPECT_NE(id.find("Sketch"), std::string::npos) << id;
    expectPlate(doc, 140);

    // Изменение последней операции: перестраиваются только эскиз и вырез последнего отверстия.
    ASSERT_TRUE(doc.setVariable("RL", "2").ok());
    timed("part_300", "rebuild_after_last_edit", 3, [&] { rep = doc.rebuild(); });
    EXPECT_TRUE(rep.failed.empty());
    const std::string last = "Hole" + std::to_string(kHoles - 1);
    EXPECT_EQ(rep.recomputed, (std::vector<std::string>{last + "Sketch", last}));
    EXPECT_EQ(rep.reused.size(), 298u);
    expectPlate(doc, 140, 2.0);
    ASSERT_TRUE(doc.setVariable("RL", num(kR)).ok());
    timed("part_300", "rebuild_last_edit_back", 1, [&] { rep = doc.rebuild(); });
    EXPECT_TRUE(rep.recomputed.empty()) << "the previous result of the last pair comes from the pool";
    expectPlate(doc, 140);

    // Перестроение без изменений — только повторное использование результатов.
    timed("part_300", "idle_rebuild", 0.5, [&] { rep = doc.rebuild(); });
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
    // Кэш BRep хранится у операций с телом (выдавливание и 149 вырезов); эскизы пересчитываются.
    EXPECT_EQ(loadRep.fromCache.size(), std::size_t(1 + kHoles));
}
