// Лист А3 с видами (разрез А–А на месте главного вида, вид сверху с секущей плоскостью,
// вид слева, изометрия, выносной элемент Б), рамкой и основной надписью: экспорт в DXF и PDF
// существующими писателями, разбор файлов и сверка координат с геометрией модели в масштабе.

#include "test_drawing_views_support.hpp"
#include "test_support.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <cstdlib>
#include <sstream>

using namespace dvtest;
using bendcore::kernel::Shape;

namespace {

const CuttingPlane kPlane{{0, 20, 0}, {0, 1, 0}};

struct ViewsSheet {
    Drawing drawing;
    SheetView section, top, left, iso, detail;
    std::size_t hatchLines = 0;
};

ViewsSheet build(const TrueTypeFont* font) {
    auto must = [](Status s) {
        if (!s) throw std::runtime_error(s.error().message);
    };
    const Shape s = blockWithHole();
    Drawing d = Drawing::create(gostSheet(GostFormat::A3, 1, Orientation::Landscape).value()).value();
    FrameOptions fo;
    fo.font = font;
    TitleBlockData td = dv2test::sampleTitleData();
    td.scale = "2:1";
    if (!addFrameAndTitleBlock(d, td, fo)) throw std::runtime_error("title block");

    HlrOptions noHidden;
    noHidden.hidden = false;
    SectionView sec = sectionShape(s, defaultFrontFrame(), kPlane, SectionKind::Cut, noHidden).value();
    const ProjectedView top = projectShape(s, standardFrame(defaultFrontFrame(), StandardView::Top)).value();
    const ProjectedView left = projectShape(s, standardFrame(defaultFrontFrame(), StandardView::Left)).value();
    const ViewFrame isoFrame = axonometricFrame(defaultFrontFrame(), Axonometry::Isometric);
    const ProjectedView iso = projectShape(s, isoFrame, noHidden).value();

    Box2 area;
    area.add({45, 70});
    area.add({275, 285});
    const Layout lay = layoutViews({{&sec.view, -1, SheetSide::Below},
                                    {&top, 0, SheetSide::Below},
                                    {&left, 0, SheetSide::RightOf}},
                                   area, 25)
                           .value();
    if (lay.scale.label() != "2:1") throw std::runtime_error("unexpected scale " + lay.scale.label());
    const double k = lay.scale.value();
    ViewsSheet out{std::move(d), {sec.view, k, lay.anchors[0]}, {top, k, lay.anchors[1]},
                   {left, k, lay.anchors[2]}, {}, {}};
    const double isoK = axonometricFactor(Axonometry::Isometric, true);
    out.iso = SheetView{iso, isoK, {0, 0}};
    out.iso.anchor = {340 - isoK * 0.5 * (iso.bounds.minX + iso.bounds.maxX),
                      225 - isoK * 0.5 * (iso.bounds.minY + iso.bounds.maxY)};
    const ProjectedView det = detailView(top, {30, 20}, 8).value();
    out.detail = SheetView{det, 4.0, {340 - 4.0 * 30, 115 - 4.0 * 20}};

    Drawing& dr = out.drawing;
    must(renderView(dr, out.section));
    auto spacing = renderSectionHatch(dr, out.section, sec);
    if (!spacing) throw std::runtime_error(spacing.error().message);
    must(renderView(dr, out.top));
    must(renderView(dr, out.left));
    must(renderView(dr, out.iso));
    must(renderView(dr, out.detail));
    must(renderCenterLines(dr, out.top, findCenterGeometry(s, top.frame).value()));
    must(renderCenterLines(dr, out.left, findCenterGeometry(s, left.frame).value()));
    must(renderCenterLines(dr, out.section, findCenterGeometry(sec.remaining, sec.view.frame).value()));
    must(renderCenterLines(dr, out.detail, findCenterGeometry(s, top.frame).value()));
    if (!renderCuttingPlaneMark(dr, out.top, kPlane, "А")) throw std::runtime_error("mark");
    must(renderViewLabel(dr, out.section, "А–А"));
    must(renderDetailMark(dr, out.top, {30, 20}, 8, "Б"));
    must(renderDetailBoundary(dr, out.detail, {30, 20}, 8));
    must(renderViewLabel(dr, out.detail, "Б (4:1)"));
    for (const Entity& e : dr.entities())
        if (auto* h = std::get_if<Hatch>(&e.geometry)) out.hatchLines += hatchSegments(*h).value().size();
    return out;
}

bool dxfHasLine(const std::vector<dv2test::DxfObject>& ents, const std::string& layer,
                const std::string& ltype, Point2 a, Point2 b, double tol = 1e-5) {
    for (const auto& e : ents) {
        if (e.type != "LINE" || e.first(8).value_or("") != layer) continue;
        if (!ltype.empty() && e.first(6).value_or("") != ltype) continue;
        const Point2 p{e.real(10), e.real(20)}, q{e.real(11), e.real(21)};
        if (hasSegment({LineSeg{p, q}}, a, b, tol)) return true;
    }
    return false;
}

// Отрезки «x y m X Y l S» потока страницы PDF, в пунктах.
std::vector<LineSeg> pdfLines(const std::string& stream) {
    std::vector<LineSeg> out;
    std::istringstream is(stream);
    std::vector<std::string> tok;
    std::string t;
    while (is >> t) tok.push_back(t);
    for (std::size_t i = 6; i < tok.size(); ++i) {
        if (tok[i] != "S" || tok[i - 1] != "l" || tok[i - 4] != "m") continue;
        try {
            out.push_back({{std::stod(tok[i - 6]), std::stod(tok[i - 5])}, {std::stod(tok[i - 3]), std::stod(tok[i - 2])}});
        } catch (const std::exception&) {
        }
    }
    return out;
}

}  // namespace

