// Сквозной тест изделия уровня «редуктор» (критерий готовности v1.0, docs/ROADMAP_GENERAL_CAD.md):
// редуктор РЦ-80 из examples/gearbox/ строится воспроизводимо, и проверяется против независимых
// эталонов: состав (число деталей и количества — из построения изделия), массы простых деталей —
// против формул из их размеров, отсутствие пересечений (кроме резьбовых соединений с предельным
// объёмом по формуле), степени свободы (валы вращаются, остальное неподвижно), позиции сборочного
// чертежа = спецификации, ассоциативность всех размеров (изменение параметра вала → обновлённый
// чертёж), чтение всех записанных файлов обратно в bendcore, PDF (pdfinfo, pdftotext).

#include "gearbox.hpp"
#include "../drawing_v2/balloon_checks.hpp"

#include <bendcore/assembly_v2/bom.hpp>
#include <bendcore/assembly_v2/interference.hpp>
#include <bendcore/kernel/step_io.hpp>
#include <bendcore/modeling/gear.hpp>
#include <bendcore/modeling/part_features.hpp>

#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <set>
#include <sstream>

using namespace bendcore::examples::gearbox;
namespace asmv2 = bendcore::assembly_v2;
namespace dd = bendcore::drawing_v2::document;
namespace dv2 = bendcore::drawing_v2;
namespace fs = std::filesystem;

namespace {

constexpr double kPi = 3.14159265358979323846;

double volume(const TopoDS_Shape& s) {
    GProp_GProps p;
    BRepGProp::VolumeProperties(s, p);
    return std::abs(p.Mass());
}

struct Product {
    std::vector<PartEntry> parts;
    std::vector<StandardPart> standard;
    Gearbox gearbox;
    DrawingSet set;
    UpdateSummary summary;
    KitFiles kit;
    bendcore::drawing_v2::views::ViewCache cache{64, 2};
    std::string dir = std::string(BENDCORE_GEARBOX_OUT) + "/kit";
};

// Изделие строится один раз на весь набор тестов.
Product& product() {
    static std::unique_ptr<Product> p = [] {
        auto out = std::make_unique<Product>();
        auto parts = buildParts();
        EXPECT_TRUE(parts.ok()) << parts.error().message;
        out->parts = std::move(parts).value();
        auto st = buildStandardParts();
        EXPECT_TRUE(st.ok()) << st.error().message;
        out->standard = std::move(st).value();
        auto g = buildAssembly(out->parts, out->standard);
        EXPECT_TRUE(g.ok()) << g.error().message;
        out->gearbox = std::move(g).value();
        auto set = defineDrawings(out->parts, out->gearbox);
        EXPECT_TRUE(set.ok()) << set.error().message;
        out->set = std::move(set).value();
        auto u = updateDrawings(out->set, out->parts, out->gearbox, out->cache);
        EXPECT_TRUE(u.ok()) << u.error().message;
        out->summary = std::move(u).value();
        fs::remove_all(out->dir);
        auto kit = writeKit(out->dir, out->parts, out->gearbox, out->set);
        EXPECT_TRUE(kit.ok()) << kit.error().message;
        out->kit = std::move(kit).value();
        return out;
    }();
    return *p;
}

std::string run(const std::string& cmd, int* code) {
    std::string out;
    FILE* f = popen(cmd.c_str(), "r");
    if (!f) return out;
    std::array<char, 4096> buf{};
    while (std::size_t n = std::fread(buf.data(), 1, buf.size(), f)) out.append(buf.data(), n);
    *code = pclose(f);
    return out;
}

std::string readFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    std::stringstream s;
    s << in.rdbuf();
    return s.str();
}

const StandardPart* standardPart(const Product& p, const std::string& key) {
    for (const auto& s : p.standard)
        if (s.key == key) return &s;
    return nullptr;
}

double var(const PartEntry& p, const char* name) { return p.doc->variableValue(name).value(); }

}  // namespace

// Состав: 13 деталей собственной разработки, 11 типоразмеров стандартных изделий (подшипники
// ГОСТ 8338, шпонки ГОСТ 23360, болты, шайбы, гайки, штифты — bendcore_fasteners), 72 вхождения;
// количества — из конструкции (4 крышки × 4 болта, 4 болта фланца с двумя шайбами и гайкой).
TEST(E2EProductGearbox, Composition) {
    Product& p = product();
    EXPECT_EQ(p.parts.size(), 13u);
    EXPECT_EQ(p.standard.size(), 5u);
    const auto& doc = *p.gearbox.doc;
    EXPECT_EQ(doc.definitions().size(), 1u + 13 + 5 + 6);  // корень + детали + стандартные + крепёж
    EXPECT_EQ(doc.topLevel().size(), 72u);
    auto bom = asmv2::flatBom(doc);
    ASSERT_TRUE(bom.ok()) << bom.error().message;
    std::map<asmv2::DefinitionId, int> qty;
    for (const auto& r : bom.value().rows) qty[r.definition] = r.quantity;
    const std::map<std::string, int> expected{{"housing", 1},    {"cover", 1},      {"shaft_in", 1},  {"shaft_out", 1}, {"wheel", 1},
                                              {"cap_in_t", 1},   {"cap_in_b", 1},   {"cap_out_b", 1}, {"cap_out_t", 1}, {"spacer", 1},
                                              {"gasket", 1},     {"plug", 2},       {"plug_gasket", 2}, {"bearing205", 2}, {"bearing206", 2},
                                              {"key6", 1},       {"key10", 1},      {"key8", 1},      {"bolt6", 16},    {"spring6", 16},
                                              {"bolt10", 4},     {"washer10", 8},   {"nut10", 4},     {"pin8", 2}};
    for (const auto& [key, n] : expected) EXPECT_EQ(qty[p.gearbox.defs.at(key)], n) << key;
    int total = 0;
    for (const auto& [d, n] : qty) total += n;
    EXPECT_EQ(total, 72);
}

