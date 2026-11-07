// Тесты устойчивых булевых операций: эталоны — аналитические объёмы
// (docs/kernel/GEOMETRY_HEALING.md, раздел «Как тестировать»).

#include <bendcore/kernel/boolean.hpp>

#include <BRepAdaptor_Surface.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepGProp.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <BRep_Builder.hxx>
#include <GProp_GProps.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_ShapeMapHasher.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Compound.hxx>
#include <gp_Ax2.hxx>

#include <gtest/gtest.h>

#include <cmath>

using namespace bendcore::kernel;

namespace {

constexpr double kPi = 3.14159265358979323846;

double volume(const TopoDS_Shape& s) {
    GProp_GProps p;
    BRepGProp::VolumeProperties(s, p);
    return p.Mass();
}

int solids(const TopoDS_Shape& s) {
    int n = 0;
    for (TopExp_Explorer ex(s, TopAbs_SOLID); ex.More(); ex.Next()) ++n;
    return n;
}

int faces(const TopoDS_Shape& s) {
    NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> m;
    TopExp::MapShapes(s, TopAbs_FACE, m);
    return m.Extent();
}

TopoDS_Shape box(double x, double y, double z, double dx, double dy, double dz) {
    return BRepPrimAPI_MakeBox(gp_Pnt(x, y, z), dx, dy, dz).Shape();
}

TopoDS_Shape cylinder(const gp_Pnt& p, const gp_Dir& d, double r, double h) {
    return BRepPrimAPI_MakeCylinder(gp_Ax2(p, d), r, h).Shape();
}

void expectValidSolid(const BooleanResult& r, double exactVolume, double relTol) {
    EXPECT_TRUE(BRepCheck_Analyzer(r.shape).IsValid());
    EXPECT_NEAR(volume(r.shape), exactVolume, relTol * std::max(1.0, exactVolume));
    EXPECT_NEAR(r.volume, exactVolume, relTol * std::max(1.0, exactVolume));
}

}  // namespace

TEST(KernelBoolean, DefaultLadder) {
    const std::vector<double> ladder = fuzzyLadder(BooleanOptions{});
    ASSERT_EQ(ladder.size(), 5u);
    EXPECT_EQ(ladder[0], 0.0);
    EXPECT_NEAR(ladder[1], 1e-7, 1e-20);
    EXPECT_NEAR(ladder[2], 1e-6, 1e-19);
    EXPECT_NEAR(ladder[3], 1e-5, 1e-18);
    EXPECT_NEAR(ladder[4], 1e-4, 1e-17);

    BooleanOptions o;
    o.fuzzyLadder = {1e-3, 0.0, 1e-5, 1e-5};
    o.maxFuzzyValue = 1e-4;
    const std::vector<double> custom = fuzzyLadder(o);
    ASSERT_EQ(custom.size(), 2u);
    EXPECT_EQ(custom[0], 0.0);
    EXPECT_EQ(custom[1], 1e-5);
}

TEST(KernelBoolean, BoxesFuseCutCommon) {
    // A = [0,10]^3, B = [5,15]^3: overlap is [5,10]^3 = 125.
    const TopoDS_Shape a = box(0, 0, 0, 10, 10, 10);
    const TopoDS_Shape b = box(5, 5, 5, 10, 10, 10);
    auto fuse = booleanOp(BooleanType::Fuse, a, b);
    ASSERT_TRUE(fuse.ok()) << fuse.error().message;
    expectValidSolid(fuse.value(), 1875.0, 1e-9);
    EXPECT_EQ(fuse.value().fuzzyValue, 0.0);
    EXPECT_EQ(fuse.value().attempts, 1);
    EXPECT_EQ(solids(fuse.value().shape), 1);

    auto cut = booleanOp(BooleanType::Cut, a, b);
    ASSERT_TRUE(cut.ok()) << cut.error().message;
    expectValidSolid(cut.value(), 875.0, 1e-9);

    auto common = booleanOp(BooleanType::Common, a, b);
    ASSERT_TRUE(common.ok()) << common.error().message;
    expectValidSolid(common.value(), 125.0, 1e-9);
    EXPECT_EQ(faces(common.value().shape), 6);
}

