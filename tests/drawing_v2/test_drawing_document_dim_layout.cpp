// Автокомпоновка размеров (docs/drawing/ANNOTATIONS.md, «Автокомпоновка размеров», ГОСТ 2.307-2011,
// разд. 4): на видах эталонных деталей (кронштейн, фланец — docs/modeling/REFERENCE_PARTS.md, №1, 3)
// размеры, заданные с нарочно плохими отступами (2 мм от точки привязки, все на одной линии),
// расставляются так, что: размерные линии не ближе 10 мм к габариту изображения и не ближе 7 мм
// друг к другу; надписи не пересекаются друг с другом и с линиями изображения; выносные линии не
// пересекают размерные. Проверки — независимым разбором геометрии результата (габариты надписей по
// метрикам шрифта, линии вида на листе), значения размеров — по модели.

#include "test_drawing_document_support.hpp"

#include <bendcore/drawing_v2/annotation_style.hpp>
#include <bendcore/drawing_v2/export.hpp>
#include <bendcore/drawing_v2/stroke_font.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <map>

using namespace ddtest;
namespace vw = bendcore::drawing_v2::views;
using bendcore::modeling::ElementKind;
using annotations::AnchorKind;
using annotations::DimensionType;

namespace {

constexpr double kPi = 3.14159265358979323846;

struct Rect {
    double x0, y0, x1, y1;
};

// Габарит надписи: строка шрифта ГОСТ 2.304 (ширина по метрикам, высота −0,3h…h, наклон 15°).
Rect labelRect(const annotations::DimensionLabel& l) {
    std::u32string u;
    const std::string& s = l.text;
    for (std::size_t i = 0; i < s.size();) {
        const unsigned char c = s[i];
        const int n = c < 0x80 ? 1 : c < 0xE0 ? 2 : c < 0xF0 ? 3 : 4;
        char32_t cp = n == 1 ? c : c & (0xFF >> (n + 1));
        for (int k = 1; k < n; ++k) cp = (cp << 6) | (s[i + k] & 0x3F);
        u.push_back(cp);
        i += n;
    }
    const double w = gostStrokeFont().textWidthMm(u, l.height) + 0.27 * l.height;
    const double r = l.rotationDeg * kPi / 180.0;
    Rect b{1e300, 1e300, -1e300, -1e300};
    for (double x : {0.0, w})
        for (double y : {-0.3 * l.height, l.height}) {
            const double px = l.position.x + x * std::cos(r) - y * std::sin(r);
            const double py = l.position.y + x * std::sin(r) + y * std::cos(r);
            b = {std::min(b.x0, px), std::min(b.y0, py), std::max(b.x1, px), std::max(b.y1, py)};
        }
    return b;
}

bool rectsOverlap(const Rect& a, const Rect& b) {
    return std::min(a.x1, b.x1) > std::max(a.x0, b.x0) && std::min(a.y1, b.y1) > std::max(a.y0, b.y0);
}

// Пересечение отрезка с прямоугольником — по точкам с шагом 0,05 мм (независимо от реализации).
bool segmentInRect(Point2 a, Point2 b, const Rect& r) {
    const double L = std::hypot(b.x - a.x, b.y - a.y);
    const int n = std::max(1, int(L / 0.05));
    for (int i = 0; i <= n; ++i) {
        const double t = double(i) / n;
        const double x = a.x + t * (b.x - a.x), y = a.y + t * (b.y - a.y);
        if (x > r.x0 && x < r.x1 && y > r.y0 && y < r.y1) return true;
    }
    return false;
}

std::vector<std::pair<Point2, Point2>> viewLines(const ComputedView& cv) {
    std::vector<std::pair<Point2, Point2>> out;
    const auto& sv = cv.sheetView;
    for (const auto& e : sv.view.edges) {
        if (const auto* l = std::get_if<LineSeg>(&e.curve)) {
            out.push_back({sv.toSheet(l->a), sv.toSheet(l->b)});
        } else if (const auto* c = std::get_if<Circle>(&e.curve)) {
            for (int i = 0; i < 72; ++i) {
                const double a0 = i * 5.0 * kPi / 180, a1 = (i + 1) * 5.0 * kPi / 180;
                out.push_back({sv.toSheet({c->center.x + c->radius * std::cos(a0), c->center.y + c->radius * std::sin(a0)}),
                               sv.toSheet({c->center.x + c->radius * std::cos(a1), c->center.y + c->radius * std::sin(a1)})});
            }
        } else if (const auto* a = std::get_if<Arc>(&e.curve)) {
            double sweep = a->endDeg - a->startDeg;
            while (sweep <= 0) sweep += 360;
            const int n = std::max(2, int(sweep / 5));
            for (int i = 0; i < n; ++i) {
                const double a0 = (a->startDeg + sweep * i / n) * kPi / 180, a1 = (a->startDeg + sweep * (i + 1) / n) * kPi / 180;
                out.push_back({sv.toSheet({a->center.x + a->radius * std::cos(a0), a->center.y + a->radius * std::sin(a0)}),
                               sv.toSheet({a->center.x + a->radius * std::cos(a1), a->center.y + a->radius * std::sin(a1)})});
            }
        } else if (const auto* p = std::get_if<Polyline>(&e.curve)) {
            for (std::size_t i = 0; i + 1 < p->points.size(); ++i) out.push_back({sv.toSheet(p->points[i]), sv.toSheet(p->points[i + 1])});
        }
    }
    return out;
}

struct Audit {
    std::size_t labelOverlaps = 0;   // надписи разных размеров
    std::size_t labelOnContour = 0;  // надпись на линии изображения
    std::size_t crossings = 0;       // выносная линия пересекает размерную линию другого размера
    double minContourGap = 1e300;    // от габарита изображения до размерной линии линейного размера
    double minSpacing = 1e300;       // между параллельными размерными линиями одной стороны
};

bool properCross(Point2 a, Point2 b, Point2 c, Point2 d) {
    auto cr = [](Point2 o, Point2 p, Point2 q) { return (p.x - o.x) * (q.y - o.y) - (p.y - o.y) * (q.x - o.x); };
    const double d1 = cr(c, d, a), d2 = cr(c, d, b), d3 = cr(a, b, c), d4 = cr(a, b, d);
    return ((d1 > 1e-9 && d2 < -1e-9) || (d1 < -1e-9 && d2 > 1e-9)) && ((d3 > 1e-9 && d4 < -1e-9) || (d3 < -1e-9 && d4 > 1e-9));
}

Audit audit(const DrawingDocument& d, const std::string& view) {
    Audit a;
    const ComputedSheet& cs = d.computedSheets()[0];
    const SheetDef& sd = d.sheets()[0];
    const ComputedView* cv = nullptr;
    for (const ComputedView& v : cs.views)
        if (v.id == view) cv = &v;
    EXPECT_NE(cv, nullptr);
    if (!cv) return a;
    const vw::Box2 box = cv->sheetView.sheetBounds();
    const auto lines = viewLines(*cv);
    std::vector<std::vector<Rect>> labels;
    std::vector<const annotations::DimensionResult*> res;
    std::map<std::string, std::vector<double>> sides;  // «H+»/«V−» → координаты размерных линий
    for (std::size_t i = 0; i < sd.dimensions.size(); ++i) {
        if (sd.dimensions[i].view != view) continue;
        const auto& r = cs.dimensions[i];
        EXPECT_EQ(r.state, annotations::DimensionState::Ok) << sd.dimensions[i].id << ": " << r.reason;
        res.push_back(&r);
        std::vector<Rect> mine;
        for (const auto& l : r.geometry.labels) mine.push_back(labelRect(l));
        labels.push_back(mine);
        const auto& def = sd.dimensions[i].def;
        if ((def.type == DimensionType::Horizontal || def.type == DimensionType::Vertical) && !r.geometry.dimensionLines.empty()) {
            const LineSeg& dl = r.geometry.dimensionLines[0];
            if (def.type == DimensionType::Horizontal) {
                const double y = dl.a.y;
                a.minContourGap = std::min(a.minContourGap, y > box.maxY ? y - box.maxY : box.minY - y);
                sides[y > box.maxY ? "H+" : "H-"].push_back(y);
            } else {
                const double x = dl.a.x;
                a.minContourGap = std::min(a.minContourGap, x > box.maxX ? x - box.maxX : box.minX - x);
                sides[x > box.maxX ? "V+" : "V-"].push_back(x);
            }
        }
    }
    for (auto& [k, v] : sides) {
        std::sort(v.begin(), v.end());
        for (std::size_t i = 1; i < v.size(); ++i)
            if (v[i] - v[i - 1] > 1e-6) a.minSpacing = std::min(a.minSpacing, v[i] - v[i - 1]);
    }
    for (std::size_t i = 0; i < labels.size(); ++i) {
        for (const Rect& r : labels[i]) {
            for (std::size_t j = i + 1; j < labels.size(); ++j)
                for (const Rect& q : labels[j]) a.labelOverlaps += rectsOverlap(r, q);
            for (const auto& [p, q] : lines) a.labelOnContour += segmentInRect(p, q, r);
        }
        for (std::size_t j = 0; j < res.size(); ++j) {
            if (j == i) continue;
            for (const LineSeg& e : res[i]->geometry.extensionLines)
                for (const LineSeg& l : res[j]->geometry.dimensionLines) a.crossings += properCross(e.a, e.b, l.a, l.b);
        }
    }
    return a;
}

DrawingDocument sheetWith(std::vector<ViewDef> views) {
    DrawingDocument d;
    SheetDef sd;
    sd.id = "S1";
    sd.format = {GostFormat::A3, 1, Orientation::Landscape};
    sd.views = std::move(views);
    d.sheets().push_back(std::move(sd));
    return d;
}

ViewDef base() {
    ViewDef f;
    f.id = "F";
    return f;
}
ViewDef top() {
    ViewDef t;
    t.id = "T";
    t.kind = ViewKind::Projected;
    t.parent = "F";
    t.side = vw::SheetSide::Below;
    return t;
}

std::map<std::string, double> values(const DrawingDocument& d) {
    std::map<std::string, double> out;
    const auto& sd = d.sheets()[0];
    for (std::size_t i = 0; i < sd.dimensions.size(); ++i) out[sd.dimensions[i].id] = d.computedSheets()[0].dimensions[i].value;
    return out;
}

void arrangeAndCheck(DrawingDocument& d, const ModelSnapshot& m, const std::vector<std::string>& views, const std::string& name) {
    vw::ViewCache cache;
    auto r0 = d.update(m, cache);
    ASSERT_TRUE(r0) << r0.error().message;
    const auto before = values(d);
    std::size_t badBefore = 0;
    for (const std::string& v : views) {
        const Audit a = audit(d, v);
        badBefore += a.labelOverlaps + a.labelOnContour + (a.minContourGap < 10 - 1e-6 ? 1 : 0);
    }
    EXPECT_GT(badBefore, 0u) << "the initial placement must be bad for the test to mean something";
    for (const std::string& v : views) {
        auto ar = d.arrangeDimensions(m, "S1", v);
        ASSERT_TRUE(ar) << ar.error().message;
        EXPECT_GT(ar.value().arranged, 0u) << v;
        EXPECT_EQ(ar.value().crossings, 0u) << v;
        EXPECT_EQ(ar.value().collisions, 0u) << v;
    }
    auto r1 = d.update(m, cache);
    ASSERT_TRUE(r1) << r1.error().message;
    EXPECT_TRUE(r1.value().stale.empty());
    for (const auto& [id, v] : values(d)) EXPECT_NEAR(v, before.at(id), 1e-9) << id;
    for (const std::string& v : views) {
        const Audit a = audit(d, v);
        EXPECT_EQ(a.labelOverlaps, 0u) << v;
        EXPECT_EQ(a.labelOnContour, 0u) << v;
        EXPECT_EQ(a.crossings, 0u) << v;
        EXPECT_GE(a.minContourGap, 10.0 - 1e-6) << v;
        if (a.minSpacing < 1e299) EXPECT_GE(a.minSpacing, 7.0 - 1e-6) << v;
    }
    // Лист для просмотра (docs/drawing/ANNOTATIONS.md): PDF со шрифтом ГОСТ 2.304.
    auto pages = d.render(nullptr);
    ASSERT_TRUE(pages) << pages.error().message;
    ASSERT_TRUE(saveToFile(std::string(BENDCORE_DRAWING_V2_OUT) + "/" + name + "_arranged.pdf", writePdf(pages.value()[0]).value()));
    // Повторная компоновка ничего не меняет.
    std::vector<double> offsets;
    for (const auto& it : d.sheets()[0].dimensions) offsets.push_back(it.def.offset);
    for (const std::string& v : views) ASSERT_TRUE(d.arrangeDimensions(m, "S1", v));
    for (std::size_t i = 0; i < offsets.size(); ++i) EXPECT_NEAR(d.sheets()[0].dimensions[i].def.offset, offsets[i], 1e-9) << i;
}

}  // namespace

