// Лист А3 с видами, размерами и аннотациями → DXF/PDF/SVG; спецификация → многостраничный
// PDF. Разбор файлов: сущности DXF и их координаты против вычисленной геометрии размеров
// (масштаб листа), SOLID стрелок, тексты; ezdxf (audit), pdftotext, pdfinfo — если доступны.

#include "test_annotations_support.hpp"
#include "test_support.hpp"

#include <bendcore/drawing_v2/annotations.hpp>
#include <bendcore/drawing_v2/export.hpp>
#include <bendcore/drawing_v2/specification.hpp>

#include <BRepPrimAPI_MakeBox.hxx>

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <sstream>

using namespace annot_test;

namespace {

struct AnnotatedSheet {
    Drawing drawing;
    DimensionResult length;  // 60 на виде сверху
    std::size_t fills = 0;   // число залитых многоугольников
    std::size_t fillTriangles = 0;
};

AnnotatedSheet build(const TrueTypeFont* font) {
    BlockPart part;
    const ModelBinding m = part.binding();
    Drawing d = Drawing::create(gostSheet(GostFormat::A3, 1, Orientation::Landscape).value()).value();
    TitleBlockData td;
    td.designation = "РД.00.001";
    td.name = "Корпус";
    td.scale = "2:1";
    FrameOptions fo;
    fo.font = font;
    auto lay = addFrameAndTitleBlock(d, td, fo).value();
    const ViewFrame front = defaultFrontFrame(), top = standardFrame(front, StandardView::Top);
    const SheetView fv{projectShape(m.shape(), front).value(), 2.0, {60, 150}};
    const SheetView tv{projectShape(m.shape(), top).value(), 2.0, {60, 50}};
    EXPECT_TRUE(renderView(d, fv).ok());
    EXPECT_TRUE(renderView(d, tv).ok());
    AnnotationStyle st;
    st.font = font;

    const auto eL = lineEdge(m, {0, 0, 20}, {60, 0, 20});
    const auto c = circleEdge(m, {30, 20, 20}, 5);
    const auto cyl = cylinderFace(m, 5);
    const auto eH = lineEdge(m, {0, 0, 0}, {0, 0, 20});
    DimensionSet topDims, frontDims;
    DimensionDef L;
    L.type = DimensionType::Horizontal;
    L.anchors = {anchor(m, RefKind::Edge, *eL, AnchorKind::Start), anchor(m, RefKind::Edge, *eL, AnchorKind::End)};
    L.offset = -12;
    topDims.add(L);
    DimensionDef D;
    D.type = DimensionType::Diameter;
    D.anchors = {anchor(m, RefKind::Edge, *c, AnchorKind::Center)};
    D.tolerance = parseToleranceClass("H7").value();
    topDims.add(D);
    DimensionDef Ds;
    Ds.type = DimensionType::Diameter;
    Ds.anchors = {anchor(m, RefKind::Face, *cyl, AnchorKind::Center)};
    Ds.offset = 10;
    Ds.tolerance = parseToleranceClass("H7").value();
    Ds.toleranceDisplay = ToleranceDisplay::Deviations;
    frontDims.add(Ds);
    DimensionDef H;
    H.type = DimensionType::Vertical;
    H.anchors = {anchor(m, RefKind::Edge, *eH, AnchorKind::Start), anchor(m, RefKind::Edge, *eH, AnchorKind::End)};
    frontDims.add(H);
    EXPECT_TRUE(topDims.evaluate(m, tv, st).ok());
    EXPECT_TRUE(frontDims.evaluate(m, fv, st).ok());
    for (const auto& r : topDims.results()) EXPECT_EQ(r.state, DimensionState::Ok) << r.reason;
    for (const auto& r : frontDims.results()) EXPECT_EQ(r.state, DimensionState::Ok) << r.reason;
    EXPECT_EQ(topDims.render(d, st).value(), 2u);
    EXPECT_EQ(frontDims.render(d, st).value(), 2u);

    RoughnessSpec ra;
    ra.value = "3,2";
    EXPECT_TRUE(renderSymbol(d, roughnessSign(fv.toSheet({15, 20}), 0.0, ra, st).value(), st).ok());
    RoughnessSpec rest;
    rest.value = "6,3";
    EXPECT_TRUE(renderSymbol(d, roughnessRemainder(lay.frame, rest, st).value(), st).ok());
    FeatureControlFrame f;
    f.characteristic = GeometricCharacteristic::Perpendicularity;
    f.tolerance = "0,02";
    f.diameterZone = true;
    f.datums = {"А"};
    auto fr = featureControlFrame({200, 200}, f, st).value();
    EXPECT_TRUE(renderSymbol(d, fr.geometry, st).ok());
    EXPECT_TRUE(renderSymbol(d, frameLeader({{fr.box.x0, 203.5}, fv.toSheet({30, 22})}, st).value(), st).ok());
    EXPECT_TRUE(renderSymbol(d, datumSymbol(fv.toSheet({45, 0}), 180.0, "А", 5.0, st).value(), st).ok());
    TechnicalRequirementsOptions to;
    EXPECT_TRUE(addTechnicalRequirements(d, lay, {"Острые кромки притупить.", "Неуказанные предельные отклонения H14, h14, ±IT14/2."}, to, st).ok());

    AnnotatedSheet out{std::move(d), topDims.results()[0]};
    for (const auto& e : out.drawing.entities()) {
        if (const auto* s = std::get_if<SolidFill>(&e.geometry)) {
            ++out.fills;
            out.fillTriangles += s->points.size() - 2;
        }
    }
    return out;
}

}  // namespace

