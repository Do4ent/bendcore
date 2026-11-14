// Читатель DXF против независимых писателей:
// 1) DXF R2000 из bendcore_drawing_v2 (проверен ezdxf в tests/drawing_v2/test_dxf.cpp):
//    каждый примитив модели чертежа сверяется с прочитанным по типу, слою и координатам;
// 2) если задана BENDCORE_PYTHON_EZDXF (python с пакетом ezdxf, MIT) — файлы R12, R2000 и
//    R2018, записанные ezdxf (tests/dxf/ezdxf_fixture.py), сверяются с тем, что из них
//    читает сам ezdxf, включая центры и радиусы дуг выпуклости (ezdxf.math.bulge_to_arc).
#include <bendcore/dxf_reader.hpp>

#include <bendcore/drawing_v2/export.hpp>

#include "../drawing_v2/test_support.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace dx = bendcore::dxf;
namespace dv = bendcore::drawing_v2;

namespace {

// Писатель выводит не более 6 знаков после точки.
constexpr double kWriteTol = 5.1e-7;

double norm360(double a) {
    double r = std::fmod(a, 360.0);
    return r < 0 ? r + 360.0 : r;
}

}  // namespace

TEST(DxfReaderRoundTrip, DrawingV2SampleSheet) {
    auto sample = dv2test::makeSample(dv::GostFormat::A3, dv::Orientation::Landscape, dv2test::systemFont());
    const dv::Drawing& d = sample.drawing;
    auto text = dv::writeDxf(d);
    ASSERT_TRUE(text.ok()) << text.error().message;
    auto parsed = dx::parseDxf(text.value());
    ASSERT_TRUE(parsed.ok()) << parsed.error().message;
    const dx::Document& r = parsed.value();

    EXPECT_EQ(r.version, "AC1015");
    EXPECT_EQ(r.insunits, 4);
    EXPECT_TRUE(r.skipped.empty());
    EXPECT_TRUE(r.warnings.empty()) << r.warnings.front();
    ASSERT_EQ(r.layers.size(), d.layers().size());
    for (const auto& l : d.layers()) {
        const dx::Layer* rl = r.find_layer(l.name);
        ASSERT_NE(rl, nullptr) << l.name;
        EXPECT_EQ(rl->color, l.aciColor);
    }

    std::size_t li = 0, ci = 0, ai = 0, pi = 0, ti = 0;
    for (const dv::Entity& e : d.entities()) {
        if (const auto* l = std::get_if<dv::LineSeg>(&e.geometry)) {
            ASSERT_LT(li, r.lines.size());
            const dx::Line& rl = r.lines[li++];
            EXPECT_EQ(rl.props.layer, e.layer);
            EXPECT_NEAR(rl.start.x, l->a.x, kWriteTol);
            EXPECT_NEAR(rl.start.y, l->a.y, kWriteTol);
            EXPECT_NEAR(rl.end.x, l->b.x, kWriteTol);
            EXPECT_NEAR(rl.end.y, l->b.y, kWriteTol);
        } else if (const auto* c = std::get_if<dv::Circle>(&e.geometry)) {
            ASSERT_LT(ci, r.circles.size());
            const dx::Circle& rc = r.circles[ci++];
            EXPECT_EQ(rc.props.layer, e.layer);
            EXPECT_NEAR(rc.center.x, c->center.x, kWriteTol);
            EXPECT_NEAR(rc.center.y, c->center.y, kWriteTol);
            EXPECT_NEAR(rc.radius, c->radius, kWriteTol);
        } else if (const auto* a = std::get_if<dv::Arc>(&e.geometry)) {
            ASSERT_LT(ai, r.arcs.size());
            const dx::Arc& ra = r.arcs[ai++];
            EXPECT_NEAR(ra.center.x, a->center.x, kWriteTol);
            EXPECT_NEAR(ra.radius, a->radius, kWriteTol);
            EXPECT_NEAR(ra.start_deg, norm360(a->startDeg), kWriteTol);
            EXPECT_NEAR(ra.end_deg, norm360(a->endDeg), kWriteTol);
        } else if (const auto* p = std::get_if<dv::Polyline>(&e.geometry)) {
            ASSERT_LT(pi, r.polylines.size());
            const dx::Polyline& rp = r.polylines[pi++];
            EXPECT_EQ(rp.closed, p->closed);
            ASSERT_EQ(rp.vertices.size(), p->points.size());
            for (std::size_t k = 0; k < p->points.size(); ++k) {
                EXPECT_NEAR(rp.vertices[k].point.x, p->points[k].x, kWriteTol);
                EXPECT_NEAR(rp.vertices[k].point.y, p->points[k].y, kWriteTol);
                EXPECT_EQ(rp.vertices[k].bulge, 0.0);
            }
        } else if (const auto* t = std::get_if<dv::Text>(&e.geometry)) {
            ASSERT_LT(ti, r.texts.size());
            const dx::Text& rt = r.texts[ti++];
            EXPECT_EQ(rt.value, t->utf8);
            EXPECT_NEAR(rt.insertion.x, t->position.x, kWriteTol);
            EXPECT_NEAR(rt.insertion.y, t->position.y, kWriteTol);
            EXPECT_NEAR(rt.height, t->height, kWriteTol);
            EXPECT_NEAR(rt.rotation_deg, t->rotationDeg, kWriteTol);
            EXPECT_NEAR(rt.width_factor, t->widthFactor, kWriteTol);
            EXPECT_EQ(rt.style, t->slanted ? "GOST_SLANT" : "GOST_UPRIGHT");
        } else if (const auto* h = std::get_if<dv::Hatch>(&e.geometry)) {
            // Штриховка пишется отрезками (docs/drawing/EXPORT.md).
            auto segs = dv::hatchSegments(*h).value();
            for (const auto& s : segs) {
                ASSERT_LT(li, r.lines.size());
                const dx::Line& rl = r.lines[li++];
                EXPECT_EQ(rl.props.layer, e.layer);
                EXPECT_NEAR(rl.start.x, s.a.x, kWriteTol);
                EXPECT_NEAR(rl.end.y, s.b.y, kWriteTol);
            }
        }
    }
    EXPECT_EQ(li, r.lines.size());
    EXPECT_EQ(ci, r.circles.size());
    EXPECT_EQ(ai, r.arcs.size());
    EXPECT_EQ(pi, r.polylines.size());
    EXPECT_EQ(ti, r.texts.size());
    EXPECT_GT(ti, 10u);  // основная надпись: тексты на кириллице через \U+XXXX
}

