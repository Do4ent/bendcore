// Ссылки на окружение, обновление и адаптивность (5.7). Порт refs.py.
// Пересчёт вершин выполняется внешним решателем (передаётся функцией): PlaneGCS ещё не выбран окончательно.
#pragma once

#include <functional>
#include <set>
#include <string>
#include <vector>

#include "bendcore/vec3.hpp"

namespace bendcore {

inline const std::string kValid = "валидна";
inline const std::string kOutdated = "устарела";
inline const std::string kNotLoaded = "не загружена";
inline const std::string kLost = "потеряна";

struct Ref {
    std::string ref_id;
    int vertex = 0;
    Vec3 cached{};
    std::string state = kValid;
};

struct Tube {
    std::vector<Vec3> points;
    std::vector<Ref> refs;
    std::string mode = "адаптивна";   // адаптивна | заморожена
};

struct UpdateResult {
    std::string status;               // все в норме | вершины сдвинулись | потеряны ссылки | заморожена | не загружена
    std::vector<std::string> moved;
    std::vector<std::string> lost;
    std::vector<Vec3> points;
};

// Совпадение вершины с признаком (ограничение «coincide») и фиксация прочих вершин.
struct Coincide {
    int vertex;
    Vec3 target;
};

// Решатель: по начальным точкам, совпадениям и индексам зафиксированных вершин возвращает новые точки.
using PointSolver = std::function<std::vector<Vec3>(const std::vector<Vec3>& start,
                                                    const std::vector<Coincide>& coincide,
                                                    const std::vector<int>& fixed)>;

std::string initial_mode(const std::string& origin);   // ошибка при неизвестном происхождении

UpdateResult update_component(Tube& tube, const std::vector<std::pair<std::string, Vec3>>& features,
                              const PointSolver& solve, double tol = 1e-6,
                              const std::set<std::string>& unloaded = {});

}  // namespace bendcore