TEST(AnnotationsExport, SheetWithDimensionsToDxf) {
    AnnotatedSheet s = build(dv2test::systemFont());
    auto dxf = writeDxf(s.drawing);
    ASSERT_TRUE(dxf.ok()) << dxf.error().message;
    const std::string path = dv2test::outPath("annotations_a3.dxf");
    ASSERT_TRUE(saveToFile(path, dxf.value()).ok());
    const auto file = dv2test::parseDxf(dxf.value());
    const auto* ents = file.section("ENTITIES");
    ASSERT_NE(ents, nullptr);
    std::size_t solids = 0;
    std::vector<std::string> texts;
    bool dimLine = false;
    const LineSeg want = s.length.geometry.dimensionLines[0];
    for (const auto& e : *ents) {
        if (e.type == "SOLID") {
            ++solids;
            EXPECT_EQ(e.first(8).value_or(""), "DIM");
        }
        if (e.type == "TEXT") texts.push_back(dv2test::decodeDxfUnicode(e.first(1).value_or("")));
        if (e.type == "LINE" && e.first(8).value_or("") == "DIM") {
            const double x0 = e.real(10), y0 = e.real(20), x1 = e.real(11), y1 = e.real(21);
            if (std::abs(x0 - want.a.x) + std::abs(y0 - want.a.y) + std::abs(x1 - want.b.x) + std::abs(y1 - want.b.y) < 1e-6) {
                dimLine = true;
                // Размерная линия длины 60 при 2:1 — 120 мм листа, на 12 мм ниже вида (y = 50).
                EXPECT_NEAR(std::abs(x1 - x0), 120.0, 1e-6);
                EXPECT_NEAR(y0, 38.0, 1e-6);
                EXPECT_EQ(e.real(370), 25);  // тонкая s/2 = 0,25 мм
            }
        }
    }
    EXPECT_TRUE(dimLine);
    EXPECT_EQ(solids, s.fillTriangles);
    EXPECT_GE(s.fills, 8u);
    for (const char* t : {"60", "⌀10H7", "⌀10", "+0,015", "20", "Ra 3,2", "Ra 6,3", "⌀0,02", "А",
                          "1. Острые кромки притупить."}) {
        EXPECT_NE(std::find(texts.begin(), texts.end(), t), texts.end()) << t;
    }

    const char* py = std::getenv("BENDCORE_PYTHON_EZDXF");
    if (!py || !*py) GTEST_SKIP() << "ezdxf check skipped: set BENDCORE_PYTHON_EZDXF";
    const std::string script =
        "import sys, ezdxf\n"
        "doc = ezdxf.readfile(sys.argv[1])\n"
        "a = doc.audit()\n"
        "msp = doc.modelspace()\n"
        "t = [ezdxf.decode_dxf_unicode(e.dxf.text) for e in msp.query('TEXT')]\n"
        "print(doc.dxfversion, len(a.errors), len(msp.query('SOLID')), int('⌀10H7' in t), int('Ra 3,2' in t))\n";
    ASSERT_TRUE(saveToFile(dv2test::outPath("annotations_check_ezdxf.py"), script).ok());
    int code = 0;
    const std::string out =
        dv2test::runCommand(std::string(py) + " " + dv2test::outPath("annotations_check_ezdxf.py") + " " + path, &code);
    ASSERT_EQ(code, 0) << out;
    EXPECT_EQ(out, "AC1015 0 " + std::to_string(solids) + " 1 1\n");
}