// Массы простых деталей — против формул из размеров (≤ 1e-9 отн.); масса изделия = Σ V·ρ.
TEST(E2EProductGearbox, MassesAgainstFormulas) {
    Product& p = product();
    // Подшипники ГОСТ 8338 (кольца с желобами и скруглениями r + шарики) — теорема Паппа —
    // Гульдена для профилей колец и Z·π·Dw³/6; шпонки ГОСТ 23360 исп. 1 с фасками s:
    // (b·h − 2s²)(l − b) + 2·π(h·R²/2 − s²(R − s/3)), R = b/2. Размеры d, D, B, r — из таблицы
    // стандарта (205: 25×52×15, 206: 30×62×16, r = 1), b×h×l и s — из ГОСТ 23360.
    auto bearingVolume = [](double d, double D, double B, double r, const StandardPart& sp) {
        const double Dw = 0.3 * (D - d), Dpw = (d + D) / 2, rg = 0.52 * Dw, cl = 0.02;
        const int Z = static_cast<int>(std::floor(kPi * Dpw / (1.6 * Dw)));
        EXPECT_DOUBLE_EQ(sp.dimension("Z"), Z);
        const double spA = r * r * (1 - kPi / 4), spX = (10 - 3 * kPi) / (12 - 3 * kPi) * r;
        const double h = 0.2 * Dw + cl / 2, th = 2 * std::acos((rg - h) / rg);
        const double segA = rg * rg / 2 * (th - std::sin(th)), segY = 4 * rg * std::pow(std::sin(th / 2), 3) / (3 * (th - std::sin(th)));
        const double ri = Dpw / 2 - 0.3 * Dw, rbi = Dpw / 2 - Dw / 2 - cl / 2, ro = Dpw / 2 + 0.3 * Dw, rbo = Dpw / 2 + Dw / 2 + cl / 2;
        const double Mi = (ri * ri - d * d / 4) / 2 * B - 2 * spA * (d / 2 + spX) - segA * (rbi + rg - segY);
        const double Mo = (D * D / 4 - ro * ro) / 2 * B - 2 * spA * (D / 2 - spX) - segA * (rbo - rg + segY);
        return 2 * kPi * (Mi + Mo) + Z * kPi * Dw * Dw * Dw / 6;
    };
    auto keyVolume = [](double b, double h, double l, double s) {
        const double R = b / 2;
        return (b * h - 2 * s * s) * (l - b) + 2 * kPi * (h * R * R / 2 - s * s * (R - s / 3));
    };
    const std::map<std::string, double> standardVolume{
        {"bearing205", bearingVolume(25, 52, 15, 1, *standardPart(p, "bearing205"))},
        {"bearing206", bearingVolume(30, 62, 16, 1, *standardPart(p, "bearing206"))},
        {"key6", keyVolume(6, 6, 28, 0.25)},
        {"key10", keyVolume(10, 8, 32, 0.4)},
        {"key8", keyVolume(8, 7, 32, 0.25)}};
    for (const auto& s : p.standard) {
        const double v = volume(p.gearbox.doc->definition(p.gearbox.defs.at(s.key)).shape), f = standardVolume.at(s.key);
        EXPECT_NEAR(v, f, 1e-7 * f) << s.key;
    }
    auto formula = [&](const std::string& key) -> double {
        const PartEntry& e = *findPart(p.parts, key);
        if (key == "spacer") {
            const double D = var(e, "D"), d = var(e, "d"), l = var(e, "l"), c = var(e, "c");
            return kPi / 4 * (D * D - d * d) * l - 2 * kPi * c * c * (D / 2 - c / 3);
        }
        if (key == "plug") {
            const double s = var(e, "s"), k = var(e, "k"), d = var(e, "d"), l = var(e, "l"), c = var(e, "c");
            return std::sqrt(3.0) / 2 * s * s * k + kPi * d * d / 4 * l - kPi * c * c * (d / 2 - c / 3);
        }
        if (key == "plug_gasket") {
            const double D = var(e, "D"), d = var(e, "d");
            return kPi / 4 * (D * D - d * d) * var(e, "h");
        }
        // Прокладка разъёма: рамка минус 4 отверстия болтов и 2 штифтов, 4 скругления углов.
        const double Xf = var(e, "Xf"), y0 = var(e, "y0"), y1 = var(e, "y1"), xi = var(e, "xi"), yi0 = var(e, "yi0"),
                     yi1 = var(e, "yi1"), db = var(e, "db"), dp = var(e, "dp"), rf = var(e, "rf");
        return (2 * Xf * (y1 - y0) - 2 * xi * (yi1 - yi0) - 4 * kPi * db * db / 4 - 2 * kPi * dp * dp / 4 - 4 * (1 - kPi / 4) * rf * rf) *
               var(e, "tg");
    };
    for (const char* key : {"spacer", "plug", "plug_gasket", "gasket"}) {
        const PartEntry& e = *findPart(p.parts, key);
        const double v = volume(e.doc->finalShape().shape()), f = formula(key);
        EXPECT_NEAR(v, f, 1e-9 * f) << key;
    }
    // Зубчатые венцы: объём, снятый нарезанием зубьев, — кольцо (π·da²/4 − A)·b, где A — площадь
    // торцового профиля венца, численный интеграл по точкам формул эвольвенты и переходной кривой
    // (не по B-сплайнам модели); тело без венца — тот же документ с подавленной операцией.
    for (const auto& [key, width] : std::vector<std::pair<std::string, const char*>>{{"wheel", "bw"}, {"shaft_in", "b1"}}) {
        const PartEntry& e = *findPart(p.parts, key);
        const auto gears = bendcore::modeling::gearsOf(e.doc->finalShape());
        ASSERT_EQ(gears.size(), 1u) << key;
        const auto& g = gears.front().gear;
        EXPECT_DOUBLE_EQ(g.module, 2.0);
        EXPECT_EQ(g.teeth, key == "wheel" ? 60 : 20);
        EXPECT_NEAR(g.width, var(e, width), 1e-9);
        auto side = bendcore::modeling::toothSidePoints(g, 4000);
        ASSERT_TRUE(side.ok());
        std::vector<std::pair<double, double>> poly{{0.0, 0.0}};
        for (const auto* part : {&side.value().root, &side.value().fillet, &side.value().involute, &side.value().tipHalf})
            poly.insert(poly.end(), part->begin(), part->end());
        double a2 = 0;
        for (std::size_t i = 0; i < poly.size(); ++i) {
            const auto& u = poly[i];
            const auto& w = poly[(i + 1) % poly.size()];
            a2 += u.first * w.second - w.first * u.second;
        }
        const double A = g.teeth * std::abs(a2), da = var(e, "da");
        auto json = e.doc->toJson();
        ASSERT_TRUE(json.ok());
        auto blank = bendcore::modeling::PartDocument::fromJson(json.value(), bendcore::modeling::partRegistry());
        ASSERT_TRUE(blank.ok());
        ASSERT_TRUE(blank.value().setSuppressed("Teeth", true).ok());
        blank.value().rebuild();
        const double removed = volume(blank.value().finalShape().shape()) - volume(e.doc->finalShape().shape());
        // Фаски 0,5m × 45° по кромкам вершин на обоих торцах: z·∫ (r − ra + c)·2ψ(r)·r dr,
        // ψ(r) = π/(2z) + inv αt − inv α(r) (x = 0, прямой зуб), Симпсон по r ∈ [ra − c, ra].
        const double c = 0.5 * g.module, ra = da / 2, alpha = 20.0 * kPi / 180, rb = g.module * g.teeth / 2 * std::cos(alpha);
        auto inv = [](double a) { return std::tan(a) - a; };
        auto f = [&](double r) { return (r - ra + c) * 2 * (kPi / (2 * g.teeth) + inv(alpha) - inv(std::acos(rb / r))) * r; };
        double simpson = f(ra - c) + f(ra);
        for (int i = 1; i < 2000; ++i) simpson += (i % 2 ? 4 : 2) * f(ra - c + i * c / 2000);
        const double chamfers = 2 * g.teeth * simpson * c / 2000 / 3;
        const double expected = (kPi * da * da / 4 - A) * g.width + chamfers;
        EXPECT_NEAR(removed, expected, 3e-6 * expected) << key;
        std::printf("[gearbox] %s: teeth remove %.3f mm3 (ring - profile integral %.3f)\n", key.c_str(), removed, expected);
    }
    // Масса изделия по BOM = Σ объём · плотность всех вхождений.
    auto placed = p.gearbox.doc->placedParts();
    ASSERT_TRUE(placed.ok());
    double mass = 0;
    for (const auto& o : placed.value())
        mass += volume(o.shape) * 1e-9 * *p.gearbox.doc->definition(o.definition).properties.density;
    auto bom = asmv2::flatBom(*p.gearbox.doc);
    ASSERT_TRUE(bom.ok());
    ASSERT_TRUE(bom.value().totalMass.has_value());
    EXPECT_NEAR(*bom.value().totalMass, mass, 1e-6 * mass);
    std::printf("[gearbox] mass %.3f kg\n", mass);
}

