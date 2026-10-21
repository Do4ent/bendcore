#include "bendcore/dwfx/import_settings_io.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <vector>

namespace bendcore::dwfx {
namespace {

constexpr int kFormatVersion = 1;

std::string escape(const std::string& s) {
    std::string out;
    for (char c : s) {
        if (c == '"' || c == '\\') out.push_back('\\');
        out.push_back(c);
    }
    return out;
}

// Минимальный разбор плоского JSON: объекты, строки (с \" и \\), числа и true/false/null.
// Нужен только для формата, который пишет import_settings_to_json.
class Parser {
public:
    explicit Parser(const std::string& text) : text_(text) {}

    // Значение верхнего уровня — объект; возвращаем пары ключ → (строковое представление, вложенный объект).
    struct Node {
        std::string scalar;                       // строка, число, true/false/null
        bool is_object = false;
        std::vector<std::pair<std::string, Node>> members;
    };

    Node parse_document() {
        Node root = parse_value();
        skip_ws();
        if (pos_ != text_.size()) fail("лишние символы после JSON");
        if (!root.is_object) fail("ожидается объект");
        return root;
    }

private:
    const std::string& text_;
    std::size_t pos_ = 0;

    [[noreturn]] void fail(const std::string& what) const {
        throw std::invalid_argument("настройки импорта DWFx: " + what + " (позиция " + std::to_string(pos_) + ")");
    }

    void skip_ws() {
        while (pos_ < text_.size() && (text_[pos_] == ' ' || text_[pos_] == '\n' || text_[pos_] == '\r' ||
                                       text_[pos_] == '\t')) {
            ++pos_;
        }
    }

    char peek() {
        skip_ws();
        if (pos_ >= text_.size()) fail("неожиданный конец");
        return text_[pos_];
    }

    void expect(char c) {
        if (peek() != c) fail(std::string("ожидается '") + c + "'");
        ++pos_;
    }

    std::string parse_string() {
        expect('"');
        std::string out;
        while (true) {
            if (pos_ >= text_.size()) fail("незакрытая строка");
            const char c = text_[pos_++];
            if (c == '"') break;
            if (c == '\\') {
                if (pos_ >= text_.size()) fail("незавершённая escape-последовательность");
                const char e = text_[pos_++];
                if (e != '"' && e != '\\') fail("неподдержанная escape-последовательность");
                out.push_back(e);
            } else {
                out.push_back(c);
            }
        }
        return out;
    }

    Node parse_value() {
        Node node;
        const char c = peek();
        if (c == '{') {
            ++pos_;
            node.is_object = true;
            if (peek() == '}') {
                ++pos_;
                return node;
            }
            while (true) {
                std::string key = parse_string();
                expect(':');
                Node value = parse_value();
                node.members.emplace_back(std::move(key), std::move(value));
                const char sep = peek();
                ++pos_;
                if (sep == '}') break;
                if (sep != ',') fail("ожидается ',' или '}'");
            }
            return node;
        }
        if (c == '"') {
            node.scalar = parse_string();
            return node;
        }
        // Число, true, false, null — до разделителя.
        const std::size_t begin = pos_;
        while (pos_ < text_.size() && text_[pos_] != ',' && text_[pos_] != '}' && text_[pos_] != ' ' &&
               text_[pos_] != '\n' && text_[pos_] != '\r' && text_[pos_] != '\t') {
            ++pos_;
        }
        if (pos_ == begin) fail("ожидается значение");
        node.scalar = text_.substr(begin, pos_ - begin);
        return node;
    }
};

const Parser::Node* find_member(const Parser::Node& object, const std::string& key) {
    for (const auto& [name, value] : object.members) {
        if (name == key) return &value;
    }
    return nullptr;
}

}  // namespace

std::string import_settings_to_json(const ImportSettings& settings) {
    std::ostringstream os;
    os << "{\"import_kind\":\"dwfx\",\"version\":" << kFormatVersion << ",\"values\":{";
    bool first = true;
    for (const auto& property : import_properties()) {
        if (!first) os << ',';
        first = false;
        os << '"' << escape(property.id) << "\":\"" << escape(get_import_property(settings, property.id)) << '"';
    }
    os << "}}";
    return os.str();
}

ImportSettings import_settings_from_json(const std::string& json) {
    Parser parser(json);
    const Parser::Node root = parser.parse_document();

    const Parser::Node* kind = find_member(root, "import_kind");
    if (!kind || kind->scalar != "dwfx") throw std::invalid_argument("настройки импорта DWFx: другой вид импорта");
    const Parser::Node* version = find_member(root, "version");
    if (!version || version->scalar != std::to_string(kFormatVersion)) {
        throw std::invalid_argument("настройки импорта DWFx: неподдержанная версия формата");
    }
    const Parser::Node* values = find_member(root, "values");
    if (!values || !values->is_object) throw std::invalid_argument("настройки импорта DWFx: нет объекта values");

    for (const auto& [name, unused] : root.members) {
        if (name != "import_kind" && name != "version" && name != "values") {
            throw std::invalid_argument("настройки импорта DWFx: неизвестное поле " + name);
        }
    }

    ImportSettings settings = import_defaults();
    for (const auto& [id, value] : values->members) {
        if (value.is_object) throw std::invalid_argument("настройки импорта DWFx: значение " + id + " не скалярное");
        set_import_property(settings, id, value.scalar);  // неизвестный id и диапазон проверяются здесь
    }

    const std::vector<std::string> problems = validate_import_settings(settings);
    if (!problems.empty()) throw std::invalid_argument("настройки импорта DWFx: " + problems.front());
    return settings;
}

ImportSettings load_import_settings(const std::string& path) {
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) return import_defaults();
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::invalid_argument("настройки импорта DWFx: не открыть " + path);
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return import_settings_from_json(buffer.str());
}

void save_import_settings(const std::string& path, const ImportSettings& settings) {
    const std::string temp = path + ".tmp";
    {
        std::ofstream out(temp, std::ios::binary | std::ios::trunc);
        if (!out) throw std::runtime_error("настройки импорта DWFx: не записать " + temp);
        out << import_settings_to_json(settings) << '\n';
        out.flush();
        if (!out) throw std::runtime_error("настройки импорта DWFx: ошибка записи " + temp);
    }
    std::error_code ec;
    std::filesystem::rename(temp, path, ec);
    if (ec) throw std::runtime_error("настройки импорта DWFx: не переименовать в " + path);
}

}  // namespace bendcore::dwfx