TEST(KernelBoolean, CylindersAgainstAnalyticVolumes) {
    // Through hole r=3 in a 20x20x10 plate.
    const TopoDS_Shape plate = box(0, 0, 0, 20, 20, 10);
    const TopoDS_Shape drill = cylinder(gp_Pnt(10, 10, -1), gp_Dir(0, 0, 1), 3.0, 12.0);
    auto cut = booleanOp(BooleanType::Cut, plate, drill);
    ASSERT_TRUE(cut.ok()) << cut.error().message;
    expectValidSolid(cut.value(), 4000.0 - kPi * 9.0 * 10.0, 1e-7);

    // Steinmetz solid: two perpendicular cylinders of radius r intersect in 16/3 r^3.
    const double r = 4.0;
    const TopoDS_Shape cx = cylinder(gp_Pnt(-10, 0, 0), gp_Dir(1, 0, 0), r, 20.0);
    const TopoDS_Shape cy = cylinder(gp_Pnt(0, -10, 0), gp_Dir(0, 1, 0), r, 20.0);
    auto common = booleanOp(BooleanType::Common, cx, cy);
    ASSERT_TRUE(common.ok()) << common.error().message;
    expectValidSolid(common.value(), 16.0 / 3.0 * r * r * r, 1e-6);

    auto fuse = booleanOp(BooleanType::Fuse, cx, cy);
    ASSERT_TRUE(fuse.ok()) << fuse.error().message;
    expectValidSolid(fuse.value(), 2.0 * kPi * r * r * 20.0 - 16.0 / 3.0 * r * r * r, 1e-6);

    // Coaxial boss on a box top: volumes simply add.
    const TopoDS_Shape boss = cylinder(gp_Pnt(10, 10, 10), gp_Dir(0, 0, 1), 2.0, 5.0);
    auto bossFuse = booleanOp(BooleanType::Fuse, plate, boss);
    ASSERT_TRUE(bossFuse.ok()) << bossFuse.error().message;
    expectValidSolid(bossFuse.value(), 4000.0 + kPi * 4.0 * 5.0, 1e-7);
}

TEST(KernelBoolean, ListsOfArgumentsAndTools) {
    const TopoDS_Shape plate = box(0, 0, 0, 30, 10, 5);
    std::vector<TopoDS_Shape> holes;
    for (int i = 0; i < 3; ++i) holes.push_back(cylinder(gp_Pnt(5 + 10 * i, 5, -1), gp_Dir(0, 0, 1), 2.0, 7.0));
    auto cut = booleanOp(BooleanType::Cut, {plate}, holes);
    ASSERT_TRUE(cut.ok()) << cut.error().message;
    expectValidSolid(cut.value(), 1500.0 - 3.0 * kPi * 4.0 * 5.0, 1e-7);

    // Three disjoint cubes fused with a bar crossing all of them.
    std::vector<TopoDS_Shape> cubes = {box(0, 0, 0, 2, 2, 2), box(4, 0, 0, 2, 2, 2), box(8, 0, 0, 2, 2, 2)};
    const TopoDS_Shape bar = box(-1, 0.5, 0.5, 12, 1, 1);
    BooleanOptions o;
    o.expectedSolidCount = 1;
    auto fuse = booleanOp(BooleanType::Fuse, cubes, {bar}, o);
    ASSERT_TRUE(fuse.ok()) << fuse.error().message;
    expectValidSolid(fuse.value(), 3 * 8.0 + 12.0 - 3 * 2.0, 1e-9);
    EXPECT_EQ(solids(fuse.value().shape), 1);
}

TEST(KernelBoolean, SimplifyResultMergesCoplanarFaces) {
    const TopoDS_Shape a = box(0, 0, 0, 10, 10, 10);
    const TopoDS_Shape b = box(10, 0, 0, 10, 10, 10);
    BooleanOptions raw;
    raw.simplifyResult = false;
    auto r1 = booleanOp(BooleanType::Fuse, a, b, raw);
    ASSERT_TRUE(r1.ok()) << r1.error().message;
    EXPECT_FALSE(r1.value().simplifyApplied);
    EXPECT_EQ(faces(r1.value().shape), 10);
    auto r2 = booleanOp(BooleanType::Fuse, a, b);
    ASSERT_TRUE(r2.ok()) << r2.error().message;
    EXPECT_TRUE(r2.value().simplifyApplied);
    EXPECT_EQ(faces(r2.value().shape), 6);
    expectValidSolid(r2.value(), 2000.0, 1e-9);
}