TEST(DrawingDocumentDimensionLayout, BracketFrontAndTopViews) {
    auto part = makeBracket();
    const ModelSnapshot m = snapshotPart(*part, {"РД.01", "Кронштейн", "Сталь 45", 7850}).value();
    DrawingDocument d = sheetWith({base(), top()});
    auto v = [&](double x, double y, double z) { return anchorOn(m, ElementKind::Vertex, vertexAt(m, {x, y, z}), AnchorKind::Point); };
    auto hole = anchorOn(m, ElementKind::Edge, circleEdge(m, {40, 0, 8}, 4), AnchorKind::Center);
    auto& dims = d.sheets()[0].dimensions;
    dims.push_back(dimension("A", "F", DimensionType::Horizontal, {v(0, -15, 0), v(60, -15, 0)}, -2));
    dims.push_back(dimension("B", "F", DimensionType::Vertical, {v(0, -15, 0), v(0, -15, 50)}, 2));
    dims.push_back(dimension("T", "F", DimensionType::Vertical, {v(60, -15, 0), v(60, -15, 8)}, -2));
    dims.push_back(dimension("tw", "F", DimensionType::Horizontal, {v(0, -15, 50), v(8, -15, 50)}, 2));
    dims.push_back(dimension("Wd", "T", DimensionType::Vertical, {v(60, -15, 8), v(60, 15, 8)}, -2));
    dims.push_back(dimension("hx", "T", DimensionType::Horizontal, {v(0, -15, 50), hole}, -2));
    dims.push_back(dimension("hy", "T", DimensionType::Vertical, {v(60, -15, 8), hole}, -2));
    dims.push_back(dimension("dd", "T", DimensionType::Diameter, {hole}));
    arrangeAndCheck(d, m, {"F", "T"}, "bracket");
    // Меньший размер ближе к контуру: hy (15) — ближе, чем Wd (30), на той же стороне.
    const auto& cs = d.computedSheets()[0];
    EXPECT_LT(std::abs(cs.dimensions[6].geometry.dimensionLines[0].a.x - cs.views[1].sheetView.sheetBounds().maxX),
              std::abs(cs.dimensions[4].geometry.dimensionLines[0].a.x - cs.views[1].sheetView.sheetBounds().maxX));
}

