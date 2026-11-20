// Шрифт типа Б ГОСТ 2.304: геометрия знаков против таблиц стандарта (ширины 4–8d, высоты
// h = 10d, c = 7d, выносные элементы), интервалы a = 2d и e = 6d, наклон 75°, толщина h/10;
// основная надпись без сжатия, линии текста не пересекают линии граф; экспорт DXF (свой разбор
// и ezdxf), SVG, PDF (разбор потока, poppler).

#include "test_support.hpp"

#include <bendcore/drawing_v2/stroke_font.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <cstdlib>
#include <map>
#include <numbers>
#include <sstream>

using namespace dv2test;

namespace {

const GostStrokeFont& F() { return gostStrokeFont(); }

struct Box {
    double x0 = 1e300, y0 = 1e300, x1 = -1e300, y1 = -1e300;
    void add(Point2 p) {
        x0 = std::min(x0, p.x), y0 = std::min(y0, p.y), x1 = std::max(x1, p.x), y1 = std::max(y1, p.y);
    }
};

Box glyphBox(char32_t cp) {
    Box b;
    const StrokeGlyph* g = F().glyph(cp);
    if (!g) return b;
    for (const auto& s : g->strokes)
        for (const Point2& p : s) b.add(p);
    return b;
}

// Ширины знаков типа Б в d (ГОСТ 2.304-81, табл. 4; значения — по памяти, сверка —
// docs/STANDARDS_VERIFICATION.md).
const std::map<char32_t, double>& gostWidths() {
    static const std::map<char32_t, double> w = [] {
        std::map<char32_t, double> m;
        auto put = [&](std::u32string_view s, double v) {
            for (char32_t c : s) m[c] = v;
        };
        put(U"ГЕЗС", 5);
        put(U"БВИЙКЛНОПРТУЦЧЬЭЯ", 6);
        put(U"АДМХЫЮ", 7);
        put(U"ЖФШЩЪ", 8);
        put(U"с", 4);
        put(U"абвгдезийклнопрухцчьэя", 5);
        put(U"мъыю", 6);
        put(U"жтфшщ", 7);
        put(U"1", 3);
        put(U"0235689", 5);
        put(U"7", 5);
        put(U"4", 6);
        return m;
    }();
    return w;
}

bool segmentsCross(Point2 a, Point2 b, Point2 c, Point2 d) {
    auto cr = [](Point2 o, Point2 p, Point2 q) { return (p.x - o.x) * (q.y - o.y) - (p.y - o.y) * (q.x - o.x); };
    const double d1 = cr(c, d, a), d2 = cr(c, d, b), d3 = cr(a, b, c), d4 = cr(a, b, d);
    return ((d1 > 1e-12 && d2 < -1e-12) || (d1 < -1e-12 && d2 > 1e-12)) &&
           ((d3 > 1e-12 && d4 < -1e-12) || (d3 < -1e-12 && d4 > 1e-12));
}

std::size_t strokeCount(const Drawing& d) {
    std::size_t n = 0;
    for (const auto& e : d.entities())
        if (const auto* t = std::get_if<Text>(&e.geometry)) n += F().layout(*t).value().strokes.size();
    return n;
}

Sample gostSample() {
    auto sheet = gostSheet(GostFormat::A3, 1, Orientation::Landscape).value();
    Drawing d = Drawing::create(sheet, 0.5).value();
    d.setTextFont(TextFont::Gost2304);
    FrameOptions fo;
    fo.font = &F();
    auto layout = addFrameAndTitleBlock(d, sampleTitleData(), fo);
    if (!layout) throw std::runtime_error(layout.error().message);
    Text t;
    t.position = {50, 90};
    t.utf8 = "Технические требования: ⌀20 ±0,1; 45°; №5";
    t.height = 5;
    if (auto s = d.add({t, "TITLE", LineType::SolidThin}); !s) throw std::runtime_error(s.error().message);
    return {std::move(d), layout.value()};
}

}  // namespace