TEST(DrawingViewsExport, SheetToDxf) {
    ViewsSheet v = build(dv2test::systemFont());
    auto dxf = writeDxf(v.drawing);
    ASSERT_TRUE(dxf.ok()) << dxf.error().message;
    const std::string path = dv2test::outPath("views_a3.dxf");
    ASSERT_TRUE(saveToFile(path, dxf.value()).ok());
    const dv2test::DxfFile f = dv2test::parseDxf(dv2test::readFile(path));
    const auto* ents = f.section("ENTITIES");
    ASSERT_NE(ents, nullptr);

    // Вид сверху 2:1: контур 60×40 → 120×80 от точки привязки; окружность Ø10 → R10.
    const Point2 t0 = v.top.toSheet({0, 0}), t1 = v.top.toSheet({60, 0}), t2 = v.top.toSheet({60, 40});
    EXPECT_NEAR(t1.x - t0.x, 120, 1e-12);
    EXPECT_TRUE(dxfHasLine(*ents, "VISIBLE", "", t0, t1));
    EXPECT_TRUE(dxfHasLine(*ents, "VISIBLE", "", t1, t2));
    int holeCircles = 0;
    const Point2 hc = v.top.toSheet({30, 20});
    for (const auto& e : *ents)
        if (e.type == "CIRCLE" && e.first(8).value_or("") == "VISIBLE" && std::abs(e.real(10) - hc.x) < 1e-5 &&
            std::abs(e.real(20) - hc.y) < 1e-5 && std::abs(e.real(40) - 10) < 1e-5)
            ++holeCircles;
    EXPECT_EQ(holeCircles, 1);
    // Вид слева: очерк отверстия — штриховая (GOST_DASHED) на x = −15 и −25 вида.
    EXPECT_TRUE(dxfHasLine(*ents, "HIDDEN", "GOST_DASHED", v.left.toSheet({-15, 0}), v.left.toSheet({-15, 20})));
    EXPECT_TRUE(dxfHasLine(*ents, "HIDDEN", "GOST_DASHED", v.left.toSheet({-25, 0}), v.left.toSheet({-25, 20})));
    // Разрез: стенки отверстия — сплошные основные; осевая — штрихпунктирная тонкая
    // (Ø20 на листе): половины от центра (30, 10) вида длиной 20 + 3.
    EXPECT_TRUE(dxfHasLine(*ents, "VISIBLE", "", v.section.toSheet({25, 0}), v.section.toSheet({25, 20})));
    const Point2 ac = v.section.toSheet({30, 10});
    EXPECT_TRUE(dxfHasLine(*ents, "CENTER", "GOST_DASHDOT_THIN", ac, {ac.x, ac.y + 23}));
    EXPECT_TRUE(dxfHasLine(*ents, "CENTER", "GOST_DASHDOT_THIN", ac, {ac.x, ac.y - 23}));
    // Штриховка — отрезки на слое HATCH, их число равно числу отрезков hatchSegments.
    std::size_t hatch = 0, open = 0;
    for (const auto& e : *ents) {
        if (e.type == "LINE" && e.first(8).value_or("") == "HATCH") ++hatch;
        if (e.type == "LINE" && e.first(8).value_or("") == "SYMBOLS" && e.real(370) == 70) ++open;
    }
    EXPECT_EQ(hatch, v.hatchLines);
    EXPECT_GT(hatch, 10u);
    // Разомкнутая линия: толщина 1,5s = 0,75 мм → 70 в DXF.
    EXPECT_EQ(open, 2u);
    // Обозначения: «А–А», «Б (4:1)», буквы у стрелок.
    std::vector<std::string> texts;
    for (const auto& e : *ents)
        if (e.type == "TEXT") texts.push_back(dv2test::decodeDxfUnicode(e.first(1).value_or("")));
    for (const char* s : {"А–А", "Б (4:1)", "А", "Б"})
        EXPECT_NE(std::find(texts.begin(), texts.end(), s), texts.end()) << s;

    const char* py = std::getenv("BENDCORE_PYTHON_EZDXF");
    if (!py || !*py) GTEST_SKIP() << "ezdxf check skipped: set BENDCORE_PYTHON_EZDXF";
    const std::string script =
        "import sys, ezdxf\n"
        "doc = ezdxf.readfile(sys.argv[1])\n"
        "a = doc.audit()\n"
        "msp = doc.modelspace()\n"
        "x0, y0, x1, y1 = [float(v) for v in sys.argv[2:6]]\n"
        "hit = [l for l in msp.query('LINE[layer==\"VISIBLE\"]') if abs(l.dxf.start.x-x0)+abs(l.dxf.start.y-y0)+"
        "abs(l.dxf.end.x-x1)+abs(l.dxf.end.y-y1) < 1e-5 or abs(l.dxf.start.x-x1)+abs(l.dxf.start.y-y1)+"
        "abs(l.dxf.end.x-x0)+abs(l.dxf.end.y-y0) < 1e-5]\n"
        "t = [ezdxf.decode_dxf_unicode(e.dxf.text) for e in msp.query('TEXT')]\n"
        "print(doc.dxfversion, len(a.errors), len(hit), int('А–А' in t), "
        "len(msp.query('LINE[layer==\"HIDDEN\"]')), doc.header['$INSUNITS'])\n";
    ASSERT_TRUE(saveToFile(dv2test::outPath("views_check_ezdxf.py"), script).ok());
    std::ostringstream cmd;
    cmd.precision(17);
    cmd << py << " " << dv2test::outPath("views_check_ezdxf.py") << " " << path << " " << t0.x << " " << t0.y
        << " " << t1.x << " " << t1.y;
    int code = 0;
    const std::string out = dv2test::runCommand(cmd.str(), &code);
    ASSERT_EQ(code, 0) << out;
    int hidden = 0;
    for (const Entity& e : v.drawing.entities())
        hidden += e.layer == "HIDDEN" && std::holds_alternative<LineSeg>(e.geometry);
    EXPECT_EQ(out, "AC1015 0 1 1 " + std::to_string(hidden) + " 4\n");
}

