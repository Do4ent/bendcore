// Центровые метки и осевые линии по всем круговым элементам вида (окружности и дуги рёбер,
// конусы, круговой массив отверстий — осевая окружность центров) на фланце, построенном
// средствами OCCT с известными размерами; обозначения материалов в сечениях ГОСТ 2.306, табл. 1.

#include "test_drawing_views_support.hpp"
#include "test_support.hpp"

#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCone.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <gp_Ax2.hxx>

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <numbers>

using namespace dvtest;
using bendcore::kernel::Shape;

namespace {

constexpr double kPi = std::numbers::pi;

TopoDS_Shape cut(const TopoDS_Shape& a, const TopoDS_Shape& b) {
    BRepAlgoAPI_Cut c(a, b);
    c.Build();
    EXPECT_TRUE(c.IsDone());
    return c.Shape();
}

TopoDS_Shape fuse(const TopoDS_Shape& a, const TopoDS_Shape& b) {
    BRepAlgoAPI_Fuse f(a, b);
    f.Build();
    EXPECT_TRUE(f.IsDone());
    return f.Shape();
}

// Фланец: диск Ø120 × 12 с центральным отверстием Ø40, ступица Ø70 высотой 15 над диском,
// 6 отверстий Ø10 на окружности Ø95 под углами 0°, 60°, …, 300°.
constexpr double kD = 120, kBore = 40, kT = 12, kHub = 70, kHubH = 15, kBC = 95, kHole = 10;
constexpr int kN = 6;

Shape flange() {
    TopoDS_Shape s = BRepPrimAPI_MakeCylinder(gp_Ax2(gp_Pnt(0, 0, 0), gp::DZ()), kD / 2, kT).Shape();
    s = fuse(s, BRepPrimAPI_MakeCylinder(gp_Ax2(gp_Pnt(0, 0, kT), gp::DZ()), kHub / 2, kHubH).Shape());
    s = cut(s, BRepPrimAPI_MakeCylinder(gp_Ax2(gp_Pnt(0, 0, -1), gp::DZ()), kBore / 2, kT + kHubH + 2).Shape());
    for (int i = 0; i < kN; ++i) {
        const double a = 2 * kPi * i / kN;
        s = cut(s, BRepPrimAPI_MakeCylinder(gp_Ax2(gp_Pnt(kBC / 2 * std::cos(a), kBC / 2 * std::sin(a), -1), gp::DZ()),
                                            kHole / 2, kT + 2)
                       .Shape());
    }
    return Shape(s);
}

const ViewFrame& topFrame() {
    static const ViewFrame f = standardFrame(defaultFrontFrame(), StandardView::Top);
    return f;
}

std::vector<const Entity*> centerEntities(const Drawing& d) {
    std::vector<const Entity*> out;
    for (const Entity& e : d.entities())
        if (e.layer == "CENTER") out.push_back(&e);
    return out;
}

}  // namespace