// Reproducible near-coincidence case: box B starts 1e-6 mm after box A ends (typical of data
// exported with rounding). The exact fuse (FuzzyValue = 0) is valid but returns TWO solids;
// the ladder rejects it (expectedSolidCount = 1), rejects 1e-7 as well, and at 1e-6 OCCT
// merges the faces into ONE valid solid of volume 2000 (+ the closed gap 1e-6 * 100).
TEST(KernelBoolean, FuzzyLadderClosesMicroGap) {
    const double gap = 1e-6;
    const TopoDS_Shape a = box(0, 0, 0, 10, 10, 10);
    const TopoDS_Shape b = box(10 + gap, 0, 0, 10, 10, 10);

    // Independent reference: plain exact BRepAlgoAPI_Fuse.
    BRepAlgoAPI_Fuse exact(a, b);
    ASSERT_TRUE(exact.IsDone());
    ASSERT_TRUE(BRepCheck_Analyzer(exact.Shape()).IsValid());
    ASSERT_EQ(solids(exact.Shape()), 2);

    BooleanOptions o;
    o.expectedSolidCount = 1;
    auto res = booleanOp(BooleanType::Fuse, a, b, o);
    ASSERT_TRUE(res.ok()) << res.error().message;
    const BooleanResult& r = res.value();
    EXPECT_EQ(r.attempts, 3);
    EXPECT_NEAR(r.fuzzyValue, 1e-6, 1e-18);
    EXPECT_EQ(solids(r.shape), 1);
    EXPECT_EQ(faces(r.shape), 6);  // merged by SimplifyResult
    expectValidSolid(r, 2000.0, 1e-6);
    ASSERT_GE(r.warnings.size(), 2u);
    EXPECT_NE(r.warnings[0].find("fuzzy 0: result has 2 solids"), std::string::npos) << r.warnings[0];
}

// Cut of a box by the same box shifted by 1e-6 along the diagonal: the exact operation leaves
// an L-shaped sliver 1e-6 thick (valid, volume ~3e-4); with minSolidThickness it is rejected
// and the fuzzy attempt yields the expected empty result.
TEST(KernelBoolean, SliverSolidIsRejected) {
    const double d = 1e-6;
    const TopoDS_Shape a = box(0, 0, 0, 10, 10, 10);
    const TopoDS_Shape b = box(d, d, d, 10, 10, 10);
    BRepAlgoAPI_Cut exact(a, b);
    ASSERT_TRUE(exact.IsDone());
    ASSERT_EQ(solids(exact.Shape()), 1);
    ASSERT_NEAR(volume(exact.Shape()), 3.0 * 100.0 * d, 1e-4);

    BooleanOptions o;
    o.minSolidThickness = 1e-4;
    auto res = booleanOp(BooleanType::Cut, a, b, o);
    ASSERT_TRUE(res.ok()) << res.error().message;
    EXPECT_GT(res.value().fuzzyValue, 0.0);
    EXPECT_EQ(solids(res.value().shape), 0);
    EXPECT_NEAR(res.value().volume, 0.0, 1e-12);
}

TEST(KernelBoolean, AllAttemptsFailingReturnsCollectedReasons) {
    const TopoDS_Shape a = box(0, 0, 0, 10, 10, 10);
    const TopoDS_Shape b = box(5, 5, 5, 10, 10, 10);
    BooleanOptions o;
    o.expectedSolidCount = 3;  // impossible for this pair
    auto res = booleanOp(BooleanType::Fuse, a, b, o);
    ASSERT_FALSE(res.ok());
    EXPECT_EQ(res.error().code, ErrorCode::OperationFailed);
    const std::string& msg = res.error().message;
    EXPECT_NE(msg.find("fuzzy 0:"), std::string::npos) << msg;
    EXPECT_NE(msg.find("fuzzy 0.0001:"), std::string::npos) << msg;
    EXPECT_NE(msg.find("expected 3"), std::string::npos) << msg;
}

