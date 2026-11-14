// Чтение DXF ASCII: поток пар «групповой код — значение» → секции → объекты (от пары
// с кодом 0 до следующей) → примитивы. Описание: docs/drawing/DXF_IMPORT.md.
#include <bendcore/dxf_reader.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <numbers>
#include <sstream>

namespace bendcore::dxf {

namespace {

constexpr double kPi = std::numbers::pi;

struct Pair {
    int code;
    std::string value;
    std::size_t line;  // номер строки значения (с 1)
};

// Объект: пара (0, type) и все пары до следующей пары с кодом 0.
struct Group {
    std::string type;
    std::size_t line = 0;
    std::vector<Pair> pairs;
};

Error parseError(std::size_t line, const std::string& what) {
    return Error{ErrorCode::ParseError, "DXF line " + std::to_string(line) + ": " + what};
}

std::string_view trim(std::string_view s) {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.remove_prefix(1);
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r')) s.remove_suffix(1);
    return s;
}

std::string upper(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return char(std::toupper(c)); });
    return s;
}

Result<double> toReal(const Pair& p) {
    const std::string_view s = trim(p.value);
    double v = 0.0;
    const char* first = s.data();
    if (!s.empty() && s.front() == '+') ++first;
    const auto [ptr, ec] = std::from_chars(first, s.data() + s.size(), v);
    if (s.empty() || ec != std::errc() || ptr != s.data() + s.size() || !std::isfinite(v)) {
        return parseError(p.line, "group " + std::to_string(p.code) + ": expected a number, got '" +
                                      std::string(s) + "'");
    }
    return v;
}

Result<int> toInt(const Pair& p) {
    const std::string_view s = trim(p.value);
    long long v = 0;
    const char* first = s.data();
    if (!s.empty() && s.front() == '+') ++first;
    const auto [ptr, ec] = std::from_chars(first, s.data() + s.size(), v);
    if (s.empty() || ec != std::errc() || ptr != s.data() + s.size() || v < INT32_MIN || v > INT32_MAX) {
        return parseError(p.line, "group " + std::to_string(p.code) + ": expected an integer, got '" +
                                      std::string(s) + "'");
    }
    return int(v);
}

// --- Строки: кодовая страница → UTF-8, затем \U+XXXX ---

