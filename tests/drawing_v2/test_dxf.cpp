// DXF: файл разбирается независимым читателем групповых кодов (test_support.cpp) и
// сверяется с моделью по сущностям и координатам. Существующий src/dxf_reader.cpp для
// этого не годится: он возвращает фиксированные координаты вместо разбора.
// Дополнительно, если задана переменная BENDCORE_PYTHON_EZDXF (python с пакетом ezdxf, MIT),
// файл читается и проверяется ezdxf (audit); \U+XXXX декодируется функцией ezdxf
// decode_dxf_unicode — ezdxf хранит значение TEXT в том виде, как оно записано в файле.

#include "test_support.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <cstdlib>
#include <set>

using namespace dv2test;

namespace {

std::size_t countGeometry(const Drawing& d, std::size_t index) {
    std::size_t n = 0;
    for (const auto& e : d.entities()) n += e.geometry.index() == index ? 1 : 0;
    return n;
}

std::size_t hatchLineCount(const Drawing& d) {
    std::size_t n = 0;
    for (const auto& e : d.entities()) {
        if (const auto* h = std::get_if<Hatch>(&e.geometry)) n += hatchSegments(*h).value().size();
    }
    return n;
}

struct Written {
    Sample sample;
    std::string text;
    DxfFile dxf;
};

Written writeSample() {
    Written w{makeSample(GostFormat::A3, Orientation::Landscape, systemFont()), "", {}};
    auto r = writeDxf(w.sample.drawing);
    if (!r) throw std::runtime_error(r.error().message);
    w.text = r.value();
    saveToFile(outPath("sample_a3.dxf"), w.text).value();
    w.dxf = parseDxf(readFile(outPath("sample_a3.dxf")));
    return w;
}

const DxfObject* headerVar(const DxfFile& f, const std::string& name) {
    for (const auto& o : *f.section("HEADER")) {
        if (o.type == name) return &o;
    }
    return nullptr;
}

}  // namespace

TEST(DrawingV2Dxf, StructureHeaderAndTables) {
    auto w = writeSample();
    // Файл — чистый ASCII (кириллица — \U+XXXX).
    for (unsigned char c : w.text) ASSERT_LT(c, 0x80u);
    const auto& f = w.dxf;
    std::vector<std::string> names;
    for (const auto& [n, objs] : f.sections) names.push_back(n);
    EXPECT_EQ(names, (std::vector<std::string>{"HEADER", "CLASSES", "TABLES", "BLOCKS", "ENTITIES", "OBJECTS"}));
    EXPECT_EQ(headerVar(f, "$ACADVER")->first(1).value(), "AC1015");
    EXPECT_EQ(headerVar(f, "$DWGCODEPAGE")->first(3).value(), "ANSI_1251");
    EXPECT_EQ(headerVar(f, "$INSUNITS")->first(70).value(), "4");      // мм
    EXPECT_EQ(headerVar(f, "$MEASUREMENT")->first(70).value(), "1");   // метрическая
    EXPECT_DOUBLE_EQ(headerVar(f, "$EXTMAX")->real(10), 420.0);
    EXPECT_DOUBLE_EQ(headerVar(f, "$EXTMAX")->real(20), 297.0);

    // Дескрипторы уникальны и меньше $HANDSEED.
    const unsigned seed = std::stoul(headerVar(f, "$HANDSEED")->first(5).value(), nullptr, 16);
    std::set<unsigned> handles;
    for (const auto& [n, objs] : f.sections) {
        if (n == "HEADER") continue;  // в HEADER код 5 — значение $HANDSEED
        for (const auto& o : objs) {
            for (int code : {5, 105}) {
                for (const auto& h : o.all(code)) {
                    const unsigned v = std::stoul(h, nullptr, 16);
                    EXPECT_TRUE(handles.insert(v).second) << "duplicate handle " << h;
                    EXPECT_LT(v, seed);
                }
            }
        }
    }
    EXPECT_GT(handles.size(), 100u);

    // Таблицы: LTYPE с шаблонами ГОСТ, слои, стили.
    std::map<std::string, const DxfObject*> ltypes, layers, styles;
    for (const auto& o : *f.section("TABLES")) {
        if (o.type == "LTYPE") ltypes[o.first(2).value()] = &o;
        if (o.type == "LAYER") layers[o.first(2).value()] = &o;
        if (o.type == "STYLE") styles[o.first(2).value()] = &o;
    }
    for (const char* n : {"Continuous", "GOST_DASHED", "GOST_DASHDOT_THIN", "GOST_DASHDOT_THICK", "GOST_DASHDOTDOT"}) {
        EXPECT_TRUE(ltypes.count(n)) << n;
    }
    const auto& dashdot = *ltypes.at("GOST_DASHDOT_THIN");
    const auto elems = dashdot.all(49);
    const auto ref = defaultLineTypeSpec(LineType::DashDotThin).pattern;
    ASSERT_EQ(elems.size(), ref.size());
    for (std::size_t i = 0; i < ref.size(); ++i) EXPECT_NEAR(std::stod(elems[i]), ref[i], 1e-6);
    EXPECT_NEAR(dashdot.real(40), 18.0, 1e-9);
    EXPECT_EQ(decodeDxfUnicode(dashdot.first(3).value()), "ГОСТ 2.303: Штрихпунктирная тонкая");
    for (const char* n : {"0", "FRAME", "TITLE", "GEOM", "AXIS", "HATCH"}) EXPECT_TRUE(layers.count(n)) << n;
    EXPECT_EQ(layers.at("AXIS")->first(62).value(), "1");
    EXPECT_NEAR(styles.at("GOST_SLANT")->real(50), 15.0, 1e-9);  // 90° − 75°
}