TEST(KernelBoolean, SelfIntersectingInputIsRejected) {
    // Bow-tie polygon: its two edges cross, the prism over it self-intersects.
    BRepBuilderAPI_MakePolygon poly;
    poly.Add(gp_Pnt(0, 0, 0));
    poly.Add(gp_Pnt(10, 10, 0));
    poly.Add(gp_Pnt(10, 0, 0));
    poly.Add(gp_Pnt(0, 10, 0));
    poly.Close();
    BRepBuilderAPI_MakeFace mf(poly.Wire(), true);
    ASSERT_TRUE(mf.IsDone());
    const TopoDS_Shape bad = BRepPrimAPI_MakePrism(mf.Face(), gp_Vec(0, 0, 5)).Shape();
    auto res = booleanOp(BooleanType::Fuse, bad, box(20, 0, 0, 1, 1, 1));
    ASSERT_FALSE(res.ok());
    EXPECT_EQ(res.error().code, ErrorCode::InvalidInput);
    EXPECT_NE(res.error().message.find("argument #0"), std::string::npos) << res.error().message;
}

TEST(KernelBoolean, EmptyInputIsRejected) {
    const TopoDS_Shape a = box(0, 0, 0, 1, 1, 1);
    auto r1 = booleanOp(BooleanType::Fuse, TopoDS_Shape(), a);
    ASSERT_FALSE(r1.ok());
    EXPECT_EQ(r1.error().code, ErrorCode::InvalidInput);
    TopoDS_Compound empty;
    BRep_Builder().MakeCompound(empty);
    auto r2 = booleanOp(BooleanType::Cut, a, empty);
    ASSERT_FALSE(r2.ok());
    EXPECT_EQ(r2.error().code, ErrorCode::InvalidInput);
    auto r3 = booleanOp(BooleanType::Common, std::vector<TopoDS_Shape>{}, std::vector<TopoDS_Shape>{a});
    ASSERT_FALSE(r3.ok());
    EXPECT_EQ(r3.error().code, ErrorCode::InvalidInput);
    BooleanOptions bad;
    bad.fuzzyFactor = 1.0;
    auto r4 = booleanOp(BooleanType::Fuse, a, box(0.5, 0, 0, 1, 1, 1), bad);
    ASSERT_FALSE(r4.ok());
    EXPECT_EQ(r4.error().code, ErrorCode::InvalidInput);
}

// ---------------------------------------------------------------------------------------------
// История булевой операции (BooleanResult::history, BRepAlgoAPI_BuilderAlgo::History()).

