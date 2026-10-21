// Параметры импорта DWFx: значения по умолчанию из заметок о проекте (спецификация), проверка диапазонов
// и описание свойств для панели свойств интерфейса. Один источник правды: таблица свойств определяет и
// описание (подписи, единицы, группы, диапазоны), и доступ по идентификатору (чтение и запись строкой).
#pragma once

#include <string>
#include <vector>

namespace bendcore::dwfx {

enum class RoundingMode { Off, Standard };          // «Выкл» / «Стандартный»
enum class ConflictPolicy { KeepBody };             // «оставить тело» (по умолчанию); остальные — позже
enum class ProfilePick { NearestR };                // «Ближайший по R» (по умолчанию)
enum class FilterPreset { TubesAndNeighborhood };   // «Трубы и окрестность» (по умолчанию)

struct ImportSettings {
    // Распознавание: сравнение, вырожденные гибы, допуски.
    double degenerate_bend_deg = 0.5;        // порог вырожденного гиба, °
    double compare_linear_mm = 0.1;          // допуск сравнения длин, мм
    double compare_angle_deg = 0.1;          // допуск сравнения углов, °
    double max_geometry_deviation_mm = 0.5;  // макс. отклонение геометрии, мм
    double exact_tolerance_mm = 0.01;        // допуск распознавания точных тел, мм
    double mesh_tolerance_mm = 0.1;          // допуск распознавания сетки, мм (абсолютный)
    double mesh_tolerance_rel_od = 0.005;    // допуск распознавания сетки, доля ∅ (берётся больший из двух)

    // Радиус: совпадение с метаданными и автоисправление.
    double radius_match_rel = 0.01;          // допуск совпадения радиуса, доля R
    double radius_match_min_mm = 0.5;        // допуск совпадения радиуса, не менее мм
    ConflictPolicy conflict_policy = ConflictPolicy::KeepBody;
    bool auto_fix_radius = false;            // автоисправление радиуса гиба (по умолчанию выключено)
    double auto_fix_max_radius_rel = 0.10;   // макс. разница радиусов для автоисправления, доля R
    ProfilePick profile_pick = ProfilePick::NearestR;

    // Порты и состояние после импорта.
    bool autoports = true;                   // автопорты на концах и соосных признаках
    double autoport_linear_mm = 0.1;         // допуск автопортов, мм
    double autoport_angle_deg = 1.0;         // допуск автопортов, °
    double new_port_reach_mm = 0.0;          // вылет нового порта, мм
    bool import_frozen = true;               // труба создаётся в режиме «Заморожена»

    // Зазор и окрестность.
    double clearance_mm = 5.0;               // зазор до окружения, не менее мм
    double neighborhood_min_od_factor = 1.0; // окрестность D = max(factor × ∅, зазор + запас)
    double neighborhood_margin_mm = 5.0;     // запас окрестности, мм (подтверждено пользователем: 5 мм)
    int contact_levels = 0;                  // уровни контактирующих тел

    // Фильтр импорта.
    FilterPreset filter_preset = FilterPreset::TubesAndNeighborhood;
    bool filter_two_phase = true;            // двухфазный импорт (основной режим для крупных сборок)
    bool auto_load_neighborhood = true;      // автодогрузка окрестности при выходе за область

    // Округление распознанной геометрии.
    RoundingMode rounding_exact = RoundingMode::Off;     // для точных тел
    RoundingMode rounding_mesh = RoundingMode::Standard; // для сетки DWF
    double round_length_mm = 1.0;            // шаг длин, мм
    double round_angle_deg = 0.5;            // шаг углов, °
    double round_align_deg = 0.5;            // выравнивание, °
    double round_diameter_wall_mm = 0.1;     // шаг ∅ и стенки, мм
};

// Описание свойства для панели свойств: подпись, единица, группа, диапазон, значение по умолчанию.
struct PropertyInfo {
    std::string id;           // стабильный идентификатор (snake_case)
    std::string label;        // подпись на русском
    std::string group;        // вкладка/группа в панели
    std::string kind;         // "number" | "bool" | "enum"
    std::string unit;         // единица (мм, °, доля R, пусто)
    std::string default_value;  // значение по умолчанию как текст
    double min = 0.0;         // для number
    double max = 0.0;         // для number
    std::vector<std::string> options;  // для enum: допустимые значения
    std::string description;  // пояснение (из заметок)
};

// Все свойства импорта в порядке показа.
const std::vector<PropertyInfo>& import_properties();

// Значения по умолчанию (из заметок о проекте).
ImportSettings import_defaults();

// Чтение свойства как текста (число — в простом виде, bool — "true"/"false", enum — значение).
// Исключение std::invalid_argument при неизвестном id.
std::string get_import_property(const ImportSettings& s, const std::string& id);

// Запись свойства из текста с проверкой типа и диапазона. Исключение std::invalid_argument при ошибке.
void set_import_property(ImportSettings& s, const std::string& id, const std::string& value);

// Список нарушений диапазонов и согласованности; пустой список — настройки допустимы.
std::vector<std::string> validate_import_settings(const ImportSettings& s);

// Описание свойств для интерфейса в JSON: {"import_kind":"dwfx","properties":[ объекты PropertyInfo ]}.
// Панель свойств показывает эти свойства при выборе вида импорта DWFx.
std::string import_properties_json();

}  // namespace bendcore::dwfx