TEST(DrawingViewsExport, SheetToPdf) {
    const TrueTypeFont* font = dv2test::systemFont();
    if (!font) GTEST_SKIP() << "no system TrueType font (DejaVu Sans / Liberation Sans)";
    ViewsSheet v = build(font);
    PdfOptions po;
    po.font = font;
    po.title = "Брусок — виды";
    auto pdf = writePdf(v.drawing, po);
    ASSERT_TRUE(pdf.ok()) << pdf.error().message;
    const std::string path = dv2test::outPath("views_a3.pdf");
    ASSERT_TRUE(saveToFile(path, pdf.value()).ok());
    const dv2test::PdfFile f = dv2test::parsePdf(pdf.value());
    const auto* page = f.find("/MediaBox");
    ASSERT_NE(page, nullptr);
    EXPECT_NE(page->dict.find("1190.5512"), std::string::npos) << page->dict;
    // Контур вида сверху в пунктах: мм · 72 / 25,4.
    std::vector<LineSeg> lines;
    for (const auto& o : f.objects)
        if (!o.stream.empty()) {
            auto l = pdfLines(o.stream);
            lines.insert(lines.end(), l.begin(), l.end());
        }
    const double pt = 72.0 / 25.4;
    auto toPt = [pt](Point2 p) { return Point2{p.x * pt, p.y * pt}; };
    EXPECT_TRUE(hasSegment(lines, toPt(v.top.toSheet({0, 0})), toPt(v.top.toSheet({60, 0})), 2e-4));
    EXPECT_TRUE(hasSegment(lines, toPt(v.left.toSheet({-15, 0})), toPt(v.left.toSheet({-15, 20})), 2e-4));
    EXPECT_TRUE(hasSegment(lines, toPt(v.section.toSheet({35, 0})), toPt(v.section.toSheet({35, 20})), 2e-4));
#ifdef BENDCORE_PDFTOTEXT
    int code = 0;
    const std::string out =
        dv2test::runCommand(std::string(BENDCORE_PDFTOTEXT) + " -enc UTF-8 " + path + " - 2>&1", &code);
    EXPECT_EQ(code, 0) << out;
    for (const char* s : {"А–А", "Б (4:1)", "Вал ступенчатый", "2:1"})
        EXPECT_NE(out.find(s), std::string::npos) << s << "\n" << out;
#endif
}