namespace {

using ShapeMap = NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher>;

ShapeMap subShapes(const TopoDS_Shape& s, TopAbs_ShapeEnum type) {
    ShapeMap m;
    TopExp::MapShapes(s, type, m);
    return m;
}

// Все подформы любого типа (TopExp::MapShapes без типа).
ShapeMap allSubShapes(const TopoDS_Shape& s) {
    ShapeMap m;
    TopExp::MapShapes(s, m);
    return m;
}

double area(const TopoDS_Shape& s) {
    GProp_GProps p;
    BRepGProp::SurfaceProperties(s, p);
    return p.Mass();
}

gp_Pnt centroid(const TopoDS_Shape& s) {
    GProp_GProps p;
    BRepGProp::SurfaceProperties(s, p);
    return p.CentreOfMass();
}

// Плоская грань формы, перпендикулярная оси axis (0=X, 1=Y, 2=Z), с центром на координате c.
TopoDS_Shape planarFaceAt(const TopoDS_Shape& s, int axis, double c) {
    for (TopExp_Explorer ex(s, TopAbs_FACE); ex.More(); ex.Next()) {
        BRepAdaptor_Surface surf(TopoDS::Face(ex.Current()));
        if (surf.GetType() != GeomAbs_Plane) continue;
        const gp_Dir n = surf.Plane().Axis().Direction();
        if (std::abs(std::abs(n.Coord(axis + 1)) - 1.0) > 1e-12) continue;
        if (std::abs(centroid(ex.Current()).Coord(axis + 1) - c) < 1e-9) return ex.Current();
    }
    return {};
}

TopoDS_Shape faceOfType(const TopoDS_Shape& s, GeomAbs_SurfaceType type) {
    for (TopExp_Explorer ex(s, TopAbs_FACE); ex.More(); ex.Next())
        if (BRepAdaptor_Surface(TopoDS::Face(ex.Current())).GetType() == type) return ex.Current();
    return {};
}

// Согласованность истории с результатом:
// (1) каждый образ (Modified/Generated) лежит в результате;
// (2) элемент входа ровно в одном состоянии: IsRemoved, Modified (тогда его самого нет
//     в результате) или неизменён (тогда он сам есть в результате);
// (3) каждая грань результата — неизменённая грань входа или образ какого-либо элемента входа.
void expectConsistentHistory(const std::vector<TopoDS_Shape>& inputs, const BooleanResult& r) {
    ASSERT_FALSE(r.history.IsNull());
    const ShapeMap result = allSubShapes(r.shape);
    ShapeMap reached;
    for (TopAbs_ShapeEnum type : {TopAbs_SOLID, TopAbs_FACE, TopAbs_EDGE, TopAbs_VERTEX}) {
        for (const TopoDS_Shape& in : inputs) {
            const ShapeMap m = subShapes(in, type);
            for (int i = 1; i <= m.Extent(); ++i) {
                const TopoDS_Shape& s = m(i);
                const bool removed = r.history->IsRemoved(s);
                const auto& mod = r.history->Modified(s);
                const auto& gen = r.history->Generated(s);
                for (const auto* list : {&mod, &gen})
                    for (const TopoDS_Shape& img : *list) {
                        EXPECT_TRUE(result.Contains(img)) << "image outside the result";
                        reached.Add(img);
                    }
                if (removed) {
                    EXPECT_TRUE(mod.IsEmpty());
                    EXPECT_FALSE(result.Contains(s));
                } else if (!mod.IsEmpty()) {
                    EXPECT_FALSE(result.Contains(s)) << "a modified input element is still in the result";
                } else {
                    EXPECT_TRUE(result.Contains(s)) << "input element is neither removed, modified nor kept";
                    reached.Add(s);
                }
            }
        }
    }
    const ShapeMap resFaces = subShapes(r.shape, TopAbs_FACE);
    for (int i = 1; i <= resFaces.Extent(); ++i)
        EXPECT_TRUE(reached.Contains(resFaces(i))) << "result face #" << i << " is not reachable from history";
}

}  // namespace

