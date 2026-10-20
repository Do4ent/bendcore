// Тело трубы по осевой линии пути. Интерфейс без типов OCCT: реализация — src/occt/tube_body_occt.cpp
// (включается при BENDCORE_WITH_OCCT). Труба сплошная (wall = 0) или с осевым отверстием (0 < wall < od/2).
#pragma once

#include <string>
#include <vector>

#include "bendcore/ports.hpp"
#include "bendcore/tube_model.hpp"
#include "bendcore/tubegeom.hpp"

namespace bendcore::geom {

struct TubeSolidResult {
    bool valid = false;        // BRepCheck_Analyzer: тело валидно
    double volume = 0.0;       // мм³
    double axis_length = 0.0;  // мм, длина оси по кускам
};

// Труба ∅od по осевой линии path; wall_mm — толщина стенки (0 — сплошная).
// Объём: π·r²·L (сплошная), π·(r² − r_в²)·L (с отверстием); формулы точные при r < радиуса гиба.
// Исключение std::invalid_argument при неверном пути или при wall < 0 либо wall ≥ od/2.
TubeSolidResult make_tube(const Path& path, double od_mm, double wall_mm = 0.0);

// Сплошная труба (совместимость): make_tube(path, od_mm, 0).
TubeSolidResult make_solid_tube(const Path& path, double od_mm);

// Порты по кромкам концов тела: окружности на плоских торцах. Направление — наружу из трубы.
// Для трубы со стенкой на торце две окружности (наружная и внутренняя) — порт берётся по наружной (∅ = od).
// Стыки участков и швы не дают портов.
std::vector<Port> end_ports(const Path& path, double od_mm, double wall_mm = 0.0);

// Порт на грани тела (цилиндр, тор или торец): точка поверхности, ближайшая к pick, и нормаль наружу в ней.
// ∅ порта = od_mm. Если pick внутри тела или снаружи, всё равно берётся ближайшая точка поверхности.
Port face_port(const Path& path, double od_mm, const Vec3& pick, const std::string& name, double wall_mm = 0.0);

// Полная модель: для трубы по пути добавляет концевые порты к ручным (тело — по path, od и wall модели).
// Труба из STEP (пути нет) возвращается без изменений. Порты с тем же центром и направлением не дублируются.
TubeModel complete_model(const TubeModel& model);

// Труба из файла STEP (тело целиком, без пути): порты — кромки торцов, od — наибольший ∅ порта.
// Стенка — по внутренней окружности торца: (∅нар − ∅вн) / 2. Если торцы дают разные значения или
// внутренней окружности нет, wall = 0 (стенка не определена). Исключение std::invalid_argument, если файл не читается или тела нет.
TubeModel load_step(const std::string& file, const std::string& id);

// Запись тела трубы по пути в STEP (обмен и тесты). Исключение std::invalid_argument при ошибке записи.
void export_step(const Path& path, double od_mm, double wall_mm, const std::string& file);

}  // namespace bendcore::geom
