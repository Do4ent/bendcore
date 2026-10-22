// Данные для показа сборки в 3D-окне (этап 4 фазы 23). Без OCCT и без графики:
// модуль готовит описание сцены (оси труб, состояния, значки портов, метки зазора, «показать всё»),
// окно рисует его само. Состояния названы как в UI (python/ui_model.py, State): norm, warn, err_geom,
// err_tech, mark_cross, mark_gap, info.
#pragma once

#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "bendcore/positioning.hpp"
#include "bendcore/ports.hpp"
#include "bendcore/tubegeom.hpp"
#include "bendcore/vec3.hpp"

namespace bendcore {

// Одно состояние узла по списку замечаний — самое тяжёлое: err_geom > err_tech > mark_cross > mark_gap > warn > info > norm.
// Порядок тяжести выбран здесь и в спецификации не задан. Классы замечаний: geometric -> err_geom,
// technological -> err_tech, mark_cross, mark_gap, warning -> warn, info и suggestion -> info;
// refused (порт занят) и неизвестные классы -> warn; ref_lost состояния не меняет (это признак, см. flags_from_issues).
std::string state_from_issues(const std::vector<Issue>& issues);

// Признаки узла по замечаниям: «ref_lost» — ссылка потеряна (значок разорванной цепи).
std::vector<std::string> flags_from_issues(const std::vector<Issue>& issues);

// Ограничивающий объём для «показать всё» и «показать выбранное».
struct ViewFit {
    Vec3 lo{};
    Vec3 hi{};
    Vec3 center{};
    double radius = 0.0;   // половина диагонали коробки, мм
};

// Коробка вокруг точек, расширенная на pad по каждой оси; пустой набор — nullopt.
std::optional<ViewFit> fit_points(const std::vector<Vec3>& points, double pad = 0.0);

struct SceneTubeInput {
    PlacedTube tube;
    std::vector<Issue> issues;   // замечания трубы (геометрические, технологические и т. д.); зазоры добавит build_scene
};

struct SceneTube {
    std::string id;
    std::vector<Vec3> points;    // ось в сборке
    double od = 0.0;
    std::string state = "norm";
    std::vector<std::string> flags;
    std::vector<Issue> issues;   // входные замечания и пометки зазора, затрагивающие эту трубу
    bool selected = false;
};

struct PortGlyph {
    std::string name;
    Vec3 pos{};
    Vec3 dir{};                  // единичное направление наружу
    double od = 0.0;
    std::vector<Vec3> ring;      // окружность ∅ порта в плоскости ⟂ направлению, замкнутая (первая точка = последняя)
    Vec3 arrow_tip{};            // конец стрелки направления
    std::optional<std::string> occupied_by;
    bool ref_ok = true;
    bool show_zone = false;      // зона вылета и линия связи показаны: порт выбран или наведён, либо трассировка
    std::optional<std::pair<Vec3, Vec3>> zone;   // ось зоны вылета: от порта вдоль направления на min_straight, радиус od/2
    std::optional<std::pair<Vec3, Vec3>> link;   // линия связи: от порта к ближайшему концу занявшей его трубы
};

struct SceneOptions {
    double gap = kDefaultGapMm;
    std::vector<std::pair<std::string, std::string>> skip_pairs;   // пары труб без проверки зазора
    std::set<std::string> selected_tubes;
    std::set<std::string> selected_ports;   // выбранные и наведённые порты
    bool trace = false;                     // трассировка: зоны и связи у всех портов
    double arrow_len = 0.0;                 // мм; 0 — полтора ∅ порта
    int ring_segments = 24;                 // отрезков окружности порта (не меньше 8)
};

struct Scene {
    std::vector<SceneTube> tubes;
    std::vector<PortGlyph> ports;
    std::vector<ClearanceMark> marks;
    std::optional<ViewFit> fit_all;        // все трубы (с учётом ∅) и порты; пустая сцена — nullopt
    std::optional<ViewFit> fit_selected;   // выбранные трубы и порты; ничего не выбрано — nullopt
};

// Исключение std::invalid_argument при повторе идентификатора трубы или имени порта.
Scene build_scene(const std::vector<SceneTubeInput>& tubes, const std::vector<Port>& ports,
                  const SceneOptions& options = {});

// JSON сцены (формат «bendcore-scene», версия 1; мм; ключи в camelCase). Числа — %.10g, нечисловые — null.
std::string scene_to_json(const Scene& scene);

}  // namespace bendcore