TEST(DrawingV2StrokeFont, GlyphWidthsMatchGostTable) {
    for (const auto& [cp, w] : gostWidths()) {
        const StrokeGlyph* g = F().glyph(cp);
        ASSERT_NE(g, nullptr) << unsigned(cp);
        EXPECT_DOUBLE_EQ(g->width, w) << "U+" << std::hex << unsigned(cp);
        // Знак заполняет свою клетку: оси линий в пределах [0, w], крайние — не дальше 0,75d от границ.
        const Box b = glyphBox(cp);
        EXPECT_GE(b.x0, -1e-9) << unsigned(cp);
        EXPECT_LE(b.x1, w + 1e-9) << unsigned(cp);
        EXPECT_LE(b.x0, 0.75) << unsigned(cp);
        EXPECT_GE(b.x1, w - 0.75) << unsigned(cp);
    }
}

TEST(DrawingV2StrokeFont, GlyphHeightsCapsLowerDescenders) {
    // Прописные и цифры: от основания до h = 10d; Д Ц Щ — с выносными элементами 2d вниз.
    for (char32_t cp : std::u32string_view(U"АБВГЕЖЗИКЛМНОПРСТУФХЧШЪЫЬЭЮЯ0123456789DFGJLNRSUVWXYZ")) {
        const Box b = glyphBox(cp);
        EXPECT_NEAR(b.y1, 10.0, 1e-9) << unsigned(cp);
        EXPECT_NEAR(b.y0, 0.0, 1e-9) << unsigned(cp);
    }
    for (char32_t cp : std::u32string_view(U"ДЦЩ")) {
        EXPECT_NEAR(glyphBox(cp).y1, 10.0, 1e-9);
        EXPECT_NEAR(glyphBox(cp).y0, -2.0, 1e-9);
    }
    // Строчные: c = 7d; б — до h; р у ф — вниз на 3d; д ц щ — на 2d.
    for (char32_t cp : std::u32string_view(U"авгежзиклмнопстхчшъыьэюя")) {
        const Box b = glyphBox(cp);
        EXPECT_NEAR(b.y1, GostStrokeFont::kLowerHeight, 1e-9) << unsigned(cp);
        EXPECT_NEAR(b.y0, 0.0, 1e-9) << unsigned(cp);
    }
    EXPECT_NEAR(glyphBox(U'б').y1, 10.0, 1e-9);
    EXPECT_NEAR(glyphBox(U'ф').y1, 10.0, 1e-9);
    for (char32_t cp : std::u32string_view(U"руф")) EXPECT_NEAR(glyphBox(cp).y0, -GostStrokeFont::kDescender, 1e-9);
    for (char32_t cp : std::u32string_view(U"дцщ")) EXPECT_NEAR(glyphBox(cp).y0, -2.0, 1e-9);
}

TEST(DrawingV2StrokeFont, CoversAlphabetsDigitsAndAnnotationSigns) {
    const std::u32string need =
        U"АБВГДЕЁЖЗИЙКЛМНОПРСТУФХЦЧШЩЪЫЬЭЮЯабвгдеёжзийклмнопрстуфхцчшщъыьэюя"
        U"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789"
        U" .,:;!?-–—−+=<>/\\|()[]{}«»\"'`^~_*#%&$′″°№±×·⌀Ø∅";
    for (char32_t cp : need) EXPECT_TRUE(F().hasGlyph(cp)) << "U+" << std::hex << unsigned(cp);
    for (const StrokeGlyph& g : F().glyphs()) {
        const Box b = glyphBox(g.codePoint);
        if (g.strokes.empty()) continue;
        EXPECT_GE(b.x0, -1e-9) << unsigned(g.codePoint);
        EXPECT_LE(b.x1, g.width + 1e-9) << unsigned(g.codePoint);
        EXPECT_GE(b.y0, -3.0 - 1e-9) << unsigned(g.codePoint);
        EXPECT_LE(b.y1, 12.0 + 1e-9) << unsigned(g.codePoint);
    }
}

