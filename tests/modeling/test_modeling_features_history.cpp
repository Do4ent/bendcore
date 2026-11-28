// История операций: Generated/Modified/IsRemoved после выдавливания и выреза, сценарий
// «изменили размер эскиза → перестроение по истории сохранило отверстие и скругление».

#include "features_test_util.hpp"

#include <BRepTools_WireExplorer.hxx>
#include <gtest/gtest.h>

using namespace ft;

namespace {

constexpr double kPi = 3.14159265358979323846;

// Образы элемента s в shape: сам s (если он там есть) или его Modified.
std::vector<TopoDS_Shape> trace(const FeatureResult& r, const TopoDS_Shape& s) {
    if (contains(r.shape, s)) return {s};
    std::vector<TopoDS_Shape> out;
    for (const auto& m : r.history->Modified(s))
        if (contains(r.shape, m)) out.push_back(m);
    return out;
}

std::vector<TopoDS_Shape> generated(const FeatureResult& r, const TopoDS_Shape& s, TopAbs_ShapeEnum type) {
    std::vector<TopoDS_Shape> out;
    for (const auto& g : r.history->Generated(s))
        if (g.ShapeType() == type && contains(r.shape, g)) out.push_back(g);
    return out;
}

int wires(const TopoDS_Shape& face) { return count(face, TopAbs_WIRE); }

}  // namespace

TEST(History, ExtrudeCapsAndSidesFromProfile) {
    const Profile p = rect(0, 0, 40, 30);
    ExtrudeParams ep;
    ep.distance = 10;
    auto r = extrude(p, {}, ep);
    ASSERT_TRUE(r.ok()) << r.error().message;
    const FeatureResult& f = r.value();

    const auto top = generated(f, p.face, TopAbs_FACE);
    ASSERT_EQ(top.size(), 1u);
    EXPECT_NEAR(faceCentroid(top[0]).Z(), 10.0, 1e-9);
    const auto bottom = trace(f, p.face);
    ASSERT_EQ(bottom.size(), 1u);
    EXPECT_NEAR(faceCentroid(bottom[0]).Z(), 0.0, 1e-9);

    // Каждое ребро профиля порождает одну боковую грань, проходящую через середину ребра.
    int edges = 0;
    for (TopExp_Explorer ex(p.face, TopAbs_EDGE); ex.More(); ex.Next(), ++edges) {
        const auto sides = generated(f, ex.Current(), TopAbs_FACE);
        ASSERT_EQ(sides.size(), 1u);
        GProp_GProps lp;
        BRepGProp::LinearProperties(ex.Current(), lp);
        const gp_Pnt mid = lp.CentreOfMass();
        const gp_Pnt c = faceCentroid(sides[0]);
        EXPECT_NEAR(c.X(), mid.X(), 1e-9);
        EXPECT_NEAR(c.Y(), mid.Y(), 1e-9);
        EXPECT_NEAR(c.Z(), 5.0, 1e-9);
    }
    EXPECT_EQ(edges, 4);

    // Вершина профиля порождает боковое ребро.
    TopExp_Explorer vx(p.face, TopAbs_VERTEX);
    EXPECT_EQ(generated(f, vx.Current(), TopAbs_EDGE).size(), 1u);
}

TEST(History, BossOnTargetKeepsProfileLinks) {
    const TopoDS_Shape b = box(50, 50, 10);
    const Profile p = rect(10, 10, 20, 30, 10);
    ExtrudeParams ep;
    ep.distance = 15;
    auto r = extrude(p, b, ep);
    ASSERT_TRUE(r.ok()) << r.error().message;
    const auto top = generated(r.value(), p.face, TopAbs_FACE);
    ASSERT_EQ(top.size(), 1u);
    EXPECT_NEAR(faceCentroid(top[0]).Z(), 25.0, 1e-9);
    // Верх коробки изменён (к нему примыкает бобышка), низ остался прежним.
    const TopoDS_Face boxTop = planeFaceAt(b, 2, 10);
    const TopoDS_Face boxBottom = planeFaceAt(b, 2, 0);
    EXPECT_FALSE(r.value().history->IsRemoved(boxTop));
    EXPECT_EQ(trace(r.value(), boxTop).size(), 1u);
    EXPECT_FALSE(contains(r.value().shape, boxTop));
    EXPECT_TRUE(contains(r.value().shape, boxBottom));
    EXPECT_TRUE(r.value().history->Modified(boxBottom).IsEmpty());
    EXPECT_FALSE(r.value().history->IsRemoved(boxBottom));
}