TEST(DrawingDocumentDimensionLayout, FlangeTopViewCirclesAndHolePitch) {
    auto part = makeFlange();
    const ModelSnapshot m = snapshotPart(*part, {"РД.03", "Фланец", "Сталь 20", 7850}).value();
    DrawingDocument d = sheetWith({base(), top()});
    const double R = 47.5;
    auto hole = [&](double deg) {
        const double a = deg * kPi / 180;
        return anchorOn(m, ElementKind::Edge, circleEdge(m, {R * std::cos(a), R * std::sin(a), 12}, 5), AnchorKind::Center);
    };
    auto& dims = d.sheets()[0].dimensions;
    dims.push_back(dimension("BC", "T", DimensionType::Horizontal, {hole(180), hole(0)}, 2));
    dims.push_back(dimension("p", "T", DimensionType::Horizontal, {hole(120), hole(60)}, 2));
    dims.push_back(dimension("v", "T", DimensionType::Vertical, {hole(300), hole(60)}, -2));
    for (auto [id, c, r] : {std::tuple{"D", Vec3{0, 0, 12}, 60.0}, {"Dh", Vec3{0, 0, 27}, 35.0}, {"d0", Vec3{0, 0, 27}, 20.0},
                            {"dh", Vec3{R, 0, 12}, 5.0}}) {
        DimensionItem it = dimension(id, "T", DimensionType::Diameter, {anchorOn(m, ElementKind::Edge, circleEdge(m, c, r), AnchorKind::Center)});
        it.def.leaderAngleDeg = 0;  // все полки по горизонтали — надписи накладываются на линии
        dims.push_back(it);
    }
    arrangeAndCheck(d, m, {"T"}, "flange");
    EXPECT_NEAR(values(d).at("BC"), 95, 1e-6);
    EXPECT_NEAR(values(d).at("p"), R, 1e-6);
    EXPECT_NEAR(values(d).at("v"), 2 * R * std::sin(kPi / 3), 1e-6);
}

