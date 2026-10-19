// Этап 1. Ядро геометрии пути гнутой трубы (C++20).
// Порт tubegeom.py. Эталон — Python-реализация; значения сверяются тестами.
// Единицы: мм и градусы, внутри — double.
#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <tuple>
#include <variant>
#include <vector>

#include "bendcore/vec3.hpp"

namespace bendcore {

inline constexpr double kDegenerateBDeg = 0.5;  // порог вырожденного гиба (2.4)

struct Straight {
    double S;  // мм
};

struct Bend {
    double R;            // мм, радиус по оси
    double B;            // градусы, знак — направление
    double A = 0.0;      // градусы, поворот вокруг касательной перед гибом
};

struct Spiral {
    double R;            // мм, радиус витка (= R оправки)
    double p;            // мм, шаг
    double n;            // число витков
    int direction = +1;  // +1 правая, -1 левая
};

using Item = std::variant<Straight, Bend, Spiral>;
using Path = std::vector<Item>;

enum class Profile { Both, Right, Left };     // направление профиля
enum class Side { None, Right, Left };        // «П» / «Л» в таблице гибов

struct TableRow {
    double A;   // поворот, (−180°, 180°]
    double B;   // угол ≥ 0
    Side side;  // направление; None для профиля с одним направлением
};

double tangent_length(double R, double B_deg);
double axis_length(const Path& path);
Bend normalize_rotation(const Bend& bend, Profile direction = Profile::Both);
double wrap180(double a_deg);
TableRow table_row(const Bend& bend, Profile direction = Profile::Both, bool minimize = true);

// Центральная линия трубы как ломаная. Вырожденный гиб (|B| < порога) — прямой участок.
// Спираль: ось по «верху» трубы, касательная к соседним прямым (1.2.1). Бросает std::invalid_argument при S < 0.
std::vector<Vec3> trace(const Path& path, int steps_per_bend = 64, Vec3 start = {0.0, 0.0, 0.0});

// ---------- Проверки пути (1.2.1, 1.5, 3.4, 4.5) ----------
struct Issue {
    std::string kind;   // "geometric" | "technological" | "warning" | "suggestion"
    int index;          // номер элемента пути; -1 — весь путь
    std::string text;   // пояснение (русский, UTF-8)
};

std::vector<Issue> validate(const Path& path, double B_max = 180.0, double B_min = 0.0,
                            std::optional<double> od = std::nullopt,
                            std::optional<double> spiral_min_pitch = std::nullopt, bool spiral_allowed = true);

// Самопересечение осевой (1.5, 4.5): (s1, s2, расстояние) для пар несмежных участков ближе ∅.
std::vector<std::tuple<double, double, double>> self_intersections(const Path& path, double od, int max_points = 2500);

// Точность угла ручного трубогиба по шкале (2.2.1, 3.4).
double manual_angle_tolerance(double B, const std::vector<double>& ticks, const std::vector<double>& stops = {},
                              double on_tick = 0.5, double on_stop = 0.2, double between = 2.0, double eps = 1e-6);

// Уход конца трубы по точности профиля (3.4). tol_B — по одному на гиб.
struct Drift {
    double worst_pos, worst_dir, probable_pos, probable_dir;
    std::vector<std::tuple<int, std::string, double>> shares;  // (гиб №, источник, доля)
    bool warning;
};
Drift end_drift(const Path& path, const std::vector<double>& tol_B, double tol_A, double tol_S,
                double tol_pos = 3.0, double tol_dir_deg = 1.0);

// Таблица S–A–B: строки (S, R, B, A); B = 0 — только прямой участок.
Path path_from_table(const std::vector<std::array<double, 4>>& rows);

}  // namespace bendcore
