// PDF: структура (xref-смещения указывают на объекты, /Length потоков), MediaBox формата,
// координаты content stream в масштабе 72/25,4, встроенный шрифт; независимая проверка
// программами poppler (pdfinfo, pdffonts, pdftotext), если они установлены.

#include "test_support.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <regex>
#include <sstream>

using namespace dv2test;

namespace {

constexpr double kPt = 72.0 / 25.4;

struct Written {
    Sample sample;
    std::string path;
    PdfFile pdf;
};

Written write(GostFormat f, Orientation o, const std::string& name) {
    Written w{makeSample(f, o, systemFont()), outPath(name), {}};
    PdfOptions po;
    po.font = systemFont();
    po.title = "Вал ступенчатый";
    auto r = writePdf(w.sample.drawing, po);
    if (!r) throw std::runtime_error(r.error().message);
    saveToFile(w.path, r.value()).value();
    w.pdf = parsePdf(readFile(w.path));
    return w;
}

std::vector<double> numbersBefore(const std::string& content, std::size_t pos, int count) {
    // count чисел, стоящих непосредственно перед pos.
    std::string head = content.substr(0, pos);
    std::vector<std::string> tok;
    std::istringstream in(head);
    std::string t;
    while (in >> t) tok.push_back(t);
    std::vector<double> out;
    for (std::size_t i = tok.size() - std::size_t(count); i < tok.size(); ++i) out.push_back(std::stod(tok[i]));
    return out;
}

}  // namespace

TEST(DrawingV2Pdf, StructureMediaBoxAndScale) {
    ASSERT_NE(systemFont(), nullptr) << "no system TrueType font";
    auto w = write(GostFormat::A3, Orientation::Landscape, "sample_a3.pdf");
    const PdfFile& f = w.pdf;
    // parsePdf уже проверил: каждое смещение xref указывает на "<n> 0 obj", /Length — на endstream.
    EXPECT_GE(f.objects.size(), 10u);
    const PdfObject* page = f.find("/Type /Page ");
    ASSERT_NE(page, nullptr);
    // А3 альбомный: 420 × 297 мм = 1190,5512 × 841,8898 pt.
    EXPECT_NE(page->dict.find("/MediaBox [0 0 1190.5512 841.8898]"), std::string::npos) << page->dict;
    EXPECT_NE(f.find("/Type /Catalog"), nullptr);

    const PdfObject* content = nullptr;
    for (const auto& o : f.objects) {
        if (o.stream.find(" m ") != std::string::npos && o.stream.find("BT") != std::string::npos) content = &o;
    }
    ASSERT_NE(content, nullptr);
    const std::string& cs = content->stream;
    // Отрезок (50,100)–(150,100) мм.
    EXPECT_NE(cs.find("141.7323 283.4646 m 425.1969 283.4646 l S"), std::string::npos);
    // Рамка: (20,5)–(415,5).
    EXPECT_NE(cs.find("56.6929 14.1732 m 1176.378 14.1732 l S"), std::string::npos);
    // Толщины и штрихи (s = 0,5 → 1,4173 pt; штриховая 0,25 мм, [4 1,5] мм).
    EXPECT_NE(cs.find("1.4173 w [] 0 d"), std::string::npos);
    EXPECT_NE(cs.find("0.7087 w [11.3386 4.252] 0 d"), std::string::npos);
    EXPECT_NE(cs.find("0.7087 w [42.5197 3.8976 0.7087 3.8976] 0 d"), std::string::npos);

    // Окружность r = 20 мм вокруг (100,180): первая точка (120,180), сегменты Безье на окружности.
    const std::string start = "340.1575 510.2362 m\n";
    const std::size_t p = cs.find(start);
    ASSERT_NE(p, std::string::npos);
    std::istringstream seg(cs.substr(p + start.size()));
    double cx = 100 * kPt, cy = 180 * kPt, r = 20 * kPt, x0 = 120 * kPt, y0 = 180 * kPt;
    for (int k = 0; k < 4; ++k) {
        double v[6];
        std::string op;
        for (double& x : v) seg >> x;
        seg >> op;
        ASSERT_EQ(op, "c");
        for (double t : {0.25, 0.5, 0.75}) {
            const double a = (1 - t) * (1 - t) * (1 - t), b = 3 * t * (1 - t) * (1 - t), c = 3 * t * t * (1 - t), d = t * t * t;
            const double x = a * x0 + b * v[0] + c * v[2] + d * v[4];
            const double y = a * y0 + b * v[1] + c * v[3] + d * v[5];
            // Погрешность приближения четверти окружности кубической кривой ≈ 2,7·10⁻⁴ r.
            EXPECT_NEAR(std::hypot(x - cx, y - cy), r, 3e-4 * r + 1e-3);
        }
        EXPECT_NEAR(std::hypot(v[4] - cx, v[5] - cy), r, 1e-3);
        x0 = v[4];
        y0 = v[5];
    }
    EXPECT_NEAR(x0, 120 * kPt, 1e-3);

    // Текст «Технические требования» в (50, 90) мм, наклон 15°: Tm = [1 0 tan15 1 x y].
    const std::size_t tm = cs.find(" 141.7323 255.1181 Tm");
    ASSERT_NE(tm, std::string::npos);
    const auto m = numbersBefore(cs, tm + 1, 4);
    EXPECT_NEAR(m[0], 1.0, 1e-6);
    EXPECT_NEAR(m[1], 0.0, 1e-6);
    EXPECT_NEAR(m[2], std::tan(15.0 * M_PI / 180.0), 1e-6);
    EXPECT_NEAR(m[3], 1.0, 1e-6);
    const std::size_t tf = cs.rfind(" Tf", tm);
    const auto fs = numbersBefore(cs, tf, 1);
    EXPECT_NEAR(fs[0], 5.0 / systemFont()->capHeightRatio() * kPt, 1e-3);

    // Шрифт встроен целиком, ширины и ToUnicode.
    const PdfObject* ff = f.find("/Length1 ");
    ASSERT_NE(ff, nullptr);
    EXPECT_EQ(ff->stream.size(), systemFont()->bytes().size());
    EXPECT_TRUE(std::equal(ff->stream.begin(), ff->stream.end(), systemFont()->bytes().begin(),
                           [](char a, std::uint8_t b) { return std::uint8_t(a) == b; }));
    const PdfObject* cid = f.find("/CIDFontType2");
    ASSERT_NE(cid, nullptr);
    EXPECT_NE(cid->dict.find("/CIDToGIDMap /Identity"), std::string::npos);
    const std::uint16_t gA = systemFont()->glyphIndex(U'А');
    const int wA = int(std::lround(systemFont()->advanceWidth(gA) * 1000.0 / systemFont()->unitsPerEm()));
    EXPECT_NE(cid->dict.find(" " + std::to_string(gA) + " [" + std::to_string(wA) + "]"), std::string::npos);
    const PdfObject* tu = f.find("/Encoding /Identity-H");
    ASSERT_NE(tu, nullptr);
    const PdfObject* cmap = nullptr;
    for (const auto& o : f.objects) {
        if (o.stream.find("beginbfchar") != std::string::npos) cmap = &o;
    }
    ASSERT_NE(cmap, nullptr);
    char buf[32];
    std::snprintf(buf, sizeof buf, "<%04X> <0410>", gA);
    EXPECT_NE(cmap->stream.find(buf), std::string::npos);
}