TEST(DrawingV2Dxf, EntitiesMatchModel) {
    auto w = writeSample();
    const Drawing& d = w.sample.drawing;
    std::map<std::string, std::vector<const DxfObject*>> byType;
    for (const auto& o : *w.dxf.section("ENTITIES")) byType[o.type].push_back(&o);
    // Индексы альтернатив Geometry: LineSeg 0, Arc 1, Circle 2, Polyline 3, Text 4, Hatch 5.
    EXPECT_EQ(byType["LINE"].size(), countGeometry(d, 0) + hatchLineCount(d));
    EXPECT_EQ(byType["ARC"].size(), countGeometry(d, 1));
    EXPECT_EQ(byType["CIRCLE"].size(), countGeometry(d, 2));
    EXPECT_EQ(byType["LWPOLYLINE"].size(), countGeometry(d, 3));
    EXPECT_EQ(byType["TEXT"].size(), countGeometry(d, 4));

    // Сущности по порядку совпадают с моделью: координаты, слой, тип и толщина линии.
    std::size_t li = 0, ti = 0;
    for (const auto& e : d.entities()) {
        if (const auto* l = std::get_if<LineSeg>(&e.geometry)) {
            const DxfObject* o = byType["LINE"][li++];
            EXPECT_NEAR(o->real(10), l->a.x, 1e-6);
            EXPECT_NEAR(o->real(20), l->a.y, 1e-6);
            EXPECT_NEAR(o->real(11), l->b.x, 1e-6);
            EXPECT_NEAR(o->real(21), l->b.y, 1e-6);
            EXPECT_EQ(o->first(8).value(), e.layer);
            EXPECT_EQ(std::stoi(o->first(370).value()), dxfLineweight(d.lineWidth(e.lineType)));
            const auto& spec = d.lineTypeSpec(e.lineType);
            EXPECT_EQ(o->first(6).value(), spec.pattern.empty() ? "Continuous" : spec.dxfName);
        } else if (std::holds_alternative<Hatch>(e.geometry)) {
            li += hatchSegments(std::get<Hatch>(e.geometry)).value().size();
        } else if (const auto* t = std::get_if<Text>(&e.geometry)) {
            const DxfObject* o = byType["TEXT"][ti++];
            EXPECT_EQ(decodeDxfUnicode(o->first(1).value()), t->utf8);
            EXPECT_NEAR(o->real(40), t->height, 1e-9);
            EXPECT_NEAR(o->real(41), t->widthFactor, 1e-6);
            EXPECT_NEAR(o->real(50), t->rotationDeg, 1e-9);
            if (t->hAlign != HAlign::Left || t->vAlign != VAlign::Baseline) {
                EXPECT_NEAR(o->real(11), t->position.x, 1e-6);
                EXPECT_NEAR(o->real(21), t->position.y, 1e-6);
            }
        }
    }
    const DxfObject* c = byType["CIRCLE"][0];
    EXPECT_DOUBLE_EQ(c->real(10), 100);
    EXPECT_DOUBLE_EQ(c->real(20), 180);
    EXPECT_DOUBLE_EQ(c->real(40), 20);
    const DxfObject* a = byType["ARC"][0];
    EXPECT_DOUBLE_EQ(a->real(40), 30);
    EXPECT_DOUBLE_EQ(a->real(50), 0);
    EXPECT_DOUBLE_EQ(a->real(51), 90);
    const DxfObject* p = byType["LWPOLYLINE"][0];
    EXPECT_EQ(p->first(90).value(), "4");
    EXPECT_EQ(std::stoi(p->first(70).value()) & 1, 1);
    const auto xs = p->all(10), ys = p->all(20);
    ASSERT_EQ(xs.size(), 4u);
    EXPECT_DOUBLE_EQ(std::stod(xs[2]), 80);
    EXPECT_DOUBLE_EQ(std::stod(ys[2]), 220);
    // Тексты основной надписи — кириллица после декодирования \U+XXXX.
    std::set<std::string> texts;
    for (const auto* o : byType["TEXT"]) texts.insert(decodeDxfUnicode(o->first(1).value()));
    EXPECT_TRUE(texts.count("Вал ступенчатый"));
    EXPECT_TRUE(texts.count("АБВГ.123456.001"));
    EXPECT_TRUE(texts.count("Технические требования: Ø20"));
}

