// Читатель DXF на ручных фикстурах. Ожидаемые значения — из определения формата (группы
// кодов DXF Reference) и аналитики дуг выпуклости (tg(θ/4)), а не из самого читателя.
#include <bendcore/dxf_reader.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <cstdio>
#include <fstream>
#include <numbers>
#include <string>
#include <vector>

using namespace bendcore::dxf;

namespace {

constexpr double kTol = 1e-9;

// Пары «код — значение» в стиле AutoCAD R12: код с выравниванием пробелами.
std::string dxf(const std::vector<std::pair<int, std::string>>& pairs, const std::string& eol = "\n") {
    std::string out;
    for (const auto& [code, value] : pairs) {
        char buf[16];
        std::snprintf(buf, sizeof buf, "%3d", code);
        out += buf + eol + value + eol;
    }
    return out;
}

using P = std::vector<std::pair<int, std::string>>;

P header(const std::string& ver, int insunits, const std::string& codepage = "ANSI_1252") {
    return {{0, "SECTION"}, {2, "HEADER"}, {9, "$ACADVER"}, {1, ver},   {9, "$DWGCODEPAGE"},
            {3, codepage},  {9, "$INSUNITS"}, {70, std::to_string(insunits)},
            // Переменная со значением в группе 2 не должна приниматься за имя секции.
            {9, "$DIMSTYLE"}, {2, "Standard"}, {0, "ENDSEC"}};
}

P concat(std::initializer_list<P> parts) {
    P out;
    for (const auto& p : parts) out.insert(out.end(), p.begin(), p.end());
    return out;
}

P entities(const P& body) {
    P out = {{0, "SECTION"}, {2, "ENTITIES"}};
    out.insert(out.end(), body.begin(), body.end());
    out.push_back({0, "ENDSEC"});
    return out;
}

const P kEof = {{0, "EOF"}};

Document parseOk(const std::string& text) {
    auto r = parseDxf(text);
    EXPECT_TRUE(r.ok()) << (r.ok() ? "" : r.error().message);
    return r.ok() ? r.value() : Document{};
}

}  // namespace

TEST(DxfReader, LinesCirclesArcsAndLayersWithCrlf) {
    const P tables = {{0, "SECTION"}, {2, "TABLES"}, {0, "TABLE"}, {2, "LAYER"}, {70, "2"},
                      {0, "LAYER"},   {2, "0"},      {70, "0"},    {62, "7"},    {6, "CONTINUOUS"},
                      {0, "LAYER"},   {2, "AXIS"},   {70, "5"},    {62, "-1"},   {6, "CENTER"},
                      {0, "ENDTAB"},  {0, "ENDSEC"}};
    const P body = {{0, "LINE"},   {8, "AXIS"},    {62, "1"},   {10, "1.5"},   {20, "-2.25"}, {30, "0.0"},
                    {11, "100"},   {21, "1e2"},    {31, "0.0"}, {0, "CIRCLE"}, {8, "0"},      {10, "50.0"},
                    {20, "25.0"},  {30, "0.0"},    {40, "12.5"}, {0, "ARC"},   {10, "0"},     {20, "0"},
                    {40, "10"},    {50, "-90"},    {51, "450"}, {999, "comment between entities"}};
    const std::string text = dxf(concat({header("AC1009", 4), tables, entities(body), kEof}), "\r\n");
    const Document d = parseOk(text);

    EXPECT_EQ(d.version, "AC1009");
    EXPECT_EQ(d.insunits, 4);
    ASSERT_EQ(d.layers.size(), 2u);
    const Layer* axis = d.find_layer("AXIS");
    ASSERT_NE(axis, nullptr);
    EXPECT_EQ(axis->color, -1);
    EXPECT_EQ(axis->linetype, "CENTER");
    EXPECT_TRUE(axis->frozen);
    EXPECT_TRUE(axis->locked);

    ASSERT_EQ(d.lines.size(), 1u);
    EXPECT_DOUBLE_EQ(d.lines[0].start.x, 1.5);
    EXPECT_DOUBLE_EQ(d.lines[0].start.y, -2.25);
    EXPECT_DOUBLE_EQ(d.lines[0].end.x, 100.0);
    EXPECT_DOUBLE_EQ(d.lines[0].end.y, 100.0);
    EXPECT_EQ(d.lines[0].props.layer, "AXIS");
    EXPECT_EQ(d.lines[0].props.color, 1);

    ASSERT_EQ(d.circles.size(), 1u);
    EXPECT_DOUBLE_EQ(d.circles[0].center.x, 50.0);
    EXPECT_DOUBLE_EQ(d.circles[0].center.y, 25.0);
    EXPECT_DOUBLE_EQ(d.circles[0].radius, 12.5);
    EXPECT_EQ(d.circles[0].props.color, 256);  // по умолчанию BYLAYER

    ASSERT_EQ(d.arcs.size(), 1u);
    EXPECT_DOUBLE_EQ(d.arcs[0].start_deg, 270.0);  // −90° → 270°
    EXPECT_DOUBLE_EQ(d.arcs[0].end_deg, 90.0);     // 450° → 90°
    EXPECT_EQ(d.entity_count(), 3u);
    EXPECT_TRUE(d.warnings.empty());
}