// Пересечений нет, кроме резьбовых соединений: болт М6 и пробка М16×1,5 в отверстиях под резьбу
// (модель резьбы условная: стержень номинального диаметра в отверстии диаметра сверла D − P) —
// объём не больше кольца π/4·(d² − (D − P)²) на длине захода. Зубья шестерни и колеса (боковой
// зазор нулевой) касаются, не пересекаясь.
TEST(E2EProductGearbox, NoInterferenceExceptThreads) {
    Product& p = product();
    auto rep = asmv2::checkInterference(*p.gearbox.doc);
    ASSERT_TRUE(rep.ok()) << rep.error().message;
    const std::set<asmv2::OccurrenceId> screws(p.gearbox.threadedScrews.begin(), p.gearbox.threadedScrews.end());
    int threads = 0;
    bool gearsTouch = false;
    for (const auto& pair : rep.value().pairs) {
        const auto& a = rep.value().parts[pair.a];
        const auto& b = rep.value().parts[pair.b];
        const std::set<asmv2::OccurrenceId> occ{a.path.front(), b.path.front()};
        if (occ == std::set<asmv2::OccurrenceId>{p.gearbox.occ.at("shaft_in"), p.gearbox.occ.at("wheel")})
            gearsTouch = pair.kind == asmv2::ContactKind::Touching;
        if (pair.kind != asmv2::ContactKind::Interference) continue;
        const auto& screw = screws.count(a.path.front()) ? a : b;
        ASSERT_TRUE(screws.count(screw.path.front())) << a.pathName << " x " << b.pathName << ": " << pair.volume;
        const auto& host = &screw == &a ? b : a;
        EXPECT_TRUE(host.path.front() == p.gearbox.occ.at("housing") || host.path.front() == p.gearbox.occ.at("cover")) << host.pathName;
        EXPECT_GT(pair.volume, 0.0);
        EXPECT_LE(pair.volume, p.gearbox.threadAllowance.at(screw.definition) + 1e-6) << screw.pathName;
        ++threads;
    }
    EXPECT_EQ(threads, 18);  // 16 болтов крышек подшипников + 2 пробки
    EXPECT_TRUE(gearsTouch);
}

