// Сквозные тесты фазы 4 (docs/ROADMAP_GENERAL_CAD.md, критерий «готово»): рабочие чертежи трёх
// эталонных деталей фазы 1 (docs/modeling/REFERENCE_PARTS.md, №1–3), построенных через документ
// детали: 3 вида + изометрия, разрез/сечение, размеры, шероховатость, основная надпись.
// Проверки: значения размеров = параметры модели (аналитика), длины размерных линий в DXF =
// значение · масштаб листа, изменение параметра модели → обновлённый чертёж с новыми
// значениями и отчётом об устаревших размерах, повторное обновление — из кэша; PDF —
// pdfinfo (число страниц, формат), pdftotext (кириллица основной надписи, «А–А»); DXF — ezdxf.

#include "test_drawing_document_support.hpp"

#include <bendcore/drawing_v2/annotation_style.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <functional>
#include <map>

using namespace ddtest;
using bendcore::modeling::ElementKind;
using bendcore::modeling::PartDocument;
using annotations::AnchorKind;
using annotations::DimensionType;

namespace {

constexpr double kPi = 3.14159265358979323846;

struct Expected {
    std::string id;
    double value;
    bool linear;      // проверять длину размерной линии
    bool horizontal;  // ориентация размерной линии
};

struct PartCase {
    std::string base;
    std::unique_ptr<PartDocument> doc;
    ModelProperties props;
    SheetFormat format;
    std::string sectionLabel = "А–А";
    std::function<void(SheetDef&)> views;
    std::function<void(SheetDef&, const ModelSnapshot&)> annotate;
    std::vector<Expected> expected;
    std::map<std::string, double> change;
    std::vector<Expected> expectedAfter;
    std::function<double(const PartDocument&)> volume;  // аналитический объём, мм³
};

void standardViews(SheetDef& s, const views::Vec3& secToward, const views::Vec3& secRight, const views::Vec3& planePoint,
                   const std::string& markView, views::SectionKind kind) {
    ViewDef f;
    f.id = "F";
    s.views.push_back(f);
    ViewDef t;
    t.id = "T";
    t.kind = ViewKind::Projected;
    t.parent = "F";
    t.side = views::SheetSide::Below;
    s.views.push_back(t);
    ViewDef l;
    l.id = "L";
    l.kind = ViewKind::Projected;
    l.parent = "F";
    l.side = views::SheetSide::RightOf;
    s.views.push_back(l);
    ViewDef iso;
    iso.id = "I";
    iso.kind = ViewKind::Axonometric;
    iso.parent = "F";
    iso.hidden = false;
    s.views.push_back(iso);
    ViewDef sec;
    sec.id = "S";
    sec.kind = ViewKind::Section;
    sec.toward = secToward;
    sec.right = secRight;
    sec.planePoint = planePoint;
    sec.markView = markView;
    sec.sectionKind = kind;
    sec.hidden = false;
    s.views.push_back(sec);
}

double var(const PartDocument& d, const char* n) { return d.variableValue(n).value(); }

PartCase bracketCase() {
    PartCase c;
    c.base = "bracket";
    c.doc = makeBracket();
    c.props = {"РД.10.001", "Кронштейн", "Сталь 20 ГОСТ 1050-2013", 7850.0};
    c.format = {GostFormat::A3, 1, Orientation::Landscape};
    c.views = [](SheetDef& s) { standardViews(s, {-1, 0, 0}, {0, -1, 0}, {40, 0, 0}, "F", views::SectionKind::Cut); };
    c.annotate = [](SheetDef& s, const ModelSnapshot& m) {
        auto v = [&](double x, double y, double z) { return anchorOn(m, ElementKind::Vertex, vertexAt(m, {x, y, z}), AnchorKind::Point); };
        DimensionItem a = dimension("A", "F", DimensionType::Horizontal, {v(0, -15, 0), v(60, -15, 0)}, -24);
        a.def.textShift = 15;  // число в стороне от буквы обозначения разреза
        s.dimensions.push_back(a);
        s.dimensions.push_back(dimension("B", "F", DimensionType::Vertical, {v(0, -15, 0), v(0, -15, 50)}, 12));
        s.dimensions.push_back(dimension("T", "F", DimensionType::Vertical, {v(60, -15, 0), v(60, -15, 8)}, -12));
        s.dimensions.push_back(dimension("Wd", "T", DimensionType::Vertical, {v(60, -15, 8), v(60, 15, 8)}, -12));
        DimensionItem d = dimension("d", "T", DimensionType::Diameter,
                                    {anchorOn(m, ElementKind::Edge, circleEdge(m, {40, 0, 8}, 4), AnchorKind::Center)});
        d.def.leaderAngleDeg = 60;
        s.dimensions.push_back(d);
        s.dimensions.push_back(dimension("hx", "T", DimensionType::Horizontal,
                                         {v(0, -15, 50), anchorOn(m, ElementKind::Edge, circleEdge(m, {40, 0, 8}, 4), AnchorKind::Center)},
                                         -12));
        RoughnessItem r;
        r.id = "Ra1";
        r.view = "F";
        r.edge = elementReference(m, ElementKind::Edge, *lineEdge(m, {0, -15, 0}, {60, -15, 0})).value();
        r.at = 0.8;
        r.spec.value = "3,2";
        s.roughness.push_back(r);
    };
    c.expected = {{"A", 60, true, true}, {"B", 50, true, false}, {"T", 8, false, false},
                  {"Wd", 30, true, false}, {"d", 8, false, false}, {"hx", 40, true, true}};
    c.change = {{"A", 80}, {"d", 10}};
    c.expectedAfter = {{"A", 80, true, true}, {"B", 50, true, false}, {"d", 10, false, false}, {"hx", 40, true, true}};
    c.volume = [](const PartDocument& d) {
        const double A = var(d, "A"), B = var(d, "B"), T = var(d, "T"), Wd = var(d, "Wd"), r = var(d, "r"), dd = var(d, "d");
        return (A * T + (B - T) * T) * Wd + (1 - kPi / 4) * r * r * Wd - kPi * dd * dd / 4 * T;
    };
    return c;
}

PartCase shaftCase() {
    PartCase c;
    c.base = "shaft";
    c.doc = makeShaft();
    c.props = {"РД.10.002", "Вал", "Сталь 45 ГОСТ 1050-2013", 7850.0};
    c.format = {GostFormat::A3, 1, Orientation::Landscape};
    c.views = [](SheetDef& s) { standardViews(s, {-1, 0, 0}, {0, -1, 0}, {25, 0, 0}, "F", views::SectionKind::Slice); };
    c.annotate = [](SheetDef& s, const ModelSnapshot& m) {
        auto v = [&](double x, double y, double z) { return anchorOn(m, ElementKind::Vertex, vertexAt(m, {x, y, z}), AnchorKind::Point); };
        s.dimensions.push_back(dimension("L", "F", DimensionType::Horizontal, {v(0, 0, 13.5), v(100, 0, 8.5)}, 14));
        DimensionItem d1 = dimension("D1", "F", DimensionType::Diameter,
                                     {anchorOn(m, ElementKind::Face, cylinderFace(m, 15, {0, 0, 0}, {1, 0, 0}), AnchorKind::Center)});
        s.dimensions.push_back(d1);
        DimensionItem d2 = dimension("D2", "F", DimensionType::Diameter,
                                     {anchorOn(m, ElementKind::Face, cylinderFace(m, 10, {0, 0, 0}, {1, 0, 0}), AnchorKind::Center)});
        s.dimensions.push_back(d2);
        s.dimensions.push_back(dimension("lk", "T", DimensionType::Horizontal, {v(10, -3, 11), v(40, -3, 11)}, -20));
        RoughnessItem r;
        r.id = "Ra1";
        r.view = "F";
        r.edge = elementReference(m, ElementKind::Edge, *circleEdge(m, {0, 0, 0}, 13.5)).value();
        r.spec.value = "1,6";
        s.roughness.push_back(r);
    };
    c.expected = {{"L", 100, true, true}, {"D1", 30, false, false}, {"D2", 20, false, false}, {"lk", 30, true, true}};
    c.change = {{"L", 120}};
    c.expectedAfter = {{"L", 120, true, true}, {"D1", 30, false, false}, {"lk", 30, true, true}};
    return c;
}

PartCase flangeCase() {
    PartCase c;
    c.base = "flange";
    c.doc = makeFlange();
    c.props = {"РД.10.003", "Фланец", "Сталь 20 ГОСТ 1050-2013", 7850.0};
    c.format = {GostFormat::A2, 1, Orientation::Landscape};
    c.views = [](SheetDef& s) { standardViews(s, {0, -1, 0}, {1, 0, 0}, {0, 0, 0}, "T", views::SectionKind::Cut); };
    c.annotate = [](SheetDef& s, const ModelSnapshot& m) {
        auto circ = [&](double x, double y, double z, double r, AnchorKind k) {
            return anchorOn(m, ElementKind::Edge, circleEdge(m, {x, y, z}, r), k);
        };
        auto diam = [&](const std::string& id, annotations::Anchor a, double angle) {
            DimensionItem d = dimension(id, "T", DimensionType::Diameter, {std::move(a)});
            d.def.leaderAngleDeg = angle;
            s.dimensions.push_back(d);
        };
        diam("D", circ(0, 0, 12, 60, AnchorKind::Center), 45);
        diam("Dh", circ(0, 0, 27, 35, AnchorKind::Center), 135);
        diam("d0", circ(0, 0, 27, 20, AnchorKind::Center), 225);
        diam("dh", circ(47.5, 0, 12, 5, AnchorKind::Center), 300);
        s.dimensions.push_back(dimension("H", "F", DimensionType::Vertical,
                                         {circ(0, 0, 0, 58.5, AnchorKind::Start), circ(0, 0, 27, 35, AnchorKind::Start)}, 15));
        s.dimensions.push_back(dimension("h2", "F", DimensionType::Vertical,
                                         {circ(0, 0, 12, 60, AnchorKind::Start), circ(0, 0, 27, 35, AnchorKind::Start)}, 28));
        RoughnessItem r;
        r.id = "Ra1";
        r.view = "F";
        r.edge = elementReference(m, ElementKind::Edge, *circleEdge(m, {0, 0, 27}, 35)).value();
        r.at = 0.7;
        r.spec.value = "1,6";
        s.roughness.push_back(r);
    };
    c.expected = {{"D", 120, false, false}, {"Dh", 70, false, false}, {"d0", 40, false, false},
                  {"dh", 10, false, false}, {"H", 27, true, false},   {"h2", 15, true, false}};
    c.change = {{"D", 140}};
    c.expectedAfter = {{"D", 140, false, false}, {"Dh", 70, false, false}, {"H", 27, true, false}};
    return c;
}

std::string text(double v) { return annotations::formatNumber(v, 2); }

void checkValues(const DrawingDocument& doc, const std::vector<Expected>& exp, const std::string& dxfPath, double scale) {
    const DxfContent dxf = readDxf(dxfPath);
    for (const Expected& e : exp) {
        const auto* r = dimensionResult(doc, "S1", e.id);
        ASSERT_NE(r, nullptr) << e.id;
        ASSERT_EQ(r->state, annotations::DimensionState::Ok) << e.id << ": " << r->reason;
        EXPECT_NEAR(r->value, e.value, 1e-6) << e.id;
        const bool diameter = r->text.rfind("⌀", 0) == 0;
        EXPECT_EQ(r->text, (diameter ? "⌀" : "") + text(e.value)) << e.id;
        EXPECT_TRUE(hasText(dxf, r->text)) << e.id << " text '" << r->text << "' not in " << dxfPath;
        if (e.linear) {
            // Размерная линия на листе = значение модели · масштаб листа.
            EXPECT_TRUE(hasDimLine(dxf, e.value * scale, e.horizontal)) << e.id << ": no DIM line of " << e.value * scale;
        }
    }
}

std::string pdfText(const std::string& path) {
#ifdef BENDCORE_PDFTOTEXT
    int code = 0;
    std::string out = dv2test::runCommand(std::string(BENDCORE_PDFTOTEXT) + " -layout '" + path + "' -", &code);
    EXPECT_EQ(code, 0);
    return out;
#else
    (void)path;
    return {};
#endif
}

void runCase(PartCase c) {
    const TrueTypeFont* font = dv2test::systemFont();
    if (!font) GTEST_SKIP() << "no system TrueType font";
    auto snap = snapshotPart(*c.doc, c.props);
    ASSERT_TRUE(snap.ok()) << snap.error().message;

    DrawingDocument doc;
    doc.model().kind = ModelKind::Part;
    doc.model().path = c.base + ".json";
    doc.model().properties = c.props;
    doc.setFont(font);
    SheetDef s;
    s.id = "S1";
    s.format = c.format;
    s.title.organization = "bendcore";
    s.title.developer = {"Иванов", "11.26"};
    c.views(s);
    c.annotate(s, snap.value());
    s.roughnessRemainder = annotations::RoughnessSpec{};
    s.roughnessRemainder->value = "6,3";
    s.technicalRequirements = {"* Размеры для справок.", "Неуказанные предельные отклонения размеров: H14, h14, ±IT14/2.",
                               "Острые кромки притупить."};
    doc.sheets().push_back(std::move(s));

    views::ViewCache cache(32, 4);
    auto rep = doc.update(snap.value(), cache);
    ASSERT_TRUE(rep.ok()) << rep.error().message;
    EXPECT_TRUE(rep.value().modelChanged);  // первое обновление
    EXPECT_EQ(rep.value().viewsComputed, 5u);  // F, T, L, I (HLR) + разрез
    EXPECT_TRUE(rep.value().dangling.empty()) << rep.value().dangling.front();
    EXPECT_TRUE(rep.value().stale.empty());
    const ComputedSheet cs = doc.computedSheets()[0];  // копия: update заменяет вычисленные листы
    ASSERT_EQ(cs.views.size(), 5u);
    const double scale = cs.scale.value();
    for (const ComputedView& v : cs.views) {
        const double expectScale = v.kind == ViewKind::Axonometric ? v.scale.value() * std::sqrt(1.5) : v.scale.value();
        EXPECT_NEAR(v.sheetView.scale, expectScale, 1e-12) << v.id;
        // Вид целиком в рамке листа.
        const views::Box2 b = v.sheetView.sheetBounds();
        EXPECT_GT(b.minX, 20.0) << v.id;
        EXPECT_LT(b.maxX, cs.sheet.widthMm - 5.0) << v.id;
        EXPECT_GT(b.minY, 5.0) << v.id;
        EXPECT_LT(b.maxY, cs.sheet.heightMm - 5.0) << v.id;
    }
    EXPECT_NEAR(cs.views[1].sheetView.anchor.x, cs.views[0].sheetView.anchor.x, 1e-9);  // вид сверху под главным
    EXPECT_NEAR(cs.views[2].sheetView.anchor.y, cs.views[0].sheetView.anchor.y, 1e-9);  // вид слева справа
    EXPECT_EQ(cs.views[4].label.rfind(c.sectionLabel, 0), 0u) << cs.views[4].label;
    ASSERT_TRUE(cs.views[4].section);
    EXPECT_GT(cs.views[4].section->sectionArea, 0.0);
    ASSERT_EQ(cs.roughness.size(), 1u);
    EXPECT_TRUE(cs.roughness[0].has_value());
    // Основная надпись — из свойств модели.
    EXPECT_EQ(cs.title.designation, c.props.designation);
    EXPECT_EQ(cs.title.name, c.props.name);
    EXPECT_EQ(cs.title.material, c.props.material);
    EXPECT_EQ(cs.title.scale, cs.scale.label());
    const double vol = std::abs(snap.value().shape.volumeProperties().value().volume);
    if (c.volume) EXPECT_NEAR(vol, c.volume(*c.doc), 1e-6 * vol);
    EXPECT_EQ(cs.title.mass, annotations::formatNumber(vol * 1e-9 * 7850.0, vol * 1e-9 * 7850.0 < 1 ? 3 : 2));

    const std::string dir = dv2test::outPath(c.base);
    ExportKitOptions ko;
    ko.directory = dir;
    ko.baseName = c.base;
    ko.font = font;
    auto kit = exportKit(doc, nullptr, ko);
    ASSERT_TRUE(kit.ok()) << kit.error().message;
    EXPECT_EQ(kit.value().drawingPages, 1);
    ASSERT_EQ(kit.value().files.size(), 3u);  // PDF, DXF, SVG
    const std::string pdf = dir + "/" + c.base + ".pdf", dxf = dir + "/" + c.base + "_S1.dxf";
    checkValues(doc, c.expected, dxf, scale);
    const DxfContent dc = readDxf(dxf);
    EXPECT_TRUE(hasText(dc, c.props.designation));
    EXPECT_TRUE(hasText(dc, "Ra 3,2") || hasText(dc, "Ra 1,6"));
    EXPECT_NEAR(dc.limMaxX, cs.sheet.widthMm, 1e-6);
    EXPECT_NEAR(dc.limMaxY, cs.sheet.heightMm, 1e-6);
    if (std::string err = ezdxfCheck(dxf, c.props.designation); !err.empty()) ADD_FAILURE() << err;
#ifdef BENDCORE_PDFINFO
    {
        int code = 0;
        const std::string info = dv2test::runCommand(std::string(BENDCORE_PDFINFO) + " '" + pdf + "'", &code);
        EXPECT_EQ(code, 0);
        EXPECT_NE(info.find("Pages:           1\n"), std::string::npos) << info;
        const std::string fmt = c.format.base == GostFormat::A2 ? "1683.78 x 1190.55 pts (A2)" : "1190.55 x 841.89 pts (A3)";
        EXPECT_NE(info.find(fmt), std::string::npos) << info;
    }
#endif
    if (const std::string t = pdfText(pdf); !t.empty()) {
        for (const std::string& want : {c.props.designation, c.props.name, std::string("Сталь"), c.sectionLabel,
                                        std::string("Острые кромки притупить."), std::string("Иванов"), cs.title.mass})
            EXPECT_NE(t.find(want), std::string::npos) << "pdftotext: '" << want << "' missing";
    }

    // Повторное обновление без изменений — всё из кэша, значения те же.
    auto again = doc.update(snap.value(), cache);
    ASSERT_TRUE(again.ok()) << again.error().message;
    EXPECT_FALSE(again.value().modelChanged);
    EXPECT_EQ(again.value().viewsComputed, 0u);
    EXPECT_EQ(again.value().viewsFromCache, 5u);
    EXPECT_TRUE(again.value().stale.empty());

    // Сохранение → загрузка: те же определения, обновление даёт те же значения.
    auto json = doc.toJson();
    ASSERT_TRUE(json.ok());
    auto loaded = DrawingDocument::fromJson(json.value());
    ASSERT_TRUE(loaded.ok()) << loaded.error().message;
    EXPECT_EQ(loaded.value().toJson().value(), json.value());

    // Изменение параметра модели → чертёж с новыми значениями размеров.
    for (const auto& [k, v] : c.change) setVariable(*c.doc, k, v);
    ASSERT_TRUE(c.doc->rebuild().failed.empty());
    auto snap2 = snapshotPart(*c.doc, c.props);
    ASSERT_TRUE(snap2.ok());
    const StalenessReport stale = doc.checkModel(snap2.value());
    EXPECT_TRUE(stale.modelChanged);
    EXPECT_EQ(stale.staleViews.size(), 5u);
    auto rep2 = doc.update(snap2.value(), cache);
    ASSERT_TRUE(rep2.ok()) << rep2.error().message;
    EXPECT_TRUE(rep2.value().modelChanged);
    EXPECT_EQ(rep2.value().revision, 2u);
    EXPECT_EQ(rep2.value().viewsComputed, 5u);
    EXPECT_TRUE(rep2.value().dangling.empty()) << rep2.value().dangling.front();
    std::size_t changed = 0;
    for (const DimensionReport& d : rep2.value().dimensions) {
        EXPECT_EQ(d.state, annotations::DimensionState::Ok) << d.id << ": " << d.reason;
        EXPECT_NE(d.method, bendcore::modeling::ResolveMethod::Broken) << d.id;
        if (d.changed) ++changed;
    }
    EXPECT_EQ(rep2.value().stale.size(), changed);
    EXPECT_GE(changed, c.change.size()) << "the changed parameters must change dimensions";
    auto kit2 = exportKit(doc, nullptr, ko);
    ASSERT_TRUE(kit2.ok()) << kit2.error().message;
    checkValues(doc, c.expectedAfter, dxf, doc.computedSheets()[0].scale.value());
    std::printf("[drawing document] %-8s scale %s, %zu dims, mass %s kg; after change: %s\n", c.base.c_str(),
                cs.scale.label().c_str(), doc.sheets()[0].dimensions.size(), cs.title.mass.c_str(),
                rep2.value().stale.empty() ? "-" : rep2.value().stale.front().c_str());
}

}  // namespace

TEST(DrawingDocumentParts, BracketWorkingDrawing) { runCase(bracketCase()); }
TEST(DrawingDocumentParts, ShaftWorkingDrawing) { runCase(shaftCase()); }
TEST(DrawingDocumentParts, FlangeWorkingDrawing) { runCase(flangeCase()); }