TEST(DrawingV2Dxf, LineweightsAndUnsupportedText) {
    EXPECT_EQ(dxfLineweight(0.5), 50);
    EXPECT_EQ(dxfLineweight(0.25), 25);
    EXPECT_EQ(dxfLineweight(1.0 / 3.0), 35);
    EXPECT_EQ(dxfLineweight(0.75), 70);
    EXPECT_EQ(dxfLineweight(1.4), 140);
    auto d = Drawing::create(gostSheet(GostFormat::A4).value()).value();
    Text t;
    t.utf8 = "𝔸";  // вне BMP: в DXF R2000 не записывается
    ASSERT_TRUE(d.add({t, "0", LineType::SolidThin}));
    auto r = writeDxf(d);
    ASSERT_FALSE(r);
    EXPECT_EQ(r.error().code, ErrorCode::Unsupported);
}

TEST(DrawingV2Dxf, ReadBackWithEzdxfIfAvailable) {
    const char* py = std::getenv("BENDCORE_PYTHON_EZDXF");
    if (!py || !*py) GTEST_SKIP() << "set BENDCORE_PYTHON_EZDXF to a python with ezdxf installed";
    auto w = writeSample();
    const std::string script =
        "import sys, ezdxf\n"
        "doc = ezdxf.readfile(sys.argv[1])\n"
        "a = doc.audit()\n"
        "msp = doc.modelspace()\n"
        "t = [ezdxf.decode_dxf_unicode(e.dxf.text) for e in msp.query('TEXT')]\n"
        "print(doc.dxfversion, len(a.errors), len(msp.query('LINE')), len(msp.query('CIRCLE')),"
        " len(msp.query('ARC')), len(msp.query('LWPOLYLINE')), len(t), int('Вал ступенчатый' in t),"
        " doc.header['$INSUNITS'])\n";
    saveToFile(outPath("check_ezdxf.py"), script).value();
    int code = 0;
    const std::string out = runCommand(std::string(py) + " " + outPath("check_ezdxf.py") + " " + outPath("sample_a3.dxf"), &code);
    ASSERT_EQ(code, 0) << out;
    const Drawing& d = w.sample.drawing;
    const std::string expected = "AC1015 0 " + std::to_string(countGeometry(d, 0) + hatchLineCount(d)) + " " +
                                 std::to_string(countGeometry(d, 2)) + " " + std::to_string(countGeometry(d, 1)) +
                                 " " + std::to_string(countGeometry(d, 3)) + " " + std::to_string(countGeometry(d, 4)) +
                                 " 1 4\n";
    EXPECT_EQ(out, expected);
}