TEST(AnnotationsExport, SheetWithDimensionsToPdfAndSvg) {
    const TrueTypeFont* font = dv2test::systemFont();
    if (!font) GTEST_SKIP() << "no system TrueType font";
    AnnotatedSheet s = build(font);
    PdfOptions po;
    po.font = font;
    auto pdf = writePdf(s.drawing, po);
    ASSERT_TRUE(pdf.ok()) << pdf.error().message;
    const std::string path = dv2test::outPath("annotations_a3.pdf");
    ASSERT_TRUE(saveToFile(path, pdf.value()).ok());
    const auto f = dv2test::parsePdf(pdf.value());
    // Стрелки — залитые контуры (оператор f); размерная линия в пунктах.
    std::size_t fills = 0;
    bool line = false;
    const double pt = 72.0 / 25.4;
    const LineSeg w = s.length.geometry.dimensionLines[0];
    char buf[128];
    std::snprintf(buf, sizeof buf, "%.4f %.4f m %.4f %.4f l S", w.a.x * pt, w.a.y * pt, w.b.x * pt, w.b.y * pt);
    std::string want = buf;
    // num() удаляет хвостовые нули: сравниваем по числам.
    for (const auto& o : f.objects) {
        for (std::size_t p = o.stream.find("h f\n"); p != std::string::npos; p = o.stream.find("h f\n", p + 1)) ++fills;
        std::istringstream is(o.stream);
        std::string ln;
        while (std::getline(is, ln)) {
            double a, b, c, d;
            char m1[4], l1[4], s1[4];
            if (std::sscanf(ln.c_str(), "%lf %lf %3s %lf %lf %3s %3s", &a, &b, m1, &c, &d, l1, s1) == 7 &&
                std::string(m1) == "m" && std::abs(a - w.a.x * pt) < 1e-3 && std::abs(b - w.a.y * pt) < 1e-3 &&
                std::abs(c - w.b.x * pt) < 1e-3 && std::abs(d - w.b.y * pt) < 1e-3)
                line = true;
        }
    }
    EXPECT_EQ(fills, s.fills);
    EXPECT_TRUE(line) << want;
#ifdef BENDCORE_PDFTOTEXT
    int code = 0;
    const std::string out = dv2test::runCommand(std::string(BENDCORE_PDFTOTEXT) + " -enc UTF-8 " + path + " - 2>&1", &code);
    EXPECT_EQ(code, 0) << out;
    for (const char* t : {"Ra 3,2", "⌀10H7", "+0,015", "1. Острые кромки притупить.", "Корпус"})
        EXPECT_NE(out.find(t), std::string::npos) << t << "\n" << out;
#endif
    SvgOptions so;
    so.font = font;
    auto svg = writeSvg(s.drawing, so);
    ASSERT_TRUE(svg.ok());
    std::size_t polys = 0;
    for (std::size_t p = svg.value().find("data-linetype=\"fill\""); p != std::string::npos;
         p = svg.value().find("data-linetype=\"fill\"", p + 1))
        ++polys;
    EXPECT_EQ(polys, s.fills);
    ASSERT_TRUE(saveToFile(dv2test::outPath("annotations_a3.svg"), svg.value()).ok());
}