TEST(DxfReader, BulgeToArcAnalytic) {
    // Полуокружность: b = 1, хорда (0,0)→(10,0), центр в середине, обход против часовой
    // стрелки — дуга проходит ниже хорды (через (5,−5)).
    auto a = bulgeToArc({0, 0}, {10, 0}, 1.0).value();
    EXPECT_NEAR(a.center.x, 5.0, kTol);
    EXPECT_NEAR(a.center.y, 0.0, kTol);
    EXPECT_NEAR(a.radius, 5.0, kTol);
    EXPECT_NEAR(a.start_deg, 180.0, kTol);
    EXPECT_NEAR(a.sweep_deg, 180.0, kTol);

    // Четверть окружности радиуса 10 с центром (0,0): (10,0)→(0,10), b = tg(22,5°).
    const double b90 = std::tan(std::numbers::pi / 8.0);
    a = bulgeToArc({10, 0}, {0, 10}, b90).value();
    EXPECT_NEAR(a.center.x, 0.0, kTol);
    EXPECT_NEAR(a.center.y, 0.0, kTol);
    EXPECT_NEAR(a.radius, 10.0, kTol);
    EXPECT_NEAR(a.start_deg, 0.0, kTol);
    EXPECT_NEAR(a.sweep_deg, 90.0, kTol);

    // Та же четверть в обратном направлении (по часовой стрелке): b < 0.
    a = bulgeToArc({0, 10}, {10, 0}, -b90).value();
    EXPECT_NEAR(a.center.x, 0.0, kTol);
    EXPECT_NEAR(a.center.y, 0.0, kTol);
    EXPECT_NEAR(a.start_deg, 90.0, kTol);
    EXPECT_NEAR(a.sweep_deg, -90.0, kTol);

    // Дуга 270° (b = tg(67,5°)) радиуса 10 с центром (0,0): (10,0)→(0,−10) против часовой.
    a = bulgeToArc({10, 0}, {0, -10}, std::tan(3.0 * std::numbers::pi / 8.0)).value();
    EXPECT_NEAR(a.center.x, 0.0, kTol);
    EXPECT_NEAR(a.center.y, 0.0, kTol);
    EXPECT_NEAR(a.radius, 10.0, kTol);
    EXPECT_NEAR(a.sweep_deg, 270.0, kTol);

    EXPECT_EQ(bulgeToArc({0, 0}, {1, 0}, 0.0).error().code, ErrorCode::InvalidInput);
    EXPECT_EQ(bulgeToArc({1, 1}, {1, 1}, 0.5).error().code, ErrorCode::InvalidInput);
}