// Сквозное отверстие r = 3 в коробке 20x20x10 (цилиндр z ∈ [-1, 11]).
TEST(KernelBooleanHistory, CutBoxByCylinder) {
    const double r = 3.0;
    const TopoDS_Shape plate = box(0, 0, 0, 20, 20, 10);
    const TopoDS_Shape drill = cylinder(gp_Pnt(10, 10, -1), gp_Dir(0, 0, 1), r, 12.0);
    for (bool simplify : {false, true}) {
        SCOPED_TRACE(simplify ? "SimplifyResult" : "raw");
        BooleanOptions o;
        o.simplifyResult = simplify;
        auto res = booleanOp(BooleanType::Cut, plate, drill, o);
        ASSERT_TRUE(res.ok()) << res.error().message;
        const BooleanResult& c = res.value();
        EXPECT_EQ(c.simplifyApplied, simplify);
        ASSERT_FALSE(c.history.IsNull());
        EXPECT_EQ(faces(c.shape), 7);
        const ShapeMap result = allSubShapes(c.shape);

        // Верх и низ коробки получили контур отверстия: Modified, по одному образу площадью
        // 400 - 9π с двумя контурами.
        for (double z : {0.0, 10.0}) {
            const TopoDS_Shape f = planarFaceAt(plate, 2, z);
            ASSERT_FALSE(f.IsNull());
            EXPECT_FALSE(c.history->IsRemoved(f));
            const auto& img = c.history->Modified(f);
            ASSERT_EQ(img.Size(), 1) << "z = " << z;
            EXPECT_NEAR(area(img.First()), 400.0 - kPi * r * r, 1e-7);
            EXPECT_EQ(subShapes(img.First(), TopAbs_WIRE).Extent(), 2);
            EXPECT_FALSE(result.Contains(f));
        }
        // Боковые грани коробки отверстием не затронуты: в результате без изменений.
        for (int axis : {0, 1})
            for (double c0 : {0.0, 20.0}) {
                const TopoDS_Shape f = planarFaceAt(plate, axis, c0);
                ASSERT_FALSE(f.IsNull());
                EXPECT_TRUE(result.Contains(f));
                EXPECT_TRUE(c.history->Modified(f).IsEmpty());
                EXPECT_FALSE(c.history->IsRemoved(f));
            }

        // Цилиндрическая грань результата — Modified из боковой грани цилиндра, высота 10.
        const TopoDS_Shape side = faceOfType(drill, GeomAbs_Cylinder);
        ASSERT_FALSE(side.IsNull());
        const auto& wall = c.history->Modified(side);
        ASSERT_EQ(wall.Size(), 1);
        EXPECT_EQ(BRepAdaptor_Surface(TopoDS::Face(wall.First())).GetType(), GeomAbs_Cylinder);
        EXPECT_NEAR(area(wall.First()), 2.0 * kPi * r * 10.0, 1e-7);
        EXPECT_TRUE(faceOfType(c.shape, GeomAbs_Cylinder).IsSame(wall.First()));

        // Торцы цилиндра (z = -1 и z = 11) вне коробки — удалены.
        for (double z : {-1.0, 11.0}) {
            const TopoDS_Shape cap = planarFaceAt(drill, 2, z);
            ASSERT_FALSE(cap.IsNull());
            EXPECT_TRUE(c.history->IsRemoved(cap)) << "z = " << z;
            EXPECT_FALSE(result.Contains(cap));
        }

        // Рёбра пересечения: по одной окружности на верхней и нижней гранях (замкнутое ребро
        // длиной 2πr), Generated одновременно из грани коробки и из боковой грани цилиндра.
        ShapeMap generatedEdges;
        for (const TopoDS_Shape& g : c.history->Generated(side))
            if (g.ShapeType() == TopAbs_EDGE) generatedEdges.Add(g);
        EXPECT_EQ(generatedEdges.Extent(), 2);
        for (double z : {0.0, 10.0}) {
            int fromBox = 0;
            for (const TopoDS_Shape& g : c.history->Generated(planarFaceAt(plate, 2, z))) {
                if (g.ShapeType() != TopAbs_EDGE) continue;
                ++fromBox;
                EXPECT_TRUE(generatedEdges.Contains(g));
                EXPECT_TRUE(result.Contains(g));
                GProp_GProps lp;
                BRepGProp::LinearProperties(g, lp);
                EXPECT_NEAR(lp.Mass(), 2.0 * kPi * r, 1e-7);
                EXPECT_NEAR(lp.CentreOfMass().Z(), z, 1e-9);
            }
            EXPECT_EQ(fromBox, 1) << "z = " << z;
        }

        // Тело коробки — Modified в тело результата.
        const ShapeMap plateSolid = subShapes(plate, TopAbs_SOLID);
        const auto& solidImg = c.history->Modified(plateSolid(1));
        ASSERT_EQ(solidImg.Size(), 1);
        EXPECT_TRUE(solidImg.First().IsSame(subShapes(c.shape, TopAbs_SOLID)(1)));

        expectConsistentHistory({plate, drill}, c);
    }
}