// Размеры цепочкой и от базы как группа документа (ГОСТ 2.307): цепочка — одна размерная линия,
// звенья — соседние точки (перестраиваются при обновлении, даже если пары перепутаны); от базы —
// меньший размер ближе к контуру, шаг 7 мм; изменение модели (T: 8 → 20) меняет значения, группа
// остаётся на месте; перенос одного размера переносит группу; группа сохраняется в файле и не
// переставляется автокомпоновкой; шаг < 7 мм — ошибка.
TEST(DrawingDocumentDimensionLayout, ChainAndBaselineGroups) {
    auto part = makeBracket();
    ModelSnapshot m = snapshotPart(*part, {"РД.01", "Кронштейн", "Сталь 45", 7850}).value();
    DrawingDocument d = sheetWith({base(), top()});
    auto v = [&](double x, double y, double z) { return anchorOn(m, ElementKind::Vertex, vertexAt(m, {x, y, z}), AnchorKind::Point); };
    const auto p0 = v(0, -15, 0), pT = v(8, -15, 50), pA = v(60, -15, 0);
    auto& dims = d.sheets()[0].dimensions;
    auto add = [&](const std::string& id, DimensionType t, std::vector<annotations::Anchor> a, double off,
                   std::vector<annotations::Anchor> pts, const std::string& group, DimensionGroupMode mode) {
        DimensionItem it = dimension(id, "F", t, std::move(a), off);
        it.def.offsetFrom = std::move(pts);
        it.group = group;
        it.groupMode = mode;
        dims.push_back(std::move(it));
    };
    // Цепочка с перепутанными звеньями (как после изменения порядка точек в модели).
    add("C1", DimensionType::Horizontal, {p0, pA}, -10, {p0, pT, pA}, "G1", DimensionGroupMode::Chain);
    add("C2", DimensionType::Horizontal, {pA, pT}, -10, {p0, pT, pA}, "G1", DimensionGroupMode::Chain);
    // От базы x = 0 над изображением; длинный размер ближе к контуру — неверно, будет переставлен.
    add("B1", DimensionType::Horizontal, {p0, pA}, 10, {p0, pT, pA}, "G2", DimensionGroupMode::Baseline);
    add("B2", DimensionType::Horizontal, {p0, pT}, 17, {p0, pT, pA}, "G2", DimensionGroupMode::Baseline);
    dims.push_back(dimension("H", "F", DimensionType::Vertical, {p0, v(0, -15, 50)}, 10));
    vw::ViewCache cache(8, 1);
    auto rep = d.update(m, cache);
    ASSERT_TRUE(rep.ok()) << rep.error().message;
    auto check = [&](double T) {
        const auto& cs = d.computedSheets()[0];
        auto val = values(d);
        EXPECT_NEAR(val.at("C1"), T, 1e-6);
        EXPECT_NEAR(val.at("C2"), 60 - T, 1e-6);
        EXPECT_NEAR(val.at("B1"), 60, 1e-6);
        EXPECT_NEAR(val.at("B2"), T, 1e-6);
        // Цепочка — одна прямая: размерные линии на одной высоте, звенья стыкуются.
        const auto& l1 = cs.dimensions[0].geometry.dimensionLines[0];
        const auto& l2 = cs.dimensions[1].geometry.dimensionLines[0];
        EXPECT_NEAR(l1.a.y, l2.a.y, 1e-9);
        EXPECT_NEAR(std::max(l1.a.x, l1.b.x), std::min(l2.a.x, l2.b.x), 1e-6);
        // От базы: меньший (B2) ближе к изображению на 7 мм.
        const double yB1 = cs.dimensions[2].geometry.dimensionLines[0].a.y, yB2 = cs.dimensions[3].geometry.dimensionLines[0].a.y;
        EXPECT_NEAR(yB1 - yB2, 7.0, 1e-6);
        EXPECT_GT(yB2, cs.views[0].sheetView.sheetBounds().maxY);
    };
    check(8);
    EXPECT_NEAR(dims[2].def.offset, 17, 1e-12);
    EXPECT_NEAR(dims[3].def.offset, 10, 1e-12);
    // Модель: T = 20 — значения новые, группа на месте.
    setVariable(*part, "T", 20);
    ASSERT_TRUE(part->rebuild().failed.empty());
    m = snapshotPart(*part, {"РД.01", "Кронштейн", "Сталь 45", 7850}).value();
    rep = d.update(m, cache);
    ASSERT_TRUE(rep.ok()) << rep.error().message;
    EXPECT_TRUE(rep.value().dangling.empty()) << rep.value().dangling.front();
    check(20);
    // Перенос размера цепочки: вся цепочка на новой линии.
    const Point2 lp = d.computedSheets()[0].dimensions[1].geometry.labels[0].position;
    ASSERT_TRUE(d.moveDimension(m, "S1", "C2", {lp.x, lp.y - 5}).ok());
    EXPECT_NEAR(dims[0].def.offset, dims[1].def.offset, 1e-9);
    EXPECT_LT(dims[0].def.offset, -12);
    // Перенос размера от базы: обе линии, шаг сохраняется.
    const Point2 lb = d.computedSheets()[0].dimensions[3].geometry.labels[0].position;
    ASSERT_TRUE(d.moveDimension(m, "S1", "B2", {lb.x, lb.y + 4}).ok());
    EXPECT_NEAR(dims[2].def.offset - dims[3].def.offset, 7.0, 1e-9);
    EXPECT_GT(dims[3].def.offset, 12);
    // Не переходит на другую сторону изображения.
    ASSERT_TRUE(d.moveDimension(m, "S1", "B2", {lb.x, lb.y - 200}).ok());
    EXPECT_GE(dims[3].def.offset, 2.0 - 1e-9);
    EXPECT_NEAR(dims[2].def.offset - dims[3].def.offset, 7.0, 1e-9);
    EXPECT_FALSE(d.moveDimension(m, "S1", "nope", {0, 0}).ok());
    ASSERT_TRUE(d.update(m, cache).ok());
    check(20);
    // Автокомпоновка не трогает группы.
    const std::vector<double> before{dims[0].def.offset, dims[1].def.offset, dims[2].def.offset, dims[3].def.offset};
    ASSERT_TRUE(d.arrangeDimensions(m, "S1", "F").ok());
    for (std::size_t i = 0; i < 4; ++i) EXPECT_NEAR(dims[i].def.offset, before[i], 1e-12) << i;
    EXPECT_EQ(dims[0].def.offsetFrom.size(), 3u);
    // Файл.
    auto text = d.toJson();
    ASSERT_TRUE(text.ok());
    EXPECT_NE(text.value().find("\"groupMode\": \"baseline\""), std::string::npos);
    auto back = DrawingDocument::fromJson(text.value());
    ASSERT_TRUE(back.ok()) << back.error().message;
    EXPECT_EQ(back.value().sheets()[0].dimensions[2].group, "G2");
    EXPECT_EQ(back.value().sheets()[0].dimensions[2].groupMode, DimensionGroupMode::Baseline);
    EXPECT_EQ(back.value().toJson().value(), text.value());
    std::string bad = text.value();
    const std::size_t at = bad.find("\"groupSpacing\": 7");
    ASSERT_NE(at, std::string::npos);
    bad.replace(at, 17, "\"groupSpacing\": 5");
    EXPECT_FALSE(DrawingDocument::fromJson(bad).ok());
    // Шаг от базы меньше 7 мм — ошибка ядра.
    const auto& fv = d.computedSheets()[0].views[0];
    auto binding = annotations::ModelBinding::fromNamedShape(*m.named);
    ASSERT_TRUE(binding.ok());
    EXPECT_FALSE(annotations::baselineDimensions(DimensionType::Horizontal, p0, {pT}, binding.value(), fv.sheetView, 10, 5).ok());
    EXPECT_TRUE(annotations::baselineDimensions(DimensionType::Horizontal, p0, {pT}, binding.value(), fv.sheetView, 10, 7).ok());
}
