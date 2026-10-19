// Тело трубы по осевой линии пути. Интерфейс без типов OCCT: реализация — src/occt/tube_body_occt.cpp
// (включается при BENDCORE_WITH_OCCT). Версия 1: сплошной круглый профиль, без стенки и концов.
#pragma once

#include <vector>

#include "bendcore/ports.hpp"
#include "bendcore/tubegeom.hpp"

namespace bendcore::geom {

struct TubeSolidResult {
    bool valid = false;        // BRepCheck_Analyzer: тело валидно
    double volume = 0.0;       // мм³
    double axis_length = 0.0;  // мм, длина оси по кускам
};

// Сплошная труба ∅od по осевой линии path. Исключение std::invalid_argument при неверном пути.
TubeSolidResult make_solid_tube(const Path& path, double od_mm);

// Порты по кромкам концов тела (окружности на плоских торцах). Направление — наружу из трубы.
// Стыки участков и швы не дают портов. Версия 1: только кромки; ручные способы — отдельно.
std::vector<Port> end_ports(const Path& path, double od_mm);

}  // namespace bendcore::geom