// Степени свободы: 59 = 1 (валы связаны передаточным сопряжением) + повороты осесимметричных деталей
// вокруг своих осей. Поворот вала-шестерни на 30° и вала с колесом на −10° (u = 3) — допустимое
// движение; поворот зафиксированного вала-шестерни на φ поворачивает колесо на −φ/u.
TEST(E2EProductGearbox, DegreesOfFreedom) {
    Product& p = product();
    auto& doc = *p.gearbox.doc;
    auto rep = doc.solve();
    ASSERT_TRUE(rep.ok());
    EXPECT_TRUE(rep.value().success);
    EXPECT_LT(rep.value().residualNorm, 1e-9);
    EXPECT_EQ(rep.value().totalDof, p.gearbox.expectedDof);
    EXPECT_EQ(p.gearbox.expectedDof, 59);
    for (const auto& s : rep.value().subsystems) {
        EXPECT_TRUE(s.redundantMates.empty());
        EXPECT_TRUE(s.conflictingMates.empty());
    }
    auto occ = [&](const std::string& k) { return *doc.findOccurrence(p.gearbox.occ.at(k)); };
    std::vector<std::pair<asmv2::OccurrenceId, asmv2::Transform>> saved;
    for (const auto& o : doc.topLevel()) saved.push_back({o.id, o.placement});
    auto turnAbout = [](double y, double angle) {
        const asmv2::Vec3 axisPoint(0, y, 78);
        return asmv2::Transform::fromTranslation(axisPoint) * asmv2::Transform::fromAxisAngle(asmv2::Vec3::UnitX(), angle) *
               asmv2::Transform::fromTranslation(-axisPoint);
    };
    using Group = std::vector<std::string>;
    const Group input{"shaft_in", "key_in"}, output{"shaft_out", "wheel", "spacer", "key_wheel", "key_out"};
    // Совместный поворот по передаче: решатель ничего не сдвигает.
    std::map<std::string, asmv2::Transform> target;
    for (const auto& [group, y, angle] : std::vector<std::tuple<Group, double, double>>{{input, 0.0, kPi / 6}, {output, 80.0, -kPi / 18}})
        for (const auto& k : group) {
            target[k] = turnAbout(y, angle) * occ(k).placement;
            ASSERT_TRUE(doc.setPlacement(p.gearbox.occ.at(k), target[k]).ok());
        }
    const asmv2::Transform cap = occ("cap_out_r").placement;
    auto r = doc.solve();
    ASSERT_TRUE(r.ok());
    EXPECT_TRUE(r.value().success);
    EXPECT_LT(r.value().residualNorm, 1e-9);
    for (const auto& [k, t] : target) {
        EXPECT_LT((occ(k).placement.position - t.position).norm(), 1e-7) << k;
        EXPECT_LT((occ(k).placement.rotationMatrix() - t.rotationMatrix()).norm(), 1e-7) << k;
    }
    EXPECT_LT((occ("cap_out_r").placement.position - cap.position).norm(), 1e-9);
    for (const auto& [id, t] : saved) ASSERT_TRUE(doc.setPlacement(id, t).ok());
    // Передача: вал-шестерня повёрнут на φ и зафиксирован — колесо поворачивается на −φ/3 вокруг
    // своей оси (оси валов сонаправлены, внешнее зацепление), шпонки и втулка — вместе с ним.
    for (double phi : {kPi / 6, -kPi / 4}) {
        const asmv2::Transform wheel0 = occ("wheel").placement, rel = occ("shaft_out").placement.inverse() * wheel0;
        for (const auto& k : input) ASSERT_TRUE(doc.setPlacement(p.gearbox.occ.at(k), turnAbout(0, phi) * occ(k).placement).ok());
        ASSERT_TRUE(doc.setFixed(p.gearbox.occ.at("shaft_in"), true).ok());
        auto t = doc.solve();
        ASSERT_TRUE(t.ok());
        EXPECT_TRUE(t.value().success);
        EXPECT_LT(t.value().residualNorm, 1e-9);
        const asmv2::Transform expected = turnAbout(80, -phi / 3) * wheel0;
        EXPECT_LT((occ("wheel").placement.position - expected.position).norm(), 1e-6) << phi;
        EXPECT_LT((occ("wheel").placement.rotationMatrix() - expected.rotationMatrix()).norm(), 1e-7) << phi;
        const asmv2::Transform relAfter = occ("shaft_out").placement.inverse() * occ("wheel").placement;
        EXPECT_LT((relAfter.position - rel.position).norm(), 1e-7);
        ASSERT_TRUE(doc.setFixed(p.gearbox.occ.at("shaft_in"), false).ok());
        for (const auto& [id, tr] : saved) ASSERT_TRUE(doc.setPlacement(id, tr).ok());
    }
    auto back = doc.solve();
    ASSERT_TRUE(back.ok());
    EXPECT_LT(back.value().residualNorm, 1e-9);
}

