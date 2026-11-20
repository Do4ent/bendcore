// Условное изображение зубчатых колёс (ГОСТ 2.402) и таблица параметров венца (ГОСТ 2.403):
// линии венца на видах сбоку, с торца и в разрезе; нерассечённый зуб в разрезе (площадь
// штриховки); длина общей нормали против табличных значений; строки и раскладка таблицы;
// документ чертежа по детали с венцом (условная форма, масса по форме с зубьями, сохранение).

#include "test_drawing_document_support.hpp"

#include <bendcore/drawing_v2/gears.hpp>
#include <bendcore/modeling/gear.hpp>
#include <bendcore/modeling/part_features.hpp>

#include <BRepAdaptor_Surface.hxx>
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS.hxx>
#include <gtest/gtest.h>

#include <cmath>

using namespace ddtest;
namespace an = bendcore::drawing_v2::annotations;
namespace md = bendcore::modeling;

namespace {

constexpr double kPi = 3.14159265358979323846;

// Венец m = 2, z = 30 (d = 60, da = 64, df = 55), b = 20, ось Z от z = 0.
md::GearInfo gearOnZ() {
    md::GearInfo g;
    g.gear.module = 2;
    g.gear.teeth = 30;
    g.gear.width = 20;
    g.gear.axis = gp_Ax3(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1), gp_Dir(1, 0, 0));
    return g;
}

double volume(const TopoDS_Shape& s) {
    GProp_GProps p;
    BRepGProp::VolumeProperties(s, p);
    return p.Mass();
}

double contoursArea(const std::vector<std::vector<Point2>>& region) {
    double total = 0;
    for (const auto& c : region) {
        double a = 0;
        for (std::size_t i = 0; i < c.size(); ++i) {
            const Point2 p = c[i], q = c[(i + 1) % c.size()];
            a += p.x * q.y - q.x * p.y;
        }
        total += std::abs(a) / 2;
    }
    return total;
}

}  // namespace

