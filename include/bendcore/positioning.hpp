// Правила позиционирования трубы в сборке (этап 3 фазы 23). Без OCCT.
// Опирается на порты (ports.hpp) и ось пути (tubegeom.hpp::trace).
//
// Что делает модуль:
//  1. frame_from_port / place_from_port — ставит трубу концом А на порт: начало пути в точке порта,
//     первая касательная вдоль направления порта (наружу), поворот вокруг оси `roll_deg`.
//  2. end_connection_issues — проверка конца Б по второму порту (положение, направление, ∅, вылет).
//  3. solve_roll — подбор поворота вокруг оси порта А, при котором конец Б приходит на порт Б.
//  4. check_clearance — зазор между трубами одной сборки (норма 5 мм): пересечение — метка «mark_cross»,
//     зазор меньше нормы — метка «mark_gap»; обе без блокировки (решение по зазору).
// Единицы: мм и градусы.
#pragma once

#include <string>
#include <utility>
#include <vector>

#include "bendcore/ports.hpp"
#include "bendcore/tubegeom.hpp"
#include "bendcore/vec3.hpp"

namespace bendcore {

inline constexpr double kDefaultGapMm = 5.0;   // норма зазора между трубой и окружением / между трубами

// Жёсткое преобразование: локальный путь (старт в нуле, касательная вдоль +X, «верх» +Z) -> сборка.
struct Frame {
    Vec3 origin{0.0, 0.0, 0.0};
    Vec3 x{1.0, 0.0, 0.0};   // образ локального +X (касательная начала пути)
    Vec3 y{0.0, 1.0, 0.0};
    Vec3 z{0.0, 0.0, 1.0};

    Vec3 rotate(const Vec3& local) const;   // только поворот (для направлений)
    Vec3 apply(const Vec3& local) const;    // поворот и перенос (для точек)
};

// Система координат порта: x = направление порта (наружу), origin = положение порта.
// roll_deg — поворот вокруг оси x (правый винт). Исключение std::invalid_argument при нулевом направлении.
Frame frame_from_port(const Port& port, double roll_deg = 0.0);

struct Placement {
    Frame frame;
    std::vector<Vec3> centerline;   // ось трубы в сборке (ломаная; дуги — steps_per_bend точек на 90°)
    Vec3 end_pos{};                 // положение конца Б (точное, без погрешности дискретизации дуги)
    Vec3 end_dir{};                 // касательная в конце Б, по ходу трубы от А к Б
};

// Труба концом А на порте. Исключения как у trace (S < 0, нет «верха» у спирали).
Placement place_from_port(const Path& path, const Port& start_port, double roll_deg = 0.0, int steps_per_bend = 64);

// Присоединение конца Б к порту: ∅, вылет последнего прямого, положение и направление конца.
// Направление «от порта в трубу» у конца Б равно -end_dir; допуски берутся из порта.
std::vector<Issue> end_connection_issues(const Placement& placed, const Port& end_port, const Path& path,
                                         double od, const std::string& tube_id);

struct RollSolution {
    double roll_deg = 0.0;   // (-180, 180]
    double pos_dev = 0.0;    // мм, расстояние от конца Б до порта Б
    double dir_dev = 0.0;    // градусы, отклонение направления конца Б от порта Б
    bool within_tol = false; // pos_dev и dir_dev в допусках порта Б
};

// Поворот вокруг оси порта А, минимизирующий уход конца Б от порта Б.
// Грубый перебор шагом 2° и уточнение; если поворот ни на что не влияет (конец на оси порта А,
// направление совпадает), остаётся 0. Нужен путь с ненулевой длиной оси; иначе std::invalid_argument.
RollSolution solve_roll(const Path& path, const Port& start_port, const Port& end_port, int steps_per_bend = 64);

struct PlacedTube {
    std::string id;
    std::vector<Vec3> centerline;
    double od = 0.0;
};

// Расстояние между отрезками, мм.
double segment_distance(const Vec3& p1, const Vec3& q1, const Vec3& p2, const Vec3& q2);

// Кратчайшее расстояние между двумя ломаными, мм.
double polyline_distance(const std::vector<Vec3>& a, const std::vector<Vec3>& b);

// Зазор между поверхностями труб: пересечение — «mark_cross», зазор меньше нормы — «mark_gap».
// Пары из skip_pairs (в любом порядке id) пропускаются — например, трубы, состыкованные в одном узле.
// Сложность: пары труб с отсечением по габаритным коробкам; для тысяч труб нужен пространственный индекс.
std::vector<Issue> check_clearance(const std::vector<PlacedTube>& tubes, double gap = kDefaultGapMm,
                                   const std::vector<std::pair<std::string, std::string>>& skip_pairs = {});

}  // namespace bendcore