// Позиции сборочного чертежа = спецификации: каждая составная часть — ровно одна позиция с её
// номером; разделы ГОСТ 2.106 — документация, детали (13), стандартные изделия (11).
TEST(E2EProductGearbox, PositionsMatchSpecification) {
    Product& p = product();
    const auto& spec = *p.set.specification.specification();
    ASSERT_EQ(spec.sections.size(), 3u);
    EXPECT_EQ(spec.sections[0].items[0].designation, "РЦ.080.00.000 СБ");
    EXPECT_EQ(spec.sections[1].items.size(), 13u);
    EXPECT_EQ(spec.sections[2].items.size(), 11u);
    EXPECT_EQ(spec.positionOf(p.gearbox.defs.at("housing")), 1);
    EXPECT_EQ(spec.positionOf(p.gearbox.defs.at("plug_gasket")), 13);
    const auto& cs = p.set.assembly.computedSheets()[0];
    std::map<asmv2::DefinitionId, std::vector<std::string>> shown;
    for (const auto& b : cs.balloons) shown[b.definition].push_back(b.text);
    for (const auto& b : cs.manualBalloons) {
        ASSERT_TRUE(b.has_value());
        shown[b->definition].push_back(b->text);
    }
    std::set<int> numbers;
    for (std::size_t s = 1; s < spec.sections.size(); ++s)
        for (const auto& it : spec.sections[s].items) {
            ASSERT_TRUE(it.definition.has_value());
            ASSERT_EQ(shown[*it.definition].size(), 1u) << it.designation << it.name;
            EXPECT_EQ(shown[*it.definition][0], std::to_string(it.position));
            numbers.insert(it.position);
        }
    EXPECT_EQ(numbers.size(), 24u);
    EXPECT_EQ(*numbers.begin(), 1);
    EXPECT_EQ(*numbers.rbegin(), 24);
    EXPECT_EQ(cs.title.designation, "РЦ.080.00.000 СБ");
    // Полки с номерами не налезают на размеры листа (80±0,03 на А–А): ни на размерные и выносные
    // линии, ни на надписи (шрифт листа).
    dv2::annotations::AnnotationStyle style;
    style.font = p.set.assembly.textMetrics(nullptr);
    std::vector<dv2::annotations::Balloon> all(cs.balloons.begin(), cs.balloons.end());
    for (const auto& b : cs.manualBalloons) all.push_back(*b);
    std::size_t dims = 0;
    for (const auto& d : cs.dimensions) {
        if (d.state != dv2::annotations::DimensionState::Ok) continue;
        ++dims;
        dv2::annotations::BalloonObstacles obs;
        dv2::annotations::addDimensionObstacles(obs, d.geometry, style);
        for (const auto& b : all) {
            const dv2::Rect box = dv2::annotations::balloonShelfBox(b, {}, style);
            for (const auto& r : obs.boxes)
                EXPECT_FALSE(std::min(r.x1, box.x1) > std::max(r.x0, box.x0) && std::min(r.y1, box.y1) > std::max(r.y0, box.y0))
                    << "position " << b.text << " on the text of dimension " << d.text;
            for (const auto& l : obs.lines) {
                const double x0 = std::min(l.a.x, l.b.x), x1 = std::max(l.a.x, l.b.x), y0 = std::min(l.a.y, l.b.y),
                             y1 = std::max(l.a.y, l.b.y);
                // Отрезки размеров — горизонтальные, вертикальные или короткие (дуги, стрелки): габарит.
                EXPECT_FALSE(x1 > box.x0 && x0 < box.x1 && y1 > box.y0 && y0 < box.y1 &&
                             (std::abs(x1 - x0) < 1e-9 || std::abs(y1 - y0) < 1e-9))
                    << "position " << b.text << " on a line of dimension " << d.text;
            }
        }
    }
    EXPECT_GT(dims, 0u);
    for (const auto& w : p.summary.warnings) EXPECT_EQ(w.find("overlap dimensions"), std::string::npos) << w;
}