TEST(DrawingGears, LinesOnSideEndAndSectionViews) {
    const std::vector<md::GearInfo> gears{gearOnZ()};
    // Вид спереди (ось Z вверх): образующие.
    auto front = an::gearImages(gears, views::defaultFrontFrame());
    ASSERT_TRUE(front.ok()) << front.error().message;
    ASSERT_EQ(front.value().size(), 1u);
    EXPECT_EQ(front.value()[0].aspect, an::GearAspect::Side);
    EXPECT_FALSE(front.value()[0].section);
    views::SheetView sv;
    sv.scale = 2.0;
    sv.anchor = {100, 50};
    const auto lines = an::gearLines(front.value()[0], sv, {}, 3.0);
    ASSERT_EQ(lines.size(), 4u);
    int pitch = 0, root = 0;
    for (const an::GearLine& l : lines) {
        const LineSeg& s = std::get<LineSeg>(l.curve);
        EXPECT_NEAR(s.a.x, s.b.x, 1e-12);  // вдоль оси (вертикально)
        const double half = std::abs(s.a.x - 100.0) / 2.0;  // мм модели
        if (l.role == an::GearLine::Role::Pitch) {
            ++pitch;
            EXPECT_NEAR(half, 30.0, 1e-12);                       // d/2
            EXPECT_EQ(l.lineType, LineType::DashDotThin);         // штрихпунктирная тонкая
            EXPECT_EQ(l.layer, "CENTER");
            EXPECT_NEAR(std::abs(s.b.y - s.a.y), 20 * 2.0 + 6.0, 1e-12);  // b·M + выход 3 мм с каждой стороны
            EXPECT_NEAR(std::min(s.a.y, s.b.y), 50.0 - 3.0, 1e-12);
        } else {
            ++root;
            EXPECT_NEAR(half, 27.5, 1e-12);                       // df/2
            EXPECT_EQ(l.lineType, LineType::SolidThin);           // на виде — тонкая
            EXPECT_NEAR(std::abs(s.b.y - s.a.y), 40.0, 1e-12);
        }
    }
    EXPECT_EQ(pitch, 2);
    EXPECT_EQ(root, 2);
    // Вид сверху (с торца): делительная окружность, окружность впадин — по опции.
    const auto top = views::standardFrame(views::defaultFrontFrame(), views::StandardView::Top);
    auto end = an::gearImages(gears, top);
    ASSERT_TRUE(end.ok());
    ASSERT_EQ(end.value()[0].aspect, an::GearAspect::End);
    views::SheetView one;
    auto el = an::gearLines(end.value()[0], one);
    ASSERT_EQ(el.size(), 1u);
    EXPECT_NEAR(std::get<Circle>(el[0].curve).radius, 30.0, 1e-12);
    EXPECT_EQ(el[0].lineType, LineType::DashDotThin);
    an::GearImageOptions withRoot;
    withRoot.rootCircleOnEnd = true;
    el = an::gearLines(end.value()[0], one, {}, 3.0, withRoot);
    ASSERT_EQ(el.size(), 2u);
    EXPECT_NEAR(std::get<Circle>(el[1].curve).radius, 27.5, 1e-12);
    EXPECT_EQ(el[1].lineType, LineType::SolidThin);
    // Разрез плоскостью через ось: образующие впадин — основные.
    auto cut = an::gearImages(gears, views::defaultFrontFrame(), views::CuttingPlane{{0, 0, 0}, {0, 1, 0}});
    ASSERT_TRUE(cut.ok());
    EXPECT_TRUE(cut.value()[0].section);
    for (const an::GearLine& l : an::gearLines(cut.value()[0], sv))
        if (l.role == an::GearLine::Role::Root) EXPECT_EQ(l.lineType, LineType::SolidThick);
    // Плоскость, не проходящая через ось, — не разрез венца по оси.
    auto off = an::gearImages(gears, views::defaultFrontFrame(), views::CuttingPlane{{0, 5, 0}, {0, 1, 0}});
    EXPECT_FALSE(off.value()[0].section);
    // Аксонометрия — венец не изображается.
    auto iso = an::gearImages(gears, views::axonometricFrame(views::defaultFrontFrame(), views::Axonometry::Isometric));
    EXPECT_TRUE(iso.value().empty());
}

TEST(DrawingGears, UncutTeethInSection) {
    // Фигура сечения — прямоугольник x ∈ [−35, 35], y ∈ [−5, 25] (координаты вида спереди): полоса
    // зубьев |x| ≥ 27,5 при y ∈ [0, 20] не штрихуется: 2100 − 2·7,5·20 = 1800.
    auto cut = an::gearImages({gearOnZ()}, views::defaultFrontFrame(), views::CuttingPlane{{0, 0, 0}, {0, 1, 0}});
    ASSERT_TRUE(cut.ok());
    views::SectionView s;
    s.regions = {{{{-35, -5}, {35, -5}, {35, 25}, {-35, 25}}}};
    s.regionOwner = {0};
    const views::SectionView u = an::withUncutTeeth(s, cut.value());
    ASSERT_EQ(u.regions.size(), 1u);
    EXPECT_NEAR(contoursArea(u.regions[0]), 1800.0, 1e-9);
    // С отверстием 20 × 20 в середине (правило чётности): три части внешнего контура (1800) и
    // отверстие целиком (400) — штрихуется 1800 − 400.
    s.regions[0].push_back({{-10, 0}, {10, 0}, {10, 20}, {-10, 20}});
    const views::SectionView h = an::withUncutTeeth(s, cut.value());
    ASSERT_EQ(h.regions.size(), 1u);
    EXPECT_EQ(h.regions[0].size(), 4u);
    EXPECT_NEAR(contoursArea(h.regions[0]), 1800.0 + 400.0, 1e-9);
}