TEST(DxfReader, LwpolylineWithBulges) {
    // Прямоугольник 20×10 со скруглённой правой стороной (полуокружность, b = 1)
    // и одной вогнутой четвертью (b < 0).
    const P body = {{0, "LWPOLYLINE"}, {8, "0"},     {90, "4"},    {70, "1"},     {43, "0.0"},
                    {10, "0"},         {20, "0"},    {10, "20"},   {20, "0"},     {42, "1.0"},
                    {10, "20"},        {20, "10"},   {42, "-0.4142135623730951"}, {10, "0"},
                    {20, "10"}};
    const Document d = parseOk(dxf(concat({header("AC1015", 4), entities(body), kEof})));
    ASSERT_EQ(d.polylines.size(), 1u);
    const Polyline& p = d.polylines[0];
    EXPECT_TRUE(p.closed);
    EXPECT_TRUE(p.lightweight);
    ASSERT_EQ(p.vertices.size(), 4u);
    EXPECT_DOUBLE_EQ(p.vertices[1].point.x, 20.0);
    EXPECT_DOUBLE_EQ(p.vertices[1].bulge, 1.0);
    EXPECT_DOUBLE_EQ(p.vertices[2].bulge, -0.4142135623730951);
    EXPECT_DOUBLE_EQ(p.vertices[0].bulge, 0.0);
    EXPECT_DOUBLE_EQ(p.vertices[3].bulge, 0.0);

    // Сегмент (20,0)→(20,10), b = 1: центр (20,5), радиус 5, начало под −90° (270°).
    auto a = bulgeToArc(p.vertices[1].point, p.vertices[2].point, p.vertices[1].bulge).value();
    EXPECT_NEAR(a.center.x, 20.0, kTol);
    EXPECT_NEAR(a.center.y, 5.0, kTol);
    EXPECT_NEAR(a.radius, 5.0, kTol);
    EXPECT_NEAR(a.start_deg, 270.0, kTol);
    // Сегмент (20,10)→(0,10), b = −tg(22,5°): хорда 20, θ = −90°, r = 20/√2.
    a = bulgeToArc(p.vertices[2].point, p.vertices[3].point, p.vertices[2].bulge).value();
    EXPECT_NEAR(a.radius, 20.0 / std::sqrt(2.0), 1e-9);
    EXPECT_NEAR(a.center.x, 10.0, 1e-9);
    EXPECT_NEAR(a.center.y, 20.0, 1e-9);
    EXPECT_NEAR(a.sweep_deg, -90.0, 1e-9);
}

TEST(DxfReader, PolylineVertexSeqend) {
    const P body = {
        // Замкнутая 2D-полилиния с выпуклостью.
        {0, "POLYLINE"}, {8, "0"},   {66, "1"},  {10, "0"},   {20, "0"},  {30, "0"},   {70, "1"},
        {0, "VERTEX"},   {8, "0"},   {10, "0"},  {20, "0"},   {42, "0.5"},
        {0, "VERTEX"},   {8, "0"},   {10, "8"},  {20, "0"},
        {0, "VERTEX"},   {8, "0"},   {10, "8"},  {20, "6"},
        {0, "SEQEND"},   {8, "0"},
        // Сглаженная сплайном: управляющие точки (флаг 16) пропускаются, точки на кривой (8) — нет.
        {0, "POLYLINE"}, {70, "4"},
        {0, "VERTEX"},   {10, "0"},  {20, "0"},  {70, "16"},
        {0, "VERTEX"},   {10, "1"},  {20, "1"},  {70, "8"},
        {0, "VERTEX"},   {10, "5"},  {20, "5"},  {70, "16"},
        {0, "VERTEX"},   {10, "2"},  {20, "3"},  {70, "8"},
        {0, "SEQEND"},
        // Многогранная сеть — не 2D-кривая.
        {0, "POLYLINE"}, {70, "64"},
        {0, "VERTEX"},   {10, "0"},  {20, "0"},  {70, "192"},
        {0, "SEQEND"},
    };
    const Document d = parseOk(dxf(concat({header("AC1009", 4), entities(body), kEof})));
    ASSERT_EQ(d.polylines.size(), 2u);
    const Polyline& p = d.polylines[0];
    EXPECT_FALSE(p.lightweight);
    EXPECT_TRUE(p.closed);
    ASSERT_EQ(p.vertices.size(), 3u);
    EXPECT_DOUBLE_EQ(p.vertices[0].bulge, 0.5);
    EXPECT_DOUBLE_EQ(p.vertices[2].point.x, 8.0);
    EXPECT_DOUBLE_EQ(p.vertices[2].point.y, 6.0);
    const Polyline& s = d.polylines[1];
    ASSERT_EQ(s.vertices.size(), 2u);
    EXPECT_DOUBLE_EQ(s.vertices[0].point.x, 1.0);
    EXPECT_DOUBLE_EQ(s.vertices[1].point.y, 3.0);
    EXPECT_EQ(d.skipped.at("POLYLINE (mesh)"), 1);
}