// Многоточие, @ и греческие буквы машиностроительных надписей (ГОСТ 2.304-81, греческий
// алфавит): высоты по сетке шрифта, «…» — та же ширина и те же точки, что «...».
TEST(DrawingV2StrokeFont, EllipsisAtSignAndGreekLetters) {
    for (char32_t cp : std::u32string_view(U"…@αβγδεημνπρσφωθλτΔΣΩΑΒΓΕΖΗΙΚΜΝΟΠΡΤΥΦΧµΩ∆"))
        EXPECT_TRUE(F().hasGlyph(cp)) << "U+" << std::hex << unsigned(cp);
    EXPECT_NEAR(F().textWidthMm(U"…", 3.5), F().textWidthMm(U"...", 3.5), 1e-12);
    const Box e = glyphBox(U'…');
    EXPECT_NEAR(e.x0, 0.5, 1e-12);
    EXPECT_NEAR(e.x1, 6.5, 1e-12);
    // Прописные Δ Σ Ω — от основания до h = 10d; строчные без выносных — c = 7d.
    for (char32_t cp : std::u32string_view(U"ΔΣΩ")) {
        EXPECT_NEAR(glyphBox(cp).y0, 0.0, 1e-9) << unsigned(cp);
        EXPECT_NEAR(glyphBox(cp).y1, 10.0, 1e-9) << unsigned(cp);
    }
    for (char32_t cp : std::u32string_view(U"αεπσωντ")) {
        EXPECT_NEAR(glyphBox(cp).y0, 0.0, 1e-9) << unsigned(cp);
        EXPECT_NEAR(glyphBox(cp).y1, GostStrokeFont::kLowerHeight, 1e-9) << unsigned(cp);
    }
    // Выносные вниз на 3d: β γ η μ ρ φ; вверх до h: β δ θ λ.
    for (char32_t cp : std::u32string_view(U"βγημρφ")) EXPECT_NEAR(glyphBox(cp).y0, -GostStrokeFont::kDescender, 1e-9) << unsigned(cp);
    for (char32_t cp : std::u32string_view(U"δθλ")) EXPECT_NEAR(glyphBox(cp).y1, 10.0, 1e-9) << unsigned(cp);
    // Знаки заполняют свою ширину (оси линий не дальше 0,75d от границ клетки).
    for (char32_t cp : std::u32string_view(U"αβγδεπσφωΔΣΩ@")) {
        const Box b = glyphBox(cp);
        const double w = F().glyph(cp)->width;
        EXPECT_LE(b.x0, 0.75) << unsigned(cp);
        EXPECT_GE(b.x1, w - 0.75) << unsigned(cp);
    }
    // Надпись с греческими буквами раскладывается (нет «U+…» ошибки).
    Text t;
    t.utf8 = "α = 20°, Δ ≈ σ…";
    t.height = 5;
    EXPECT_FALSE(F().layout(t).ok());  // «≈» в шрифте нет — явная ошибка
    t.utf8 = "α = 20°, Δt = 0,1…0,3 мм, σт = 355 МПа, μ = 0,15, ω, π, @";
    auto lay = F().layout(t);
    ASSERT_TRUE(lay.ok()) << lay.error().message;
    EXPECT_GT(lay.value().strokes.size(), 40u);
}

TEST(DrawingV2StrokeFont, SpacingAndTextWidth) {
    // h = 5: d = 0,5. «НН» = 6 + 2 + 6 = 14d; «Н Н» = 6 + e + 6, e = 6d (ГОСТ 2.304, табл. 2).
    EXPECT_NEAR(F().textWidthMm(U"НН", 5.0), 7.0, 1e-12);
    EXPECT_NEAR(F().textWidthMm(U"Н Н", 5.0), (6 + 6 + 6) * 0.5, 1e-12);
    EXPECT_NEAR(F().textWidthMm(U"А1", 10.0), 7 + 2 + 3, 1e-12);
    EXPECT_NEAR(GostStrokeFont::strokeWidth(3.5), 0.35, 1e-15);
    EXPECT_NEAR(GostStrokeFont::kLineSpacing / GostStrokeFont::kCapHeight, 1.7, 1e-15);
    // Раскладка: вторая буква начинается через ширину первой и интервал a.
    Text t;
    t.utf8 = "ГН";
    t.height = 10;
    t.slanted = false;
    auto lay = F().layout(t).value();
    EXPECT_NEAR(lay.width, 13.0, 1e-12);
    EXPECT_NEAR(lay.lineWidth, 1.0, 1e-15);
    double minX = 1e300;
    for (std::size_t i = 1; i < lay.strokes.size(); ++i)
        for (const Point2& p : lay.strokes[i]) minX = std::min(minX, p.x);
    EXPECT_NEAR(minX, 5 + 2, 1e-12);  // Г (5d) + a (2d)
}

