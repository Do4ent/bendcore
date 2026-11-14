// Цели fuzz-проверки загрузчиков и мутатор (описание — fuzz_targets.hpp, docs/qa/QA.md, раздел 7).

#include "fuzz_targets.hpp"

#include <bendcore/assembly_v2/assembly_document.hpp>
#include <bendcore/drawing_v2/drawing_document.hpp>
#include <bendcore/drawing_v2/export.hpp>
#include <bendcore/drawing_v2/model.hpp>
#include <bendcore/drawing_v2/sheet.hpp>
#include <bendcore/dxf_reader.hpp>
#include <bendcore/kernel/brep_io.hpp>
#include <bendcore/kernel/shape.hpp>
#include <bendcore/kernel/step_io.hpp>
#include <bendcore/modeling/part_document.hpp>
#include <bendcore/modeling/part_features.hpp>

#include <BRepAlgoAPI_Cut.hxx>
#include <BRepFilletAPI_MakeFillet.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <gp_Ax2.hxx>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace bendcore::qa::fuzz {

namespace {

namespace fs = std::filesystem;
namespace asmv2 = bendcore::assembly_v2;
namespace ddoc = bendcore::drawing_v2::document;
namespace dv = bendcore::drawing_v2;

// Нарушение инварианта принятого документа (например, принятый документ не сохраняется):
// исключение наружу — дефект, тест его сообщает.
[[noreturn]] void invariant(const std::string& what) { throw std::logic_error("fuzz invariant: " + what); }

Outcome reject(const std::string& message, std::string* why) {
    if (why) *why = message;
    return Outcome::Rejected;
}

Outcome runPartJson(std::string_view in, std::string* why) {
    auto doc = modeling::PartDocument::fromJson(in, modeling::partRegistry());
    if (!doc) return reject(doc.error().message, why);
    auto text = doc.value().toJson();
    if (!text) invariant("accepted part document cannot be saved: " + text.error().message);
    return Outcome::Accepted;
}

Outcome runAssemblyJson(std::string_view in, std::string* why) {
    auto loaded = asmv2::AssemblyDocument::fromJson(in, asmv2::LoadOptions{});
    if (!loaded) return reject(loaded.error().message, why);
    asmv2::AssemblyDocument& doc = loaded.value().document;
    auto text = doc.toJson();
    if (!text) invariant("accepted assembly document cannot be saved: " + text.error().message);
    // Решение сопряжений и развёртка деталей на принятом документе: только явные ошибки.
    (void)doc.solve();
    (void)doc.placedParts();
    return Outcome::Accepted;
}

Outcome runDrawingJson(std::string_view in, std::string* why) {
    auto doc = ddoc::DrawingDocument::fromJson(in);
    if (!doc) return reject(doc.error().message, why);
    auto text = doc.value().toJson();
    if (!text) invariant("accepted drawing document cannot be saved: " + text.error().message);
    return Outcome::Accepted;
}

Outcome runStep(std::string_view in, std::string* why) {
    auto s = kernel::readStep(in);
    if (!s) return reject(s.error().message, why);
    (void)s.value().counts();
    return Outcome::Accepted;
}

Outcome runDxf(std::string_view in, std::string* why) {
    auto d = dxf::parseDxf(in);
    if (!d) return reject(d.error().message, why);
    (void)dxf::scaleToMillimeters(d.value());
    return Outcome::Accepted;
}

Outcome runBrepBinary(std::string_view in, std::string* why) {
    auto s = kernel::readBrepBinary(reinterpret_cast<const std::uint8_t*>(in.data()), in.size());
    if (!s) return reject(s.error().message, why);
    (void)s.value().counts();
    return Outcome::Accepted;
}

Outcome runBrepText(std::string_view in, std::string* why) {
    auto s = kernel::readBrepText(in);
    if (!s) return reject(s.error().message, why);
    (void)s.value().counts();
    return Outcome::Accepted;
}

std::string readFile(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    if (!f) throw std::runtime_error("fuzz seeds: cannot read " + p.string());
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

template <class R>
auto must(R&& r, const char* what) {
    if (!r) throw std::runtime_error(std::string("fuzz seeds: ") + what + ": " + r.error().message);
    return std::move(r.value());
}

// Брусок 40×30×20 со скруглённым ребром и сквозным отверстием: плоские, цилиндрические и
// тороидальные грани — разнообразие сущностей STEP/BRep.
TopoDS_Shape seedShape() {
    TopoDS_Shape box = BRepPrimAPI_MakeBox(40, 30, 20).Shape();
    BRepFilletAPI_MakeFillet fillet(box);
    TopExp_Explorer ex(box, TopAbs_EDGE);
    fillet.Add(3.0, TopoDS::Edge(ex.Current()));
    TopoDS_Shape rounded = fillet.Shape();
    TopoDS_Shape hole = BRepPrimAPI_MakeCylinder(gp_Ax2(gp_Pnt(20, 15, -1), gp::DZ()), 5, 22).Shape();
    return BRepAlgoAPI_Cut(rounded, hole).Shape();
}

std::vector<std::string> assemblySeeds() {
    asmv2::AssemblyDocument doc("FuzzSeed");
    const auto plate = must(doc.addPart("Plate", {BRepPrimAPI_MakeBox(60, 40, 8).Shape(), nullptr},
                                        {"FZ.001", "Plate", "Steel", 7850.0}),
                            "addPart");
    const auto pin = must(doc.addPart("Pin", {BRepPrimAPI_MakeCylinder(4, 20).Shape(), nullptr}), "addPart");
    const auto a = must(doc.addOccurrence(plate, asmv2::Transform::identity(), "Plate"), "addOccurrence");
    const auto b = must(doc.addOccurrence(pin, asmv2::Transform::fromTranslation(asmv2::Vec3(10, 10, 8)), "Pin"),
                        "addOccurrence");
    (void)must(doc.setFixed(a, true), "setFixed");
    // Нижний торец штифта (грань по индексу) на верхнюю грань плиты: индексы граней взяты из
    // TopologyIndex — для затравки важна только структура файла.
    (void)doc.addMate(asmv2::MateSpec::coincident({{a}, asmv2::ElementRef::face(5), asmv2::EntityUse::Auto},
                                                  {{b}, asmv2::ElementRef::face(2), asmv2::EntityUse::Auto}));
    std::vector<std::string> out;
    out.push_back(must(doc.toJson(), "assembly toJson"));  // с встроенным BRep (без него деталь без source не загрузить)
    asmv2::SaveOptions compact;
    compact.indent = -1;
    out.push_back(must(doc.toJson(compact), "assembly toJson"));
    return out;
}

std::vector<std::string> drawingSeeds() {
    ddoc::DrawingDocument d;
    d.model().kind = ddoc::ModelKind::Part;
    d.model().path = "bracket_part.json";
    d.model().properties = {"FZ.10.001", "Bracket", "Steel 20", 7850.0};
    ddoc::SheetDef s;
    s.id = "S1";
    s.format = {dv::GostFormat::A3, 1, dv::Orientation::Landscape};
    s.scale = dv::views::ScaleRatio{1, 1};
    ddoc::ViewDef f;
    f.id = "F";
    s.views.push_back(f);
    ddoc::ViewDef t;
    t.id = "T";
    t.kind = ddoc::ViewKind::Projected;
    t.parent = "F";
    t.side = dv::views::SheetSide::Below;
    s.views.push_back(t);
    s.technicalRequirements = {"Sharp edges blunted."};
    d.sheets().push_back(std::move(s));
    return {must(d.toJson(), "drawing toJson")};
}

std::vector<std::string> dxfSeeds() {
    auto sheet = must(dv::gostSheet(dv::GostFormat::A4, 1, dv::Orientation::Portrait), "gostSheet");
    dv::Drawing d = must(dv::Drawing::create(sheet, 0.5), "Drawing::create");
    (void)must(d.addLayer({"GEOM", 7}), "addLayer");
    (void)must(d.add({dv::LineSeg{{50, 100}, {150, 100}}, "GEOM", dv::LineType::SolidThick}), "add");
    (void)must(d.add({dv::Circle{{100, 180}, 20}, "GEOM", dv::LineType::SolidThick}), "add");
    (void)must(d.add({dv::Arc{{100, 180}, 30, 0, 90}, "GEOM", dv::LineType::SolidThin}), "add");
    (void)must(d.add({dv::Polyline{{{60, 200}, {80, 200}, {80, 220}, {60, 220}}, true}, "GEOM",
                      dv::LineType::SolidThick}),
               "add");
    std::vector<std::string> out{must(dv::writeDxf(d), "writeDxf")};
    // R12 без заголовка с LWPOLYLINE и выпуклостью — ветви, которых нет в выводе писателя.
    out.push_back(
        "0\nSECTION\n2\nHEADER\n9\n$INSUNITS\n70\n4\n0\nENDSEC\n0\nSECTION\n2\nENTITIES\n"
        "0\nLWPOLYLINE\n8\n0\n90\n3\n70\n1\n10\n0.0\n20\n0.0\n42\n0.5\n10\n10.0\n20\n0.0\n10\n10.0\n20\n10.0\n"
        "0\nTEXT\n8\n0\n10\n1.0\n20\n2.0\n40\n3.5\n1\n\\U+0410bc\n"
        "0\nARC\n8\n0\n10\n5.0\n20\n5.0\n40\n2.0\n50\n0.0\n51\n180.0\n0\nENDSEC\n0\nEOF\n");
    return out;
}

}  // namespace

const std::vector<Target>& targets() {
    static const std::vector<Target> all = {
        {"part_json", true, runPartJson},       {"assembly_json", true, runAssemblyJson},
        {"drawing_json", true, runDrawingJson}, {"step", false, runStep},
        {"dxf", false, runDxf},                 {"brep_binary", false, runBrepBinary},
        {"brep_text", false, runBrepText},
    };
    return all;
}

const Target* findTarget(std::string_view name) {
    for (const Target& t : targets()) {
        if (name == t.name) return &t;
    }
    return nullptr;
}

std::vector<std::string> seeds(std::string_view target, const std::string& sourceDir) {
    const fs::path root(sourceDir);
    std::vector<std::string> out;
    if (target == "part_json") {
        out.push_back(readFile(root / "tests/wasm/fixtures/bracket.part.json"));
        for (const char* f : {"block.json", "plate.json", "shaft.json"})
            out.push_back(readFile(root / "tests/assembly-fixtures" / f));
    } else if (target == "assembly_json") {
        out = assemblySeeds();
    } else if (target == "drawing_json") {
        out = drawingSeeds();
    } else if (target == "step") {
        // Небольшие файлы корпуса (полный набор — в test_step_corpus) и STEP писателя ядра.
        for (const char* f : {"box_100x60x20.step", "plate_120x80x10_hole_d20.step", "box_80x60x40_all_fillets_r5.step",
                              "assembly_bracket_pins_ap214.step"}) {
            const fs::path p = root / "tests/corpus/generated" / f;
            if (fs::exists(p)) out.push_back(readFile(p));
        }
        out.push_back(must(kernel::writeStep(kernel::Shape(seedShape())), "writeStep"));
    } else if (target == "dxf") {
        out = dxfSeeds();
    } else if (target == "brep_binary") {
        auto bin = must(kernel::writeBrepBinary(kernel::Shape(seedShape())), "writeBrepBinary");
        out.emplace_back(bin.begin(), bin.end());
    } else if (target == "brep_text") {
        out.push_back(must(kernel::writeBrepText(kernel::Shape(seedShape())), "writeBrepText"));
    } else {
        throw std::runtime_error("fuzz seeds: unknown target " + std::string(target));
    }
    if (out.empty()) throw std::runtime_error("fuzz seeds: no seeds for " + std::string(target));
    return out;
}

// ------------------------------------------------------------------ мутатор

namespace {

const char* const kInterestingNumbers[] = {
    "0",          "-1",          "-0",         "1e308",          "-1e308",
    "1e999",      "-1e999",      "1e-320",     "4294967296",     "18446744073709551616",
    "-9223372036854775809",      "9223372036854775807",          "2147483648",
    "nan",        "inf",         "-inf",       "99999999999999999999999999", "0.000000000000000000001",
};

bool isDigit(char c) { return c >= '0' && c <= '9'; }

nlohmann::json deepNest(std::size_t depth, bool objects) {
    nlohmann::json j = nullptr;
    for (std::size_t i = 0; i < depth; ++i) {
        nlohmann::json outer = objects ? nlohmann::json::object() : nlohmann::json::array();
        if (objects) outer["k"] = std::move(j);
        else outer.push_back(std::move(j));
        j = std::move(outer);
    }
    return j;
}

}  // namespace

std::string Mutator::mutateBytes(const std::string& in) {
    std::string s = in;
    switch (below(9)) {
    case 0:  // обрезка
        return s.substr(0, below(s.size() + 1));
    case 1: {  // перестановка пар байтов
        const std::uint64_t n = 1 + below(8);
        for (std::uint64_t i = 0; i < n && s.size() > 1; ++i) std::swap(s[below(s.size())], s[below(s.size())]);
        return s;
    }
    case 2: {  // перемешивание блока
        if (s.empty()) return s;
        const std::size_t a = below(s.size()), len = 1 + below(std::min<std::size_t>(64, s.size() - a));
        std::shuffle(s.begin() + a, s.begin() + a + len, rng_);
        return s;
    }
    case 3: {  // случайные байты
        const std::uint64_t n = 1 + below(16);
        for (std::uint64_t i = 0; i < n && !s.empty(); ++i) s[below(s.size())] = char(below(256));
        return s;
    }
    case 4: {  // замена числа на «интересное»
        std::vector<std::pair<std::size_t, std::size_t>> runs;
        for (std::size_t i = 0; i < s.size();) {
            if (isDigit(s[i])) {
                std::size_t j = i;
                while (j < s.size() && (isDigit(s[j]) || s[j] == '.' || s[j] == 'e' || s[j] == 'E')) ++j;
                runs.emplace_back(i, j - i);
                i = j;
            } else {
                ++i;
            }
        }
        if (runs.empty()) return s;
        const auto [pos, len] = runs[below(runs.size())];
        const char* v = kInterestingNumbers[below(std::size(kInterestingNumbers))];
        return s.substr(0, pos) + v + s.substr(pos + len);
    }
    case 5: {  // повтор блока
        if (s.empty()) return s;
        const std::size_t a = below(s.size()), len = 1 + below(std::min<std::size_t>(4096, s.size() - a));
        const std::string block = s.substr(a, len);
        const std::size_t copies = 1 + below(below(2) ? 4 : 200);
        std::string rep;
        for (std::size_t i = 0; i < copies; ++i) rep += block;
        return s.insert(below(s.size() + 1), rep);
    }
    case 6: {  // удаление блока
        if (s.empty()) return s;
        const std::size_t a = below(s.size());
        return s.erase(a, 1 + below(std::min<std::size_t>(4096, s.size() - a)));
    }
    case 7: {  // глубокая вложенность скобок в случайном месте
        const std::size_t depth = below(2) ? 1000 : 200000;
        const char open = below(2) ? '[' : '{';
        std::string ins(depth, open);
        if (below(2)) ins += std::string(depth, open == '[' ? ']' : '}');
        return s.insert(below(s.size() + 1), ins);
    }
    default: {  // строка-блок, повторённая очень много раз (длинные списки примитивов, сущностей)
        const std::size_t nl = s.find('\n', below(s.size() + 1));
        if (nl == std::string::npos) return s + s;
        const std::size_t start = s.rfind('\n', nl == 0 ? 0 : nl - 1);
        const std::size_t from = start == std::string::npos ? 0 : start + 1;
        const std::string line = s.substr(from, nl + 1 - from);
        std::string rep;
        for (int i = 0; i < 5000; ++i) rep += line;
        return s.insert(from, rep);
    }
    }
}

std::string Mutator::mutateJsonFields(const std::string& in) {
    nlohmann::json doc = nlohmann::json::parse(in, nullptr, false);
    if (doc.is_discarded()) return mutateBytes(in);
    const std::uint64_t edits = 1 + below(3);
    for (std::uint64_t e = 0; e < edits; ++e) {
        // Случайный путь от корня вглубь до значения.
        nlohmann::json* node = &doc;
        nlohmann::json* parent = nullptr;
        std::string key;
        std::size_t index = 0;
        for (int depth = 0; depth < 12; ++depth) {
            if (!(node->is_object() || node->is_array()) || node->empty() || below(4) == 0) break;
            parent = node;
            if (node->is_object()) {
                auto it = node->begin();
                std::advance(it, below(node->size()));
                key = it.key();
                node = &it.value();
            } else {
                index = below(node->size());
                key.clear();
                node = &(*node)[index];
            }
        }
        switch (below(12)) {
        case 0: *node = nullptr; break;
        case 1: *node = !node->is_boolean() || !node->get<bool>(); break;
        case 2: *node = ""; break;
        case 3: *node = std::string(100000, 'x'); break;
        case 4: *node = 1e308; break;
        case 5: *node = -1; break;
        case 6: *node = std::numeric_limits<std::int64_t>::min(); break;
        case 7: *node = std::numeric_limits<std::uint64_t>::max(); break;
        case 8: *node = nlohmann::json::array(); break;
        case 9: *node = nlohmann::json::object(); break;
        case 10: *node = deepNest(below(2) ? 100 : 5000, below(2) != 0); break;
        default:  // удалить поле или элемент
            if (parent && parent->is_object()) parent->erase(key);
            else if (parent && parent->is_array()) parent->erase(index);
            else *node = nullptr;
            break;
        }
    }
    return doc.dump();
}

std::string Mutator::mutate(const std::string& input, bool json) {
    std::string s = json && below(2) ? mutateJsonFields(input) : mutateBytes(input);
    // Изредка — вторая мутация поверх первой.
    if (below(4) == 0) s = mutateBytes(s);
    return s;
}

std::size_t peakRssBytes() {
    std::ifstream f("/proc/self/status");
    std::string line;
    while (std::getline(f, line)) {
        if (line.rfind("VmHWM:", 0) == 0) {
            std::istringstream ss(line.substr(6));
            std::size_t kb = 0;
            ss >> kb;
            return kb * 1024;
        }
    }
    return 0;
}

}  // namespace bendcore::qa::fuzz