TEST(DxfReader, TextEncodingsAndSpecialCodes) {
    // R2000: строки в $DWGCODEPAGE (ANSI_1251); 0xC4 0xE5 0xF2 0xE0 0xEB 0xFC = «Деталь».
    const std::string cp1251 = "\xC4\xE5\xF2\xE0\xEB\xFC";
    const P body = {{0, "TEXT"},  {8, "0"},        {10, "10"},   {20, "20"},  {40, "3.5"},
                    {1, cp1251 + " %%c20 %%p0.1 45%%d 100%%%"},  {50, "30"},  {41, "0.8"},
                    {51, "15"},   {7, "GOST"},     {72, "1"},    {11, "12"},  {21, "22"},
                    {0, "TEXT"},  {10, "0"},       {20, "0"},    {40, "5"},   {1, "\\U+0428\\U+0430\\U+0444\\U+0442"}};
    const Document d = parseOk(dxf(concat({header("AC1015", 4, "ANSI_1251"), entities(body), kEof})));
    ASSERT_EQ(d.texts.size(), 2u);
    const Text& t = d.texts[0];
    EXPECT_EQ(t.value, "Деталь Ø20 ±0.1 45° 100%");
    EXPECT_FALSE(t.multiline);
    EXPECT_DOUBLE_EQ(t.insertion.x, 10.0);
    EXPECT_DOUBLE_EQ(t.height, 3.5);
    EXPECT_DOUBLE_EQ(t.rotation_deg, 30.0);
    EXPECT_DOUBLE_EQ(t.width_factor, 0.8);
    EXPECT_DOUBLE_EQ(t.oblique_deg, 15.0);
    EXPECT_EQ(t.style, "GOST");
    EXPECT_EQ(t.h_justify, 1);
    ASSERT_TRUE(t.alignment.has_value());
    EXPECT_DOUBLE_EQ(t.alignment->x, 12.0);
    EXPECT_EQ(d.texts[1].value, "Шафт");
    EXPECT_FALSE(d.texts[1].alignment.has_value());

    // R2018 (AC1032): строки в UTF-8 независимо от $DWGCODEPAGE.
    const P body2 = {{0, "TEXT"}, {10, "0"}, {20, "0"}, {40, "2.5"}, {1, "Вал Ø10"}};
    const Document d2 = parseOk(dxf(concat({header("AC1032", 4, "ANSI_1252"), entities(body2), kEof})));
    ASSERT_EQ(d2.texts.size(), 1u);
    EXPECT_EQ(d2.texts[0].value, "Вал Ø10");
}

