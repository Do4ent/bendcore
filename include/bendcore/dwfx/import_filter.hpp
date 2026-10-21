// Фильтр импорта DWFx (9.8): решает, какие компоненты загружаются, по правилам из ImportSettings,
// ручным включениям и исключениям и окрестности вокруг труб. Вход — записи компонентов (имя, слой, тип,
// видимость, габарит, точки поверхности) и осевые труб-опор; сам разбор структуры файла здесь не делается.
//
// Порядок правил для компонента:
//   ручное решение → порт (всегда) → видимость → тип → маска имени → слой → минимальный габарит →
//   пресет: «Только трубы» (трубы), «Трубы и окрестность» (трубы или окрестность), «Всё с упрощением» (всё, что
//   прошло правила). Окрестность — коридор ∅/2 + D вокруг осевых опор, D = max(кратность × ∅, зазор + запас).
//   Единица отбора «компонент» — решение общее для группы (поле group_id).
#pragma once

#include "bendcore/dwfx/import_settings.hpp"

#include <array>
#include <cstddef>
#include <map>
#include <string>
#include <vector>

namespace bendcore::dwfx {

using Point3 = std::array<double, 3>;

enum class FilterObjectType { Body, Surface, Curve, Annotation };

struct FilterComponent {
    std::string id;
    std::string name;
    std::string layer;
    FilterObjectType type = FilterObjectType::Body;
    std::string group_id;                 // родительский компонент; пусто — сам по себе
    bool visible = true;
    bool is_tube = false;                 // распознана как труба
    bool is_port = false;                 // тело порта — включается всегда
    std::array<double, 6> bbox{};         // min xyz, max xyz, мм
    std::vector<Point3> samples;          // точки поверхности (вершины сетки); пусто — углы и центр габарита
};

enum class SupportSource { File, Target };  // опорная труба из файла или из целевой сборки

struct FilterSupportAxis {
    std::string tube_id;
    SupportSource source = SupportSource::File;
    std::vector<Point3> polyline;         // осевая трубы (путь с дугами, точками поворота)
    double od_mm = 0.0;                   // наружный диаметр
};

struct FilterDecision {
    std::string id;
    bool include = false;
    std::string reason;                   // код причины: manual_include, port, hidden, type, name_mask, layer, min_size,
                                          // not_tube, neighborhood, outside_neighborhood, tube, rules, group
};

struct FilterResult {
    std::vector<FilterDecision> decisions;      // в порядке входа
    std::vector<std::string> not_applied;       // правила из настроек, которые эта версия не применяет
    std::size_t included = 0;
};

// Маска имени: «*» и «?», без учёта регистра (только ASCII; для кириллицы регистр не сворачивается).
bool glob_match(const std::string& pattern, const std::string& text);

// Расстояние от точки до ломаной (минимум по отрезкам), мм.
double distance_point_polyline(const Point3& point, const std::vector<Point3>& polyline);

// Применение фильтра. manual: id → true (включить) / false (исключить), сильнее правил.
FilterResult apply_import_filter(const ImportSettings& settings, const std::vector<FilterComponent>& components,
                                 const std::vector<FilterSupportAxis>& supports,
                                 const std::map<std::string, bool>& manual);

}  // namespace bendcore::dwfx