// Раскладка позиций сборочного чертежа (ГОСТ 2.109-73): в каждой колонне и строке полок номера
// по возрастанию (сверху вниз, слева направо), линии-выноски попарно не пересекаются и не
// пересекают размерные и выносные линии, полки не налезают на буквы и стрелки обозначений
// разрезов, знаки шероховатости и баз.
TEST(E2EProductGearbox, BalloonLayoutOrderedWithoutCrossings) {
    namespace tb = dv2::test_balloons;
    Product& p = product();
    const auto& cs = p.set.assembly.computedSheets()[0];
    const auto& sd = p.set.assembly.sheets()[0];
    std::vector<dv2::annotations::Balloon> all(cs.balloons.begin(), cs.balloons.end());
    std::vector<std::string> viewOf(cs.balloonViews.begin(), cs.balloonViews.end());
    ASSERT_EQ(cs.manualBalloons.size(), sd.balloons.size());
    for (std::size_t i = 0; i < cs.manualBalloons.size(); ++i) {
        ASSERT_TRUE(cs.manualBalloons[i].has_value());
        all.push_back(*cs.manualBalloons[i]);
        viewOf.push_back(sd.balloons[i].view);
    }
    ASSERT_EQ(all.size(), 24u);
    const auto lines = tb::shelfLines(all, viewOf);
    std::size_t inLines = 0;
    for (const auto& l : lines) inLines += l.balloons.size() > 1 ? l.balloons.size() : 0;
    EXPECT_EQ(inLines, all.size()) << "every position stands on a column or a row of shelves";
    EXPECT_TRUE(tb::unorderedNumbers(lines).empty()) << tb::join(tb::unorderedNumbers(lines));
    EXPECT_TRUE(tb::crossingLeaders(all).empty()) << tb::join(tb::crossingLeaders(all));
    std::vector<dv2::LineSeg> dimLines;
    for (const auto* list : {&cs.dimensions, &cs.threadNotes})
        for (const auto& d : *list) {
            if (d.state != dv2::annotations::DimensionState::Ok) continue;
            dimLines.insert(dimLines.end(), d.geometry.dimensionLines.begin(), d.geometry.dimensionLines.end());
            dimLines.insert(dimLines.end(), d.geometry.extensionLines.begin(), d.geometry.extensionLines.end());
        }
    ASSERT_FALSE(dimLines.empty());
    EXPECT_TRUE(tb::leadersCrossingLines(all, dimLines).empty()) << tb::join(tb::leadersCrossingLines(all, dimLines));
    // Обозначения разрезов (буквы, стрелки, штрихи) — тексты и линии слоя SYMBOLS отрисованного
    // листа; знаки шероховатости и баз — их геометрия.
    dv2::annotations::AnnotationStyle style;
    style.font = p.set.assembly.textMetrics(nullptr);
    auto drawings = p.set.assembly.render(nullptr);
    ASSERT_TRUE(drawings.ok()) << drawings.error().message;
    std::vector<dv2::Rect> boxes;
    std::vector<dv2::LineSeg> marks;
    std::size_t letters = 0;
    for (const auto& e : drawings.value()[0].entities()) {
        if (e.layer != "SYMBOLS") continue;
        if (const auto* t = std::get_if<dv2::Text>(&e.geometry)) {
            const double w = dv2::annotations::estimateTextWidth(t->utf8, t->height, style.font);
            const double x0 = t->hAlign == dv2::HAlign::Left ? t->position.x
                              : t->hAlign == dv2::HAlign::Center ? t->position.x - w / 2 : t->position.x - w;
            const double y0 = t->vAlign == dv2::VAlign::Baseline ? t->position.y
                              : t->vAlign == dv2::VAlign::Middle ? t->position.y - t->height / 2 : t->position.y - t->height;
            boxes.push_back({x0, y0, x0 + w, y0 + t->height});
            ++letters;
        } else if (const auto* l = std::get_if<dv2::LineSeg>(&e.geometry)) {
            marks.push_back(*l);
        } else if (const auto* pl = std::get_if<dv2::Polyline>(&e.geometry)) {
            for (std::size_t k = 0; k + 1 < pl->points.size(); ++k) marks.push_back({pl->points[k], pl->points[k + 1]});
        }
    }
    EXPECT_GE(letters, 8u);  // А, Б, В, Г у стрелок и над разрезами
    for (const auto* list : {&cs.roughness, &cs.datums})
        for (const auto& g : *list) {
            if (!g) continue;
            marks.insert(marks.end(), g->lines.begin(), g->lines.end());
            for (const auto& pl : g->polylines)
                for (std::size_t k = 0; k + 1 < pl.points.size(); ++k) marks.push_back({pl.points[k], pl.points[k + 1]});
        }
    EXPECT_TRUE(tb::shelvesHitting(all, boxes, marks, style).empty()) << tb::join(tb::shelvesHitting(all, boxes, marks, style));
    std::vector<std::string> throughLetters;
    for (const auto& b : all)
        for (const auto& r : boxes)
            if (tb::segmentHitsRect(b.target, b.at, r)) {
                throughLetters.push_back(b.text);
                break;
            }
    EXPECT_TRUE(throughLetters.empty()) << tb::join(throughLetters);
    for (const auto& w : p.summary.warnings) EXPECT_EQ(w.find("out of order"), std::string::npos) << w;
}

// Все размеры, знаки, позиции привязаны к модели; изменение длины выходного конца вала (L0:
// 40 → 50) обновляет чертёж вала: L 172 → 182, длина конца 40 → 50, остальное без изменений.
TEST(E2EProductGearbox, DrawingsAreAssociative) {
    Product& p = product();
    EXPECT_TRUE(p.summary.dangling.empty()) << p.summary.dangling.front();
    EXPECT_GE(p.summary.dimensions, 50u);
    EXPECT_EQ(p.summary.balloons, 24u);
    std::size_t tolerances = 0, roughness = 0, frames = 0;
    for (const auto& pd : p.set.parts) {
        const auto& sd = pd.doc.sheets()[0];
        const auto& cs = pd.doc.computedSheets()[0];
        for (std::size_t i = 0; i < sd.dimensions.size(); ++i) {
            EXPECT_EQ(cs.dimensions[i].state, bendcore::drawing_v2::annotations::DimensionState::Ok) << pd.file << "/" << sd.dimensions[i].id;
            if (sd.dimensions[i].def.tolerance) ++tolerances;
        }
        for (const auto& r : cs.roughness) EXPECT_TRUE(r.has_value()) << pd.file;
        for (const auto& t : cs.tolerances) EXPECT_TRUE(t.has_value()) << pd.file;
        for (const auto& t : cs.datums) EXPECT_TRUE(t.has_value()) << pd.file;
        roughness += cs.roughness.size();
        frames += cs.tolerances.size();
        EXPECT_FALSE(sd.technicalRequirements.empty());
        EXPECT_FALSE(cs.title.mass.empty()) << pd.file;
    }
    EXPECT_GE(tolerances, 15u);
    EXPECT_GE(roughness, 12u);
    EXPECT_GE(frames, 7u);

    PartEntry& shaft = const_cast<PartEntry&>(*findPart(p.parts, "shaft_out"));
    dd::DrawingDocument* drawing = nullptr;
    for (auto& pd : p.set.parts)
        if (pd.key == "shaft_out") drawing = &pd.doc;
    ASSERT_NE(drawing, nullptr);
    auto value = [&](const std::string& id) {
        for (std::size_t i = 0; i < drawing->sheets()[0].dimensions.size(); ++i)
            if (drawing->sheets()[0].dimensions[i].id == id) return drawing->computedSheets()[0].dimensions[i].value;
        return -1.0;
    };
    EXPECT_NEAR(value("L"), 172.0, 1e-9);
    EXPECT_NEAR(value("Lend"), 40.0, 1e-9);
    ASSERT_TRUE(shaft.doc->setVariable("L0", "50").ok());
    ASSERT_TRUE(shaft.doc->rebuild().failed.empty());
    auto snap = dd::snapshotPart(*shaft.doc, drawing->model().properties);
    ASSERT_TRUE(snap.ok());
    auto rep = drawing->update(snap.value(), p.cache);
    ASSERT_TRUE(rep.ok()) << rep.error().message;
    EXPECT_TRUE(rep.value().modelChanged);
    EXPECT_TRUE(rep.value().dangling.empty()) << rep.value().dangling.front();
    EXPECT_NEAR(value("L"), 182.0, 1e-9);
    EXPECT_NEAR(value("Lend"), 50.0, 1e-9);
    EXPECT_NEAR(value("Lseat"), 16.0, 1e-9);
    const std::set<std::string> stale(rep.value().stale.begin(), rep.value().stale.end());
    EXPECT_EQ(stale, (std::set<std::string>{"S1/L: 172 → 182", "S1/Lend: 40 → 50"}));
    for (const auto& r : drawing->computedSheets()[0].roughness) EXPECT_TRUE(r.has_value());
    // Возврат параметра.
    ASSERT_TRUE(shaft.doc->setVariable("L0", "40").ok());
    ASSERT_TRUE(shaft.doc->rebuild().failed.empty());
    auto back = drawing->update(dd::snapshotPart(*shaft.doc, drawing->model().properties).value(), p.cache);
    ASSERT_TRUE(back.ok());
    EXPECT_NEAR(value("L"), 172.0, 1e-9);
}