TEST(History, CutPocketModifiesTopAndGeneratesFloor) {
    const TopoDS_Shape b = box(100, 60, 20);
    const Profile p = rect(10, 10, 40, 30, 20);
    ExtrudeParams ep;
    ep.mode = FeatureMode::Cut;
    ep.distance = 5;
    ep.reversed = true;
    auto r = extrude(p, b, ep);
    ASSERT_TRUE(r.ok()) << r.error().message;
    const FeatureResult& f = r.value();

    const TopoDS_Face top = planeFaceAt(b, 2, 20);
    EXPECT_FALSE(f.history->IsRemoved(top));
    const auto topImg = f.history->Modified(top);
    ASSERT_EQ(topImg.Size(), 1);
    EXPECT_EQ(wires(topImg.First()), 2);  // наружный контур и контур кармана
    EXPECT_NEAR(area(topImg.First()), 100 * 60 - 30 * 20, 1e-6);

    const TopoDS_Face bottom = planeFaceAt(b, 2, 0);
    EXPECT_TRUE(contains(f.shape, bottom));
    EXPECT_TRUE(f.history->Modified(bottom).IsEmpty());
    EXPECT_FALSE(f.history->IsRemoved(bottom));

    // Дно кармана — из профиля, стенки — из рёбер профиля.
    const auto floor = generated(f, p.face, TopAbs_FACE);
    ASSERT_EQ(floor.size(), 1u);
    EXPECT_NEAR(faceCentroid(floor[0]).Z(), 15.0, 1e-9);
    for (TopExp_Explorer ex(p.face, TopAbs_EDGE); ex.More(); ex.Next())
        EXPECT_EQ(generated(f, ex.Current(), TopAbs_FACE).size(), 1u);
    // Начальный торец инструмента (в плоскости верха) в результат не вошёл.
    EXPECT_TRUE(trace(f, p.face).empty());
}

TEST(History, CutRemovingWholeFace) {
    const TopoDS_Shape b = box(100, 60, 20);
    const Profile p = rect(-10, -10, 110, 70, 20);  // шире тела: срезает весь верх
    ExtrudeParams ep;
    ep.mode = FeatureMode::Cut;
    ep.distance = 2;
    ep.reversed = true;
    auto r = extrude(p, b, ep);
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_NEAR(volume(r.value().shape), 100 * 60 * 18, 1e-6);
    const TopoDS_Face top = planeFaceAt(b, 2, 20);
    EXPECT_TRUE(r.value().history->IsRemoved(top));
    EXPECT_TRUE(r.value().history->Modified(top).IsEmpty());
    const auto newTop = generated(r.value(), p.face, TopAbs_FACE);
    ASSERT_EQ(newTop.size(), 1u);
    EXPECT_NEAR(faceCentroid(newTop[0]).Z(), 18.0, 1e-9);
    EXPECT_NEAR(area(newTop[0]), 100 * 60, 1e-6);
    // Боковые грани коробки укорочены — Modified.
    const TopoDS_Face side = planeFaceAt(b, 0, 0);
    ASSERT_EQ(r.value().history->Modified(side).Size(), 1);
    EXPECT_NEAR(area(r.value().history->Modified(side).First()), 60 * 18, 1e-6);
}

// Деталь строится рецептом, который ссылается на элементы только через историю и элементы
// эскиза: верхняя грань = Generated(грань профиля), ребро скругления = Generated(вершина профиля).
struct Rebuilt {
    TopoDS_Shape shape;
    bool referencesResolved = false;
};

Rebuilt buildPart(double width) {
    Rebuilt out;
    const Profile p = rect(0, 0, width, 40);
    // Вершина эскиза (width, 40) — третья вершина контура, найденная по порядку обхода.
    TopoDS_Vertex corner;
    for (BRepTools_WireExplorer we(TopoDS::Wire(TopExp_Explorer(p.face, TopAbs_WIRE).Current())); we.More(); we.Next())
        if (BRep_Tool::Pnt(we.CurrentVertex()).Distance(gp_Pnt(width, 40, 0)) < 1e-9) corner = we.CurrentVertex();
    if (corner.IsNull()) return out;

    ExtrudeParams ep;
    ep.distance = 10;
    auto base = extrude(p, {}, ep);
    if (!base.ok()) return out;
    const auto tops = generated(base.value(), p.face, TopAbs_FACE);
    const auto vEdges = generated(base.value(), corner, TopAbs_EDGE);
    if (tops.size() != 1 || vEdges.size() != 1) return out;

    HoleParams hp;
    hp.diameter = 8;
    hp.end = HoleEnd::Through;
    auto h = hole(base.value().shape, TopoDS::Face(tops[0]), gp_Pnt(width / 2, 20, 10), hp);
    if (!h.ok()) return out;
    const auto edgeNow = trace(h.value(), vEdges[0]);
    if (edgeNow.size() != 1) return out;

    auto f = fillet(h.value().shape, edgeNow, 3.0);
    if (!f.ok()) return out;
    out.shape = f.value().shape;
    out.referencesResolved = true;
    return out;
}

TEST(History, SketchDimensionChangeRebuildKeepsDependentFeatures) {
    for (double width : {60.0, 90.0, 35.0}) {
        const Rebuilt part = buildPart(width);
        ASSERT_TRUE(part.referencesResolved) << "width " << width;
        EXPECT_TRUE(valid(part.shape));
        const double expected = width * 40 * 10 - kPi * 16 * 10 - (1 - kPi / 4) * 9 * 10;
        EXPECT_NEAR(volume(part.shape), expected, 1e-5) << "width " << width;
        EXPECT_EQ(count(part.shape, TopAbs_FACE), 6 + 1 + 1);  // + стенка отверстия + скругление
    }
}
