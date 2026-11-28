#include "test_support.hpp"

#include <array>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <memory>
#include <regex>
#include <sstream>
#include <stdexcept>

namespace dv2test {

std::string outPath(const std::string& name) { return std::string(BENDCORE_DRAWING_V2_OUT) + "/" + name; }

const TrueTypeFont* systemFont() {
    static std::unique_ptr<TrueTypeFont> font = [] {
        auto path = findSystemFont();
        if (!path) return std::unique_ptr<TrueTypeFont>();
        auto f = TrueTypeFont::loadFile(path.value());
        if (!f) return std::unique_ptr<TrueTypeFont>();
        return std::make_unique<TrueTypeFont>(std::move(f).value());
    }();
    return font.get();
}

std::vector<std::string> DxfObject::all(int code) const {
    std::vector<std::string> out;
    for (const auto& p : pairs) {
        if (p.code == code) out.push_back(p.value);
    }
    return out;
}

std::optional<std::string> DxfObject::first(int code) const {
    for (const auto& p : pairs) {
        if (p.code == code) return p.value;
    }
    return std::nullopt;
}

double DxfObject::real(int code) const {
    auto v = first(code);
    return v ? std::stod(*v) : NAN;
}

const std::vector<DxfObject>* DxfFile::section(const std::string& name) const {
    for (const auto& [n, objs] : sections) {
        if (n == name) return &objs;
    }
    return nullptr;
}

static std::string trim(std::string s) {
    while (!s.empty() && (s.back() == '\r' || s.back() == ' ')) s.pop_back();
    std::size_t i = 0;
    while (i < s.size() && s[i] == ' ') ++i;
    return s.substr(i);
}

DxfFile parseDxf(const std::string& text) {
    std::vector<std::string> lines;
    {
        std::istringstream in(text);
        std::string l;
        while (std::getline(in, l)) lines.push_back(l);
    }
    if (lines.size() % 2 != 0) throw std::runtime_error("DXF: odd number of lines");
    std::vector<DxfPair> pairs;
    for (std::size_t i = 0; i < lines.size(); i += 2) {
        const std::string c = trim(lines[i]);
        std::size_t used = 0;
        const int code = std::stoi(c, &used);
        if (used != c.size()) throw std::runtime_error("DXF: bad group code '" + c + "'");
        std::string v = lines[i + 1];
        if (!v.empty() && v.back() == '\r') v.pop_back();
        pairs.push_back({code, code == 1 || code == 3 ? v : trim(v)});
    }
    DxfFile f;
    std::size_t i = 0;
    bool eof = false;
    while (i < pairs.size()) {
        if (pairs[i].code == 0 && pairs[i].value == "EOF") {
            eof = true;
            break;
        }
        if (!(pairs[i].code == 0 && pairs[i].value == "SECTION") || i + 1 >= pairs.size() || pairs[i + 1].code != 2) {
            throw std::runtime_error("DXF: expected SECTION at pair " + std::to_string(i));
        }
        std::string name = pairs[i + 1].value;
        i += 2;
        std::vector<DxfObject> objs;
        DxfObject cur;
        bool closed = false;
        // В HEADER объекты начинаются с кода 9, в остальных секциях — с кода 0.
        const int startCode = name == "HEADER" ? 9 : 0;
        bool have = false;
        for (; i < pairs.size(); ++i) {
            if (pairs[i].code == 0 && pairs[i].value == "ENDSEC") {
                closed = true;
                ++i;
                break;
            }
            if (pairs[i].code == startCode) {
                if (have) objs.push_back(cur);
                cur = DxfObject{pairs[i].value, {}};
                have = true;
            } else {
                if (!have) throw std::runtime_error("DXF: data before first object in " + name);
                cur.pairs.push_back(pairs[i]);
            }
        }
        if (have) objs.push_back(cur);
        if (!closed) throw std::runtime_error("DXF: section without ENDSEC: " + name);
        f.sections.emplace_back(name, std::move(objs));
    }
    if (!eof) throw std::runtime_error("DXF: no EOF");
    return f;
}

std::string decodeDxfUnicode(const std::string& s) {
    std::u32string out;
    for (std::size_t i = 0; i < s.size();) {
        if (s.compare(i, 3, "\\U+") == 0 && i + 7 <= s.size()) {
            out.push_back(char32_t(std::stoul(s.substr(i + 3, 4), nullptr, 16)));
            i += 7;
        } else {
            out.push_back(char32_t(static_cast<unsigned char>(s[i])));
            ++i;
        }
    }
    return encodeUtf8(out);
}

const PdfObject* PdfFile::find(const std::string& needle) const {
    for (const auto& o : objects) {
        if (o.dict.find(needle) != std::string::npos) return &o;
    }
    return nullptr;
}

PdfFile parsePdf(const std::string& b) {
    if (b.compare(0, 5, "%PDF-") != 0) throw std::runtime_error("PDF: no header");
    const std::size_t sx = b.rfind("startxref");
    if (sx == std::string::npos) throw std::runtime_error("PDF: no startxref");
    PdfFile f;
    f.startxref = std::stoul(b.substr(sx + 9));
    if (b.compare(f.startxref, 4, "xref") != 0) throw std::runtime_error("PDF: startxref does not point to xref");
    // "xref\n0 N\n", затем N записей по 20 байт.
    const std::size_t l1 = b.find('\n', f.startxref);
    const std::size_t l2 = b.find('\n', l1 + 1);
    std::istringstream in(b.substr(l1 + 1, l2 - l1 - 1));
    std::size_t first = 0, count = 0;
    in >> first >> count;
    if (first != 0 || count == 0) throw std::runtime_error("PDF: xref must start at 0");
    std::size_t pos = l2 + 1;
    for (std::size_t k = 0; k < count; ++k) {
        const std::string entry = b.substr(pos, 20);
        if (entry.size() != 20 || (entry[17] != 'n' && entry[17] != 'f') || entry[18] != ' ' || entry[19] != '\n') {
            throw std::runtime_error("PDF: malformed xref entry " + std::to_string(k));
        }
        f.xrefOffsets.push_back(std::stoul(entry.substr(0, 10)));
        pos += 20;
    }
    for (std::size_t id = 1; id < f.xrefOffsets.size(); ++id) {
        const std::size_t off = f.xrefOffsets[id];
        const std::string head = std::to_string(id) + " 0 obj\n";
        if (b.compare(off, head.size(), head) != 0) {
            throw std::runtime_error("PDF: xref offset of object " + std::to_string(id) + " does not point to it");
        }
        PdfObject o{int(id), off, "", ""};
        const std::size_t start = off + head.size();
        const std::size_t end = b.find("\nendobj", start);
        const std::size_t st = b.find(">>\nstream\n", start);
        if (st != std::string::npos && st < end) {
            o.dict = b.substr(start, st + 2 - start);
            std::smatch m;
            const std::regex len(R"(/Length (\d+) )");
            if (!std::regex_search(o.dict, m, len)) throw std::runtime_error("PDF: stream without /Length");
            const std::size_t n = std::stoul(m[1]);
            const std::size_t data = st + 10;
            if (b.compare(data + n, 10, "\nendstream") != 0) {
                throw std::runtime_error("PDF: /Length of object " + std::to_string(id) + " is wrong");
            }
            o.stream = b.substr(data, n);
        } else {
            o.dict = b.substr(start, end - start);
        }
        f.objects.push_back(std::move(o));
    }
    return f;
}

std::string runCommand(const std::string& cmd, int* exitCode) {
    std::string out;
    FILE* p = popen(cmd.c_str(), "r");
    if (!p) {
        *exitCode = -1;
        return out;
    }
    std::array<char, 4096> buf{};
    std::size_t n;
    while ((n = fread(buf.data(), 1, buf.size(), p)) > 0) out.append(buf.data(), n);
    const int st = pclose(p);
    *exitCode = WIFEXITED(st) ? WEXITSTATUS(st) : -1;
    return out;
}

std::string readFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

TitleBlockData sampleTitleData() {
    TitleBlockData d;
    d.designation = "АБВГ.123456.001";
    d.name = "Вал ступенчатый";
    d.material = "Сталь 45 ГОСТ 1050-2013";
    d.litera = "О";
    d.mass = "1,25";
    d.scale = "1:2";
    d.sheet = 1;
    d.sheets = 2;
    d.organization = "ООО «Бенд»";
    d.developer = {"Иванов", "11.26"};
    d.checker = {"Петров", "11.26"};
    d.normControl = {"Сидоров", ""};
    d.approver = {"Кузнецов", ""};
    return d;
}

Sample makeSample(GostFormat format, Orientation orientation, const TrueTypeFont* font) {
    auto sheet = gostSheet(format, 1, orientation).value();
    Drawing d = Drawing::create(sheet, 0.5).value();
    FrameOptions fo;
    fo.font = font;
    auto layout = addFrameAndTitleBlock(d, sampleTitleData(), fo);
    if (!layout) throw std::runtime_error("title block: " + layout.error().message);
    auto must = [](Status s) {
        if (!s) throw std::runtime_error(s.error().message);
    };
    must(d.addLayer({"GEOM", 7}));
    must(d.addLayer({"AXIS", 1}));
    must(d.addLayer({"HATCH", 8}));
    must(d.add({LineSeg{{50, 100}, {150, 100}}, "GEOM", LineType::SolidThick}));
    must(d.add({LineSeg{{50, 120}, {150, 120}}, "GEOM", LineType::Dashed}));
    must(d.add({LineSeg{{40, 140}, {160, 140}}, "AXIS", LineType::DashDotThin}));
    must(d.add({Circle{{100, 180}, 20}, "GEOM", LineType::SolidThick}));
    must(d.add({Arc{{100, 180}, 30, 0, 90}, "GEOM", LineType::SolidThin}));
    must(d.add({Polyline{{{60, 200}, {80, 200}, {80, 220}, {60, 220}}, true}, "GEOM", LineType::SolidThick}));
    must(d.add({Hatch{{{{60, 200}, {80, 200}, {80, 220}, {60, 220}}}, 45, 2}, "HATCH", LineType::SolidThin}));
    Text t;
    t.position = {50, 90};
    t.utf8 = "Технические требования: Ø20";
    t.height = 5;
    must(d.add({t, "GEOM", LineType::SolidThin}));
    return Sample{std::move(d), layout.value()};
}

}  // namespace dv2test