TEST(DrawingGears, CommonNormalAndTable) {
    // Длина общей нормали при m = 1, x = 0 (таблица ГОСТ 16532 / справочники): z = 20 — zn = 3,
    // W = 7,6604; z = 17 — zn = 2, W = 4,6523 (по формуле: 2,9521·(π·1,5 + 17·0,014904)).
    md::GearSpec s;
    s.module = 1;
    s.teeth = 20;
    int zn = 0;
    EXPECT_NEAR(an::commonNormalLength(s, zn), 7.6604, 5e-5);
    EXPECT_EQ(zn, 3);
    s.teeth = 60;
    s.module = 2;
    EXPECT_NEAR(an::commonNormalLength(s, zn), 40.05837, 5e-5);
    EXPECT_EQ(zn, 7);
    // Смещение увеличивает W на 2x·m·sin α.
    s.shift = 0.3;
    const double w = an::commonNormalLength(s, zn);
    md::GearSpec s0 = s;
    s0.shift = 0;
    int zn0 = 0;
    const double w0 = an::commonNormalLength(s0, zn0);
    if (zn == zn0) EXPECT_NEAR(w - w0, 2 * 0.3 * 2 * std::sin(20 * kPi / 180), 1e-12);

    md::GearSpec wheel;
    wheel.module = 2;
    wheel.teeth = 60;
    an::GearTableDef def;
    def.accuracy = "8-B ГОСТ 1643-81";
    def.matingTeeth = 20;
    def.matingDesignation = "РЦ.080.00.003";
    def.centerDistance = 80;
    auto rows = an::gearTableRows(wheel, def);
    ASSERT_TRUE(rows.ok());
    std::map<std::string, std::pair<std::string, std::string>> byName;
    for (const auto& r : rows.value()) byName[r.name] = {r.symbol, r.value};
    EXPECT_EQ(byName["Модуль"].second, "2");
    EXPECT_EQ(byName["Число зубьев"].second, "60");
    EXPECT_EQ(byName["Тип зуба"].second, "Прямозубый");
    EXPECT_EQ(byName["Нормальный исходный контур"].second, "ГОСТ 13755-2015");
    EXPECT_EQ(byName["Коэффициент смещения"].second, "0");
    EXPECT_EQ(byName["Степень точности"].second, "8-B ГОСТ 1643-81");
    EXPECT_EQ(byName["Длина общей нормали"].second, "40,058");
    EXPECT_EQ(byName["Делительный диаметр"].second, "120");
    EXPECT_EQ(byName["Число зубьев сопряжённого колеса"].second, "20");
    EXPECT_EQ(byName["Межосевое расстояние"].second, "80");
    EXPECT_EQ(byName.count("Угол наклона"), 0u);  // прямой зуб
    // Порядок частей: основные (1), контроль (2), справочные (3).
    int last = 0;
    for (const auto& r : rows.value()) {
        EXPECT_GE(r.part, last);
        last = r.part;
    }
    // Косозубое колесо: тип, направление, угол 15°.
    md::GearSpec hel = wheel;
    hel.helix = 15 * kPi / 180;
    hel.hand = md::GearHand::Left;
    auto hr = an::gearTableRows(hel, {});
    ASSERT_TRUE(hr.ok());
    bool angle = false, hand = false;
    for (const auto& r : hr.value()) {
        angle = angle || (r.name == "Угол наклона" && r.value == "15°");
        hand = hand || (r.name == "Направление линии зуба" && r.value == "Левое");
    }
    EXPECT_TRUE(angle);
    EXPECT_TRUE(hand);

    // Таблица на листе А3: правый верхний угол рамки, 110 мм, строки по 8 мм.
    auto sheet = gostSheet(GostFormat::A3, 1, Orientation::Landscape);
    ASSERT_TRUE(sheet.ok());
    auto d = Drawing::create(sheet.value());
    ASSERT_TRUE(d.ok());
    const Rect frame{20, 5, 415, 292};
    auto lay = an::addGearTable(d.value(), frame, rows.value());
    ASSERT_TRUE(lay.ok()) << lay.error().message;
    EXPECT_NEAR(lay.value().box.x1, 415.0, 1e-12);
    EXPECT_NEAR(lay.value().box.x0, 305.0, 1e-12);
    EXPECT_NEAR(lay.value().box.y1, 292.0, 1e-12);
    EXPECT_NEAR(lay.value().box.y0, 292.0 - 8.0 * rows.value().size(), 1e-12);
    std::vector<std::string> texts;
    int thick = 0;
    for (const Entity& e : d.value().entities()) {
        if (const auto* t = std::get_if<Text>(&e.geometry)) {
            texts.push_back(t->utf8);
            EXPECT_GE(t->position.x, 305.0);
            EXPECT_LE(t->position.x, 415.0);
        }
        if (std::holds_alternative<LineSeg>(e.geometry) && e.lineType == LineType::SolidThick) ++thick;
    }
    auto has = [&](const std::string& s) { return std::find(texts.begin(), texts.end(), s) != texts.end(); };
    EXPECT_TRUE(has("Модуль"));
    EXPECT_TRUE(has("40,058"));
    EXPECT_TRUE(has("РЦ.080.00.003"));
    // Длинное наименование не помещается в 61 мм шрифтом 3,5 и 2,5 — переносится на две строки.
    EXPECT_FALSE(has("Обозначение чертежа сопряжённого колеса"));
    const bool split = std::any_of(texts.begin(), texts.end(), [](const std::string& t) {
        return t.rfind("Обозначение чертежа", 0) == 0;
    }) && std::any_of(texts.begin(), texts.end(), [](const std::string& t) {
        const std::string tail = "колеса";
        return t.size() > tail.size() && t.compare(t.size() - tail.size(), tail.size(), tail) == 0;
    });
    EXPECT_TRUE(split);
    // Основные линии: рамка таблицы (2 горизонтали + 4 вертикали) + 2 границы частей.
    EXPECT_EQ(thick, 8);
    // Таблица выше рамки — ошибка.
    std::vector<an::GearTableRow> many(40, an::GearTableRow{"Модуль", "m", "2", 1});
    EXPECT_FALSE(an::addGearTable(d.value(), frame, many).ok());
}