TEST(AnnotationsExport, SpecificationMultiPagePdf) {
    const TrueTypeFont* font = dv2test::systemFont();
    if (!font) GTEST_SKIP() << "no system TrueType font";
    namespace asm2 = bendcore::assembly_v2;
    asm2::AssemblyDocument doc("Unit");
    ASSERT_TRUE(doc.setProperties(doc.root(), {"РД.00.000", "Узел", "", std::nullopt}).ok());
    const auto I = asm2::Transform::identity();
    auto part = [&](const char* des, const std::string& name, int qty) {
        auto d = doc.addPart(name, {BRepPrimAPI_MakeBox(5, 5, 5).Shape(), nullptr}, {des, name, "Сталь 20", std::nullopt});
        for (int i = 0; i < qty; ++i) ASSERT_TRUE(doc.addOccurrence(d.value(), I).ok());
    };
    part("РД.00.001", "Основание", 1);
    part("РД.00.002", "Планка", 2);
    for (int i = 0; i < 35; ++i) part("", "Винт М4×" + std::to_string(6 + i) + " ГОСТ 17473-80", 2);
    auto spec = buildSpecification(doc).value();
    SpecSheetOptions so;
    so.font = font;
    auto sd = renderSpecification(spec, so);
    ASSERT_TRUE(sd.ok()) << sd.error().message;
    ASSERT_GE(sd.value().sheets.size(), 2u);
    const std::string pagesCount = std::to_string(sd.value().sheets.size());
    std::vector<const Drawing*> pages;
    for (const auto& s : sd.value().sheets) pages.push_back(&s);
    PdfOptions po;
    po.font = font;
    po.title = "РД.00.000 Спецификация";
    auto pdf = writePdfPages(pages, po);
    ASSERT_TRUE(pdf.ok()) << pdf.error().message;
    const std::string path = dv2test::outPath("specification.pdf");
    ASSERT_TRUE(saveToFile(path, pdf.value()).ok());
    const auto f = dv2test::parsePdf(pdf.value());
    const auto* pagesObj = f.find("/Type /Pages");
    ASSERT_NE(pagesObj, nullptr);
    EXPECT_NE(pagesObj->dict.find("/Count " + pagesCount), std::string::npos);
    // А4 книжный: 595,2756 × 841,8898 пт.
    EXPECT_NE(f.find("/MediaBox [0 0 595.2756 841.8898]"), nullptr);
    // DXF каждого листа: тексты граф и позиций.
    auto dxf = writeDxf(sd.value().sheets[0]);
    ASSERT_TRUE(dxf.ok());
    ASSERT_TRUE(saveToFile(dv2test::outPath("specification_1.dxf"), dxf.value()).ok());
#ifdef BENDCORE_PDFINFO
    int code = 0;
    const std::string info = dv2test::runCommand(std::string(BENDCORE_PDFINFO) + " " + path + " 2>&1", &code);
    EXPECT_NE(info.find("Pages:           " + pagesCount), std::string::npos) << info;
#endif
#ifdef BENDCORE_PDFTOTEXT
    int c2 = 0;
    const std::string out = dv2test::runCommand(std::string(BENDCORE_PDFTOTEXT) + " -enc UTF-8 " + path + " - 2>&1", &c2);
    const auto pDoc = out.find("Документация"), pParts = out.find("Детали"), pStd = out.find("Стандартные изделия");
    ASSERT_NE(pDoc, std::string::npos) << out;
    ASSERT_NE(pParts, std::string::npos);
    ASSERT_NE(pStd, std::string::npos);
    EXPECT_LT(pDoc, pParts);
    EXPECT_LT(pParts, pStd);
    EXPECT_NE(out.find("Сборочный чертёж"), std::string::npos);
    EXPECT_NE(out.find("РД.00.000 СБ"), std::string::npos);
    EXPECT_LT(out.find("Винт М4×6 "), out.find("Винт М4×10 "));
#endif
}