TEST(DrawingV2StrokeFont, Slant75DegreesRotationAndAlignment) {
    Text t;
    t.utf8 = "Н";
    t.height = 7;
    t.position = {100, 50};
    for (bool slanted : {true, false}) {
        t.slanted = slanted;
        auto lay = F().layout(t).value();
        const auto& stem = lay.strokes.at(0);  // левая вертикаль снизу вверх
        const double ang = std::atan2(stem.back().y - stem.front().y, stem.back().x - stem.front().x) * 180 / std::numbers::pi;
        EXPECT_NEAR(ang, slanted ? kGostTextSlantDeg : 90.0, 1e-9);
        EXPECT_NEAR(std::hypot(stem.back().y - stem.front().y, 0.0), 7.0, 1e-9);  // высота h
        EXPECT_NEAR(stem.front().x, 100.0, 1e-12);
        EXPECT_NEAR(stem.front().y, 50.0, 1e-12);
    }
    // Поворот 90°, выравнивание по центру и середине высоты: центр основания строки сдвинут на h/2.
    t.slanted = false;
    t.rotationDeg = 90;
    t.hAlign = HAlign::Center;
    t.vAlign = VAlign::Middle;
    auto lay = F().layout(t).value();
    Box b;
    for (const auto& s : lay.strokes)
        for (const Point2& p : s) b.add(p);
    EXPECT_NEAR((b.x0 + b.x1) / 2, 100.0, 1e-9);
    EXPECT_NEAR((b.y0 + b.y1) / 2, 50.0, 1e-9);
    EXPECT_NEAR(b.y1 - b.y0, 6 * 0.7, 1e-9);  // ширина «Н» 6d вдоль повёрнутой строки
    // Знака нет в шрифте.
    t.utf8 = "a≈b";
    auto bad = F().layout(t);
    ASSERT_FALSE(bad);
    EXPECT_EQ(bad.error().code, bendcore::kernel::ErrorCode::Unsupported);
    EXPECT_NE(bad.error().message.find("U+2248"), std::string::npos);
}

TEST(DrawingV2StrokeFont, TitleBlockFitsWithoutCompression) {
    Sample s = gostSample();
    const Drawing& d = s.drawing;
    std::map<std::string, const Text*> texts;
    std::vector<LineSeg> frame;
    for (const auto& e : d.entities()) {
        if (const auto* t = std::get_if<Text>(&e.geometry); t && e.layer == "TITLE") texts[t->utf8] = t;
        if (const auto* l = std::get_if<LineSeg>(&e.geometry); l && e.layer == "FRAME") frame.push_back(*l);
    }
    // Ни одного сжатого текста: обозначение (96d = 67,2 мм ≤ 119 мм) — 7 мм; наименование при 7 мм
    // не помещается (105d = 73,5 мм > 69 мм) и пишется 5 мм; материал (152d) — 3,5 мм.
    for (const auto& [str, t] : texts) EXPECT_DOUBLE_EQ(t->widthFactor, 1.0) << str;
    EXPECT_DOUBLE_EQ(texts.at("АБВГ.123456.001")->height, 7.0);
    EXPECT_DOUBLE_EQ(texts.at("Вал ступенчатый")->height, 5.0);
    EXPECT_DOUBLE_EQ(texts.at("Сталь 45 ГОСТ 1050-2013")->height, 3.5);
    EXPECT_DOUBLE_EQ(texts.at("ООО «Бенд»")->height, 5.0);
    for (const char* label : {"Т.контр.", "Н.контр.", "Разраб.", "№ докум.", "Масштаб", "Инв. № подл.", "Подп. и дата"})
        EXPECT_DOUBLE_EQ(texts.at(label)->height, 2.5) << label;
    // Линии каждого текста основной надписи не пересекают линии граф (с учётом наклона 75°).
    std::size_t checked = 0;
    for (const auto& [str, t] : texts) {
        if (str.rfind("Технические", 0) == 0) continue;
        auto lay = F().layout(*t).value();
        for (const auto& st : lay.strokes)
            for (std::size_t i = 1; i < st.size(); ++i)
                for (const LineSeg& l : frame) {
                    ASSERT_FALSE(segmentsCross(st[i - 1], st[i], l.a, l.b)) << "'" << str << "' crosses a title block line";
                    ++checked;
                }
    }
    EXPECT_GT(checked, 10000u);
    // Длинный текст, который не помещается и при 2,5, — ошибка, а не сжатие.
    Drawing d2 = Drawing::create(gostSheet(GostFormat::A3, 1, Orientation::Landscape).value(), 0.5).value();
    TitleBlockData td = sampleTitleData();
    td.developer.surname = "Константинопольский-Задунайский";
    FrameOptions fo;
    fo.font = &F();
    auto r = addFrameAndTitleBlock(d2, td, fo);
    ASSERT_FALSE(r);
    EXPECT_NE(r.error().message.find("surname"), std::string::npos);
}