void appendUtf8(std::string& out, char32_t cp) {
    if (cp < 0x80) {
        out.push_back(char(cp));
    } else if (cp < 0x800) {
        out.push_back(char(0xC0 | (cp >> 6)));
        out.push_back(char(0x80 | (cp & 0x3F)));
    } else if (cp < 0x10000) {
        out.push_back(char(0xE0 | (cp >> 12)));
        out.push_back(char(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(char(0x80 | (cp & 0x3F)));
    } else {
        out.push_back(char(0xF0 | (cp >> 18)));
        out.push_back(char(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(char(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(char(0x80 | (cp & 0x3F)));
    }
}

bool isValidUtf8(std::string_view s) {
    std::size_t i = 0;
    while (i < s.size()) {
        const unsigned char c = s[i];
        int n = 0;
        char32_t cp = 0;
        if (c < 0x80) { ++i; continue; }
        if ((c & 0xE0) == 0xC0) { n = 1; cp = c & 0x1F; }
        else if ((c & 0xF0) == 0xE0) { n = 2; cp = c & 0x0F; }
        else if ((c & 0xF8) == 0xF0) { n = 3; cp = c & 0x07; }
        else return false;
        for (int k = 1; k <= n; ++k) {
            if (i + k >= s.size()) return false;
            const unsigned char cc = s[i + k];
            if ((cc & 0xC0) != 0x80) return false;
            cp = (cp << 6) | (cc & 0x3F);
        }
        if ((n == 1 && cp < 0x80) || (n == 2 && cp < 0x800) || (n == 3 && (cp < 0x10000 || cp > 0x10FFFF)) ||
            (cp >= 0xD800 && cp <= 0xDFFF)) {
            return false;
        }
        i += n + 1;
    }
    return true;
}

// Windows-1251, байты 0x80…0xBF (0xC0…0xFF → U+0410…U+044F). 0x98 не определён.
constexpr std::array<char16_t, 64> kCp1251High = {
    0x0402, 0x0403, 0x201A, 0x0453, 0x201E, 0x2026, 0x2020, 0x2021, 0x20AC, 0x2030, 0x0409, 0x2039, 0x040A,
    0x040C, 0x040B, 0x040F, 0x0452, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014, 0xFFFD, 0x2122,
    0x0459, 0x203A, 0x045A, 0x045C, 0x045B, 0x045F, 0x00A0, 0x040E, 0x045E, 0x0408, 0x00A4, 0x0490, 0x00A6,
    0x00A7, 0x0401, 0x00A9, 0x0404, 0x00AB, 0x00AC, 0x00AD, 0x00AE, 0x0407, 0x00B0, 0x00B1, 0x0406, 0x0456,
    0x0491, 0x00B5, 0x00B6, 0x00B7, 0x0451, 0x2116, 0x0454, 0x00BB, 0x0458, 0x0405, 0x0455, 0x0457};

// Windows-1252, байты 0x80…0x9F (0xA0…0xFF совпадают с Latin-1). Неопределённые — U+FFFD.
constexpr std::array<char16_t, 32> kCp1252High = {
    0x20AC, 0xFFFD, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021, 0x02C6, 0x2030, 0x0160,
    0x2039, 0x0152, 0xFFFD, 0x017D, 0xFFFD, 0xFFFD, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022,
    0x2013, 0x2014, 0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0xFFFD, 0x017E, 0x0178};

class Decoder {
public:
    // AC1021 (R2007) и новее хранят строки в UTF-8; более ранние — в $DWGCODEPAGE.
    void configure(const std::string& version, const std::string& codepage) {
        utf8_ = version >= "AC1021";
        codepage_ = upper(codepage);
    }

    std::string decode(const std::string& raw, std::vector<std::string>& warnings) {
        const bool ascii = std::all_of(raw.begin(), raw.end(), [](char c) { return (unsigned char)c < 0x80; });
        std::string bytes;
        if (ascii) {
            bytes = raw;
        } else if (utf8_ || (codepage_ != "ANSI_1251" && codepage_ != "ANSI_1252" && isValidUtf8(raw))) {
            bytes = raw;
        } else if (codepage_ == "ANSI_1251" || codepage_ == "ANSI_1252") {
            for (unsigned char c : raw) {
                if (c < 0x80) {
                    bytes.push_back(char(c));
                } else if (codepage_ == "ANSI_1251") {
                    appendUtf8(bytes, c >= 0xC0 ? char32_t(0x0410 + (c - 0xC0)) : char32_t(kCp1251High[c - 0x80]));
                } else {
                    appendUtf8(bytes, c >= 0xA0 ? char32_t(c) : char32_t(kCp1252High[c - 0x80]));
                }
            }
        } else {
            for (unsigned char c : raw) {
                if (c < 0x80) bytes.push_back(char(c));
                else appendUtf8(bytes, 0xFFFD);
            }
            if (!warnedCodepage_) {
                warnings.push_back("strings with non-ASCII bytes in code page '" + codepage_ +
                                   "' are not decoded; such bytes are replaced by U+FFFD");
                warnedCodepage_ = true;
            }
        }
        return unescapeUnicode(bytes);
    }

    static std::string unescapeUnicode(const std::string& s) {
        std::string out;
        for (std::size_t i = 0; i < s.size(); ++i) {
            if (s[i] == '\\' && i + 7 <= s.size() && (s[i + 1] == 'U' || s[i + 1] == 'u') && s[i + 2] == '+') {
                unsigned v = 0;
                const auto [ptr, ec] = std::from_chars(s.data() + i + 3, s.data() + i + 7, v, 16);
                if (ec == std::errc() && ptr == s.data() + i + 7) {
                    appendUtf8(out, char32_t(v));
                    i += 6;
                    continue;
                }
            }
            out.push_back(s[i]);
        }
        return out;
    }

private:
    bool utf8_ = false;
    std::string codepage_;
    bool warnedCodepage_ = false;
};

// Коды %%c, %%d, %%p, %%%, %%nnn; %%u/%%o (подчёркивание/надчёркивание) удаляются.
std::string expandPercentCodes(const std::string& s) {
    std::string out;
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '%' && i + 2 < s.size() && s[i + 1] == '%') {
            const char c = char(std::tolower((unsigned char)s[i + 2]));
            if (c == 'c') { appendUtf8(out, 0x00D8); i += 2; continue; }
            if (c == 'd') { appendUtf8(out, 0x00B0); i += 2; continue; }
            if (c == 'p') { appendUtf8(out, 0x00B1); i += 2; continue; }
            if (c == 'u' || c == 'o' || c == 'k') { i += 2; continue; }
            if (c == '%') { out.push_back('%'); i += 2; continue; }
            if (std::isdigit((unsigned char)c) && i + 4 < s.size() && std::isdigit((unsigned char)s[i + 3]) &&
                std::isdigit((unsigned char)s[i + 4])) {
                appendUtf8(out, char32_t(std::stoi(s.substr(i + 2, 3))));
                i += 4;
                continue;
            }
        }
        out.push_back(s[i]);
    }
    return out;
}

// Текст MTEXT без форматирования: \P и \N → перевод строки, \~ → неразрывный пробел,
// \\ \{ \} → символ, { } удаляются, \L \l \O \o \K \k удаляются, коды с параметром до ';'
// (\f \F \H \W \Q \T \A \C \c \p) удаляются, дробь \Sa^b; → a/b, ^I → табуляция.
std::string plainMtext(const std::string& s) {
    std::string out;
    for (std::size_t i = 0; i < s.size(); ++i) {
        const char c = s[i];
        if (c == '{' || c == '}') continue;
        if (c == '^' && i + 1 < s.size()) {
            const char n = s[i + 1];
            if (n == ' ') out.push_back('^');
            else if (n == 'I') out.push_back('\t');
            else if (n == 'J') out.push_back('\n');
            ++i;
            continue;
        }
        if (c != '\\' || i + 1 >= s.size()) {
            out.push_back(c);
            continue;
        }
        const char k = s[i + 1];
        switch (k) {
            case 'P': case 'N': out.push_back('\n'); ++i; break;
            case '~': appendUtf8(out, 0x00A0); ++i; break;
            case '\\': case '{': case '}': out.push_back(k); ++i; break;
            case 'L': case 'l': case 'O': case 'o': case 'K': case 'k': ++i; break;
            case 'S': {
                const std::size_t end = s.find(';', i + 2);
                std::string frac = s.substr(i + 2, end == std::string::npos ? std::string::npos : end - i - 2);
                for (char& f : frac) {
                    if (f == '^' || f == '#') f = '/';
                }
                out += frac;
                i = end == std::string::npos ? s.size() : end;
                break;
            }
            case 'f': case 'F': case 'H': case 'W': case 'Q': case 'T': case 'A': case 'C': case 'c': case 'p': {
                const std::size_t end = s.find(';', i + 2);
                i = end == std::string::npos ? s.size() : end;
                break;
            }
            default: out.push_back(c); break;
        }
    }
    return out;
}

double normDeg(double a) {
    double r = std::fmod(a, 360.0);
    if (r < 0) r += 360.0;
    if (r >= 360.0) r -= 360.0;
    return r;
}

// Направление выдавливания (OCS): +Z — без преобразования, −Z — зеркало по X,
// остальные — не поддерживаются.
enum class Ocs { World, MirrorX, Other };

struct Reader {
    Document doc;
    Decoder decoder;
    int nonZeroZ = 0;

    std::string str(const Pair& p) { return decoder.decode(p.value, doc.warnings); }

    // Общие свойства и направление выдавливания. Ошибка — нечисловое значение.
    Result<Ocs> props(const Group& g, EntityProps& pr) {
        double n[3] = {0.0, 0.0, 1.0};
        for (const Pair& p : g.pairs) {
            switch (p.code) {
                case 8: pr.layer = str(p); break;
                case 6: pr.linetype = str(p); break;
                case 5: pr.handle = std::string(trim(p.value)); break;
                case 62: { auto v = toInt(p); if (!v) return v.error(); pr.color = v.value(); break; }
                case 67: { auto v = toInt(p); if (!v) return v.error(); pr.paper_space = v.value() == 1; break; }
                case 210: case 220: case 230: {
                    auto v = toReal(p);
                    if (!v) return v.error();
                    n[(p.code - 210) / 10] = v.value();
                    break;
                }
                default: break;
            }
        }
        const double len = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
        if (len == 0.0) return parseError(g.line, g.type + ": zero extrusion direction");
        const double tol = 1e-9;
        if (std::fabs(n[0] / len) < tol && std::fabs(n[1] / len) < tol) {
            return n[2] > 0 ? Ocs::World : Ocs::MirrorX;
        }
        return Ocs::Other;
    }

    // Значение группы code (первое) как число; def — если группы нет.
    static Result<double> real(const Group& g, int code, double def) {
        for (const Pair& p : g.pairs) {
            if (p.code == code) return toReal(p);
        }
        return def;
    }
    static Result<int> integer(const Group& g, int code, int def) {
        for (const Pair& p : g.pairs) {
            if (p.code == code) return toInt(p);
        }
        return def;
    }
    static bool has(const Group& g, int code) {
        return std::any_of(g.pairs.begin(), g.pairs.end(), [code](const Pair& p) { return p.code == code; });
    }

    void skip(const std::string& what) { ++doc.skipped[what]; }

    void noteZ(double z) {
        if (z != 0.0) ++nonZeroZ;
    }

#define BENDCORE_DXF_GET(var, expr) \
    auto var##_r = (expr);          \
    if (!var##_r) return var##_r.error(); \
    const auto var = var##_r.value()

    Result<bool> line(const Group& g) {
        Line l;
        auto o = props(g, l.props);
        if (!o) return o.error();
        BENDCORE_DXF_GET(x1, real(g, 10, 0.0));
        BENDCORE_DXF_GET(y1, real(g, 20, 0.0));
        BENDCORE_DXF_GET(z1, real(g, 30, 0.0));
        BENDCORE_DXF_GET(x2, real(g, 11, 0.0));
        BENDCORE_DXF_GET(y2, real(g, 21, 0.0));
        BENDCORE_DXF_GET(z2, real(g, 31, 0.0));
        noteZ(std::max(std::fabs(z1), std::fabs(z2)));
        l.start = {x1, y1};
        l.end = {x2, y2};
        doc.lines.push_back(std::move(l));
        return true;
    }

    Result<bool> circleOrArc(const Group& g, bool arc) {
        EntityProps pr;
        auto o = props(g, pr);
        if (!o) return o.error();
        if (o.value() == Ocs::Other) {
            skip(g.type + " (OCS)");
            return true;
        }
        BENDCORE_DXF_GET(cx, real(g, 10, 0.0));
        BENDCORE_DXF_GET(cy, real(g, 20, 0.0));
        BENDCORE_DXF_GET(cz, real(g, 30, 0.0));
        BENDCORE_DXF_GET(r, real(g, 40, 0.0));
        if (!(r > 0.0)) return parseError(g.line, g.type + ": radius must be positive");
        noteZ(cz);
        const bool mirror = o.value() == Ocs::MirrorX;
        const Point2D c{mirror ? -cx : cx, cy};
        if (!arc) {
            doc.circles.push_back(Circle{std::move(pr), c, r});
            return true;
        }
        BENDCORE_DXF_GET(a0, real(g, 50, 0.0));
        BENDCORE_DXF_GET(a1, real(g, 51, 0.0));
        Arc a{std::move(pr), c, r, normDeg(a0), normDeg(a1)};
        if (mirror) {  // угол θ в OCS(−Z) соответствует 180° − θ, обход меняет направление
            a.start_deg = normDeg(180.0 - a1);
            a.end_deg = normDeg(180.0 - a0);
        }
        doc.arcs.push_back(std::move(a));
        return true;
    }

    Result<bool> lwpolyline(const Group& g) {
        Polyline pl;
        auto o = props(g, pl.props);
        if (!o) return o.error();
        if (o.value() == Ocs::Other) {
            skip("LWPOLYLINE (OCS)");
            return true;
        }
        BENDCORE_DXF_GET(flags, integer(g, 70, 0));
        BENDCORE_DXF_GET(count, integer(g, 90, -1));
        BENDCORE_DXF_GET(elev, real(g, 38, 0.0));
        noteZ(elev);
        pl.closed = (flags & 1) != 0;
        const bool mirror = o.value() == Ocs::MirrorX;
        for (const Pair& p : g.pairs) {
            if (p.code == 10) {
                auto v = toReal(p);
                if (!v) return v.error();
                pl.vertices.push_back({{mirror ? -v.value() : v.value(), 0.0}, 0.0});
            } else if (p.code == 20 || p.code == 42) {
                if (pl.vertices.empty()) {
                    return parseError(p.line, "LWPOLYLINE: group " + std::to_string(p.code) + " before the first vertex");
                }
                auto v = toReal(p);
                if (!v) return v.error();
                if (p.code == 20) pl.vertices.back().point.y = v.value();
                else pl.vertices.back().bulge = mirror ? -v.value() : v.value();
            }
        }
        if (count >= 0 && std::size_t(count) != pl.vertices.size()) {
            doc.warnings.push_back("LWPOLYLINE at line " + std::to_string(g.line) + ": group 90 declares " +
                                   std::to_string(count) + " vertices, " + std::to_string(pl.vertices.size()) +
                                   " found");
        }
        if (pl.vertices.empty()) return parseError(g.line, "LWPOLYLINE without vertices");
        doc.polylines.push_back(std::move(pl));
        return true;
    }

    // POLYLINE и следующие VERTEX до SEQEND (vertices — группы VERTEX).
    Result<bool> polyline(const Group& g, const std::vector<const Group*>& vertices) {
        Polyline pl;
        pl.lightweight = false;
        auto o = props(g, pl.props);
        if (!o) return o.error();
        BENDCORE_DXF_GET(flags, integer(g, 70, 0));
        if (flags & (16 | 64)) {
            skip("POLYLINE (mesh)");
            return true;
        }
        const bool is3d = (flags & 8) != 0;
        if (!is3d && o.value() == Ocs::Other) {
            skip("POLYLINE (OCS)");
            return true;
        }
        BENDCORE_DXF_GET(elev, real(g, 30, 0.0));
        if (!is3d) noteZ(elev);
        const bool mirror = !is3d && o.value() == Ocs::MirrorX;
        const bool splineFit = (flags & 4) != 0;
        pl.closed = (flags & 1) != 0;
        for (const Group* v : vertices) {
            BENDCORE_DXF_GET(vflags, integer(*v, 70, 0));
            if (splineFit && (vflags & 16)) continue;  // управляющая точка сплайна, не на кривой
            BENDCORE_DXF_GET(x, real(*v, 10, 0.0));
            BENDCORE_DXF_GET(y, real(*v, 20, 0.0));
            BENDCORE_DXF_GET(z, real(*v, 30, 0.0));
            BENDCORE_DXF_GET(b, real(*v, 42, 0.0));
            if (is3d) noteZ(z);
            pl.vertices.push_back({{mirror ? -x : x, y}, is3d ? 0.0 : (mirror ? -b : b)});
        }
        if (pl.vertices.empty()) return parseError(g.line, "POLYLINE without vertices");
        doc.polylines.push_back(std::move(pl));
        return true;
    }

    Result<bool> text(const Group& g) {
        Text t;
        auto o = props(g, t.props);
        if (!o) return o.error();
        if (o.value() != Ocs::World) {
            skip("TEXT (OCS)");
            return true;
        }
        BENDCORE_DXF_GET(x, real(g, 10, 0.0));
        BENDCORE_DXF_GET(y, real(g, 20, 0.0));
        BENDCORE_DXF_GET(z, real(g, 30, 0.0));
        noteZ(z);
        t.insertion = {x, y};
        BENDCORE_DXF_GET(h, real(g, 40, 0.0));
        BENDCORE_DXF_GET(rot, real(g, 50, 0.0));
        BENDCORE_DXF_GET(wf, real(g, 41, 1.0));
        BENDCORE_DXF_GET(obl, real(g, 51, 0.0));
        BENDCORE_DXF_GET(hj, integer(g, 72, 0));
        BENDCORE_DXF_GET(vj, integer(g, 73, 0));
        t.height = h;
        t.rotation_deg = rot;
        t.width_factor = wf;
        t.oblique_deg = obl;
        t.h_justify = hj;
        t.v_justify = vj;
        if (has(g, 11)) {
            BENDCORE_DXF_GET(ax, real(g, 11, 0.0));
            BENDCORE_DXF_GET(ay, real(g, 21, 0.0));
            t.alignment = Point2D{ax, ay};
        }
        for (const Pair& p : g.pairs) {
            if (p.code == 1) t.value = expandPercentCodes(str(p));
            else if (p.code == 7) t.style = str(p);
        }
        doc.texts.push_back(std::move(t));
        return true;
    }

    Result<bool> mtext(const Group& g) {
        Text t;
        t.multiline = true;
        auto o = props(g, t.props);
        if (!o) return o.error();
        if (o.value() != Ocs::World) {
            skip("MTEXT (OCS)");
            return true;
        }
        BENDCORE_DXF_GET(x, real(g, 10, 0.0));
        BENDCORE_DXF_GET(y, real(g, 20, 0.0));
        BENDCORE_DXF_GET(z, real(g, 30, 0.0));
        noteZ(z);
        t.insertion = {x, y};
        BENDCORE_DXF_GET(h, real(g, 40, 0.0));
        BENDCORE_DXF_GET(rot, real(g, 50, 0.0));
        BENDCORE_DXF_GET(att, integer(g, 71, 1));
        t.height = h;
        t.attachment = att;
        t.rotation_deg = rot;
        if (has(g, 11)) {  // вектор направления в мировой СК имеет приоритет над углом
            BENDCORE_DXF_GET(dx, real(g, 11, 1.0));
            BENDCORE_DXF_GET(dy, real(g, 21, 0.0));
            if (dx != 0.0 || dy != 0.0) t.rotation_deg = normDeg(std::atan2(dy, dx) * 180.0 / kPi);
        }
        std::string chunks, last;
        for (const Pair& p : g.pairs) {
            if (p.code == 3) chunks += p.value;
            else if (p.code == 1) last = p.value;
            else if (p.code == 7) t.style = str(p);
        }
        Pair whole{1, chunks + last, g.line};
        t.value = expandPercentCodes(plainMtext(str(whole)));
        doc.texts.push_back(std::move(t));
        return true;
    }

#undef BENDCORE_DXF_GET

    Result<bool> layerRecord(const Group& g) {
        Layer l;
        for (const Pair& p : g.pairs) {
            if (p.code == 2) l.name = str(p);
            else if (p.code == 6) l.linetype = str(p);
            else if (p.code == 62 || p.code == 70) {
                auto v = toInt(p);
                if (!v) return v.error();
                if (p.code == 62) l.color = v.value();
                else {
                    l.frozen = (v.value() & 1) != 0;
                    l.locked = (v.value() & 4) != 0;
                }
            }
        }
        if (l.name.empty()) return parseError(g.line, "LAYER record without a name");
        doc.layers.push_back(std::move(l));
        return true;
    }
};

// Пары → группы (каждая начинается с кода 0). Комментарии 999 отбрасываются.
Result<std::vector<Group>> groups(std::string_view text) {
    std::vector<Group> out;
    std::size_t pos = 0, lineNo = 0;
    auto nextLine = [&](std::string_view& line) {
        if (pos >= text.size()) return false;
        std::size_t end = text.find('\n', pos);
        if (end == std::string_view::npos) end = text.size();
        line = text.substr(pos, end - pos);
        if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
        pos = end + 1;
        ++lineNo;
        return true;
    };
    std::string_view codeLine, valueLine;
    while (nextLine(codeLine)) {
        if (trim(codeLine).empty() && pos >= text.size()) break;  // пустая последняя строка
        if (!nextLine(valueLine)) return parseError(lineNo, "group code without a value (odd number of lines)");
        const std::string_view cs = trim(codeLine);
        int code = 0;
        const auto [ptr, ec] = std::from_chars(cs.data(), cs.data() + cs.size(), code);
        if (cs.empty() || ec != std::errc() || ptr != cs.data() + cs.size()) {
            return parseError(lineNo - 1, "expected a group code, got '" + std::string(cs) + "'");
        }
        if (code == 999) continue;
        if (code == 0) {
            out.push_back(Group{std::string(trim(valueLine)), lineNo, {}});
        } else {
            if (out.empty()) return parseError(lineNo - 1, "data before the first group code 0");
            out.back().pairs.push_back(Pair{code, std::string(valueLine), lineNo});
        }
    }
    return out;
}

}  // namespace

const Layer* Document::find_layer(const std::string& name) const {
    for (const Layer& l : layers) {
        if (l.name == name) return &l;
    }
    return nullptr;
}

Result<Document> parseDxf(std::string_view text) {
    if (text.substr(0, 3) == "\xEF\xBB\xBF") text.remove_prefix(3);
    if (text.substr(0, 18) == "AutoCAD Binary DXF") {
        return Error{ErrorCode::Unsupported, "binary DXF is not supported; save the drawing as ASCII DXF"};
    }
    auto gs = groups(text);
    if (!gs) return gs.error();
    const std::vector<Group>& all = gs.value();

    Reader rd;
    bool sawSection = false, sawEof = false;
    std::size_t i = 0;
    while (i < all.size()) {
        const Group& g = all[i];
        if (g.type == "EOF") {
            sawEof = true;
            break;
        }
        if (g.type != "SECTION") return parseError(g.line, "expected SECTION, got '" + g.type + "'");
        sawSection = true;
        // Имя секции — первая пара с кодом 2 (в HEADER дальше идут переменные вроде
        // $DIMSTYLE, значения которых тоже имеют код 2).
        if (g.pairs.empty() || g.pairs.front().code != 2) {
            return parseError(g.line, "SECTION without a name (group 2)");
        }
        const std::string name(trim(g.pairs.front().value));
        // Конец секции.
        std::size_t end = i + 1;
        while (end < all.size() && all[end].type != "ENDSEC") {
            if (all[end].type == "SECTION" || all[end].type == "EOF") {
                return parseError(all[end].line, "section " + name + " is not closed by ENDSEC");
            }
            ++end;
        }
        if (end >= all.size()) return parseError(g.line, "section " + name + " is not closed by ENDSEC");

        if (name == "HEADER") {
            // Переменные: пары (9, $ИМЯ) внутри единственной группы SECTION.
            std::string var;
            for (const Pair& p : g.pairs) {
                if (p.code == 9) {
                    var = std::string(trim(p.value));
                } else if (var == "$ACADVER" && p.code == 1) {
                    rd.doc.version = std::string(trim(p.value));
                } else if (var == "$DWGCODEPAGE" && p.code == 3) {
                    rd.doc.codepage = std::string(trim(p.value));
                } else if (var == "$INSUNITS" && p.code == 70) {
                    auto v = toInt(p);
                    if (!v) return v.error();
                    rd.doc.insunits = v.value();
                }
            }
            rd.decoder.configure(rd.doc.version, rd.doc.codepage);
        } else if (name == "TABLES") {
            for (std::size_t k = i + 1; k < end; ++k) {
                if (all[k].type == "LAYER") {
                    auto r = rd.layerRecord(all[k]);
                    if (!r) return r.error();
                }
            }
        } else if (name == "ENTITIES") {
            for (std::size_t k = i + 1; k < end; ++k) {
                const Group& e = all[k];
                Result<bool> r = true;
                if (e.type == "LINE") r = rd.line(e);
                else if (e.type == "CIRCLE") r = rd.circleOrArc(e, false);
                else if (e.type == "ARC") r = rd.circleOrArc(e, true);
                else if (e.type == "LWPOLYLINE") r = rd.lwpolyline(e);
                else if (e.type == "TEXT") r = rd.text(e);
                else if (e.type == "MTEXT") r = rd.mtext(e);
                else if (e.type == "POLYLINE") {
                    std::vector<const Group*> verts;
                    std::size_t m = k + 1;
                    while (m < end && all[m].type == "VERTEX") verts.push_back(&all[m++]);
                    if (m < end && all[m].type == "SEQEND") {
                        ++m;
                    } else {
                        rd.doc.warnings.push_back("POLYLINE at line " + std::to_string(e.line) +
                                                  " is not terminated by SEQEND");
                    }
                    r = rd.polyline(e, verts);
                    k = m - 1;
                } else if (e.type == "VERTEX" || e.type == "SEQEND") {
                    return parseError(e.line, e.type + " outside of a POLYLINE");
                } else {
                    rd.skip(e.type);
                }
                if (!r) return r.error();
            }
        }
        // CLASSES, BLOCKS, OBJECTS, THUMBNAILIMAGE — не читаются (примитивы блоков
        // появляются в чертеже только через INSERT, который учитывается в skipped).
        i = end + 1;
    }
    if (!sawSection) return Error{ErrorCode::ParseError, "no DXF sections found"};
    if (!sawEof) rd.doc.warnings.push_back("file has no EOF marker");
    if (rd.nonZeroZ > 0) {
        rd.doc.warnings.push_back(std::to_string(rd.nonZeroZ) +
                                  " entities have non-zero Z or elevation; they are projected onto XY");
    }
    return std::move(rd.doc);
}

Result<Document> readDxfFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return Error{ErrorCode::IoError, "cannot open DXF file '" + path + "'"};
    const std::string text((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    if (f.bad()) return Error{ErrorCode::IoError, "cannot read DXF file '" + path + "'"};
    return parseDxf(text);
}

bool isDxfPath(const std::string& path) {
    const std::size_t dot = path.rfind('.');
    return dot != std::string::npos && upper(path.substr(dot + 1)) == "DXF";
}

std::optional<double> millimetersPerUnit(int insunits) {
    switch (insunits) {
        case 1: return 25.4;                     // дюйм
        case 2: return 304.8;                    // фут
        case 3: return 1609344.0;                // миля
        case 4: return 1.0;                      // мм
        case 5: return 10.0;                     // см
        case 6: return 1000.0;                   // м
        case 7: return 1.0e6;                    // км
        case 8: return 25.4e-6;                  // микродюйм
        case 9: return 0.0254;                   // мил
        case 10: return 914.4;                   // ярд
        case 11: return 1.0e-7;                  // ангстрем
        case 12: return 1.0e-6;                  // нм
        case 13: return 1.0e-3;                  // мкм
        case 14: return 100.0;                   // дм
        case 15: return 1.0e4;                   // декаметр
        case 16: return 1.0e5;                   // гектометр
        case 17: return 1.0e12;                  // гигаметр
        case 18: return 1.495978707e14;          // астрономическая единица
        case 19: return 9.4607304725808e18;      // световой год
        case 20: return 3.0856775814913673e19;   // парсек
        default: return std::nullopt;
    }
}

Result<Document> scaleToMillimeters(const Document& doc) {
    const auto k = millimetersPerUnit(doc.insunits);
    if (!k) {
        return Error{ErrorCode::InvalidInput,
                     "drawing units are not defined ($INSUNITS = " + std::to_string(doc.insunits) + ")"};
    }
    const double s = *k;
    Document out = doc;
    auto sc = [s](Point2D& p) {
        p.x *= s;
        p.y *= s;
    };
    for (Line& l : out.lines) { sc(l.start); sc(l.end); }
    for (Circle& c : out.circles) { sc(c.center); c.radius *= s; }
    for (Arc& a : out.arcs) { sc(a.center); a.radius *= s; }
    for (Polyline& p : out.polylines) {
        for (PolylineVertex& v : p.vertices) sc(v.point);  // выпуклость безразмерна
    }
    for (Text& t : out.texts) {
        sc(t.insertion);
        if (t.alignment) sc(*t.alignment);
        t.height *= s;
    }
    out.insunits = 4;
    return out;
}

Result<BulgeArc> bulgeToArc(Point2D p1, Point2D p2, double bulge) {
    const double dx = p2.x - p1.x, dy = p2.y - p1.y;
    const double c = std::hypot(dx, dy);
    if (bulge == 0.0 || !std::isfinite(bulge)) {
        return Error{ErrorCode::InvalidInput, "bulge must be finite and non-zero"};
    }
    if (c == 0.0) return Error{ErrorCode::InvalidInput, "bulge segment has coincident end points"};
    // Расстояние от середины хорды до центра вдоль левой нормали: (c/2)·(1 − b²)/(2b).
    const double d = (c / 2.0) * (1.0 - bulge * bulge) / (2.0 * bulge);
    const double nx = -dy / c, ny = dx / c;
    BulgeArc a;
    a.center = {(p1.x + p2.x) / 2.0 + nx * d, (p1.y + p2.y) / 2.0 + ny * d};
    a.radius = (c / 2.0) * (1.0 + bulge * bulge) / (2.0 * std::fabs(bulge));
    a.start_deg = normDeg(std::atan2(p1.y - a.center.y, p1.x - a.center.x) * 180.0 / kPi);
    a.sweep_deg = 4.0 * std::atan(bulge) * 180.0 / kPi;
    return a;
}

}  // namespace bendcore::dxf
