// Параметры импорта DWFx: значения по умолчанию из заметок, чтение и запись по идентификатору,
// проверка диапазонов и описание для интерфейса (JSON).
#include "bendcore/dwfx/import_settings.hpp"
#include "bendcore/dwfx/import_settings_io.hpp"

#include <cstdio>
#include <set>
#include <stdexcept>
#include <string>

using namespace bendcore::dwfx;

static int failures = 0;

static void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("FAIL: %s\n", what);
        ++failures;
    }
}

static bool throws(void (*fn)()) {
    try {
        fn();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

static void check_defaults_match_notes() {
    const ImportSettings d = import_defaults();
    check(d.degenerate_bend_deg == 0.5, "заметки: порог вырожденного гиба 0,5°");
    check(d.compare_linear_mm == 0.1 && d.compare_angle_deg == 0.1, "заметки: допуски сравнения 0,1 мм и 0,1°");
    check(d.max_geometry_deviation_mm == 0.5, "заметки: макс. отклонение геометрии 0,5 мм");
    check(d.exact_tolerance_mm == 0.01, "заметки: точные тела 0,01 мм");
    check(d.mesh_tolerance_mm == 0.1 && d.mesh_tolerance_rel_od == 0.005, "заметки: сетка 0,1 мм или 0,5 % ∅");
    check(d.radius_match_rel == 0.01 && d.radius_match_min_mm == 0.5, "заметки: совпадение радиуса 1 % R, не менее 0,5 мм");
    check(d.conflict_policy == ConflictPolicy::KeepBody, "заметки: конфликт радиуса — оставить тело");
    check(!d.auto_fix_radius, "заметки: автоисправление радиуса выключено");
    check(d.auto_fix_max_radius_rel == 0.10, "заметки: макс. разница радиусов 10 % R");
    check(d.profile_pick == ProfilePick::NearestR, "заметки: профиль «Ближайший по R»");
    check(d.autoport_linear_mm == 0.1 && d.autoport_angle_deg == 1.0, "заметки: автопорты 0,1 мм и 1°");
    check(d.new_port_reach_mm == 0.0, "заметки: вылет нового порта 0");
    check(d.clearance_mm == 5.0, "заметки: зазор 5 мм");
    check(d.neighborhood_margin_mm == 5.0, "подтверждено пользователем: запас окрестности 5 мм");
    check(d.filter_preset == FilterPreset::TubesAndNeighborhood && d.filter_type_bodies && !d.filter_type_surfaces,
          "фильтр 9.8: пресет «Трубы и окрестность», тела включены, поверхности выключены");
    check(d.neighborhood_unit == NeighborhoodUnit::Body, "фильтр 9.8: отбор по телам");
    check(d.neighborhood_min_od_factor == 1.0, "заметки: окрестность max(1×∅, ...)");
    check(d.contact_levels == 0, "заметки: контактирующие тела 0 уровней");
    check(d.filter_preset == FilterPreset::TubesAndNeighborhood, "заметки: пресет «Трубы и окрестность»");
    check(d.auto_load_neighborhood, "заметки: автодогрузка окрестности включена");
    check(d.rounding_exact == RoundingMode::Off && d.rounding_mesh == RoundingMode::Standard,
          "заметки: округление «Выкл» для точных тел, «Стандартный» для сетки DWF");
    check(d.round_length_mm == 1.0 && d.round_angle_deg == 0.5 && d.round_align_deg == 0.5 &&
              d.round_diameter_wall_mm == 0.1,
          "заметки: шаги округления 1 мм, 0,5°, 0,5°, 0,1 мм");
    check(d.import_frozen, "заметки: трубы из импорта — «Заморожена»");
}

static void check_schema() {
    const auto& props = import_properties();
    check(props.size() == 46, "описание: 46 свойств импорта (31 по заметкам и параметрам + 15 фильтра 9.8)");
    std::set<std::string> ids;
    for (const auto& p : props) ids.insert(p.id);
    check(ids.size() == props.size(), "описание: идентификаторы уникальны");

    const std::string json = import_properties_json();
    check(json.find("\"import_kind\":\"dwfx\"") != std::string::npos, "JSON: вид импорта dwfx");
    check(json.back() == '}', "JSON: объект");
    check(json.find("\"id\":\"auto_fix_radius\"") != std::string::npos, "JSON: есть auto_fix_radius");
    check(json.find("\"id\":\"auto_fix_radius\",\"label\"") != std::string::npos, "JSON: порядок полей");
    check(json.find("\"default\":\"false\"") != std::string::npos, "JSON: логическое значение по умолчанию");
    check(json.find("\"options\":[\"off\",\"standard\"]") != std::string::npos, "JSON: варианты округления");
    check(json.find("в заметках не указан") != std::string::npos,
          "JSON: отмечено значение, не заданное в заметках");
}

static void check_round_trip() {
    ImportSettings s = import_defaults();
    for (const auto& p : import_properties()) {
        const std::string value = get_import_property(s, p.id);
        check(value == p.default_value, ("по умолчанию совпадает с описанием: " + p.id).c_str());
        set_import_property(s, p.id, value);
        check(get_import_property(s, p.id) == value, ("запись и чтение: " + p.id).c_str());
    }
    set_import_property(s, "compare_linear_mm", "0,25");
    check(s.compare_linear_mm == 0.25, "число с запятой принимается");
    set_import_property(s, "auto_fix_radius", "true");
    check(s.auto_fix_radius, "логическое значение true");
    set_import_property(s, "rounding_mesh", "off");
    check(s.rounding_mesh == RoundingMode::Off, "вариант округления off");
}

static void bad_number() { ImportSettings s; set_import_property(s, "compare_linear_mm", "abc"); }
static void bad_bool() { ImportSettings s; set_import_property(s, "auto_fix_radius", "maybe"); }
static void bad_enum() { ImportSettings s; set_import_property(s, "rounding_mesh", "fine"); }
static void unknown_id() { ImportSettings s; set_import_property(s, "no_such_property", "1"); }

static void check_errors() {
    check(throws(bad_number), "нечисловое значение отклоняется");
    check(throws(bad_bool), "неверное логическое значение отклоняется");
    check(throws(bad_enum), "недопустимый вариант отклоняется");
    check(throws(unknown_id), "неизвестный идентификатор отклоняется");
}

static void check_validation() {
    check(validate_import_settings(import_defaults()).empty(), "значения по умолчанию допустимы");

    ImportSettings s = import_defaults();
    s.clearance_mm = 4.0;
    check(!validate_import_settings(s).empty(), "зазор меньше 5 мм отклоняется (заметки)");

    s = import_defaults();
    s.radius_match_rel = 0.5;
    check(!validate_import_settings(s).empty(), "допуск радиуса больше 20 % отклоняется");

    s = import_defaults();
    s.round_length_mm = 0.0;
    check(!validate_import_settings(s).empty(), "нулевой шаг округления отклоняется");
}

// Раздел «Импорт → DWFx»: сохранение и загрузка пользовательских значений.
static void expect_bad_json(const std::string& json, const char* what) {
    try {
        import_settings_from_json(json);
        check(false, what);
    } catch (const std::invalid_argument&) {
    }
}

static void check_settings_io() {
    const std::string defaults_json = import_settings_to_json(import_defaults());
    check(defaults_json.find("\"import_kind\":\"dwfx\"") != std::string::npos, "JSON настроек: вид dwfx");
    check(defaults_json.find("\"neighborhood_margin_mm\":\"5\"") != std::string::npos,
          "JSON настроек: запас окрестности 5 мм");

    const ImportSettings round = import_settings_from_json(defaults_json);
    for (const auto& p : import_properties()) {
        check(get_import_property(round, p.id) == get_import_property(import_defaults(), p.id),
              ("JSON настроек: круговой проход по умолчанию " + p.id).c_str());
    }

    ImportSettings custom = import_defaults();
    set_import_property(custom, "compare_linear_mm", "0.25");
    set_import_property(custom, "rounding_mesh", "off");
    set_import_property(custom, "neighborhood_margin_mm", "7.5");
    const std::string path = "dwfx_settings_test.json";
    save_import_settings(path, custom);
    const ImportSettings loaded = load_import_settings(path);
    check(loaded.compare_linear_mm == 0.25, "файл: значение 0,25 сохранено и загружено");
    check(loaded.rounding_mesh == RoundingMode::Off, "файл: округление off сохранено");
    check(loaded.neighborhood_margin_mm == 7.5, "файл: запас 7,5 мм сохранён");
    std::remove(path.c_str());

    const ImportSettings absent = load_import_settings("no_such_dwfx_settings_file.json");
    check(get_import_property(absent, "neighborhood_margin_mm") == "5", "нет файла: значения по умолчанию");

    // Отсутствующие ключи — значения по умолчанию; неизвестные поля и значения — отказ.
    const ImportSettings partial = import_settings_from_json(
        "{\"import_kind\":\"dwfx\",\"version\":1,\"values\":{\"clearance_mm\":\"6\"}}");
    check(partial.clearance_mm == 6.0 && partial.compare_linear_mm == 0.1, "частичный файл: остальное по умолчанию");

    expect_bad_json("{\"import_kind\":\"step\",\"version\":1,\"values\":{}}", "неверный вид импорта отвергается");
    expect_bad_json("{\"import_kind\":\"dwfx\",\"version\":2,\"values\":{}}", "неверная версия отвергается");
    expect_bad_json("{\"import_kind\":\"dwfx\",\"version\":1,\"values\":{\"no_such\":\"1\"}}",
                    "неизвестный идентификатор отвергается");
    expect_bad_json("{\"import_kind\":\"dwfx\",\"version\":1,\"values\":{\"clearance_mm\":\"abc\"}}",
                    "нечисловое значение отвергается");
    expect_bad_json("{\"import_kind\":\"dwfx\",\"version\":1,\"values\":{\"clearance_mm\":\"4\"}}",
                    "зазор меньше 5 мм отвергается");
    expect_bad_json("{\"import_kind\":\"dwfx\",\"version\":1,\"values\":{},\"extra\":1}",
                    "неизвестное поле верхнего уровня отвергается");
    // Текстовые фильтры с кавычками и обратной косой черте проходят круговой проход.
    ImportSettings masked = import_defaults();
    set_import_property(masked, "filter_name_mask", "Труба \"A\"\\*");
    set_import_property(masked, "filter_layers", "1,2");
    set_import_property(masked, "filter_preset", "all_simplified");
    set_import_property(masked, "neighborhood_unit", "component");
    const ImportSettings masked_back = import_settings_from_json(import_settings_to_json(masked));
    check(masked_back.filter_name_mask == "Труба \"A\"\\*", "фильтр: маска с кавычками и \\ сохраняется");
    check(masked_back.filter_preset == FilterPreset::AllSimplified && masked_back.filter_layers == "1,2" &&
              masked_back.neighborhood_unit == NeighborhoodUnit::Component,
          "фильтр: пресет, слои и единица окрестности сохраняются");
    expect_bad_json("{\"import_kind\":\"dwfx\",\"version\":1,\"values\":{\"filter_min_bbox_mm\":\"-1\"}}",
                    "отрицательный минимальный габарит отвергается");
    expect_bad_json("{\"import_kind\":\"dwfx\",\"version\":1,\"values\":{\"filter_preset\":\"all\"}}",
                    "неизвестный пресет фильтра отвергается");

    expect_bad_json("{\"import_kind\":\"dwfx\",\"version\":1,\"values\":{", "обрезанный JSON отвергается");
}

int main() {
    check_defaults_match_notes();
    check_schema();
    check_round_trip();
    check_errors();
    check_validation();
    check_settings_io();
    if (failures == 0) std::printf("OK dwfx_settings\n");
    return failures == 0 ? 0 : 1;
}