TEST(DrawingV2StrokeFont, DxfPolylinesOnTextLayerAndInvisibleText) {
    Sample s = gostSample();
    const std::string text = writeDxf(s.drawing).value();
    saveToFile(outPath("gost_font_a3.dxf"), text).value();
    DxfFile f = parseDxf(text);
    std::size_t texts = 0, invisible = 0, polylines = 0;
    std::map<int, std::size_t> weights;
    bool found = false;
    // Первая линия «Т» текста технических требований (h = 5): верхняя черта от (50 + 10·0,5·tg15°, 95).
    const double slant = std::tan(15.0 * std::numbers::pi / 180.0);
    for (const DxfObject& o : *f.section("ENTITIES")) {
        if (o.type == "TEXT") {
            ++texts;
            if (o.first(60) == std::optional<std::string>("1")) ++invisible;
        }
        if (o.type == "LWPOLYLINE" && o.first(8) == std::optional<std::string>("TITLE")) {
            ++polylines;
            ++weights[std::stoi(*o.first(370))];
            const auto xs = o.all(10), ys = o.all(20);
            if (xs.size() == 2 && std::abs(std::stod(xs[0]) - (50 + 5 * slant)) < 1e-6 && std::abs(std::stod(ys[0]) - 95) < 1e-6 &&
                std::abs(std::stod(xs[1]) - (53 + 5 * slant)) < 1e-6)
                found = true;
        }
    }
    std::size_t modelTexts = 0;
    for (const auto& e : s.drawing.entities()) modelTexts += std::holds_alternative<Text>(e.geometry) ? 1 : 0;
    EXPECT_EQ(texts, modelTexts);
    EXPECT_EQ(invisible, modelTexts);
    EXPECT_EQ(polylines, strokeCount(s.drawing));
    EXPECT_TRUE(found) << "stroke of 'Т' at the expected sheet coordinates";
    // Толщины: h 2,5 → 0,25 (25); 3,5 → 0,35 (35); 5 → 0,5 (50); 7 → 0,7 (70).
    for (int w : {25, 35, 50, 70}) EXPECT_GT(weights[w], 0u) << w;
    // Без исходного текста — только линии.
    DxfOptions o;
    o.sourceText = false;
    DxfFile f2 = parseDxf(writeDxf(s.drawing, o).value());
    for (const DxfObject& x : *f2.section("ENTITIES")) EXPECT_NE(x.type, "TEXT");

    const char* py = std::getenv("BENDCORE_PYTHON_EZDXF");
    if (!py || !*py) GTEST_SKIP() << "set BENDCORE_PYTHON_EZDXF to a python with ezdxf installed";
    const std::string script =
        "import sys, ezdxf\n"
        "doc = ezdxf.readfile(sys.argv[1])\n"
        "a = doc.audit()\n"
        "msp = doc.modelspace()\n"
        "pl = msp.query('LWPOLYLINE[layer==\"TITLE\"]')\n"
        "t = [e for e in msp.query('TEXT')]\n"
        "inv = sum(1 for e in t if e.dxf.invisible == 1)\n"
        "names = [ezdxf.decode_dxf_unicode(e.dxf.text) for e in t]\n"
        "ys = [p[1] for e in pl for p in e.get_points('xy')]\n"
        "print(len(a.errors), len(pl), len(t), inv, int('Вал ступенчатый' in names), round(min(ys), 3))\n";
    saveToFile(outPath("check_gost_font_ezdxf.py"), script).value();
    int code = 0;
    const std::string out =
        runCommand(std::string(py) + " " + outPath("check_gost_font_ezdxf.py") + " " + outPath("gost_font_a3.dxf"), &code);
    ASSERT_EQ(code, 0) << out;
    std::istringstream in(out);
    std::size_t errors = 99, pl = 0, tx = 0, inv = 0, name = 0;
    double minY = 0;
    in >> errors >> pl >> tx >> inv >> name >> minY;
    EXPECT_EQ(errors, 0u);
    EXPECT_EQ(pl, strokeCount(s.drawing));
    EXPECT_EQ(tx, modelTexts);
    EXPECT_EQ(inv, modelTexts);
    EXPECT_EQ(name, 1u);
    EXPECT_GT(minY, 5.0);  // все линии текста — выше нижней линии рамки
}

