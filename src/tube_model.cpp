#include "bendcore/tube_model.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <map>
#include <sstream>
#include <stdexcept>

namespace bendcore {
namespace {

std::string trim(const std::string& s) {
    std::size_t b = 0;
    std::size_t e = s.size();
    while (b < e && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
    return s.substr(b, e - b);
}

std::string lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

[[noreturn]] void fail(int line, const std::string& what) {
    throw std::invalid_argument("строка " + std::to_string(line) + ": " + what);
}

double parse_number(const std::string& s, const char* column, int line) {
    if (s.empty()) fail(line, std::string("не задано «") + column + "»");
    char* end = nullptr;
    const double v = std::strtod(s.c_str(), &end);
    if (end == s.c_str() || *end != '\0' || !std::isfinite(v)) {
        fail(line, std::string("«") + column + "» — не число: " + s);
    }
    return v;
}

double parse_number_or(const std::string& s, double fallback, const char* column, int line) {
    return s.empty() ? fallback : parse_number(s, column, line);
}

int parse_direction(const std::string& s, int line) {
    const double v = parse_number_or(s, 1.0, "direction", line);
    if (v != 1.0 && v != -1.0) fail(line, "«direction» — только +1 или −1");
    return static_cast<int>(v);
}

// Общая сборка порта из уже разобранных значений (CSV и JSON).
Port make_port(const Vec3& pos, const Vec3& dir, double od, const std::string& name, int line) {
    try {
        return manual_port(pos, dir, od, name);
    } catch (const std::invalid_argument& e) {
        fail(line, e.what());
    }
}

// ---- CSV ----

std::vector<std::string> split_csv(const std::string& line) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : line) {
        if (c == ',') {
            out.push_back(trim(cur));
            cur.clear();
        } else if (c != '\r') {
            cur += c;
        }
    }
    out.push_back(trim(cur));
    return out;
}

// ---- JSON (минимальный разбор: объекты, массивы, строки без \u, числа, true/false/null) ----

struct JValue {
    enum class Kind { Null, Bool, Number, String, Array, Object } kind = Kind::Null;
    double number = 0.0;
    bool boolean = false;
    std::string str;
    std::vector<JValue> items;      // массив
    std::vector<std::string> keys;  // объект: ключи и значения по индексу
    std::vector<JValue> values;
};

class JsonParser {
public:
    explicit JsonParser(const std::string& text) : s_(text) {}

    JValue parse_document() {
        JValue v = parse_value();
        skip_ws();
        if (pos_ != s_.size()) error("лишний текст после JSON");
        return v;
    }

private:
    const std::string& s_;
    std::size_t pos_ = 0;

    [[noreturn]] void error(const std::string& what) const {
        // номер строки по позиции
        const int line = 1 + static_cast<int>(std::count(s_.begin(), s_.begin() + static_cast<std::ptrdiff_t>(pos_), '\n'));
        fail(line, "JSON: " + what);
    }

    void skip_ws() {
        while (pos_ < s_.size() && std::isspace(static_cast<unsigned char>(s_[pos_]))) ++pos_;
    }

    char peek() {
        skip_ws();
        if (pos_ >= s_.size()) error("неожиданный конец");
        return s_[pos_];
    }

    void expect(char c) {
        if (peek() != c) error(std::string("ожидалось «") + c + "»");
        ++pos_;
    }

    JValue parse_value() {
        const char c = peek();
        if (c == '{') return parse_object();
        if (c == '[') return parse_array();
        if (c == '"') {
            JValue v;
            v.kind = JValue::Kind::String;
            v.str = parse_string();
            return v;
        }
        if (c == 't' || c == 'f' || c == 'n') return parse_literal();
        return parse_number_token();
    }

    JValue parse_object() {
        JValue v;
        v.kind = JValue::Kind::Object;
        expect('{');
        if (peek() == '}') {
            ++pos_;
            return v;
        }
        while (true) {
            if (peek() != '"') error("ожидался ключ в кавычках");
            v.keys.push_back(parse_string());
            expect(':');
            v.values.push_back(parse_value());
            const char c = peek();
            ++pos_;
            if (c == '}') break;
            if (c != ',') error("ожидалась «,» или «}»");
        }
        return v;
    }

    JValue parse_array() {
        JValue v;
        v.kind = JValue::Kind::Array;
        expect('[');
        if (peek() == ']') {
            ++pos_;
            return v;
        }
        while (true) {
            v.items.push_back(parse_value());
            const char c = peek();
            ++pos_;
            if (c == ']') break;
            if (c != ',') error("ожидалась «,» или «]»");
        }
        return v;
    }