TEST(DrawingViewsCenters, FlangeWithCircularHolePattern) {
    const Shape s = flange();
    auto g = findCenterGeometry(s, topFrame());
    ASSERT_TRUE(g.ok()) << g.error().message;
    const CenterGeometry& cg = g.value();
    // Метки: центр фланца (по наибольшему радиусу, 60) и 6 отверстий r = 5 на окружности 47,5.
    ASSERT_EQ(cg.marks.size(), 1u + kN);
    int holes = 0;
    for (const CenterMark& m : cg.marks) {
        const double d = std::hypot(m.center.x, m.center.y);
        if (d < 1e-6) {
            EXPECT_NEAR(m.radius, kD / 2, 1e-9);
            EXPECT_FALSE(m.radial);
            continue;
        }
        ++holes;
        EXPECT_NEAR(d, kBC / 2, 1e-6);
        EXPECT_NEAR(m.radius, kHole / 2, 1e-9);
        ASSERT_TRUE(m.radial);
        EXPECT_NEAR(m.radial->x, m.center.x / d, 1e-9);
        EXPECT_NEAR(m.radial->y, m.center.y / d, 1e-9);
        // Угол отверстия — кратен 60°.
        const double k = std::atan2(m.center.y, m.center.x) / (2 * kPi / kN);
        EXPECT_NEAR(k, std::round(k), 1e-9);
    }
    EXPECT_EQ(holes, kN);
    ASSERT_EQ(cg.circles.size(), 1u);
    EXPECT_NEAR(cg.circles[0].center.x, 0, 1e-9);
    EXPECT_NEAR(cg.circles[0].center.y, 0, 1e-9);
    EXPECT_NEAR(cg.circles[0].radius, kBC / 2, 1e-6);
    EXPECT_EQ(cg.circles[0].members.size(), std::size_t(kN));

    // Вывод при 1:1 с привязкой (200, 150): осевая окружность Ø95 — штрихпунктирная тонкая;
    // у отверстий Ø10 (< 12 мм) — радиальная сплошная тонкая длиной 2·(5 + 3); у центра —
    // перекрестие штрихпунктирной, половины по 60 + вылет 2…5 мм.
    auto d = Drawing::create(gostSheet(GostFormat::A3, 1, Orientation::Landscape).value()).value();
    const SheetView v{projectShape(s, topFrame()).value(), 1.0, {200, 150}};
    ASSERT_TRUE(renderCenterLines(d, v, cg).ok());
    int pitch = 0, radial = 0, cross = 0;
    for (const Entity* e : centerEntities(d)) {
        if (const auto* c = std::get_if<Circle>(&e->geometry)) {
            EXPECT_EQ(e->lineType, LineType::DashDotThin);
            EXPECT_NEAR(c->center.x, 200, 1e-9);
            EXPECT_NEAR(c->center.y, 150, 1e-9);
            EXPECT_NEAR(c->radius, kBC / 2, 1e-6);
            ++pitch;
            continue;
        }
        const auto* l = std::get_if<LineSeg>(&e->geometry);
        ASSERT_NE(l, nullptr);
        const Point2 m{(l->a.x + l->b.x) / 2 - 200, (l->a.y + l->b.y) / 2 - 150};
        if (e->lineType == LineType::SolidThin) {
            // Радиальная линия: через центр отверстия, направлена к центру фланца.
            EXPECT_NEAR(std::hypot(m.x, m.y), kBC / 2, 1e-6);
            const double dx = l->b.x - l->a.x, dy = l->b.y - l->a.y;
            EXPECT_NEAR(std::abs(dx * m.y - dy * m.x) / std::hypot(dx, dy), 0.0, 1e-6);
            EXPECT_NEAR(std::hypot(dx, dy), 2 * (kHole / 2 + 3), 1e-9);
            ++radial;
        } else {
            EXPECT_EQ(e->lineType, LineType::DashDotThin);
            const double len = std::hypot(l->b.x - l->a.x, l->b.y - l->a.y);
            EXPECT_GE(len, kD / 2 + 2 - 1e-9);
            EXPECT_LE(len, kD / 2 + 5 + 1e-9);
            ++cross;
        }
    }
    EXPECT_EQ(pitch, 1);
    EXPECT_EQ(radial, kN);
    EXPECT_EQ(cross, 4);

    // DXF: окружность на слое CENTER радиусом 47,5 мм с типом линии штрихпунктирной.
    auto dxf = writeDxf(d);
    ASSERT_TRUE(dxf.ok());
    const dv2test::DxfFile f = dv2test::parseDxf(dxf.value());
    std::size_t circles = 0;
    for (const dv2test::DxfObject& o : *f.section("ENTITIES")) {
        if (o.type != "CIRCLE" || o.first(8) != std::string("CENTER")) continue;
        EXPECT_NEAR(o.real(40), kBC / 2, 1e-6);
        EXPECT_NEAR(o.real(10), 200, 1e-6);
        EXPECT_NE(o.first(6).value_or(""), "Continuous");
        ++circles;
    }
    EXPECT_EQ(circles, 1u);

    // Вид спереди: осевые линии центра (x = 0) и отверстий (x = ±47,5, ±23,75 — пары отверстий
    // за одной осью совпадают).
    auto front = findCenterGeometry(s, defaultFrontFrame());
    ASSERT_TRUE(front.ok());
    std::vector<double> xs;
    for (const AxisLine& a : front.value().axes) {
        EXPECT_NEAR(a.a.x, a.b.x, 1e-9);
        xs.push_back(a.a.x);
    }
    std::sort(xs.begin(), xs.end());
    ASSERT_EQ(xs.size(), 5u);
    const double want[] = {-kBC / 2, -kBC / 4, 0, kBC / 4, kBC / 2};
    for (int i = 0; i < 5; ++i) EXPECT_NEAR(xs[std::size_t(i)], want[i], 1e-6);
    EXPECT_TRUE(front.value().circles.empty());
}