// Технические требования листа корпуса: строки перенесены по начертанию шрифта ГОСТ 2.304
// (наклон 75°, толщина линии) и не доходят до рамки; «(по 4 на торцах бобышек)» прежде
// выходила на линию рамки.
TEST(E2EProductGearbox, TechnicalRequirementsInsideFrame) {
    Product& p = product();
    const dd::DrawingDocument* housing = nullptr;
    for (const auto& pd : p.set.parts)
        if (pd.key == "housing") housing = &pd.doc;
    ASSERT_NE(housing, nullptr);
    auto sheets = housing->render(nullptr);
    ASSERT_TRUE(sheets.ok()) << sheets.error().message;
    const auto& d = sheets.value().front();
    const double frameRight = d.sheet().widthMm - 5.0, fieldLeft = frameRight - 185.0;
    auto lines = bendcore::drawing_v2::annotations::technicalRequirementsLines(housing->sheets()[0].technicalRequirements);
    ASSERT_TRUE(lines.ok());
    const std::set<std::string> tt(lines.value().begin(), lines.value().end());
    std::size_t found = 0;
    bool bossLine = false;
    for (const auto& e : d.entities()) {
        const auto* t = std::get_if<bendcore::drawing_v2::Text>(&e.geometry);
        if (!t || !tt.count(t->utf8)) continue;
        ++found;
        bossLine |= t->utf8.find("бобышек") != std::string::npos;
        auto lay = bendcore::drawing_v2::gostStrokeFont().layout(*t);
        ASSERT_TRUE(lay.ok());
        double lo = 1e300, hi = -1e300;
        for (const auto& s : lay.value().strokes)
            for (const auto& q : s) lo = std::min(lo, q.x), hi = std::max(hi, q.x);
        EXPECT_GE(lo - lay.value().lineWidth / 2, fieldLeft - 1e-9) << t->utf8;
        EXPECT_LE(hi + lay.value().lineWidth / 2, frameRight - 1.0 + 1e-9) << t->utf8;
    }
    EXPECT_EQ(found, lines.value().size());
    EXPECT_TRUE(bossLine);
}