    std::string parse_string() {
        expect('"');
        std::string out;
        while (true) {
            if (pos_ >= s_.size()) error("незакрытая строка");
            const char c = s_[pos_++];
            if (c == '"') break;
            if (c != '\\') {
                out += c;
                continue;
            }
            if (pos_ >= s_.size()) error("незакрытая строка");
            const char e = s_[pos_++];
            switch (e) {
            case '"': out += '"'; break;
            case '\\': out += '\\'; break;
            case '/': out += '/'; break;
            case 'n': out += '\n'; break;
            case 't': out += '\t'; break;
            case 'r': out += '\r'; break;
            default: error("неподдерживаемая escape-последовательность");
            }
        }
        return out;
    }

    JValue parse_literal() {
        JValue v;
        if (s_.compare(pos_, 4, "true") == 0) {
            v.kind = JValue::Kind::Bool;
            v.boolean = true;
            pos_ += 4;
        } else if (s_.compare(pos_, 5, "false") == 0) {
            v.kind = JValue::Kind::Bool;
            pos_ += 5;
        } else if (s_.compare(pos_, 4, "null") == 0) {
            pos_ += 4;
        } else {
            error("неизвестное значение");
        }
        return v;
    }

    JValue parse_number_token() {
        const std::size_t start = pos_;
        while (pos_ < s_.size() && (std::isdigit(static_cast<unsigned char>(s_[pos_])) || s_[pos_] == '-' ||
                                    s_[pos_] == '+' || s_[pos_] == '.' || s_[pos_] == 'e' || s_[pos_] == 'E')) {
            ++pos_;
        }
        if (pos_ == start) error("ожидалось значение");
        const std::string token = s_.substr(start, pos_ - start);
        char* end = nullptr;
        const double d = std::strtod(token.c_str(), &end);
        if (end == token.c_str() || *end != '\0' || !std::isfinite(d)) error("некорректное число: " + token);
        JValue v;
        v.kind = JValue::Kind::Number;
        v.number = d;
        return v;
    }
};

const JValue* find_key(const JValue& obj, const std::string& key) {
    for (std::size_t i = 0; i < obj.keys.size(); ++i) {
        if (obj.keys[i] == key) return &obj.values[i];
    }
    return nullptr;
}

[[noreturn]] void json_fail(const std::string& what) { throw std::invalid_argument("JSON: " + what); }

double json_number(const JValue& obj, const char* key, double fallback, bool required) {
    const JValue* v = find_key(obj, key);
    if (!v) {
        if (required) json_fail(std::string("не задано «") + key + "»");
        return fallback;
    }
    if (v->kind != JValue::Kind::Number) json_fail(std::string("«") + key + "» — должно быть числом");
    return v->number;
}

Vec3 json_vec3(const JValue& obj, const char* key) {
    const JValue* v = find_key(obj, key);
    if (!v || v->kind != JValue::Kind::Array || v->items.size() != 3) {
        json_fail(std::string("«") + key + "» — массив из трёх чисел");
    }
    Vec3 out{};
    for (int k = 0; k < 3; ++k) {
        if (v->items[k].kind != JValue::Kind::Number) json_fail(std::string("«") + key + "» — только числа");
        out[k] = v->items[k].number;
    }
    return out;
}

}  // namespace

ImportData import_csv(const std::string& text) {
    ImportData data;
    std::istringstream in(text);
    std::string raw;
    int lineNo = 0;
    std::map<std::string, std::size_t> col;
    bool headerRead = false;

    while (std::getline(in, raw)) {
        ++lineNo;
        const std::string line = trim(raw);
        if (line.empty() || line[0] == '#') continue;
        const auto cells = split_csv(raw);

        if (!headerRead) {
            for (std::size_t i = 0; i < cells.size(); ++i) col[lower(cells[i])] = i;
            if (!col.count("kind")) fail(lineNo, "в заголовке нет колонки «kind»");
            headerRead = true;
            continue;
        }

        auto get = [&](const char* name) -> std::string {
            auto it = col.find(name);
            if (it == col.end() || it->second >= cells.size()) return "";
            return cells[it->second];
        };

        const std::string kind = lower(get("kind"));
        if (kind == "straight") {
            data.path.push_back(Straight{parse_number(get("length"), "length", lineNo)});
        } else if (kind == "bend") {
            const double R = parse_number(get("radius"), "radius", lineNo);
            const double B = parse_number(get("angle"), "angle", lineNo);
            const double A = parse_number_or(get("rotation"), 0.0, "rotation", lineNo);
            data.path.push_back(Bend{R, B, A});
        } else if (kind == "spiral") {
            const double R = parse_number(get("radius"), "radius", lineNo);
            const double p = parse_number(get("pitch"), "pitch", lineNo);
            const double n = parse_number(get("turns"), "turns", lineNo);
            data.path.push_back(Spiral{R, p, n, parse_direction(get("direction"), lineNo)});
        } else if (kind == "port") {
            const Vec3 pos{parse_number(get("x"), "x", lineNo), parse_number(get("y"), "y", lineNo),
                           parse_number(get("z"), "z", lineNo)};
            const Vec3 dir{parse_number(get("dx"), "dx", lineNo), parse_number(get("dy"), "dy", lineNo),
                           parse_number(get("dz"), "dz", lineNo)};
            const double od = parse_number(get("od"), "od", lineNo);
            data.ports.push_back(make_port(pos, dir, od, get("name"), lineNo));
        } else {
            fail(lineNo, "неизвестный kind: «" + get("kind") + "» (straight, bend, spiral, port)");
        }
    }
    if (!headerRead) throw std::invalid_argument("пустой CSV: нет заголовка");
    return data;
}