TEST(DrawingViewsCenters, ArcsOfHalfCircleAndConeAndRectangularHoles) {
    // Паз 60×20 с полукруглыми концами R10 в плите: дуги рёбер с охватом 180° — метки в центрах
    // полуокружностей (20, 30) и (60, 30); углы плиты без скруглений — меток нет.
    TopoDS_Shape plate = BRepPrimAPI_MakeBox(80, 60, 10).Shape();
    TopoDS_Shape slot = BRepPrimAPI_MakeBox(gp_Pnt(20, 20, -1), 40, 20, 12).Shape();
    slot = fuse(slot, BRepPrimAPI_MakeCylinder(gp_Ax2(gp_Pnt(20, 30, -1), gp::DZ()), 10, 12).Shape());
    slot = fuse(slot, BRepPrimAPI_MakeCylinder(gp_Ax2(gp_Pnt(60, 30, -1), gp::DZ()), 10, 12).Shape());
    const Shape s(cut(plate, slot));
    auto g = findCenterGeometry(s, topFrame());
    ASSERT_TRUE(g.ok());
    ASSERT_EQ(g.value().marks.size(), 2u);
    for (const CenterMark& m : g.value().marks) {
        EXPECT_NEAR(m.center.y, 30, 1e-9);
        EXPECT_TRUE(std::abs(m.center.x - 20) < 1e-9 || std::abs(m.center.x - 60) < 1e-9);
        EXPECT_NEAR(m.radius, 10, 1e-9);
    }
    // Ниже порога охвата (> π) — меток нет.
    auto g2 = findCenterGeometry(s, topFrame(), 1.1 * kPi);
    ASSERT_TRUE(g2.ok());
    EXPECT_TRUE(g2.value().marks.empty());

    // Конус (усечённый Ø40 → Ø20, высота 30) лёжа вдоль X: осевая линия по длине конуса; с торца —
    // метка по наибольшему радиусу.
    const Shape cone(BRepPrimAPI_MakeCone(gp_Ax2(gp_Pnt(0, 0, 0), gp::DX()), 20, 10, 30).Shape());
    auto ga = findCenterGeometry(cone, defaultFrontFrame());
    ASSERT_TRUE(ga.ok());
    ASSERT_EQ(ga.value().axes.size(), 1u);
    EXPECT_NEAR(std::min(ga.value().axes[0].a.x, ga.value().axes[0].b.x), 0, 1e-9);
    EXPECT_NEAR(std::max(ga.value().axes[0].a.x, ga.value().axes[0].b.x), 30, 1e-9);
    EXPECT_NEAR(ga.value().axes[0].radius, 20, 1e-9);
    ViewFrame end;
    end.toward = {1, 0, 0};
    end.right = {0, 1, 0};
    auto ge = findCenterGeometry(cone, end);
    ASSERT_TRUE(ge.ok());
    ASSERT_EQ(ge.value().marks.size(), 1u);
    EXPECT_NEAR(ge.value().marks[0].radius, 20, 1e-9);

    // Четыре отверстия по углам квадратной плиты без центрального элемента — не круговой массив.
    TopoDS_Shape sq = BRepPrimAPI_MakeBox(80, 80, 10).Shape();
    for (double x : {15.0, 65.0})
        for (double y : {15.0, 65.0})
            sq = cut(sq, BRepPrimAPI_MakeCylinder(gp_Ax2(gp_Pnt(x, y, -1), gp::DZ()), 4, 12).Shape());
    auto gs = findCenterGeometry(Shape(sq), topFrame());
    ASSERT_TRUE(gs.ok());
    EXPECT_EQ(gs.value().marks.size(), 4u);
    EXPECT_TRUE(gs.value().circles.empty());
    for (const CenterMark& m : gs.value().marks) EXPECT_FALSE(m.radial);
}