// Слияние соседних коробок: без упрощения 10 граней (верх, низ, перед и зад — по две),
// с SimplifyResult — 6; в обоих случаях история согласована, а при упрощении пары
// компланарных граней обеих коробок — Modified в одну и ту же грань.
TEST(KernelBooleanHistory, SimplifyResultHistoryIsMerged) {
    const TopoDS_Shape a = box(0, 0, 0, 10, 10, 10);
    const TopoDS_Shape b = box(10, 0, 0, 10, 10, 10);
    BooleanOptions raw;
    raw.simplifyResult = false;
    auto r1 = booleanOp(BooleanType::Fuse, a, b, raw);
    auto r2 = booleanOp(BooleanType::Fuse, a, b);
    ASSERT_TRUE(r1.ok()) << r1.error().message;
    ASSERT_TRUE(r2.ok()) << r2.error().message;
    ASSERT_EQ(faces(r1.value().shape), 10);
    ASSERT_EQ(faces(r2.value().shape), 6);
    ASSERT_TRUE(r2.value().simplifyApplied);
    expectConsistentHistory({a, b}, r1.value());
    expectConsistentHistory({a, b}, r2.value());

    // Общие грани x = 10 исчезли в обоих вариантах.
    for (const BooleanResult* r : {&r1.value(), &r2.value()}) {
        EXPECT_TRUE(r->history->IsRemoved(planarFaceAt(a, 0, 10)));
        EXPECT_TRUE(r->history->IsRemoved(planarFaceAt(b, 0, 10)));
    }
    const TopoDS_Shape topA = planarFaceAt(a, 2, 10), topB = planarFaceAt(b, 2, 10);
    // Без упрощения верх каждой коробки — Modified в грань той же площади 10x10: ребро x = 10
    // у верхов A и B совпадает, операция сливает пару рёбер в одно общее ребро, и OCCT
    // перестраивает ограниченные им грани (новая TopoDS_Face на той же поверхности).
    // Торцы x = 0 и x = 20 этого ребра не содержат и остаются в результате как есть.
    for (const TopoDS_Shape& top : {topA, topB}) {
        EXPECT_FALSE(subShapes(r1.value().shape, TopAbs_FACE).Contains(top));
        const auto& img = r1.value().history->Modified(top);
        ASSERT_EQ(img.Size(), 1);
        EXPECT_NEAR(area(img.First()), 100.0, 1e-9);
        EXPECT_NEAR(centroid(img.First()).X(), centroid(top).X(), 1e-9);
    }
    for (const TopoDS_Shape& end : {planarFaceAt(a, 0, 0), planarFaceAt(b, 0, 20)}) {
        EXPECT_TRUE(subShapes(r1.value().shape, TopAbs_FACE).Contains(end));
        EXPECT_TRUE(r1.value().history->Modified(end).IsEmpty());
        EXPECT_FALSE(r1.value().history->IsRemoved(end));
    }
    // С упрощением верхи обеих коробок — Modified в одну грань 20x10 (история
    // ShapeUpgrade_UnifySameDomain объединена с историей операции).
    const auto& imgA = r2.value().history->Modified(topA);
    const auto& imgB = r2.value().history->Modified(topB);
    ASSERT_EQ(imgA.Size(), 1);
    ASSERT_EQ(imgB.Size(), 1);
    EXPECT_TRUE(imgA.First().IsSame(imgB.First()));
    EXPECT_NEAR(area(imgA.First()), 200.0, 1e-9);
    EXPECT_NEAR(centroid(imgA.First()).X(), 10.0, 1e-9);
    // Торцы x = 0 и x = 20: UnifySameDomain (SafeInputMode) перестраивает тело целиком, и
    // торцы, геометрически прежние, сообщаются как Modified — по одному образу 10x10 в той же
    // плоскости; через историю они достижимы, а не потеряны.
    for (const TopoDS_Shape& end : {planarFaceAt(a, 0, 0), planarFaceAt(b, 0, 20)}) {
        const auto& img = r2.value().history->Modified(end);
        ASSERT_EQ(img.Size(), 1);
        EXPECT_NEAR(area(img.First()), 100.0, 1e-9);
        EXPECT_NEAR(centroid(img.First()).X(), centroid(end).X(), 1e-9);
    }
}

// Пустой результат (Common без перекрытия, Cut без остатка) истории не имеет.
TEST(KernelBooleanHistory, EmptyResultHasNoHistory) {
    const TopoDS_Shape a = box(0, 0, 0, 10, 10, 10);
    auto common = booleanOp(BooleanType::Common, a, box(50, 0, 0, 10, 10, 10));
    ASSERT_TRUE(common.ok()) << common.error().message;
    EXPECT_EQ(faces(common.value().shape), 0);
    EXPECT_TRUE(common.value().history.IsNull());

    auto cut = booleanOp(BooleanType::Cut, a, box(-1, -1, -1, 12, 12, 12));
    ASSERT_TRUE(cut.ok()) << cut.error().message;
    EXPECT_EQ(solids(cut.value().shape), 0);
    EXPECT_TRUE(cut.value().history.IsNull());
}