ImportData import_json(const std::string& text) {
    JsonParser parser(text);
    const JValue root = parser.parse_document();
    if (root.kind != JValue::Kind::Object) throw std::invalid_argument("JSON: корень должен быть объектом");
    if (const JValue* f = find_key(root, "format")) {
        if (f->kind != JValue::Kind::String || f->str != "bendcore-tube") {
            throw std::invalid_argument("JSON: «format» должен быть \"bendcore-tube\"");
        }
    }
    if (const JValue* v = find_key(root, "version")) {
        if (v->kind != JValue::Kind::Number || v->number != 1.0) {
            throw std::invalid_argument("JSON: поддерживается только «version» 1");
        }
    }

    ImportData data;
    if (const JValue* path = find_key(root, "path")) {
        if (path->kind != JValue::Kind::Array) throw std::invalid_argument("JSON: «path» — массив");
        for (const JValue& item : path->items) {
            if (item.kind != JValue::Kind::Object) throw std::invalid_argument("JSON: участок пути — объект");
            const JValue* kindValue = find_key(item, "kind");
            if (!kindValue || kindValue->kind != JValue::Kind::String) {
                throw std::invalid_argument("JSON: участок без «kind»");
            }
            const std::string kind = lower(kindValue->str);
            if (kind == "straight") {
                data.path.push_back(Straight{json_number(item, "length", 0.0, true)});
            } else if (kind == "bend") {
                const double R = json_number(item, "radius", 0.0, true);
                const double B = json_number(item, "angle", 0.0, true);
                const double A = json_number(item, "rotation", 0.0, false);
                data.path.push_back(Bend{R, B, A});
            } else if (kind == "spiral") {
                const double R = json_number(item, "radius", 0.0, true);
                const double p = json_number(item, "pitch", 0.0, true);
                const double n = json_number(item, "turns", 0.0, true);
                const double dir = json_number(item, "direction", 1.0, false);
                if (dir != 1.0 && dir != -1.0) throw std::invalid_argument("JSON: «direction» — только +1 или −1");
                data.path.push_back(Spiral{R, p, n, static_cast<int>(dir)});
            } else {
                throw std::invalid_argument("JSON: неизвестный kind: «" + kindValue->str + "»");
            }
        }
    }
    if (const JValue* ports = find_key(root, "ports")) {
        if (ports->kind != JValue::Kind::Array) throw std::invalid_argument("JSON: «ports» — массив");
        for (const JValue& item : ports->items) {
            if (item.kind != JValue::Kind::Object) throw std::invalid_argument("JSON: порт — объект");
            const Vec3 pos = json_vec3(item, "pos");
            const Vec3 dir = json_vec3(item, "direction");
            const double od = json_number(item, "od", 0.0, true);
            std::string name;
            if (const JValue* n = find_key(item, "name")) {
                if (n->kind != JValue::Kind::String) throw std::invalid_argument("JSON: «name» — строка");
                name = n->str;
            }
            try {
                data.ports.push_back(manual_port(pos, dir, od, name));
            } catch (const std::invalid_argument& e) {
                throw std::invalid_argument(std::string("JSON: порт: ") + e.what());
            }
        }
    }
    if (data.path.empty() && data.ports.empty()) throw std::invalid_argument("JSON: нет ни «path», ни «ports»");
    return data;
}

TubeModel make_model(const std::string& id, const std::string& source, const ImportData& data, double od_mm,
                     double wall_mm) {
    if (!(od_mm > 0.0)) throw std::invalid_argument("∅ трубы должен быть больше нуля");
    if (wall_mm < 0.0 || (wall_mm > 0.0 && wall_mm >= od_mm / 2.0)) {
        throw std::invalid_argument("стенка должна быть от 0 до радиуса трубы (не включая)");
    }
    if (data.path.empty() && data.ports.empty()) throw std::invalid_argument("нет ни пути, ни портов");

    TubeModel m;
    m.id = id;
    m.source = source;
    m.od = od_mm;
    m.wall = wall_mm;
    if (!data.path.empty()) m.path = data.path;
    m.ports = data.ports;

    std::vector<std::string> names;
    for (Port& p : m.ports) {
        if (p.name.empty()) p.name = auto_name("tube", names);
        if (std::find(names.begin(), names.end(), p.name) != names.end()) {
            throw std::invalid_argument("повтор имени порта: " + p.name);
        }
        names.push_back(p.name);
    }
    return m;
}

}  // namespace bendcore
