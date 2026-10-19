// Порты (спецификация 5.3). Порт этапа 4. Порт C++ от ports.py.
#pragma once

#include <optional>
#include <string>
#include <vector>

#include "bendcore/tubegeom.hpp"
#include "bendcore/vec3.hpp"

namespace bendcore {

inline constexpr double kDefaultTolPos = 3.0;   // мм, допуск положения конца (6.13)
inline constexpr double kDefaultTolAng = 1.0;   // градусы, допуск направления (6.13)

struct Port {
    std::string name;
    Vec3 pos{};                               // мм
    Vec3 direction{};                         // вектор наружу
    double od = 0.0;                          // требуемый ∅, мм
    double min_straight = 0.0;                // минимальный вылет прямого участка, мм
    double tol_pos = kDefaultTolPos;
    double tol_ang = kDefaultTolAng;
    std::string conn_type;                    // справочный атрибут
    bool ref_ok = true;                       // ссылка на признак окружения валидна
    std::optional<std::string> occupied_by;   // id трубы, если порт занят
    bool associative = true;                  // порт по точке + направлению + ∅ (5.3)
    std::optional<double> socket_depth;       // измеренная глубина раструба фитинга (9.11)

    void flip();                              // «Развернуть» (5.3)
    Vec3 unit_dir() const;
};

double first_straight(const Path& path);
double last_straight(const Path& path);

// Проверки присоединения конца трубы (А — начало пути, Б — конец) к порту.
// end_pos / end_dir — положение конца в сборке и направление от порта в трубу; без них не проверяются.
std::vector<Issue> check_connection(const Port& port, const Path& path, double od, const std::string& tube_id,
                                    char end = 'A', std::optional<Vec3> end_pos = std::nullopt,
                                    std::optional<Vec3> end_dir = std::nullopt);

// Порт «по точке + направлению + ∅» (5.3): ручной способ. direction — наружу, нормируется.
// Исключение std::invalid_argument при od <= 0 или нулевом направлении.
Port manual_port(const Vec3& pos, const Vec3& direction, double od, const std::string& name);

std::string auto_name(const std::string& component, const std::vector<std::string>& existing);
std::optional<Issue> socket_hint(const Port& port, double E_current, bool decided = false);
std::pair<Vec3, Vec3> start_from_port(const Port& port);   // позиция и направление порта
std::vector<const Port*> candidates(const std::vector<Port>& ports, double od);
std::vector<Issue> connect(Port& port, const std::string& tube_id, const Path& path, double od);

}  // namespace bendcore
