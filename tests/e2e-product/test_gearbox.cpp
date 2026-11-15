// Сквозной тест изделия уровня «редуктор» (критерий готовности v1.0, docs/ROADMAP_GENERAL_CAD.md):
// редуктор РЦ-80 из examples/gearbox/ строится воспроизводимо, и проверяется против независимых
// эталонов: состав (число деталей и количества — из построения изделия), массы простых деталей —
// против формул из их размеров, отсутствие пересечений (кроме резьбовых соединений с предельным
// объёмом по формуле), степени свободы (валы вращаются, остальное неподвижно), позиции сборочного
// чертежа = спецификации, ассоциативность всех размеров (изменение параметра вала → обновлённый
// чертёж), чтение всех записанных файлов обратно в bendcore, PDF (pdfinfo, pdftotext).

#include "gearbox.hpp"

#include <bendcore/assembly_v2/bom.hpp>
#include <bendcore/assembly_v2/interference.hpp>
#include <bendcore/kernel/step_io.hpp>
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
        auto kit = writeKit(out->dir, out->parts, out->standard, out->gearbox, out->set);
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

// Состав: 13 деталей собственной разработки, 11 типоразмеров стандартных изделий (подшипники,
// шпонки — документы детали; болты, шайбы, гайки, штифты — bendcore_fasteners), 72 вхождения;
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
    for (const auto& s : p.standard) {
        const double v = volume(s.doc->finalShape().shape());
        EXPECT_NEAR(v, s.formulaVolume, 1e-9 * s.formulaVolume) << s.key;
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
// объём не больше кольца π/4·(d² − (D − P)²) на длине захода. Делительные цилиндры колёс касаются.
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

// Степени свободы: 60 = 2 вала + повороты осесимметричных деталей вокруг своих осей; поворот
// вала на 30° вокруг его оси решается без невязки, колесо и шпонки поворачиваются вместе с
// валом, корпус и крышки неподвижны.
TEST(E2EProductGearbox, DegreesOfFreedom) {
    Product& p = product();
    auto& doc = *p.gearbox.doc;
    auto rep = doc.solve();
    ASSERT_TRUE(rep.ok());
    EXPECT_TRUE(rep.value().success);
    EXPECT_LT(rep.value().residualNorm, 1e-9);
    EXPECT_EQ(rep.value().totalDof, p.gearbox.expectedDof);
    EXPECT_EQ(p.gearbox.expectedDof, 60);
    for (const auto& s : rep.value().subsystems) {
        EXPECT_TRUE(s.redundantMates.empty());
        EXPECT_TRUE(s.conflictingMates.empty());
    }
    auto occ = [&](const std::string& k) { return *doc.findOccurrence(p.gearbox.occ.at(k)); };
    std::vector<std::pair<asmv2::OccurrenceId, asmv2::Transform>> saved;
    for (const auto& o : doc.topLevel()) saved.push_back({o.id, o.placement});
    using Group = std::vector<std::string>;
    for (const auto& [group, y] : std::vector<std::pair<Group, double>>{
             {{"shaft_in", "key_in"}, 0.0}, {{"shaft_out", "wheel", "spacer", "key_wheel", "key_out"}, 80.0}}) {
        // Поворот вала с жёстко связанными деталями на 30° вокруг оси — допустимое движение:
        // решатель ничего не сдвигает (невязка 0), крышки и корпус на месте.
        const asmv2::Vec3 axisPoint(0, y, 78);
        const asmv2::Transform turn = asmv2::Transform::fromTranslation(axisPoint) *
                                      asmv2::Transform::fromAxisAngle(asmv2::Vec3::UnitX(), kPi / 6) *
                                      asmv2::Transform::fromTranslation(-axisPoint);
        std::map<std::string, asmv2::Transform> target;
        for (const auto& k : group) {
            target[k] = turn * occ(k).placement;
            ASSERT_TRUE(doc.setPlacement(p.gearbox.occ.at(k), target[k]).ok());
        }
        const asmv2::Transform cap = occ("cap_out_r").placement;
        auto r = doc.solve();
        ASSERT_TRUE(r.ok());
        EXPECT_TRUE(r.value().success) << group[0];
        EXPECT_LT(r.value().residualNorm, 1e-9);
        for (const auto& k : group) {
            EXPECT_LT((occ(k).placement.position - target[k].position).norm(), 1e-7) << k;
            EXPECT_LT((occ(k).placement.rotationMatrix() - target[k].rotationMatrix()).norm(), 1e-7) << k;
        }
        EXPECT_LT((occ("cap_out_r").placement.position - cap.position).norm(), 1e-9);
        // Поворот одного вала: после решения ось вала неподвижна, жёстко связанные детали
        // повернулись вместе с ним (относительное положение сохранено).
        const asmv2::Transform before = occ(group[0]).placement, rel = before.inverse() * occ(group[1]).placement;
        ASSERT_TRUE(doc.setPlacement(p.gearbox.occ.at(group[0]), turn * before).ok());
        auto r2 = doc.solve();
        ASSERT_TRUE(r2.ok());
        EXPECT_TRUE(r2.value().success);
        const asmv2::Transform after = occ(group[0]).placement;
        for (double x : {-50.0, 50.0})
            EXPECT_LT((after.applyToPoint(before.inverse().applyToPoint(asmv2::Vec3(x, y, 78))) - asmv2::Vec3(x, y, 78)).norm(), 1e-7);
        const asmv2::Transform relAfter = after.inverse() * occ(group[1]).placement;
        EXPECT_LT((relAfter.position - rel.position).norm(), 1e-7);
        EXPECT_LT((relAfter.rotationMatrix() - rel.rotationMatrix()).norm(), 1e-7);
        for (const auto& [id, t] : saved) ASSERT_TRUE(doc.setPlacement(id, t).ok());
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
    EXPECT_EQ(solved.value().totalDof, 60);
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
    EXPECT_EQ(solved.value().totalDof, 60);
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