TEST(DrawingViewsHatch, MaterialByName) {
    EXPECT_EQ(hatchMaterialByName(""), HatchMaterial::Metal);
    EXPECT_EQ(hatchMaterialByName("Сталь 45 ГОСТ 1050-2013"), HatchMaterial::Metal);
    EXPECT_EQ(hatchMaterialByName("СЧ 20"), HatchMaterial::Metal);
    EXPECT_EQ(hatchMaterialByName("Полиамид ПА6 ГОСТ ..."), HatchMaterial::NonMetal);
    EXPECT_EQ(hatchMaterialByName("Резина 7-ИРП-1225"), HatchMaterial::NonMetal);
    EXPECT_EQ(hatchMaterialByName("PA6 (dry)"), HatchMaterial::NonMetal);
    EXPECT_EQ(hatchMaterialByName("ABS"), HatchMaterial::NonMetal);
    EXPECT_EQ(hatchMaterialByName("Фторопласт-4"), HatchMaterial::NonMetal);
    EXPECT_EQ(hatchMaterialByName("Стеклотекстолит СТЭФ"), HatchMaterial::NonMetal);
    EXPECT_EQ(hatchMaterialByName("Стекло М1"), HatchMaterial::Glass);
    EXPECT_EQ(hatchMaterialByName("Оргстекло ТОСП"), HatchMaterial::Glass);
    EXPECT_EQ(hatchMaterialByName("PMMA"), HatchMaterial::Glass);
    EXPECT_EQ(hatchMaterialByName("Сосна 2 сорт"), HatchMaterial::Wood);
    EXPECT_EQ(hatchMaterialByName("Фанера ФК"), HatchMaterial::Wood);
    EXPECT_EQ(hatchMaterialByName("Бетон B25"), HatchMaterial::Concrete);
    EXPECT_EQ(hatchMaterialByName("Масло И-20А"), HatchMaterial::Liquid);
    EXPECT_EQ(hatchMaterialByName("Water"), HatchMaterial::Liquid);
    for (HatchMaterial m : {HatchMaterial::Metal, HatchMaterial::NonMetal, HatchMaterial::Wood, HatchMaterial::Glass,
                            HatchMaterial::Concrete, HatchMaterial::Liquid})
        EXPECT_EQ(hatchMaterialFromString(toString(m)), m);
    EXPECT_FALSE(hatchMaterialFromString("steel"));
}

