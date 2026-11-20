// Зубчатый венец в документе детали: заготовка (эскиз-окружность ⌀da → выдавливание), венец на
// боковой грани заготовки, отверстие ступицы на торце. Размеры эскиза (da = m(z + 2)), высота
// заготовки, m и z меняются; венец и зависимое отверстие перестраиваются, объём — площадь
// торцового профиля по формулам × H минус отверстие; атрибуты «gear.*»; сохранение и загрузка.

#include "modeling_document_test_util.hpp"

#include <bendcore/modeling/gear.hpp>

#include <BRepAdaptor_Surface.hxx>
#include <TopExp_Explorer.hxx>

#include <map>

using namespace bendcore::modeling;
using namespace bendcore::modeling::testutil;

namespace {

using Values = std::map<std::string, double>;

// Площадь торцового профиля по точкам аналитических кривых (не B-сплайнов).
double profileArea(const GearSpec& s) {
    auto side = toothSidePoints(s, 4000);
    EXPECT_TRUE(side.ok());
    if (!side.ok()) return 0.0;
    std::vector<std::pair<double, double>> poly{{0.0, 0.0}};
    for (const auto* part : {&side.value().root, &side.value().fillet, &side.value().involute, &side.value().tipHalf})
        poly.insert(poly.end(), part->begin(), part->end());
    double a = 0.0;
    for (std::size_t i = 0; i < poly.size(); ++i) {
        const auto& p = poly[i];
        const auto& q = poly[(i + 1) % poly.size()];
        a += p.first * q.second - q.first * p.second;
    }
    return s.teeth * std::abs(a);
}

// Объём фаски c × 45° по кромке вершин на одном торце: z·∫ (r − ra + c)·2ψ(r)·r dr,
// ψ(r) = π/(2z) + inv αt − inv α(r) (прямой зуб, x = 0), Симпсон по r ∈ [ra − c, ra].
double chamferVolume(const GearSpec& s, double c) {
    const GearGeometry g = gearGeometry(s).value();
    const double ra = g.da / 2, rb = g.db / 2;
    const auto inv = [](double a) { return std::tan(a) - a; };
    const auto f = [&](double r) { return (r - ra + c) * 2 * (kPi / (2 * s.teeth) + inv(g.alphaT) - inv(std::acos(rb / r))) * r; };
    const int n = 2000;
    double sum = f(ra - c) + f(ra);
    for (int i = 1; i < n; ++i) sum += (i % 2 ? 4 : 2) * f(ra - c + i * c / n);
    return s.teeth * sum * c / n / 3;
}

TopoDS_Shape cylinderFace(const TopoDS_Shape& shape, double r) {
    for (TopExp_Explorer ex(shape, TopAbs_FACE); ex.More(); ex.Next()) {
        BRepAdaptor_Surface s(TopoDS::Face(ex.Current()), false);
        if (s.GetType() == GeomAbs_Cylinder && std::abs(s.Cylinder().Radius() - r) < 1e-7) return ex.Current();
    }
    return {};
}

}  // namespace