// Записанные файлы открываются в bendcore: документы деталей (тот же объём), сборка (источники
// загружены, те же положения и степени свободы), чертежи (обновление по файлам — без висячих
// размеров, те же значения), спецификация, STEP сборки (72 детали) и деталей, DXF, PDF.
TEST(E2EProductGearbox, FilesOpenBack) {
    Product& p = product();
    const auto registry = bendcore::modeling::partRegistry();
    for (const auto& e : p.parts) {
        auto d = bendcore::modeling::PartDocument::fromJson(readFile(p.dir + "/" + e.file), registry);
        ASSERT_TRUE(d.ok()) << e.file << ": " << d.error().message;
        const double v0 = volume(e.doc->finalShape().shape());
        EXPECT_NEAR(volume(d.value().finalShape().shape()), v0, 1e-9 * v0) << e.file;
    }
    auto loaded = asmv2::AssemblyDocument::loadFile(p.kit.assemblyDocument, registry);
    ASSERT_TRUE(loaded.ok()) << loaded.error().message;
    for (const auto& s : loaded.value().report.sources)
        EXPECT_TRUE(s.status == asmv2::SourceStatus::Loaded || s.status == asmv2::SourceStatus::Embedded) << s.name << ": " << s.message;
    for (const auto& m : loaded.value().report.mates) EXPECT_EQ(m.outcome, asmv2::MateOutcome::Kept) << m.name;
    auto& ld = loaded.value().document;
    auto solved = ld.solve();
    ASSERT_TRUE(solved.ok());
    EXPECT_TRUE(solved.value().success);
    EXPECT_EQ(solved.value().totalDof, 59);
    ASSERT_EQ(ld.topLevel().size(), p.gearbox.doc->topLevel().size());
    for (std::size_t i = 0; i < ld.topLevel().size(); ++i)
        EXPECT_LT((ld.topLevel()[i].placement.position - p.gearbox.doc->topLevel()[i].placement.position).norm(), 1e-7);
    EXPECT_LT(fs::file_size(p.kit.assemblyDocument), 3u << 20);

    for (std::size_t i = 0; i < p.kit.drawingDocuments.size(); ++i) {
        const std::string& file = p.kit.drawingDocuments[i];
        auto d = dd::DrawingDocument::loadFile(file);
        ASSERT_TRUE(d.ok()) << file << ": " << d.error().message;
        const dd::DrawingDocument& orig = i < p.set.parts.size() ? p.set.parts[i].doc : p.set.assembly;
        dd::SpecificationDocument* spec = nullptr;
        auto specDoc = dd::SpecificationDocument::fromJson(readFile(p.kit.specificationDocument));
        ASSERT_TRUE(specDoc.ok()) << specDoc.error().message;
        if (d.value().model().kind == dd::ModelKind::Assembly) {
            auto snap = dd::fileModelLoader(p.dir)(d.value().model());
            ASSERT_TRUE(snap.ok()) << snap.error().message;
            ASSERT_TRUE(specDoc.value().update(snap.value()).ok());
            spec = &specDoc.value();
        }
        auto rep = d.value().update(dd::fileModelLoader(p.dir), p.cache, spec ? spec->specification() : nullptr);
        ASSERT_TRUE(rep.ok()) << file << ": " << rep.error().message;
        EXPECT_FALSE(rep.value().modelChanged) << file;
        EXPECT_TRUE(rep.value().dangling.empty()) << file << ": " << rep.value().dangling.front();
        EXPECT_TRUE(rep.value().stale.empty()) << file;
        const auto& a = d.value().computedSheets()[0].dimensions;
        const auto& b = orig.computedSheets()[0].dimensions;
        ASSERT_EQ(a.size(), b.size());
        for (std::size_t k = 0; k < a.size(); ++k) EXPECT_NEAR(a[k].value, b[k].value, 1e-9) << file;
    }

    auto step = asmv2::AssemblyDocument::importStep(readFile(p.kit.assemblyStep));
    ASSERT_TRUE(step.ok()) << step.error().message;
    auto placed = step.value().placedParts();
    ASSERT_TRUE(placed.ok());
    EXPECT_EQ(placed.value().size(), 72u);
    for (const auto& path : p.kit.partSteps) {
        auto s = bendcore::kernel::readStep(readFile(path));
        ASSERT_TRUE(s.ok()) << path << ": " << s.error().message;
        EXPECT_GT(volume(s.value().occt()), 0.0) << path;
    }
    const PartEntry& housing = *findPart(p.parts, "housing");
    auto hs = bendcore::kernel::readStep(readFile(p.dir + "/housing.step"));
    ASSERT_TRUE(hs.ok());
    const double hv = volume(housing.doc->finalShape().shape());
    EXPECT_NEAR(volume(hs.value().occt()), hv, 1e-6 * hv);

    // DXF на каждый лист: 6 рабочих чертежей, сборочный чертёж, листы спецификации.
    EXPECT_EQ(p.kit.dxf.size(), std::size_t(p.kit.pdfPages));
    EXPECT_GE(p.kit.pdfPages, 8);
    for (const auto& f : p.kit.dxf) {
        const std::string t = readFile(f);
        EXPECT_NE(t.find("SECTION"), std::string::npos) << f;
        EXPECT_NE(t.find("EOF"), std::string::npos) << f;
    }
#ifdef BENDCORE_PDFINFO
    {
        int code = 0;
        const std::string info = run(std::string(BENDCORE_PDFINFO) + " '" + p.kit.pdf + "'", &code);
        EXPECT_EQ(code, 0);
        EXPECT_NE(info.find("Pages:           " + std::to_string(p.kit.pdfPages) + "\n"), std::string::npos) << info;
    }
#endif
#ifdef BENDCORE_PDFTOTEXT
    {
        int code = 0;
        const std::string text = run(std::string(BENDCORE_PDFTOTEXT) + " '" + p.kit.pdf + "' -", &code);
        EXPECT_EQ(code, 0);
        for (const char* want : {"РЦ.080.00.001", "Корпус", "Вал-шестерня", "Колесо зубчатое", "РЦ.080.00.000 СБ",
                                 "Подшипник 205", "Болт М10×45 ГОСТ 7798-70", "Стандартные изделия"})
            EXPECT_NE(text.find(want), std::string::npos) << "pdftotext: " << want;
    }
#endif
}

// Комплект, сохранённый в репозитории (examples/gearbox/), открывается и совпадает по составу и
// степеням свободы с построенным заново (генератор воспроизводим).
TEST(E2EProductGearbox, CommittedKitOpens) {
    const std::string dir = BENDCORE_GEARBOX_COMMITTED;
    if (!fs::exists(dir + "/gearbox.bcasm.json")) GTEST_SKIP() << "no committed kit";
    auto loaded = asmv2::AssemblyDocument::loadFile(dir + "/gearbox.bcasm.json", bendcore::modeling::partRegistry());
    ASSERT_TRUE(loaded.ok()) << loaded.error().message;
    for (const auto& s : loaded.value().report.sources)
        EXPECT_EQ(s.status == asmv2::SourceStatus::Loaded || s.status == asmv2::SourceStatus::Embedded, true) << s.name << ": " << s.message;
    auto solved = loaded.value().document.solve();
    ASSERT_TRUE(solved.ok());
    EXPECT_EQ(solved.value().totalDof, 59);
    EXPECT_EQ(loaded.value().document.topLevel().size(), 72u);
    // Документы чертежей обновляются по закоммиченным моделям без изменений и висячих размеров.
    bendcore::drawing_v2::views::ViewCache cache(16, 2);
    for (const char* f : {"shaft_out.bcdrw.json", "wheel.bcdrw.json", "cap_out_t.bcdrw.json"}) {
        auto d = dd::DrawingDocument::loadFile(dir + "/" + f);
        ASSERT_TRUE(d.ok()) << f << ": " << d.error().message;
        auto rep = d.value().update(dd::fileModelLoader(dir), cache);
        ASSERT_TRUE(rep.ok()) << f << ": " << rep.error().message;
        EXPECT_FALSE(rep.value().modelChanged) << f;
        EXPECT_TRUE(rep.value().dangling.empty()) << f;
    }
}