TEST(DxfReader, MtextChunksAndFormatting) {
    const P body = {{0, "MTEXT"}, {8, "NOTES"}, {10, "5"},  {20, "6"},  {40, "2.5"}, {71, "7"},
                    {3, "{\\fArial|b1|i0;Технические "},  {3, "требования}\\P1. Ra \\S6,3^;"},
                    {1, "\\P2. 50\\~мм\\{x\\}"},           {7, "Standard"}, {11, "0"},   {21, "1"}};
    const Document d = parseOk(dxf(concat({header("AC1021", 4), entities(body), kEof})));
    ASSERT_EQ(d.texts.size(), 1u);
    const Text& t = d.texts[0];
    EXPECT_TRUE(t.multiline);
    EXPECT_EQ(t.value, "Технические требования\n1. Ra 6,3/\n2. 50 мм{x}");
    EXPECT_EQ(t.attachment, 7);
    EXPECT_DOUBLE_EQ(t.rotation_deg, 90.0);  // направление (0,1)
    EXPECT_EQ(t.props.layer, "NOTES");
    EXPECT_DOUBLE_EQ(t.insertion.y, 6.0);
}

TEST(DxfReader, ExtrusionDirections) {
    // Дуга в OCS с выдавливанием (0,0,−1): центр (10,5) в OCS → (−10,5) в мировой СК;
    // дуга 0°…90° в OCS → 90°…180° против часовой в мировой СК.
    const P body = {{0, "ARC"},    {10, "10"},  {20, "5"},  {40, "2"},  {50, "0"},  {51, "90"},
                    {210, "0"},    {220, "0"},  {230, "-1"},
                    {0, "LWPOLYLINE"}, {90, "2"}, {70, "0"}, {10, "1"}, {20, "2"}, {42, "0.5"}, {10, "3"},
                    {20, "2"},     {210, "0"},  {220, "0"}, {230, "-1"},
                    // Наклонная плоскость — не читается, учитывается в skipped.
                    {0, "CIRCLE"}, {10, "0"},   {20, "0"},  {40, "1"},  {210, "0.6"}, {220, "0"}, {230, "0.8"},
                    {0, "SPLINE"}, {8, "0"},    {0, "INSERT"}, {2, "BLOCK1"}};
    const Document d = parseOk(dxf(concat({header("AC1015", 4), entities(body), kEof})));
    ASSERT_EQ(d.arcs.size(), 1u);
    EXPECT_DOUBLE_EQ(d.arcs[0].center.x, -10.0);
    EXPECT_DOUBLE_EQ(d.arcs[0].center.y, 5.0);
    EXPECT_DOUBLE_EQ(d.arcs[0].start_deg, 90.0);
    EXPECT_DOUBLE_EQ(d.arcs[0].end_deg, 180.0);
    ASSERT_EQ(d.polylines.size(), 1u);
    EXPECT_DOUBLE_EQ(d.polylines[0].vertices[0].point.x, -1.0);
    EXPECT_DOUBLE_EQ(d.polylines[0].vertices[0].bulge, -0.5);
    EXPECT_TRUE(d.circles.empty());
    EXPECT_EQ(d.skipped.at("CIRCLE (OCS)"), 1);
    EXPECT_EQ(d.skipped.at("SPLINE"), 1);
    EXPECT_EQ(d.skipped.at("INSERT"), 1);
}

TEST(DxfReader, BlocksAreNotModelGeometryAndZIsReported) {
    const P blocks = {{0, "SECTION"}, {2, "BLOCKS"}, {0, "BLOCK"}, {2, "B"}, {0, "LINE"}, {10, "0"},
                      {20, "0"},      {11, "1"},     {21, "1"},    {0, "ENDBLK"}, {0, "ENDSEC"}};
    const P body = {{0, "LINE"}, {10, "0"}, {20, "0"}, {30, "4"}, {11, "1"}, {21, "0"}, {31, "4"}};
    const Document d = parseOk(dxf(concat({header("AC1015", 4), blocks, entities(body), kEof})));
    EXPECT_EQ(d.lines.size(), 1u);
    ASSERT_EQ(d.warnings.size(), 1u);
    EXPECT_NE(d.warnings[0].find("projected onto XY"), std::string::npos);
}