TEST(DrawingV2StrokeFont, SvgPathsAndTransparentText) {
    Sample s = gostSample();
    const std::string svg = writeSvg(s.drawing).value();
    saveToFile(outPath("gost_font_a3.svg"), svg).value();
    std::size_t groups = 0, pos = 0;
    while ((pos = svg.find("<g class=\"text\"", pos)) != std::string::npos) ++groups, ++pos;
    std::size_t modelTexts = 0;
    for (const auto& e : s.drawing.entities()) modelTexts += std::holds_alternative<Text>(e.geometry) ? 1 : 0;
    EXPECT_EQ(groups, modelTexts);
    EXPECT_NE(svg.find("data-text=\"Вал ступенчатый\""), std::string::npos);
    EXPECT_NE(svg.find("stroke-width=\"0.7\" stroke-linecap=\"round\""), std::string::npos);
    EXPECT_NE(svg.find("fill-opacity=\"0\""), std::string::npos);
    // Начало верхней черты «Т» (h = 5) на листе A3: y_svg = 297 − 95.
    const double slant = std::tan(15.0 * std::numbers::pi / 180.0);
    std::ostringstream m;
    m << "M" << 50 + 5 * slant;
    EXPECT_NE(svg.find(m.str().substr(0, 8)), std::string::npos);
    EXPECT_NE(svg.find(" 202 L"), std::string::npos);
#ifdef BENDCORE_XMLLINT
    int code = 0;
    runCommand(std::string(BENDCORE_XMLLINT) + " --noout " + outPath("gost_font_a3.svg") + " 2>&1", &code);
    EXPECT_EQ(code, 0);
#endif
}

