// Параметры импорта DWFx: значения по умолчанию из заметок, чтение и запись по идентификатору,
// проверка диапазонов и описание для интерфейса (JSON).
#include "bendcore/dwfx/import_settings.hpp"

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
    check(props.size() == 31, "описание: 31 свойство импорта (по заметкам)");
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
    check(json.find("ЗНАЧЕНИЕ НЕ ЗАДАНО В ЗАМЕТКАХ") != std::string::npos,
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

int main() {
    check_defaults_match_notes();
    check_schema();
    check_round_trip();
    check_errors();
    check_validation();
    if (failures == 0) std::printf("OK dwfx_settings\n");
    return failures == 0 ? 0 : 1;
}