TEST(DxfReader, UnitsScaling) {
    const P body = {{0, "CIRCLE"}, {10, "1"}, {20, "2"}, {40, "0.5"},
                    {0, "TEXT"},   {10, "1"}, {20, "1"}, {40, "0.125"}, {1, "A"}};
    const Document d = parseOk(dxf(concat({header("AC1015", 1), entities(body), kEof})));
    EXPECT_EQ(d.insunits, 1);
    const Document mm = scaleToMillimeters(d).value();
    EXPECT_EQ(mm.insunits, 4);
    EXPECT_DOUBLE_EQ(mm.circles[0].center.x, 25.4);
    EXPECT_DOUBLE_EQ(mm.circles[0].center.y, 50.8);
    EXPECT_DOUBLE_EQ(mm.circles[0].radius, 12.7);
    EXPECT_DOUBLE_EQ(mm.texts[0].height, 3.175);

    EXPECT_DOUBLE_EQ(*millimetersPerUnit(6), 1000.0);
    EXPECT_DOUBLE_EQ(*millimetersPerUnit(2), 304.8);
    EXPECT_FALSE(millimetersPerUnit(0).has_value());
    const Document unitless = parseOk(dxf(concat({entities(body), kEof})));
    EXPECT_EQ(unitless.insunits, 0);
    EXPECT_EQ(scaleToMillimeters(unitless).error().code, ErrorCode::InvalidInput);
}

TEST(DxfReader, Errors) {
    auto bad = parseDxf(dxf(concat({entities({{0, "LINE"}, {10, "1,5"}, {20, "0"}}), kEof})));
    ASSERT_FALSE(bad.ok());
    EXPECT_EQ(bad.error().code, ErrorCode::ParseError);
    EXPECT_NE(bad.error().message.find("line 8"), std::string::npos) << bad.error().message;

    EXPECT_EQ(parseDxf("  0\nSECTION\n  2\nENTITIES\n  0\n").error().code, ErrorCode::ParseError);
    EXPECT_EQ(parseDxf("  0\nSECTION\n  2\nENTITIES\n  0\nEOF\n").error().code, ErrorCode::ParseError);
    EXPECT_EQ(parseDxf("abc\nSECTION\n").error().code, ErrorCode::ParseError);
    EXPECT_EQ(parseDxf("").error().code, ErrorCode::ParseError);
    EXPECT_EQ(parseDxf(std::string("AutoCAD Binary DXF\r\n\x1a\0", 22)).error().code, ErrorCode::Unsupported);
    EXPECT_EQ(parseDxf(dxf(concat({entities({{0, "VERTEX"}, {10, "0"}}), kEof}))).error().code,
              ErrorCode::ParseError);
    EXPECT_EQ(parseDxf(dxf(concat({entities({{0, "CIRCLE"}, {40, "-1"}}), kEof}))).error().code,
              ErrorCode::ParseError);
    EXPECT_EQ(readDxfFile("/nonexistent/dir/file.dxf").error().code, ErrorCode::IoError);

    EXPECT_TRUE(isDxfPath("a/b/PART.DXF"));
    EXPECT_FALSE(isDxfPath("part.dwg"));
}

TEST(DxfReader, ReadsFile) {
    const std::string path = ::testing::TempDir() + "bendcore_dxf_reader_file.dxf";
    {
        std::ofstream f(path, std::ios::binary);
        f << dxf(concat({header("AC1015", 4), entities({{0, "LINE"}, {10, "3"}, {20, "4"}, {11, "5"}, {21, "6"}}), kEof}));
    }
    auto d = readDxfFile(path);
    std::remove(path.c_str());
    ASSERT_TRUE(d.ok()) << d.error().message;
    ASSERT_EQ(d.value().lines.size(), 1u);
    EXPECT_DOUBLE_EQ(d.value().lines[0].end.y, 6.0);
}
