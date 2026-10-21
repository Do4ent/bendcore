// Параметры импорта DWFx. Значения и диапазоны — по заметкам о проекте (спецификация).
#include "bendcore/dwfx/import_settings.hpp"

#include <cmath>
#include <cstdlib>
#include <functional>
#include <locale>
#include <sstream>
#include <type_traits>
#include <stdexcept>
#include <utility>

namespace bendcore::dwfx {
namespace {

std::string format_number(double v) {
    std::ostringstream os;
    os.imbue(std::locale::classic());
    os.precision(15);
    os << v;
    return os.str();
}

// Строгое чтение числа: вся строка должна быть числом; запятая как десятичный разделитель допускается.
double parse_number(const std::string& text) {
    std::string t = text;
    for (char& c : t) {
        if (c == ',') c = '.';
    }
    char* end = nullptr;
    const double v = std::strtod(t.c_str(), &end);
    if (t.empty() || end == nullptr || *end != '\0' || !std::isfinite(v)) {
        throw std::invalid_argument("ожидалось число: '" + text + "'");
    }
    return v;
}

bool parse_bool(const std::string& text) {
    if (text == "true" || text == "1") return true;
    if (text == "false" || text == "0") return false;
    throw std::invalid_argument("ожидалось true или false: '" + text + "'");
}

template <typename E>
E parse_enum(const std::string& text, const std::vector<std::pair<std::string, E>>& table) {
    for (const auto& [name, value] : table) {
        if (name == text) return value;
    }
    throw std::invalid_argument("недопустимое значение: '" + text + "'");
}

template <typename E>
std::string enum_name(E value, const std::vector<std::pair<std::string, E>>& table) {
    for (const auto& [name, v] : table) {
        if (v == value) return name;
    }
    return "";
}

const std::vector<std::pair<std::string, RoundingMode>> kRounding = {{"off", RoundingMode::Off},
                                                                       {"standard", RoundingMode::Standard}};
const std::vector<std::pair<std::string, ConflictPolicy>> kConflict = {{"keep_body", ConflictPolicy::KeepBody}};
const std::vector<std::pair<std::string, ProfilePick>> kProfile = {{"nearest_r", ProfilePick::NearestR}};
const std::vector<std::pair<std::string, FilterPreset>> kPreset = {
    {"tubes_only", FilterPreset::TubesOnly},
    {"tubes_and_neighborhood", FilterPreset::TubesAndNeighborhood},
    {"all_simplified", FilterPreset::AllSimplified}};
const std::vector<std::pair<std::string, NeighborhoodUnit>> kUnit = {{"body", NeighborhoodUnit::Body},
                                                                      {"component", NeighborhoodUnit::Component}};

struct Binding {
    PropertyInfo info;
    std::function<std::string(const ImportSettings&)> get;
    std::function<void(ImportSettings&, const std::string&)> set;
};

PropertyInfo number(std::string id, std::string label, std::string group, std::string unit, double min, double max,
                    std::string description) {
    PropertyInfo p;
    p.id = std::move(id);
    p.label = std::move(label);
    p.group = std::move(group);
    p.kind = "number";
    p.unit = std::move(unit);
    p.min = min;
    p.max = max;
    p.description = std::move(description);
    return p;
}

PropertyInfo boolean(std::string id, std::string label, std::string group, std::string description) {
    PropertyInfo p;
    p.id = std::move(id);
    p.label = std::move(label);
    p.group = std::move(group);
    p.kind = "bool";
    p.description = std::move(description);
    return p;
}

PropertyInfo choice(std::string id, std::string label, std::string group, std::vector<std::string> options,
                    std::string description) {
    PropertyInfo p;
    p.id = std::move(id);
    p.label = std::move(label);
    p.group = std::move(group);
    p.kind = "enum";
    p.options = std::move(options);
    p.description = std::move(description);
    return p;
}

PropertyInfo text(std::string id, std::string label, std::string group, std::string description) {
    PropertyInfo p;
    p.id = std::move(id);
    p.label = std::move(label);
    p.group = std::move(group);
    p.kind = "text";
    p.description = std::move(description);
    return p;
}

// Текстовое свойство (маска, список слоёв).
Binding str(PropertyInfo info, std::string ImportSettings::*field) {
    Binding b;
    b.info = std::move(info);
    b.get = [field](const ImportSettings& s) { return s.*field; };
    b.set = [field](ImportSettings& s, const std::string& v) {
        if (v.find('\n') != std::string::npos || v.find('\r') != std::string::npos) {
            throw std::invalid_argument("текст не должен содержать переводов строк");
        }
        s.*field = v;
    };
    return b;
}

// Числовое свойство: доступ к полю структуры по указателю на член.
template <typename T>
Binding num(PropertyInfo info, T ImportSettings::*field) {
    Binding b;
    b.info = std::move(info);
    b.get = [field](const ImportSettings& s) { return format_number(static_cast<double>(s.*field)); };
    b.set = [field](ImportSettings& s, const std::string& v) {
        const double x = parse_number(v);
        if constexpr (std::is_integral_v<T>) {
            if (x != std::floor(x)) throw std::invalid_argument("ожидалось целое число: '" + v + "'");
        }
        s.*field = static_cast<T>(x);
    };
    return b;
}

Binding flag(PropertyInfo info, bool ImportSettings::*field) {
    Binding b;
    b.info = std::move(info);
    b.get = [field](const ImportSettings& s) { return std::string((s.*field) ? "true" : "false"); };
    b.set = [field](ImportSettings& s, const std::string& v) { s.*field = parse_bool(v); };
    return b;
}

template <typename E>
Binding pick(PropertyInfo info, E ImportSettings::*field, std::vector<std::pair<std::string, E>> table) {
    Binding b;
    b.info = std::move(info);
    b.get = [field, table](const ImportSettings& s) { return enum_name(s.*field, table); };
    b.set = [field, table](ImportSettings& s, const std::string& v) { s.*field = parse_enum(v, table); };
    return b;
}

const std::vector<Binding>& bindings() {
    static const std::vector<Binding> table = [] {
        std::vector<Binding> t;
        t.push_back(num(number("degenerate_bend_deg", "Порог вырожденного гиба", "Распознавание", "°", 0.01, 5.0,
                               "Гиб меньше порога считается прямым участком (заметки: 0,5°)."),
                        &ImportSettings::degenerate_bend_deg));
        t.push_back(num(number("compare_linear_mm", "Допуск сравнения длин", "Распознавание", "мм", 0.0, 1.0,
                               "Допуск сравнения длин при проверках (заметки: 0,1 мм)."),
                        &ImportSettings::compare_linear_mm));
        t.push_back(num(number("compare_angle_deg", "Допуск сравнения углов", "Распознавание", "°", 0.0, 5.0,
                               "Допуск сравнения углов при проверках (заметки: 0,1°)."),
                        &ImportSettings::compare_angle_deg));
        t.push_back(num(number("max_geometry_deviation_mm", "Макс. отклонение геометрии", "Распознавание", "мм", 0.0,
                               5.0, "Предел отклонения распознанной геометрии от исходной (заметки: 0,5 мм)."),
                        &ImportSettings::max_geometry_deviation_mm));
        t.push_back(num(number("exact_tolerance_mm", "Допуск распознавания точных тел", "Распознавание", "мм", 0.0,
                               1.0, "Для точных тел (заметки: 0,01 мм)."),
                        &ImportSettings::exact_tolerance_mm));
        t.push_back(num(number("mesh_tolerance_mm", "Допуск распознавания сетки, абсолютный", "Распознавание", "мм",
                               0.0, 2.0, "Для сетки DWF (заметки: 0,1 мм или 0,5 % ∅). Берётся больший из двух."),
                        &ImportSettings::mesh_tolerance_mm));
        t.push_back(num(number("mesh_tolerance_rel_od", "Допуск распознавания сетки, доля ∅", "Распознавание", "доля ∅",
                               0.0, 0.05, "Для сетки DWF: 0,5 % ∅ (заметки). Берётся больший из двух."),
                        &ImportSettings::mesh_tolerance_rel_od));

        t.push_back(num(number("radius_match_rel", "Допуск совпадения радиуса", "Радиус", "доля R", 0.0, 0.2,
                               "При импорте: 1 % R (заметки)."),
                        &ImportSettings::radius_match_rel));
        t.push_back(num(number("radius_match_min_mm", "Допуск совпадения радиуса, не менее", "Радиус", "мм", 0.0, 5.0,
                               "Нижняя граница допуска радиуса (заметки: 0,5 мм)."),
                        &ImportSettings::radius_match_min_mm));
        t.push_back(pick(choice("radius_conflict_policy", "Конфликт радиуса", "Радиус", {"keep_body"},
                                "Вне допуска по умолчанию «оставить тело» без блокировки импорта."),
                         &ImportSettings::conflict_policy, kConflict));
        t.push_back(flag(boolean("auto_fix_radius", "Автоисправление радиуса гиба", "Радиус",
                                 "По умолчанию выключено (заметки)."),
                         &ImportSettings::auto_fix_radius));
        t.push_back(num(number("auto_fix_max_radius_rel", "Макс. разница радиусов для автоисправления", "Радиус",
                               "доля R", 0.0, 1.0, "По умолчанию 10 % R (заметки)."),
                        &ImportSettings::auto_fix_max_radius_rel));
        t.push_back(pick(choice("profile_pick", "Выбор профиля при автоисправлении", "Радиус", {"nearest_r"},
                                "По умолчанию «Ближайший по R» (заметки)."),
                         &ImportSettings::profile_pick, kProfile));

        t.push_back(flag(boolean("autoports", "Автопорты при импорте", "Порты",
                                 "Поиск соосного признака окружения, один порт на конец (заметки)."),
                         &ImportSettings::autoports));
        t.push_back(num(number("autoport_linear_mm", "Допуск автопортов, линейный", "Порты", "мм", 0.0, 1.0,
                               "Заметки: 0,1 мм."),
                        &ImportSettings::autoport_linear_mm));
        t.push_back(num(number("autoport_angle_deg", "Допуск автопортов, угловой", "Порты", "°", 0.0, 5.0,
                               "Заметки: 1°."),
                        &ImportSettings::autoport_angle_deg));
        t.push_back(num(number("new_port_reach_mm", "Вылет нового порта", "Порты", "мм", 0.0, 100.0,
                               "По умолчанию 0 (заметки)."),
                        &ImportSettings::new_port_reach_mm));
        t.push_back(flag(boolean("import_frozen", "Создавать трубу «Заморожена»", "Порты",
                                 "Распознанные трубы в сборке создаются замороженными (заметки)."),
                         &ImportSettings::import_frozen));

        t.push_back(num(number("clearance_mm", "Зазор до окружения, не менее", "Окрестность", "мм", 5.0, 100.0,
                               "Заметки: не менее 5 мм; тот же зазор между трубами сборки."),
                        &ImportSettings::clearance_mm));
        t.push_back(num(number("neighborhood_min_od_factor", "Окрестность: кратность ∅", "Окрестность", "×∅", 0.1,
                               10.0, "D = max(кратность × ∅, зазор + запас) (заметки: 1×∅)."),
                        &ImportSettings::neighborhood_min_od_factor));
        t.push_back(num(number("neighborhood_margin_mm", "Окрестность: запас", "Окрестность", "мм", 0.0, 100.0,
                               "Запас сверх зазора (подтверждено пользователем: 5 мм; в заметках не указан)."),
                        &ImportSettings::neighborhood_margin_mm));
        t.push_back(num(number("contact_levels", "Уровни контактирующих тел", "Окрестность", "уровень", 0.0, 10.0,
                               "Заметки: 0."),
                        &ImportSettings::contact_levels));

        t.push_back(pick(choice("filter_preset", "Пресет назначения", "Фильтр",
                                {"tubes_only", "tubes_and_neighborhood", "all_simplified"},
                                "Пресеты 9.8: «Только трубы», «Трубы и окрестность» (по умолчанию, заметки), «Всё с упрощением»."),
                         &ImportSettings::filter_preset, kPreset));
        t.push_back(flag(boolean("filter_two_phase", "Двухфазный импорт", "Фильтр",
                                 "Основной режим для крупных сборок (заметки). Значение по умолчанию уточнить."),
                         &ImportSettings::filter_two_phase));
        t.push_back(flag(boolean("auto_load_neighborhood", "Автодогрузка окрестности", "Фильтр",
                                 "Включена по умолчанию (заметки)."),
                         &ImportSettings::auto_load_neighborhood));
        t.push_back(flag(boolean("filter_only_visible", "Структура: только видимые", "Фильтр: структура",
                                 "Галочки на компонентах и ветвях, учитываются только видимые. По умолчанию выключено (в заметках не указано)."),
                         &ImportSettings::filter_only_visible));
        t.push_back(str(text("filter_name_mask", "Свойства: маска имён", "Фильтр: свойства и размер",
                             "Маска имён компонентов (например, «Труба*»). Пусто — все компоненты (в заметках не указано)."),
                        &ImportSettings::filter_name_mask));
        t.push_back(str(text("filter_layers", "Свойства: слои", "Фильтр: свойства и размер",
                             "Слои через запятую. Пусто — все слои (в заметках не указано)."),
                        &ImportSettings::filter_layers));
        t.push_back(flag(boolean("filter_type_bodies", "Тип: тела", "Фильтр: свойства и размер",
                                 "Включать тела. По умолчанию включено (в заметках не указано)."),
                         &ImportSettings::filter_type_bodies));
        t.push_back(flag(boolean("filter_type_surfaces", "Тип: поверхности", "Фильтр: свойства и размер",
                                 "Включать поверхности. По умолчанию выключено (в заметках не указано)."),
                         &ImportSettings::filter_type_surfaces));
        t.push_back(flag(boolean("filter_type_curves", "Тип: кривые", "Фильтр: свойства и размер",
                                 "Включать кривые. По умолчанию выключено (в заметках не указано)."),
                         &ImportSettings::filter_type_curves));
        t.push_back(flag(boolean("filter_type_annotations", "Тип: аннотации", "Фильтр: свойства и размер",
                                 "Включать аннотации. По умолчанию выключено (в заметках не указано)."),
                         &ImportSettings::filter_type_annotations));
        t.push_back(num(number("filter_min_bbox_mm", "Минимальный габарит компонента", "Фильтр: свойства и размер",
                               "мм", 0.0, 1000.0, "0 — без ограничения (в заметках не указано)."),
                        &ImportSettings::filter_min_bbox_mm));
        t.push_back(flag(boolean("filter_simplify_outer_shell", "Упрощение: внешняя оболочка", "Фильтр: упрощение",
                                 "Внешняя оболочка вместо внутренностей. По умолчанию выключено (в заметках не указано)."),
                         &ImportSettings::filter_simplify_outer_shell));
        t.push_back(num(number("filter_simplify_remove_small_mm", "Упрощение: удалять мелкие элементы", "Фильтр: упрощение",
                               "мм", 0.0, 100.0, "Размер, меньше которого элемент удаляется; 0 — выключено (в заметках не указано)."),
                        &ImportSettings::filter_simplify_remove_small_mm));
        t.push_back(flag(boolean("filter_simplify_decimate", "Упрощение: децимация сетки DWF", "Фильтр: упрощение",
                                 "Допуск децимации меньше допуска распознавания. По умолчанию выключено (в заметках не указано)."),
                         &ImportSettings::filter_simplify_decimate));
        t.push_back(flag(boolean("filter_simplify_replace_bbox", "Упрощение: габаритное тело", "Фильтр: упрощение",
                                 "Замена детали габаритным телом. По умолчанию выключено (в заметках не указано)."),
                         &ImportSettings::filter_simplify_replace_bbox));
        t.push_back(pick(choice("neighborhood_unit", "Окрестность: единица отбора", "Окрестность", {"body", "component"},
                                "Тело (по умолчанию) или компонент целиком (9.8)."),
                         &ImportSettings::neighborhood_unit, kUnit));
        t.push_back(flag(boolean("neighborhood_support_file_tubes", "Опора: трубы файла", "Окрестность",
                                 "Трубы файла, отмеченные по имени, слою или вручную. По умолчанию включено (в заметках не указано)."),
                         &ImportSettings::neighborhood_support_file_tubes));
        t.push_back(flag(boolean("neighborhood_support_target_tubes", "Опора: трубы сборки", "Окрестность",
                                 "Существующие трубы, пути и порты целевой сборки. По умолчанию включено (в заметках не указано)."),
                         &ImportSettings::neighborhood_support_target_tubes));

        t.push_back(pick(choice("rounding_exact", "Округление: точные тела", "Округление", {"off", "standard"},
                                "По умолчанию «Выкл» (заметки)."),
                         &ImportSettings::rounding_exact, kRounding));
        t.push_back(pick(choice("rounding_mesh", "Округление: сетка DWF", "Округление", {"off", "standard"},
                                "По умолчанию «Стандартный» (заметки)."),
                         &ImportSettings::rounding_mesh, kRounding));
        t.push_back(num(number("round_length_mm", "Шаг длин", "Округление", "мм", 0.01, 10.0, "Заметки: 1 мм."),
                        &ImportSettings::round_length_mm));
        t.push_back(num(number("round_angle_deg", "Шаг углов", "Округление", "°", 0.01, 10.0, "Заметки: 0,5°."),
                        &ImportSettings::round_angle_deg));
        t.push_back(num(number("round_align_deg", "Шаг выравнивания", "Округление", "°", 0.01, 10.0,
                               "Заметки: 0,5°."),
                        &ImportSettings::round_align_deg));
        t.push_back(num(number("round_diameter_wall_mm", "Шаг ∅ и стенки", "Округление", "мм", 0.01, 1.0,
                               "Заметки: 0,1 мм."),
                        &ImportSettings::round_diameter_wall_mm));

        // Значения по умолчанию из структуры — единственный источник.
        const ImportSettings defaults;
        for (auto& b : t) b.info.default_value = b.get(defaults);
        return t;
    }();
    return table;
}

const Binding& find(const std::string& id) {
    for (const auto& b : bindings()) {
        if (b.info.id == id) return b;
    }
    throw std::invalid_argument("неизвестное свойство импорта: '" + id + "'");
}

std::string json_escape(const std::string& s) {
    std::string out;
    for (char c : s) {
        if (c == '"' || c == '\\') out.push_back('\\');
        out.push_back(c);
    }
    return out;
}

}  // namespace

const std::vector<PropertyInfo>& import_properties() {
    static const std::vector<PropertyInfo> infos = [] {
        std::vector<PropertyInfo> v;
        for (const auto& b : bindings()) v.push_back(b.info);
        return v;
    }();
    return infos;
}

ImportSettings import_defaults() { return ImportSettings{}; }

std::string get_import_property(const ImportSettings& s, const std::string& id) { return find(id).get(s); }

void set_import_property(ImportSettings& s, const std::string& id, const std::string& value) {
    find(id).set(s, value);
}

std::vector<std::string> validate_import_settings(const ImportSettings& s) {
    std::vector<std::string> errors;
    auto need = [&](bool ok, const char* id, const char* what) {
        if (!ok) errors.push_back(std::string(id) + ": " + what);
    };
    need(s.degenerate_bend_deg > 0.0 && s.degenerate_bend_deg <= 5.0, "degenerate_bend_deg", "в пределах (0; 5]");
    need(s.compare_linear_mm >= 0.0, "compare_linear_mm", "не меньше 0");
    need(s.compare_angle_deg >= 0.0, "compare_angle_deg", "не меньше 0");
    need(s.max_geometry_deviation_mm > 0.0, "max_geometry_deviation_mm", "больше 0");
    need(s.exact_tolerance_mm > 0.0, "exact_tolerance_mm", "больше 0");
    need(s.mesh_tolerance_mm > 0.0, "mesh_tolerance_mm", "больше 0");
    need(s.mesh_tolerance_rel_od >= 0.0 && s.mesh_tolerance_rel_od <= 0.05, "mesh_tolerance_rel_od", "в пределах [0; 0,05]");
    need(s.radius_match_rel >= 0.0 && s.radius_match_rel <= 0.2, "radius_match_rel", "в пределах [0; 0,2]");
    need(s.radius_match_min_mm >= 0.0, "radius_match_min_mm", "не меньше 0");
    need(s.auto_fix_max_radius_rel > 0.0 && s.auto_fix_max_radius_rel <= 1.0, "auto_fix_max_radius_rel", "в пределах (0; 1]");
    need(s.autoport_linear_mm > 0.0, "autoport_linear_mm", "больше 0");
    need(s.autoport_angle_deg > 0.0, "autoport_angle_deg", "больше 0");
    need(s.new_port_reach_mm >= 0.0, "new_port_reach_mm", "не меньше 0");
    need(s.clearance_mm >= 5.0, "clearance_mm", "не менее 5 мм (заметки)");
    need(s.neighborhood_min_od_factor > 0.0, "neighborhood_min_od_factor", "больше 0");
    need(s.neighborhood_margin_mm >= 0.0, "neighborhood_margin_mm", "не меньше 0");
    need(s.contact_levels >= 0, "contact_levels", "не меньше 0");
    need(s.filter_min_bbox_mm >= 0.0, "filter_min_bbox_mm", "не меньше 0");
    need(s.filter_simplify_remove_small_mm >= 0.0, "filter_simplify_remove_small_mm", "не меньше 0");
    need(s.round_length_mm > 0.0, "round_length_mm", "больше 0");
    need(s.round_angle_deg > 0.0, "round_angle_deg", "больше 0");
    need(s.round_align_deg > 0.0, "round_align_deg", "больше 0");
    need(s.round_diameter_wall_mm > 0.0, "round_diameter_wall_mm", "больше 0");
    return errors;
}

std::string import_properties_json() {
    std::ostringstream os;
    os.imbue(std::locale::classic());
    os << "{\"import_kind\":\"dwfx\",\"properties\":[";
    bool first = true;
    for (const auto& p : import_properties()) {
        if (!first) os << ",";
        first = false;
        os << "{\"id\":\"" << json_escape(p.id) << "\",\"label\":\"" << json_escape(p.label) << "\",\"group\":\""
           << json_escape(p.group) << "\",\"kind\":\"" << p.kind << "\",\"unit\":\"" << json_escape(p.unit)
           << "\",\"default\":\"" << json_escape(p.default_value) << "\"";
        if (p.kind == "number") os << ",\"min\":" << format_number(p.min) << ",\"max\":" << format_number(p.max);
        if (p.kind == "enum") {
            os << ",\"options\":[";
            for (std::size_t i = 0; i < p.options.size(); ++i) {
                if (i) os << ",";
                os << "\"" << json_escape(p.options[i]) << "\"";
            }
            os << "]";
        }
        os << ",\"description\":\"" << json_escape(p.description) << "\"}";
    }
    os << "]}";
    return os.str();
}

}  // namespace bendcore::dwfx