TEST(DrawingV2Pdf, A4MediaBoxAndErrors) {
    ASSERT_NE(systemFont(), nullptr);
    auto w = write(GostFormat::A4, Orientation::Portrait, "sample_a4.pdf");
    EXPECT_NE(w.pdf.find("/MediaBox [0 0 595.2756 841.8898]"), nullptr);
    // Текст без шрифта — ошибка; символ, которого нет в шрифте, — ошибка.
    auto d = Drawing::create(gostSheet(GostFormat::A4).value()).value();
    Text t;
    t.utf8 = "Тест";
    ASSERT_TRUE(d.add({t, "0", LineType::SolidThin}));
    EXPECT_FALSE(writePdf(d, {}));
    auto d2 = Drawing::create(gostSheet(GostFormat::A4).value()).value();
    t.utf8 = std::string("\xEE\x80\x80");  // U+E000, частная область
    ASSERT_TRUE(d2.add({t, "0", LineType::SolidThin}));
    PdfOptions po;
    po.font = systemFont();
    if (systemFont()->glyphIndex(char32_t(0xE000)) == 0) {
        auto r = writePdf(d2, po);
        ASSERT_FALSE(r);
        EXPECT_EQ(r.error().code, ErrorCode::Unsupported);
    }
    // Чертёж без текста пишется без шрифта.
    auto d3 = Drawing::create(gostSheet(GostFormat::A4).value()).value();
    ASSERT_TRUE(d3.add({LineSeg{{10, 10}, {20, 20}}, "0", LineType::SolidThick}));
    auto r3 = writePdf(d3, {});
    ASSERT_TRUE(r3);
    EXPECT_EQ(parsePdf(r3.value()).find("/FontFile2"), nullptr);
}

TEST(DrawingV2Pdf, PopplerReadsPageSizeFontAndCyrillicText) {
    ASSERT_NE(systemFont(), nullptr);
    auto w = write(GostFormat::A3, Orientation::Landscape, "sample_a3.pdf");
    int code = 0;
#ifdef BENDCORE_PDFINFO
    {
        const std::string out = runCommand(std::string(BENDCORE_PDFINFO) + " " + w.path + " 2>&1", &code);
        EXPECT_EQ(code, 0) << out;
        EXPECT_TRUE(std::regex_search(out, std::regex(R"(Page size:\s+1190\.55 x 841\.89 pts \(A3\))"))) << out;
        EXPECT_TRUE(std::regex_search(out, std::regex(R"(Pages:\s+1\n)"))) << out;
        EXPECT_EQ(out.find("Syntax Error"), std::string::npos) << out;
        EXPECT_NE(out.find("Вал ступенчатый"), std::string::npos) << out;  // /Title в UTF-16BE
    }
#endif
#ifdef BENDCORE_PDFFONTS
    {
        const std::string out = runCommand(std::string(BENDCORE_PDFFONTS) + " " + w.path + " 2>&1", &code);
        EXPECT_EQ(code, 0) << out;
        EXPECT_TRUE(std::regex_search(out, std::regex(systemFont()->postScriptName() +
                                                      R"(\s+CID TrueType\s+Identity-H\s+yes\s+no\s+yes)")))
            << out;
    }
#endif
#ifdef BENDCORE_PDFTOTEXT
    {
        const std::string out = runCommand(std::string(BENDCORE_PDFTOTEXT) + " -enc UTF-8 " + w.path + " - 2>&1", &code);
        EXPECT_EQ(code, 0) << out;
        for (const char* s : {"Вал ступенчатый", "АБВГ.123456.001", "Сталь 45 ГОСТ 1050-2013", "Разраб.", "Иванов",
                              "Масштаб", "1:2", "Технические требования: Ø20"}) {
            EXPECT_NE(out.find(s), std::string::npos) << s << "\n---\n" << out;
        }
    }
#else
    GTEST_SKIP() << "pdftotext not installed";
#endif
}