TEST(DrawingV2StrokeFont, PdfStrokedTextAndSearchableLayer) {
    Sample s = gostSample();
    // Без шрифта TrueType — только линии, без ресурсов шрифта.
    const std::string bare = writePdf(s.drawing).value();
    PdfFile pb = parsePdf(bare);
    EXPECT_EQ(pb.find("/FontFile2"), nullptr);
    const PdfObject* content = nullptr;
    for (const auto& o : pb.objects)
        if (o.stream.find(" m\n") != std::string::npos) content = &o;
    ASSERT_NE(content, nullptr);
    EXPECT_EQ(content->stream.find("BT"), std::string::npos);
    EXPECT_NE(content->stream.find(" w [] 0 d 1 J"), std::string::npos);
    // Толщина линии шрифта h = 7: 0,7 мм = 1,9843 pt.
    EXPECT_NE(content->stream.find("1.9843 w [] 0 d 1 J"), std::string::npos);
    // Начало верхней черты «Т» (50 + 5·tg15°, 95) мм в пунктах.
    const double k = 72.0 / 25.4, slant = std::tan(15.0 * std::numbers::pi / 180.0);
    auto fmt = [](double v) {  // 4 знака без хвостовых нулей, как в писателе PDF
        char b[32];
        std::snprintf(b, sizeof b, "%.4f", v);
        std::string s = b;
        while (s.back() == '0') s.pop_back();
        if (s.back() == '.') s.pop_back();
        return s;
    };
    const std::string start = fmt((50 + 5 * slant) * k) + " " + fmt(95 * k) + " m";
    EXPECT_NE(content->stream.find(start), std::string::npos) << start;
    std::size_t strokes = 0, p = 0;
    while ((p = content->stream.find("S\n", p)) != std::string::npos) ++strokes, ++p;
    EXPECT_GE(strokes, strokeCount(s.drawing));

    const TrueTypeFont* font = systemFont();
    if (!font) GTEST_SKIP() << "no system TrueType font for the searchable layer";
    PdfOptions po;
    po.font = font;
    const std::string pdf = writePdf(s.drawing, po).value();
    saveToFile(outPath("gost_font_a3.pdf"), pdf).value();
    PdfFile pf = parsePdf(pdf);
    EXPECT_NE(pf.find("/FontFile2"), nullptr);
    bool invisible = false;
    for (const auto& o : pf.objects) invisible = invisible || o.stream.find("BT 3 Tr /F1") != std::string::npos;
    EXPECT_TRUE(invisible);
#ifdef BENDCORE_PDFTOTEXT
    int code = 0;
    const std::string out = runCommand(std::string(BENDCORE_PDFTOTEXT) + " -enc UTF-8 " + outPath("gost_font_a3.pdf") + " - 2>&1", &code);
    EXPECT_EQ(code, 0);
    for (const char* want : {"АБВГ.123456.001", "Вал ступенчатый", "ООО «Бенд»", "⌀20"})
        EXPECT_NE(out.find(want), std::string::npos) << "pdftotext: " << want << "\n" << out;
#endif
}

// Образец шрифта: все знаки прямым и наклонным шрифтом на листе A3 (PDF/SVG для визуальной
// проверки, docs/drawing/examples/). Линии строки не выходят за её длину по метрикам (плюс наклон).
TEST(DrawingV2StrokeFont, SpecimenSheet) {
    Drawing d = Drawing::create(gostSheet(GostFormat::A3, 1, Orientation::Landscape).value(), 0.5).value();
    d.setTextFont(TextFont::Gost2304);
    ASSERT_TRUE(d.addLayer({"TEXT", 7}));
    const char* lines[] = {"АБВГДЕЁЖЗИЙКЛМНОПРСТУФХЦЧШЩЪЫЬЭЮЯ", "абвгдеёжзийклмнопрстуфхцчшщъыьэюя",
                           "ABCDEFGHIJKLMNOPQRSTUVWXYZ 0123456789", "abcdefghijklmnopqrstuvwxyz",
                           ".,:;!?-–—+=<>/\\|()[]{}«»\"'*#%&$′″°№±×⌀…@",
                           "αβγδεηθλμνπρστφω ΔΣΩ"};
    double y = 280;
    for (bool slanted : {false, true}) {
        for (const char* s : lines) {
            Text t;
            t.position = {15, y};
            t.utf8 = s;
            t.height = 7;
            t.slanted = slanted;
            ASSERT_TRUE(d.add({t, "TEXT", LineType::SolidThin}));
            auto lay = F().layout(t).value();
            Box b;
            for (const auto& st : lay.strokes)
                for (const Point2& p : st) b.add(p);
            EXPECT_LE(b.x1 - 15, lay.width + 12 * 0.7 * 0.268 + 1e-9) << s;  // длина + наклон верха
            EXPECT_GE(b.x0 - 15, -1e-9) << s;
            y -= GostStrokeFont::kLineSpacing * 0.7 + 2;
        }
        y -= 10;
    }
    const std::string pdf = writePdf(d).value();
    saveToFile(outPath("gost_font_specimen.pdf"), pdf).value();
    saveToFile(outPath("gost_font_specimen.svg"), writeSvg(d).value()).value();
    EXPECT_NO_THROW(parsePdf(pdf));
}