TEST(DrawingGears, DrawingOfAGearPart) {
    const TrueTypeFont* font = dv2test::systemFont();
    if (!font) GTEST_SKIP() << "no system TrueType font";
    // Заготовка ⌀64 × 20 (вал-цилиндр makeRod) и венец m = 2, z = 30 на её боковой грани.
    auto part = makeRod(32, 20);
    {
        const md::NamedShape s = part->finalShape();
        TopoDS_Shape rim;
        for (TopExp_Explorer ex(s.shape(), TopAbs_FACE); ex.More(); ex.Next()) {
            BRepAdaptor_Surface a(TopoDS::Face(ex.Current()), false);
            if (a.GetType() == GeomAbs_Cylinder) rim = ex.Current();
        }
        ASSERT_FALSE(rim.IsNull());
        md::FeatureSpec g;
        g.type = "GearTeeth";
        g.id = "Rim";
        g.parameters = {{"module", "2"}, {"teeth", "30"}};
        g.references["face"] = {md::makeReference(s, rim).value()};
        g.options = {{"axis", "face"}};
        ASSERT_TRUE(part->addFeature(g).ok());
        part->rebuild();
        ASSERT_EQ(part->feature("Rim").value().state, md::FeatureState::Ok);
    }
    const double realVolume = volume(part->finalShape().shape());
    auto snap = snapshotPart(*part, {"РД.30.001", "Колесо", "Сталь 45", 7850.0});
    ASSERT_TRUE(snap.ok()) << snap.error().message;
    ASSERT_EQ(snap.value().gears.size(), 1u);
    // Условная форма — цилиндр ⌀64 × 20; масса — по форме с зубьями.
    EXPECT_NEAR(volume(snap.value().shape.occt()), kPi * 32 * 32 * 20, 1e-6 * kPi * 32 * 32 * 20);
    ASSERT_TRUE(snap.value().massKg);
    EXPECT_NEAR(*snap.value().massKg, realVolume * 1e-9 * 7850.0, 1e-12);
    EXPECT_LT(realVolume, kPi * 32 * 32 * 20);
    // Условная форма — 3 грани (цилиндр вершин и два торца), окружности торцов — по одному ребру.
    int faces = 0, circles = 0;
    for (TopExp_Explorer ex(snap.value().shape.occt(), TopAbs_FACE); ex.More(); ex.Next()) ++faces;
    {
        TopTools_IndexedMapOfShape edges;
        TopExp::MapShapes(snap.value().shape.occt(), TopAbs_EDGE, edges);
        circles = edges.Extent();
    }
    EXPECT_EQ(faces, 3);
    EXPECT_EQ(circles, 3);  // две окружности торцов и шов

    DrawingDocument d;
    d.model().kind = ModelKind::Part;
    SheetDef s;
    s.id = "S1";
    s.format = {GostFormat::A3, 1, Orientation::Landscape};
    ViewDef f;
    f.id = "F";
    f.hidden = false;
    s.views.push_back(f);
    ViewDef t;
    t.id = "T";
    t.kind = ViewKind::Projected;
    t.parent = "F";
    t.side = views::SheetSide::Below;
    t.hidden = false;
    s.views.push_back(t);
    ViewDef sec;
    sec.id = "A";
    sec.kind = ViewKind::Section;
    sec.parent = "F";
    sec.side = views::SheetSide::RightOf;
    sec.toward = {1, 0, 0};
    sec.right = {0, 1, 0};
    sec.planePoint = {0, 0, 0};
    sec.markView = "F";
    sec.hidden = false;
    s.views.push_back(sec);
    an::GearTableDef table;
    table.accuracy = "8-B ГОСТ 1643-81";
    s.gearTable = table;
    s.roughnessRemainder = an::RoughnessSpec{};
    d.sheets().push_back(s);
    d.setFont(font);
    views::ViewCache cache(8, 1);
    auto rep = d.update(snap.value(), cache);
    ASSERT_TRUE(rep.ok()) << rep.error().message;
    const ComputedSheet& cs = d.computedSheets().front();
    std::map<std::string, const ComputedView*> byId;
    for (const ComputedView& cv : cs.views) byId[cv.id] = &cv;
    ASSERT_EQ(byId.at("F")->gears.size(), 1u);
    EXPECT_EQ(byId.at("F")->gears[0].aspect, an::GearAspect::Side);
    EXPECT_EQ(byId.at("T")->gears[0].aspect, an::GearAspect::End);
    ASSERT_EQ(byId.at("A")->gears.size(), 1u);
    EXPECT_TRUE(byId.at("A")->gears[0].section);
    // Разрез цилиндра 64 × 20 через ось: 1280, без полос зубьев 2·(32 − 27,5)·20 = 180 → 1100.
    double hatched = 0;
    for (const auto& region : byId.at("A")->section->regions) hatched += contoursArea(region);
    EXPECT_NEAR(hatched, 1100.0, 1e-3);
    EXPECT_FALSE(cs.gearTable.empty());
    // Лист: делительные линии (штрихпунктир), таблица, знак «остальное» левее таблицы.
    auto sheets = d.render(font);
    ASSERT_TRUE(sheets.ok()) << sheets.error().message;
    int dashDot = 0;
    bool module = false;
    for (const Entity& e : sheets.value()[0].entities()) {
        if (e.lineType == LineType::DashDotThin && e.layer == "CENTER") ++dashDot;
        if (const auto* tx = std::get_if<Text>(&e.geometry)) module = module || tx->utf8 == "Модуль";
    }
    EXPECT_GE(dashDot, 2 + 1 + 2);  // образующие вида спереди, окружность вида сверху, разрез
    EXPECT_TRUE(module);
    // Сохранение и загрузка: флаг видов и таблица.
    auto json = d.toJson();
    ASSERT_TRUE(json.ok());
    auto back = DrawingDocument::fromJson(json.value());
    ASSERT_TRUE(back.ok()) << back.error().message;
    ASSERT_TRUE(back.value().sheets()[0].gearTable);
    EXPECT_EQ(back.value().sheets()[0].gearTable->accuracy, "8-B ГОСТ 1643-81");
    EXPECT_EQ(back.value().toJson().value(), json.value());
}