namespace {

std::vector<std::string> split(const std::string& line) {
    std::istringstream is(line);
    std::vector<std::string> out;
    for (std::string w; is >> w;) out.push_back(w);
    return out;
}

std::string unhex(const std::string& h) {
    if (h == "-") return "";
    std::string out;
    for (std::size_t i = 0; i + 1 < h.size(); i += 2) out.push_back(char(std::stoi(h.substr(i, 2), nullptr, 16)));
    return out;
}

void checkAgainstEzdxf(const std::string& version) {
    SCOPED_TRACE(version);
    const char* py = std::getenv("BENDCORE_PYTHON_EZDXF");
    const std::string dxfPath = dv2test::outPath("ezdxf_" + version + ".dxf");
    const std::string expPath = dv2test::outPath("ezdxf_" + version + ".txt");
    int code = 0;
    const std::string out = dv2test::runCommand(std::string(py) + " -I " BENDCORE_EZDXF_FIXTURE " " + version + " " +
                                                    dxfPath + " " + expPath + " 2>&1",
                                                &code);
    ASSERT_EQ(code, 0) << out;
    auto parsed = dx::readDxfFile(dxfPath);
    ASSERT_TRUE(parsed.ok()) << parsed.error().message;
    const dx::Document& r = parsed.value();

    std::ifstream ef(expPath);
    std::size_t li = 0, ci = 0, ai = 0, pi = 0, ti = 0;
    const dx::Polyline* lastPoly = nullptr;
    std::size_t bulgeSeg = 0;
    int layers = 0;
    const double tol = 1e-9;
    for (std::string line; std::getline(ef, line);) {
        const auto f = split(line);
        if (f.empty()) continue;
        auto num = [&](std::size_t k) { return std::stod(f.at(k)); };
        if (f[0] == "VERSION") {
            EXPECT_EQ(r.version, f[1]);
        } else if (f[0] == "INSUNITS") {
            EXPECT_EQ(r.insunits, std::stoi(f[1]));
        } else if (f[0] == "LAYER") {
            ++layers;
            const dx::Layer* l = r.find_layer(unhex(f[1]));
            ASSERT_NE(l, nullptr) << unhex(f[1]);
            EXPECT_EQ(l->color, std::stoi(f[2]));
        } else if (f[0] == "LINE") {
            ASSERT_LT(li, r.lines.size());
            const dx::Line& l = r.lines[li++];
            EXPECT_NEAR(l.start.x, num(1), tol);
            EXPECT_NEAR(l.start.y, num(2), tol);
            EXPECT_NEAR(l.end.x, num(3), tol);
            EXPECT_NEAR(l.end.y, num(4), tol);
            EXPECT_EQ(l.props.layer, unhex(f[5]));
            EXPECT_EQ(l.props.color, std::stoi(f[6]));
        } else if (f[0] == "CIRCLE") {
            ASSERT_LT(ci, r.circles.size());
            const dx::Circle& c = r.circles[ci++];
            EXPECT_NEAR(c.center.x, num(1), tol);
            EXPECT_NEAR(c.center.y, num(2), tol);
            EXPECT_NEAR(c.radius, num(3), tol);
        } else if (f[0] == "ARC") {
            ASSERT_LT(ai, r.arcs.size());
            const dx::Arc& a = r.arcs[ai++];
            EXPECT_NEAR(a.center.x, num(1), tol);
            EXPECT_NEAR(a.center.y, num(2), tol);
            EXPECT_NEAR(a.radius, num(3), tol);
            EXPECT_NEAR(a.start_deg, num(4), tol);
            EXPECT_NEAR(a.end_deg, num(5), tol);
        } else if (f[0] == "POLY") {
            ASSERT_LT(pi, r.polylines.size());
            const dx::Polyline& p = r.polylines[pi++];
            EXPECT_EQ(p.lightweight, f[1] == "1");
            EXPECT_EQ(p.closed, f[2] == "1");
            const std::size_t n = std::stoul(f[3]);
            ASSERT_EQ(p.vertices.size(), n);
            for (std::size_t k = 0; k < n; ++k) {
                EXPECT_NEAR(p.vertices[k].point.x, num(4 + 3 * k), tol);
                EXPECT_NEAR(p.vertices[k].point.y, num(5 + 3 * k), tol);
                EXPECT_NEAR(p.vertices[k].bulge, num(6 + 3 * k), tol);
            }
            lastPoly = &p;
            bulgeSeg = 0;
        } else if (f[0] == "BULGE") {
            ASSERT_NE(lastPoly, nullptr);
            // Следующий сегмент с ненулевой выпуклостью последней полилинии.
            const auto& v = lastPoly->vertices;
            while (bulgeSeg < v.size() && v[bulgeSeg].bulge == 0.0) ++bulgeSeg;
            ASSERT_LT(bulgeSeg, v.size());
            const auto& p2 = v[(bulgeSeg + 1) % v.size()].point;
            auto arc = dx::bulgeToArc(v[bulgeSeg].point, p2, v[bulgeSeg].bulge);
            ++bulgeSeg;
            ASSERT_TRUE(arc.ok());
            EXPECT_NEAR(arc.value().center.x, num(1), 1e-9);
            EXPECT_NEAR(arc.value().center.y, num(2), 1e-9);
            EXPECT_NEAR(arc.value().radius, num(3), 1e-9);
        } else if (f[0] == "TEXT" || f[0] == "MTEXT") {
            ASSERT_LT(ti, r.texts.size());
            const dx::Text& t = r.texts[ti++];
            EXPECT_EQ(t.multiline, f[0] == "MTEXT");
            EXPECT_EQ(t.value, unhex(f[1]));
            EXPECT_NEAR(t.insertion.x, num(2), tol);
            EXPECT_NEAR(t.insertion.y, num(3), tol);
            EXPECT_NEAR(t.height, num(4), tol);
            EXPECT_NEAR(t.rotation_deg, num(5), tol);
        }
    }
    EXPECT_EQ(int(r.layers.size()), layers);
    EXPECT_EQ(li, r.lines.size());
    EXPECT_EQ(ci, r.circles.size());
    EXPECT_EQ(ai, r.arcs.size());
    EXPECT_EQ(pi, r.polylines.size());
    EXPECT_EQ(ti, r.texts.size());
    EXPECT_GE(li + ci + ai + pi + ti, 7u);
}

}  // namespace

TEST(DxfReaderRoundTrip, EzdxfWrittenFiles) {
    const char* py = std::getenv("BENDCORE_PYTHON_EZDXF");
    if (!py || !*py) GTEST_SKIP() << "set BENDCORE_PYTHON_EZDXF to a python with ezdxf installed";
    for (const char* v : {"R12", "R2000", "R2018"}) checkAgainstEzdxf(v);
}
