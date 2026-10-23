// Пакетная проверка труб сборки (4.2, 4.3, 4.5): все трубы за один запуск, единая таблица нарушений.
// Классы — по таблице 4.5. Коллизии с окружением (B-rep) здесь не считаются: они требуют OCCT;
// между трубами проверяются пересечение и нарушение зазора (пометка, не блокирует).
#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "bendcore/bend_output.hpp"
#include "bendcore/tubegeom.hpp"

namespace bendcore::check {

// Класс нарушения (4.5) в порядке убывания тяжести.
enum class Severity { Geometric = 0, Technological = 1, Warning = 2, Mark = 3, Info = 4 };
const char* severity_name(Severity s);  // «Геометрическая ошибка», …

// Труба для проверки: путь, лимиты профиля и положение в сборке.
struct TubeInput {
    std::string id;                 // идентификатор трубы в сборке
    std::string name;               // имя для таблицы (по умолчанию id)
    Path path;
    double od = 0.0;                // наружный ∅, мм
    double B_min = 0.0;             // диапазон угла профиля, градусы
    double B_max = 180.0;
    double L_min = 0.0;             // мин. прямой между гибами, мм
    double N_first = 0.0;           // мин. участок заготовки на конце подачи и на другом конце, мм
    double N_last = 0.0;
    double E_A = 0.0, E_B = 0.0;    // глубина вставки на концах, мм
    bool cut_allowed = true;        // подрезка допускается
    std::optional<double> spiral_min_pitch;
    bool spiral_allowed = true;
    output::Frame placement;        // положение трубы в сборке (СК детали → СК сборки)
};

struct Violation {
    std::string tube_id;
    int element = -1;               // номер элемента пути; −1 — вся труба
    Severity severity = Severity::Info;
    std::string rule;               // код правила: "S<0", "Lmin", "B_max", "clearance", …
    std::string message;
    std::optional<double> value;    // факт
    std::optional<double> limit;    // норма
    std::string other_tube;         // вторая труба для коллизий
    std::optional<double> at_s;     // место на трубе: длина по оси от конца А, мм
};

// Состояния проверки (4.2). «Не проверено», «идёт проверка» и «устарело» ведёт интерфейс по отпечатку.
enum class TubeState { Passed, HasViolations };

struct TubeSummary {
    TubeState state = TubeState::Passed;
    std::optional<Severity> worst;  // худший класс, nullopt — нарушений нет
    bool blocks_output = false;     // геометрическая или технологическая ошибка (3.6, 7.7)
    std::uint64_t fingerprint = 0;  // отпечаток входа: изменился — результат устарел
};

struct Options {
    double clearance = 5.0;         // норма зазора между трубами, мм (4.3)
    bool tube_to_tube = true;
};

struct Result {
    std::vector<Violation> violations;          // по классу, затем по трубе и элементу
    std::map<std::string, TubeSummary> tubes;
    std::map<Severity, int> counts;
    int pairs_tested = 0;                       // пары труб после широкой фазы
};

std::uint64_t fingerprint(const TubeInput& t);
Result run(const std::vector<TubeInput>& tubes, const Options& opt = {});

// Минимальное расстояние между осевыми полилиниями и длины по осям в ближайших точках.
struct Closest {
    double distance = 0.0;
    double s_a = 0.0, s_b = 0.0;
};
Closest closest_polylines(const std::vector<Vec3>& a, const std::vector<Vec3>& b);

}  // namespace bendcore::check