TEST(DrawingViewsHatch, MaterialPatternsStayInsideTheRegion) {
    // Квадрат 40×40 с отверстием 10×10 в центре; шаг 2 мм.
    const std::vector<std::vector<Point2>> loops = {{{0, 0}, {40, 0}, {40, 40}, {0, 40}}, {{15, 15}, {25, 15}, {25, 25}, {15, 25}}};
    auto inside = [](Point2 p) {
        const bool outer = p.x >= -1e-9 && p.x <= 40 + 1e-9 && p.y >= -1e-9 && p.y <= 40 + 1e-9;
        const bool hole = p.x > 15 + 1e-9 && p.x < 25 - 1e-9 && p.y > 15 + 1e-9 && p.y < 25 - 1e-9;
        return outer && !hole;
    };
    const HatchPattern p45{45, 1};
    // Металл — один Hatch под 45°; неметалл — сетка 45° и 135° с тем же шагом (ГОСТ 2.306, табл. 1, пп. 1, 2).
    auto metal = materialHatchGeometry(loops, HatchMaterial::Metal, p45, 2);
    ASSERT_TRUE(metal.ok());
    ASSERT_EQ(metal.value().hatches.size(), 1u);
    EXPECT_DOUBLE_EQ(metal.value().hatches[0].angleDeg, 45);
    auto plastic = materialHatchGeometry(loops, HatchMaterial::NonMetal, p45, 2);
    ASSERT_TRUE(plastic.ok());
    ASSERT_EQ(plastic.value().hatches.size(), 2u);
    EXPECT_DOUBLE_EQ(plastic.value().hatches[0].angleDeg, 45);
    EXPECT_DOUBLE_EQ(plastic.value().hatches[1].angleDeg, 135);
    EXPECT_DOUBLE_EQ(plastic.value().hatches[1].spacing, 2);
    // Сумма длин линий сетки ≈ 2 · площадь / шаг (по каждому направлению площадь / шаг).
    double gridLength = 0;
    for (const Hatch& h : plastic.value().hatches)
        for (const LineSeg& sg : hatchSegments(h).value()) gridLength += std::hypot(sg.b.x - sg.a.x, sg.b.y - sg.a.y);
    EXPECT_NEAR(gridLength, 2 * 1500.0 / 2.0, 0.05 * 1500.0);

    // Стекло: штрихи под 45° целиком внутри; жидкость: горизонтальные штрихи внутри; древесина —
    // ломаные внутри; бетон — точки и треугольники внутри.
    auto glass = materialHatchGeometry(loops, HatchMaterial::Glass, p45, 2);
    ASSERT_TRUE(glass.ok());
    ASSERT_FALSE(glass.value().lines.empty());
    for (const LineSeg& l : glass.value().lines) {
        EXPECT_TRUE(inside(l.a) && inside(l.b) && inside({(l.a.x + l.b.x) / 2, (l.a.y + l.b.y) / 2}));
        EXPECT_NEAR(std::atan2(l.b.y - l.a.y, l.b.x - l.a.x), kPi / 4, 1e-9);
    }
    auto liquid = materialHatchGeometry(loops, HatchMaterial::Liquid, p45, 2);
    ASSERT_TRUE(liquid.ok());
    ASSERT_FALSE(liquid.value().lines.empty());
    for (const LineSeg& l : liquid.value().lines) {
        EXPECT_NEAR(l.a.y, l.b.y, 1e-12);
        EXPECT_TRUE(inside(l.a) && inside(l.b) && inside({(l.a.x + l.b.x) / 2, l.a.y}));
        EXPECT_LE(std::abs(l.b.x - l.a.x), 6 + 1e-9);  // штрих 3s
    }
    auto wood = materialHatchGeometry(loops, HatchMaterial::Wood, p45, 2);
    ASSERT_TRUE(wood.ok());
    ASSERT_FALSE(wood.value().polylines.empty());
    for (const Polyline& pl : wood.value().polylines)
        for (std::size_t i = 0; i + 1 < pl.points.size(); ++i) {
            const Point2 a = pl.points[i], b = pl.points[i + 1];
            EXPECT_TRUE(inside({(a.x + b.x) / 2, (a.y + b.y) / 2}));
        }
    auto concrete = materialHatchGeometry(loops, HatchMaterial::Concrete, p45, 2);
    ASSERT_TRUE(concrete.ok());
    EXPECT_FALSE(concrete.value().dots.empty());
    EXPECT_FALSE(concrete.value().polylines.empty());
    for (const Circle& c : concrete.value().dots) EXPECT_TRUE(inside(c.center));
    for (const Polyline& t : concrete.value().polylines) {
        EXPECT_TRUE(t.closed);
        ASSERT_EQ(t.points.size(), 3u);
        for (const Point2& q : t.points) EXPECT_TRUE(inside(q));
    }
    // Ошибки: шаг вне 1…10 мм.
    EXPECT_FALSE(materialHatchGeometry(loops, HatchMaterial::Glass, p45, 0.5).ok());
    EXPECT_FALSE(materialHatchGeometry(loops, HatchMaterial::Glass, p45, 12).ok());
}