TEST(DocumentGear, SketchChangeKeepsGearAndHubBore) {
    PartDocument doc(partRegistry());
    for (const auto& [n, v] : Values{{"m", 2}, {"z", 20}, {"H", 16}}) setVar(doc, n, v);
    ASSERT_TRUE(doc.setVariable("D", "m * (z + 2)").ok());
    FeatureSpec sk;
    sk.type = "Sketch";
    sk.id = "Blank";
    sk.options = {{"plane", "XY"}};
    sk.sketch = centredCircle("D / 2");
    add(doc, sk);
    add(doc, {"Extrude", "Disc", "", {{"distance", "H"}}, {}, {}, false, {{"sketch", "Blank"}}});
    doc.rebuild();
    ASSERT_EQ(info(doc, "Disc").state, FeatureState::Ok) << messages(info(doc, "Disc"));
    NamedShape s0 = doc.finalShape();
    const TopoDS_Shape rim = cylinderFace(s0.shape(), 22.0);
    ASSERT_FALSE(rim.IsNull());
    add(doc, {"GearTeeth", "Rim", "", {{"module", "m"}, {"teeth", "z"}}, {{"face", {refTo(s0, rim)}}}, {{"axis", "face"}}, false});
    doc.rebuild();
    auto g = info(doc, "Rim");
    ASSERT_EQ(g.state, FeatureState::Ok) << messages(g);
    NamedShape s1 = doc.finalShape();
    const TopoDS_Shape top = planarFace(s1.shape(), gp::DZ(), 16.0);
    ASSERT_FALSE(top.IsNull());
    add(doc, {"Hole", "Bore", "", {{"diameter", "10"}, {"x", "0"}, {"y", "0"}, {"z", "H"}}, {{"face", {refTo(s1, top)}}},
              {{"end", "through"}}, false});

    const std::vector<Values> scenarios = {{{"m", 2}, {"z", 20}, {"H", 16}}, {{"m", 2.5}, {"z", 20}, {"H", 16}},
                                           {{"m", 2}, {"z", 31}, {"H", 12}}, {{"m", 1.5}, {"z", 40}, {"H", 25}},
                                           {{"m", 2}, {"z", 20}, {"H", 16}}};
    for (const auto& v : scenarios) {
        for (const auto& [n, x] : v) setVar(doc, n, x);
        doc.rebuild();
        const std::string where = "m=" + num(v.at("m")) + " z=" + num(v.at("z")) + " H=" + num(v.at("H"));
        ASSERT_EQ(info(doc, "Rim").state, FeatureState::Ok) << where << ": " << messages(info(doc, "Rim"));
        ASSERT_EQ(info(doc, "Bore").state, FeatureState::Ok) << where << ": " << messages(info(doc, "Bore"));
        GearSpec spec;
        spec.module = v.at("m");
        spec.teeth = static_cast<int>(v.at("z"));
        spec.width = v.at("H");
        // Фаски зубьев 0,5m × 45° на обоих торцах (параметр chamfer по умолчанию).
        const double expected = profileArea(spec) * v.at("H") - kPi * 25.0 * v.at("H") - 2 * chamferVolume(spec, 0.5 * spec.module);
        const NamedShape fin = doc.finalShape();
        EXPECT_NEAR(volume(fin.shape()), expected, 3e-6 * expected) << where;
        const auto gears = gearsOf(fin);
        ASSERT_EQ(gears.size(), 1u) << where;
        EXPECT_EQ(gears.front().gear.teeth, spec.teeth);
        EXPECT_DOUBLE_EQ(gears.front().gear.module, spec.module);
        EXPECT_NEAR(gears.front().gear.width, v.at("H"), 1e-9);
        EXPECT_TRUE(gears.front().gear.axis.Direction().IsParallel(gp::DZ(), 1e-12));
        // Боковые грани зубьев (без вершин): 5 на зуб и 2 конуса фасок.
        EXPECT_EQ(gears.front().faces.size(), 7u * spec.teeth) << where;
        EXPECT_TRUE(hasCylinder(fin.shape(), 5.0, 2, 0, 0)) << where;
    }
    // Сохранение и загрузка: тот же объём и тот же венец.
    auto json = doc.toJson();
    ASSERT_TRUE(json.ok());
    auto back = PartDocument::fromJson(json.value(), partRegistry());
    ASSERT_TRUE(back.ok()) << back.error().message;
    EXPECT_NEAR(volume(back.value().finalShape().shape()), volume(doc.finalShape().shape()), 1e-9);
    EXPECT_EQ(gearsOf(back.value().finalShape()).size(), 1u);
}

TEST(DocumentGear, HelicalNewBodyAndErrors) {
    PartDocument doc(partRegistry());
    add(doc, {"GearTeeth", "G", "", {{"module", "1.5"}, {"teeth", "17"}, {"width", "12"}, {"helix", "15"}},
              {}, {{"mode", "newBody"}, {"axis", "Y"}, {"hand", "left"}}, false});
    doc.rebuild();
    auto g = info(doc, "G");
    // Фаска по умолчанию (0,5m) на косом зубе не строится — предупреждение.
    ASSERT_EQ(g.state, FeatureState::Warning) << messages(g);
    EXPECT_NE(messages(g).find("chamfer is not built on helical teeth"), std::string::npos);
    GearSpec spec;
    spec.module = 1.5;
    spec.teeth = 17;
    spec.helix = 15 * kPi / 180;
    const double expected = profileArea(spec) * 12;
    EXPECT_NEAR(volume(doc.finalShape().shape()), expected, 3e-5 * expected);
    const auto gears = gearsOf(doc.finalShape());
    ASSERT_EQ(gears.size(), 1u);
    EXPECT_EQ(gears.front().gear.hand, GearHand::Left);
    EXPECT_EQ(gears.front().faces.size(), 6u * 17 + 2);
    // Нецелое z и нарезание без заготовки — ошибки.
    PartDocument bad(partRegistry());
    add(bad, {"GearTeeth", "G", "", {{"teeth", "17.5"}, {"width", "10"}}, {}, {{"mode", "newBody"}, {"axis", "Z"}}, false});
    add(bad, {"GearTeeth", "H", "", {{"width", "10"}}, {}, {{"mode", "cut"}, {"axis", "Z"}}, false});
    bad.rebuild();
    EXPECT_EQ(info(bad, "G").state, FeatureState::Error);
    EXPECT_EQ(info(bad, "H").state, FeatureState::Error);
}
